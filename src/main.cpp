#include "lib/net/http/server.hpp"

using namespace net::http;

int main()
{
    HTTPServer app;

    app.set_idle_timeout_ms(15000);
    app.set_route_idle_timeout_ms("/slow", 60000);

    app.get("/", [](Request &req, Response &res)
            { return res.send("Hello, World!"); });

        app.get("/health", [](Request &req, Response &res)
            { return res.send("OK", 200, "text/plain"); });

    app.post("/echo", [](Request &req, Response &res)
             { return res.send(req.body, 200, "text/plain"); });

        app.put("/resource", [](Request &req, Response &res)
            { return res.send("Updated", 200, "text/plain"); });

        app.patch("/resource", [](Request &req, Response &res)
              { return res.send("Patched", 200, "text/plain"); });

        app.del("/resource", [](Request &req, Response &res)
            { return res.send("Deleted", 200, "text/plain"); });

        app.head("/health", [](Request &req, Response &res)
             { return res.send("", 200, "text/plain"); });

        app.options("/resource", [](Request &req, Response &res)
            { return res.send("", 204, "text/plain"); });

    app.listen(8080);

    return 0;
}