#include <iostream>

#include "lib/net/http/server.hpp"
using namespace net::http;



int main()
{
    HTTPServer app;
    constexpr uint16_t port = 8080;
    app.set_idle_timeout_ms(15000);
    app.set_route_idle_timeout_ms("/slow", 60000);
    app.get("/", [](Request &req, Response &res) { return res.send("Hello, World!"); });
    app.get("/health", [](Request &req, Response &res) { return res.send("OK"); });
    app.post("/echo", [](Request &req, Response &res) { return res.send(req.body); });
    app.put("/resource", [](Request &req, Response &res) { return res.send("Updated"); });
    app.patch("/resource", [](Request &req, Response &res) { return res.send("Patched"); });
    app.del("/resource", [](Request &req, Response &res) { return res.send("Deleted"); });
    app.head("/health", [](Request &req, Response &res) { return res.send(""); });
    app.options("/resource", [](Request &req, Response &res) { return res.send("", HTTPStatus::NoContent); });
    app.listen(port, []() { std::cout << "Server is running on http://localhost:" << port << std::endl; });

    return 0;
}