#include "event_loop.hpp"

#ifndef _WIN32

#include <algorithm>
#include <chrono>
#include <coroutine>
#include <exception>
#include <string>
#include <vector>

#include <errno.h>
#include <sys/epoll.h>
#include <unistd.h>

namespace net::http
{
    namespace
    {
        constexpr int kReadBufferSize = 4096;

        struct FireAndForget
        {
            struct promise_type
            {
                FireAndForget get_return_object() noexcept { return {}; }
                std::suspend_never initial_suspend() noexcept { return {}; }
                std::suspend_never final_suspend() noexcept { return {}; }
                void return_void() noexcept {}
                void unhandled_exception() { std::terminate(); }
            };
        };

        class EpollLoop;

        class EpollAwaitable
        {
        public:
            EpollAwaitable(EpollLoop &loop, int fd, uint32_t events, int timeout_ms = 0);

            bool await_ready() noexcept { return false; }
            void await_suspend(std::coroutine_handle<> h);
            bool await_resume();

            void resume();
            void timeout();
            bool is_expired(std::chrono::steady_clock::time_point now) const;

        private:
            EpollLoop &loop_;
            int fd_{-1};
            uint32_t events_{0};
            std::coroutine_handle<> handle_{};
            int timeout_ms_{0};
            bool timed_out_{false};
            std::chrono::steady_clock::time_point deadline_{};
        };

        class EpollLoop
        {
        public:
            EpollLoop();
            ~EpollLoop();

            int handle() const { return epoll_fd_; }
            void add(int fd, uint32_t events, void *data);
            void remove(int fd);
            void run();

            void register_timeout(EpollAwaitable *waiter);
            void clear_timeout(EpollAwaitable *waiter);

        private:
            void check_timeouts();

            int epoll_fd_{-1};
            std::vector<EpollAwaitable *> waiters_;
        };

        inline EpollAwaitable::EpollAwaitable(EpollLoop &loop, int fd, uint32_t events, int timeout_ms)
            : loop_(loop), fd_(fd), events_(events), timeout_ms_(timeout_ms)
        {
        }

        inline void EpollAwaitable::await_suspend(std::coroutine_handle<> h)
        {
            handle_ = h;
            loop_.add(fd_, events_, this);
            if (timeout_ms_ > 0)
            {
                deadline_ = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms_);
                loop_.register_timeout(this);
            }
        }

        inline bool EpollAwaitable::await_resume()
        {
            if (!timed_out_)
            {
                loop_.clear_timeout(this);
            }
            return timed_out_;
        }

        inline void EpollAwaitable::resume()
        {
            loop_.remove(fd_);
            timed_out_ = false;
            if (handle_)
            {
                handle_.resume();
            }
        }

        inline void EpollAwaitable::timeout()
        {
            loop_.remove(fd_);
            timed_out_ = true;
            if (handle_)
            {
                handle_.resume();
            }
        }

        inline bool EpollAwaitable::is_expired(std::chrono::steady_clock::time_point now) const
        {
            return timeout_ms_ > 0 && deadline_ <= now;
        }

        inline EpollLoop::EpollLoop()
        {
            epoll_fd_ = epoll_create1(0);
        }

        inline EpollLoop::~EpollLoop()
        {
            if (epoll_fd_ >= 0)
            {
                ::close(epoll_fd_);
            }
        }

        inline void EpollLoop::add(int fd, uint32_t events, void *data)
        {
            epoll_event event{};
            event.events = events;
            event.data.ptr = data;
            if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &event) != 0)
            {
                epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &event);
            }
        }

        inline void EpollLoop::remove(int fd)
        {
            epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
        }

        inline void EpollLoop::run()
        {
            constexpr int kMaxEvents = 64;
            epoll_event events[kMaxEvents];
            while (true)
            {
                int ready = epoll_wait(epoll_fd_, events, kMaxEvents, 250);
                if (ready <= 0)
                {
                    check_timeouts();
                    continue;
                }
                for (int i = 0; i < ready; ++i)
                {
                    auto *waiter = static_cast<EpollAwaitable *>(events[i].data.ptr);
                    if (waiter)
                    {
                        waiter->resume();
                    }
                }
                check_timeouts();
            }
        }

        inline void EpollLoop::register_timeout(EpollAwaitable *waiter)
        {
            waiters_.push_back(waiter);
        }

        inline void EpollLoop::clear_timeout(EpollAwaitable *waiter)
        {
            waiters_.erase(std::remove(waiters_.begin(), waiters_.end(), waiter), waiters_.end());
        }

        inline void EpollLoop::check_timeouts()
        {
            auto now = std::chrono::steady_clock::now();
            std::vector<EpollAwaitable *> expired;
            for (auto *waiter : waiters_)
            {
                if (waiter && waiter->is_expired(now))
                {
                    expired.push_back(waiter);
                }
            }
            waiters_.erase(std::remove_if(waiters_.begin(), waiters_.end(), [&](EpollAwaitable *waiter)
                                          { return waiter && waiter->is_expired(now); }),
                           waiters_.end());
            for (auto *waiter : expired)
            {
                waiter->timeout();
            }
        }

        FireAndForget handle_client(net::socket::SocketHandle client, int idle_timeout_ms, RawRequestHandler handler, EpollLoop &loop)
        {
            std::string pending;
            char buffer[kReadBufferSize];
            int current_timeout_ms = idle_timeout_ms;

            while (true)
            {
                EpollAwaitable wait_read(loop, static_cast<int>(client), EPOLLIN, current_timeout_ms);
                bool timed_out = co_await wait_read;
                if (timed_out)
                {
                    break;
                }
                std::string error;
                int received = net::socket::recv_data(client, buffer, kReadBufferSize, &error);
                if (received < 0 && net::socket::would_block(net::socket::get_last_error()))
                {
                    continue;
                }
                if (received <= 0)
                {
                    break;
                }

                pending.append(buffer, static_cast<std::size_t>(received));
                while (true)
                {
                    if (pending.find("\r\n\r\n") == std::string::npos)
                    {
                        break;
                    }

                    RequestDecision decision = handler(client, pending);
                    if (decision.next_idle_timeout_ms > 0)
                    {
                        current_timeout_ms = decision.next_idle_timeout_ms;
                    }
                    if (decision.action == RequestAction::NeedMoreData)
                    {
                        break;
                    }
                    if (decision.action == RequestAction::Close)
                    {
                        net::socket::close_socket(client);
                        co_return;
                    }

                    if (pending.empty())
                    {
                        break;
                    }
                }
            }

            net::socket::close_socket(client);
        }

        FireAndForget accept_loop(net::socket::SocketHandle listener, int idle_timeout_ms, RawRequestHandler handler, EpollLoop &loop)
        {
            while (true)
            {
                EpollAwaitable wait_accept(loop, static_cast<int>(listener), EPOLLIN);
                co_await wait_accept;
                while (true)
                {
                    std::string error;
                    net::socket::SocketHandle client = net::socket::accept_client(listener, &error);
                    if (client == net::socket::kInvalidSocket)
                    {
                        if (net::socket::would_block(net::socket::get_last_error()))
                        {
                            break;
                        }
                        break;
                    }
                    handle_client(client, idle_timeout_ms, handler, loop);
                }
            }
        }
    }

    void run_event_loop(net::socket::SocketHandle listener, int idle_timeout_ms, RawRequestHandler handler)
    {
        EpollLoop loop;
        accept_loop(listener, idle_timeout_ms, std::move(handler), loop);
        loop.run();
    }
}

#endif
