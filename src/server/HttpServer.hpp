#pragma once

#include <memory>
#include <crow.h>
#include <string>

#include "../engine/GameEngine.hpp"
#include "../storage/MongoStore.hpp"
#include "../infra/TaskQueue.hpp"
#include "SessionManager.hpp"

namespace FindTheBug {

    class HttpServer {
    public:
        HttpServer(
            std::shared_ptr<GameEngine> engine,
            std::shared_ptr<MongoStore> storage,
            std::shared_ptr<SessionManager> sessionManager,
            std::shared_ptr<TaskQueue> taskQueue
            );
        ~HttpServer() = default;

        void run(uint16_t port = 8080);
    private:

		crow::response handleAction(const crow::request& req);

		// WebSocket handlers
		void handleWebSocketOpen(crow::websocket::connection& conn);
		void handleWebSocketClose(crow::websocket::connection& conn, const std::string& reason);
		void handleWebSocketMessage(crow::websocket::connection& conn, const std::string& data, bool is_binary);

        // Lógica de Negócio
		void processCreateLobby(crow::websocket::connection* conn, const std::string& playerName);
        void processJoinAsPlayer(crow::websocket::connection* conn, const std::string& sessionId, const std::string& playerName);
		void processJoinAsMaster(crow::websocket::connection* conn, const std::string& sessionId, const std::string& masterName);
		void processStartGame(crow::websocket::connection* conn, const std::string& sessionId, const std::string& playerName, const std::string& caseId);
		void processGetLobbyInfo(crow::websocket::connection* conn, const std::string& sessionId);
		void processSubmitSolution(crow::websocket::connection* conn, const std::string& sessionId, const std::vector<std::string>& answers);
        void processValidateSolution(crow::websocket::connection* conn, const std::string& sessionId, bool approved);
        void processGameAction(crow::websocket::connection* conn, const std::string& sessionId, const std::string& playerId, ActionType actionType, const std::string& targetId);
        void processSaveNote(crow::websocket::connection* conn, const std::string& sessionId, const std::string& playerId, const std::string& clueId, const std::string& content);

        // Helpers
        void broadcastGameState(const std::string& sessionId);
		void broadcastLobbyState(const std::string& sessionId);
		std::string generateSessionId();

		// Componentes
        std::shared_ptr<GameEngine> engine;
        std::shared_ptr<MongoStore> storage;
        std::shared_ptr<SessionManager> sessionManager;
        std::shared_ptr<TaskQueue> taskQueue;
    };
}