#pragma once

#include <memory>
#include <crow.h>
#include "../engine/GameEngine.hpp"
#include "SessionManager.hpp"

namespace FindTheBug {

    class HttpServer {
    public:
        HttpServer(std::shared_ptr<GameEngine> engine);
        ~HttpServer() = default;

        void run(uint16_t port = 8080);

    private:

        crow::response handleAction(const crow::request& req);
        void handleWebSocketOpen(crow::websocket::connection& conn);
        void handleWebSocketClose(crow::websocket::connection& conn, const std::string& reason);
        void handleWebSocketMessage(crow::websocket::connection& conn,
            const std::string& data, bool is_binary);

        std::shared_ptr<GameEngine> engine;
        std::shared_ptr<SessionManager> sessionManager;
    };

}