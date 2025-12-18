#include "SessionManager.hpp"
#include <iostream>
#include <vector>
#include <print>

using namespace FindTheBug;

static std::mutex global_send_mutex;
static std::mutex global_log_mutex;

void SessionManager::registerConnection(const std::string& sessionId, crow::websocket::connection* conn) {
    if (!conn) return;

    std::lock_guard<std::mutex> lock(mutex_);

    sessionConnections_[sessionId].insert(conn);

    connectionToSession_[conn] = sessionId;

    log("[SessionManager] Conexao registrada na sessao: " + sessionId);
}

void SessionManager::unregisterConnection(crow::websocket::connection* conn) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (connectionToSession_.contains(conn)) {
        std::string sessionId = connectionToSession_[conn];

        sessionConnections_[sessionId].erase(conn);

        if (sessionConnections_[sessionId].empty()) {
            sessionConnections_.erase(sessionId);
        }

        connectionToSession_.erase(conn);

        log("[SessionManager] Conexao removida da sessao: " + sessionId);
    }
}

void FindTheBug::SessionManager::closeSession(const std::string& sessionId)
{
    std::vector<crow::websocket::connection*> targets;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (sessionConnections_.contains(sessionId)) {
            for (auto* conn : sessionConnections_[sessionId]) {
                targets.push_back(conn);
            }
            sessionConnections_.erase(sessionId);
        }
    }

    for (auto* conn : targets) {
        if (conn) {
            std::lock_guard<std::mutex> lock(mutex_);
            connectionToSession_.erase(conn);
        }
        try { conn->close("Partida Encerrada"); } catch(...){}
    }
    log("[SessionManager] Sessao " + sessionId + " encerrada e limpa da RAM.");
}

void SessionManager::broadcastToSession(const std::string& sessionId, const std::string& message) {
    
    std::vector<crow::websocket::connection*> targets;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (sessionConnections_.contains(sessionId)) {
            for (auto* conn : sessionConnections_[sessionId]) {
                targets.push_back(conn);
            }
        }
    }

    if (!targets.empty()) {
        log("[SessionManager] Broadcast para " + sessionId + " (" + std::to_string(targets.size()) + " alvos)");
        for (auto* conn : targets) {
            sendTo(conn, message);
        }
    }
}

void SessionManager::sendTo(crow::websocket::connection* conn, const std::string& msg) {
    if (!conn) return;

    try {
        std::lock_guard<std::mutex> lock(global_send_mutex);
        conn->send_text(msg);
    }
    catch (const std::exception& e) {
        log("[ERRO] Falha no envio: " + std::string(e.what()));
    }
    catch (...) {
        log("[ERRO] Falha desconhecida no envio");
    }
}

void SessionManager::log(const std::string& msg) {
    try {
        std::lock_guard<std::mutex> lock(global_log_mutex);
        std::print("{}\n", msg);
    }
    catch (...) {}
}