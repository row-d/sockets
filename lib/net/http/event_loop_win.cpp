#include "event_loop.hpp"

#ifdef _WIN32

#include <algorithm>
#include <chrono>
#include <coroutine>
#include <cstring>
#include <exception>
#include <string>
#include <vector>

#include <mswsock.h>
#include <winsock2.h>
#include <ws2tcpip.h>

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

        struct OverlappedBase
        {
            OVERLAPPED overlapped{};
            std::coroutine_handle<> handle{};
            int bytes{0};
            int error{0};
            HANDLE cancel_handle{nullptr};
            bool timed_out{false};

            void complete(DWORD transferred, DWORD error_code)
            {
                bytes = static_cast<int>(transferred);
                error = static_cast<int>(error_code);
                if (handle)
                {
                    handle.resume();
                }
            }
        };

        class IocpLoop
        {
        public:
            IocpLoop()
            {
                iocp_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
            }

            ~IocpLoop()
            {
                if (iocp_)
                {
                    CloseHandle(iocp_);
                }
            }

            HANDLE handle() const { return iocp_; }

            void run()
            {
                constexpr DWORD kIocpWaitMs = 100;
                while (true)
                {
                    DWORD bytes = 0;
                    ULONG_PTR key = 0;
                    LPOVERLAPPED overlapped = nullptr;
                    BOOL ok = GetQueuedCompletionStatus(iocp_, &bytes, &key, &overlapped, kIocpWaitMs);
                    if (!overlapped)
                    {
                        check_timeouts();
                        continue;
                    }
                    auto *op = reinterpret_cast<OverlappedBase *>(overlapped);
                    DWORD error = ok ? 0 : GetLastError();
                    op->complete(bytes, error);
                }
            }

            void register_timeout(OverlappedBase *op, std::chrono::steady_clock::time_point deadline)
            {
                timeouts_.push_back({op, deadline});
            }

            void clear_timeout(OverlappedBase *op)
            {
                timeouts_.erase(std::remove_if(timeouts_.begin(), timeouts_.end(), [&](const PendingTimeout &entry)
                                               { return entry.op == op; }),
                                timeouts_.end());
            }

        private:
            struct PendingTimeout
            {
                OverlappedBase *op;
                std::chrono::steady_clock::time_point deadline;
            };

            void check_timeouts()
            {
                auto now = std::chrono::steady_clock::now();
                for (auto &entry : timeouts_)
                {
                    if (entry.deadline <= now && entry.op && entry.op->cancel_handle)
                    {
                        entry.op->timed_out = true;
                        CancelIoEx(entry.op->cancel_handle, &entry.op->overlapped);
                    }
                }
                timeouts_.erase(std::remove_if(timeouts_.begin(), timeouts_.end(), [&](const PendingTimeout &entry)
                                               { return entry.op && entry.op->timed_out; }),
                                timeouts_.end());
            }

            HANDLE iocp_{nullptr};
            std::vector<PendingTimeout> timeouts_;
        };

        class AcceptAwaitable : public OverlappedBase
        {
        public:
            AcceptAwaitable(IocpLoop &loop, net::socket::SocketHandle listener)
                : loop_(loop), listener_(listener)
            {
                std::memset(buffer_, 0, sizeof(buffer_));
            }

            bool await_ready() noexcept { return false; }

            bool await_suspend(std::coroutine_handle<> h)
            {
                handle = h;
                if (!accept_ex_)
                {
                    error = WSAEINVAL;
                    return false;
                }
                accept_socket_ = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
                if (accept_socket_ == net::socket::kInvalidSocket)
                {
                    error = WSAGetLastError();
                    return false;
                }

                CreateIoCompletionPort(reinterpret_cast<HANDLE>(accept_socket_), loop_.handle(), 0, 0);

                DWORD bytes = 0;
                BOOL ok = accept_ex_(listener_, accept_socket_, buffer_, 0,
                                    sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16, &bytes, &overlapped);
                if (ok)
                {
                    bytes = static_cast<DWORD>(bytes);
                    complete(bytes, 0);
                    return false;
                }

                int last = WSAGetLastError();
                if (last != ERROR_IO_PENDING)
                {
                    error = last;
                    return false;
                }

                return true;
            }

            net::socket::SocketHandle await_resume()
            {
                if (error != 0)
                {
                    if (accept_socket_ != net::socket::kInvalidSocket)
                    {
                        net::socket::close_socket(accept_socket_);
                    }
                    return net::socket::kInvalidSocket;
                }

                setsockopt(accept_socket_, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
                           reinterpret_cast<char *>(&listener_), sizeof(listener_));
                return accept_socket_;
            }

            void set_accept_ex(LPFN_ACCEPTEX accept_ex)
            {
                accept_ex_ = accept_ex;
            }

        private:
            IocpLoop &loop_;
            net::socket::SocketHandle listener_{};
            net::socket::SocketHandle accept_socket_{net::socket::kInvalidSocket};
            char buffer_[2 * (sizeof(sockaddr_in) + 16)]{};
            LPFN_ACCEPTEX accept_ex_{nullptr};
        };

        struct RecvResult
        {
            int bytes;
            bool timed_out;
        };

        class RecvAwaitable : public OverlappedBase
        {
        public:
            RecvAwaitable(IocpLoop &loop, net::socket::SocketHandle socket, char *buffer, int length, int timeout_ms)
                : loop_(loop), socket_(socket), buffer_(buffer), length_(length), timeout_ms_(timeout_ms)
            {
            }

            bool await_ready() noexcept { return false; }

            bool await_suspend(std::coroutine_handle<> h)
            {
                handle = h;
                cancel_handle = reinterpret_cast<HANDLE>(socket_);
                WSABUF wsabuf{};
                wsabuf.buf = buffer_;
                wsabuf.len = static_cast<ULONG>(length_);
                DWORD flags = 0;
                DWORD bytes = 0;
                int result = WSARecv(socket_, &wsabuf, 1, &bytes, &flags, &overlapped, nullptr);
                if (result == 0)
                {
                    complete(bytes, 0);
                    return false;
                }

                int last = WSAGetLastError();
                if (last != WSA_IO_PENDING)
                {
                    error = last;
                    return false;
                }
                if (timeout_ms_ > 0)
                {
                    loop_.register_timeout(this, std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms_));
                }
                return true;
            }

            RecvResult await_resume()
            {
                loop_.clear_timeout(this);
                if (error != 0)
                {
                    return {-1, timed_out || error == ERROR_OPERATION_ABORTED};
                }
                return {bytes, timed_out};
            }

        private:
            IocpLoop &loop_;
            net::socket::SocketHandle socket_{};
            char *buffer_{nullptr};
            int length_{0};
            int timeout_ms_{0};
        };

        static LPFN_ACCEPTEX load_accept_ex(net::socket::SocketHandle listener)
        {
            GUID guid = WSAID_ACCEPTEX;
            LPFN_ACCEPTEX accept_ex = nullptr;
            DWORD bytes = 0;
            WSAIoctl(listener, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid), &accept_ex,
                     sizeof(accept_ex), &bytes, nullptr, nullptr);
            return accept_ex;
        }

        FireAndForget handle_client(net::socket::SocketHandle client, int idle_timeout_ms, RawRequestHandler handler, IocpLoop &loop)
        {
            std::string pending;
            char buffer[kReadBufferSize];
            int current_timeout_ms = idle_timeout_ms;

            while (true)
            {
                RecvAwaitable recv_op(loop, client, buffer, kReadBufferSize, current_timeout_ms);
                RecvResult result = co_await recv_op;
                if (result.timed_out)
                {
                    break;
                }
                int received = result.bytes;
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

        FireAndForget accept_loop(net::socket::SocketHandle listener, int idle_timeout_ms, RawRequestHandler handler, IocpLoop &loop)
        {
            LPFN_ACCEPTEX accept_ex = load_accept_ex(listener);
            while (true)
            {
                AcceptAwaitable accept_op(loop, listener);
                accept_op.set_accept_ex(accept_ex);
                net::socket::SocketHandle client = co_await accept_op;
                if (client == net::socket::kInvalidSocket)
                {
                    continue;
                }

                handle_client(client, idle_timeout_ms, handler, loop);
            }
        }
    }

    void run_event_loop(net::socket::SocketHandle listener, int idle_timeout_ms, RawRequestHandler handler)
    {
        IocpLoop loop;
        CreateIoCompletionPort(reinterpret_cast<HANDLE>(listener), loop.handle(), 0, 0);
        accept_loop(listener, idle_timeout_ms, std::move(handler), loop);
        loop.run();
    }
}

#endif
