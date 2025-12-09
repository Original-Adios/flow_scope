#pragma once
#include <httplib.h>
#include "../core/manager.hpp"

namespace flow_scope {

class HttpServer {
public:
    HttpServer(int port = 8080) : port_(port) {
        svr_.Get("/metrics", [](const httplib::Request&, httplib::Response& res) {
            auto& mgr = Manager::get_instance();
            auto data = mgr.get_snapshot();
            
            res.set_content(data->to_json().dump(), "application/json");
        });
    }

    void start() {
        printf("Starting flow_scope HTTP Server on port %d...\n", port_);
        svr_.listen("0.0.0.0", port_);
    }

private:
    httplib::Server svr_;
    int port_;
};

} // namespace flow_scope