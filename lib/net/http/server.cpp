#include "server.hpp"

#include <stdexcept>
#include <string>
#include <unordered_map>

#include "event_loop.hpp"
#include "parser.hpp"
#include "lib/net/socket/socket.hpp"

namespace net::http
{
	namespace
	{
		void send_not_found(Response &response)
		{
			response.send("Not Found", 404, "text/plain");
		}

		void send_server_error(Response &response)
		{
			response.send("Internal Server Error", 500, "text/plain");
		}
	}

	void HTTPServer::get(const std::string &path, Handler handler)
	{
		register_handler("GET", path, std::move(handler));
	}

	void HTTPServer::post(const std::string &path, Handler handler)
	{
		register_handler("POST", path, std::move(handler));
	}

	void HTTPServer::put(const std::string &path, Handler handler)
	{
		register_handler("PUT", path, std::move(handler));
	}

	void HTTPServer::del(const std::string &path, Handler handler)
	{
		register_handler("DELETE", path, std::move(handler));
	}

	void HTTPServer::patch(const std::string &path, Handler handler)
	{
		register_handler("PATCH", path, std::move(handler));
	}

	void HTTPServer::head(const std::string &path, Handler handler)
	{
		register_handler("HEAD", path, std::move(handler));
	}

	void HTTPServer::options(const std::string &path, Handler handler)
	{
		register_handler("OPTIONS", path, std::move(handler));
	}

	void HTTPServer::set_idle_timeout_ms(int timeout_ms)
	{
		idle_timeout_ms_ = timeout_ms;
	}

	void HTTPServer::set_route_idle_timeout_ms(const std::string &path, int timeout_ms)
	{
		route_idle_timeouts_[path] = timeout_ms;
	}

	void HTTPServer::register_handler(const std::string &method, const std::string &path, Handler handler)
	{
		handlers_[method][path] = std::move(handler);
	}

	void HTTPServer::listen(int port)
	{
		net::socket::SocketInit socket_guard;

		std::string error;
		net::socket::SocketHandle listener = net::socket::create_tcp_listener(static_cast<uint16_t>(port), &error);
		if (listener == net::socket::kInvalidSocket)
		{
			throw std::runtime_error(error.empty() ? "Failed to create listener" : error);
		}
		run_event_loop(listener, idle_timeout_ms_, [&](net::socket::SocketHandle client, std::string &raw)
					   {
						   Request request;
						   std::size_t consumed = 0;
						   ParseStatus status = parse_request(raw, request, consumed);
						   if (status == ParseStatus::NeedMoreData)
						   {
							   return RequestDecision{RequestAction::NeedMoreData, 0};
						   }
						   if (status == ParseStatus::Error)
						   {
							   Response response(client, false);
							   send_server_error(response);
							   return RequestDecision{RequestAction::Close, 0};
						   }

						   bool keep_alive = should_keep_alive(request);
						   Response response(client, keep_alive);
						   auto method_it = handlers_.find(request.method);
						   if (method_it != handlers_.end())
						   {
							   auto handler = method_it->second.find(request.path);
							   if (handler != method_it->second.end())
							   {
								   handler->second(request, response);
								   if (!response.is_sent())
								   {
									   send_server_error(response);
								   }
							   }
							   else
							   {
								   send_not_found(response);
							   }
						   }
						   else
						   {
							   send_not_found(response);
						   }
						   raw.erase(0, consumed);
						   int next_timeout = 0;
						   auto timeout_it = route_idle_timeouts_.find(request.path);
						   if (timeout_it != route_idle_timeouts_.end())
						   {
							   next_timeout = timeout_it->second;
						   }
						   return RequestDecision{keep_alive ? RequestAction::KeepAlive : RequestAction::Close, next_timeout};
					   });
	}
}
