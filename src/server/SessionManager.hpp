#pragma once

#include "crow.h"

namespace FindTheBug {

    class SessionManager {
    public:
        explicit SessionManager() = default;

        void registerConnection(const std::string& sessionId, const std::string& role, crow::websocket::connection* conn);
        void removeConnection(crow::websocket::connection* conn);

        crow::websocket::connection* getMaster(const std::string& sessionId);
        const std::vector<crow::websocket::connection*>& getPlayers(const std::string& sessionId);

    private:
        std::unordered_map<std::string, crow::websocket::connection*> sessionMasters;
        std::unordered_map<std::string, std::vector<crow::websocket::connection*>> sessionPlayers;
        std::unordered_map<crow::websocket::connection*, std::string> connectionToSession;
    };

}