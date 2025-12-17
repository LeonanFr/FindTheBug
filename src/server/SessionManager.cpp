#include "SessionManager.hpp"

#include "crow.h"

using namespace FindTheBug;

void SessionManager::registerConnection(const std::string& sessionId, const std::string& role, crow::websocket::connection* conn) {
    connectionToSession[conn] = sessionId;
    if (role == "MASTER") {
        sessionMasters[sessionId] = conn;
    }
    else {
        sessionPlayers[sessionId].push_back(conn);
    }
}

void SessionManager::removeConnection(crow::websocket::connection* conn) {
    if (!connectionToSession.contains(conn)) return;

    std::string sessionId = connectionToSession[conn];

    if (sessionMasters[sessionId] == conn) {
        sessionMasters.erase(sessionId);
    }
    else {
        auto& players = sessionPlayers[sessionId];
        std::erase(players, conn);
    }
    connectionToSession.erase(conn);
}

crow::websocket::connection* SessionManager::getMaster(const std::string& sessionId) {
    return sessionMasters.contains(sessionId) ? sessionMasters[sessionId] : nullptr;
}

const std::vector<crow::websocket::connection*>& SessionManager::getPlayers(const std::string& sessionId) {
    static const std::vector<crow::websocket::connection*> empty;
    return sessionPlayers.contains(sessionId) ? sessionPlayers[sessionId] : empty;
}