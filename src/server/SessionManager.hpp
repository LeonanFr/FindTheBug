#pragma once

#include "crow.h"
#include "../shared/DTOs.hpp"
#include <unordered_map>
#include <vector>
#include <mutex>


namespace FindTheBug {

    class SessionManager {
    public:
        explicit SessionManager() = default;
		~SessionManager() = default;

		//Lobby
		std::string createLobby(const std::string& hostName, crow::websocket::connection* conn);
		bool joinAsPlayer(const std::string& sessionId, const std::string& playerName, crow::websocket::connection* conn);
		bool joinAsMaster(const std::string& sessionId, const std::string& masterName, crow::websocket::connection* conn);
		void leaveLobby(crow::websocket::connection* conn);

		std::optional<LobbyInfo> getLobby(const std::string& sessionId) const;
		bool lobbyExists(const std::string& sessionId) const;
		crow::websocket::connection* getMasterConnection(const std::string& sessionId);
		std::vector<crow::websocket::connection*> getPlayerConnections(const std::string& sessionId);

		bool updateLobbyPhase(const std::string& sessionId, GamePhase newPhase);

		void migrateHostIfNeeded(const std::string& sessionId);

		void cleanupInactiveLobbies(int maxInactiveMinutes = 2);

    private:
		mutable std::mutex mutex_;
		std::unordered_map<std::string, LobbyInfo> lobbies_;

		std::string generateSessionId() const;

		void broadcastToLobby(const std::string& sessionId,
			const crow::json::wvalue& message,
			bool excludeMaster = false);

		void notifyLobbyUpdate(const std::string& sessionId);
    };

}