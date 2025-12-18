#pragma once

#include "../shared/DTOs.hpp"
#include <optional>
#include <memory>
#include <string>

namespace FindTheBug {
	class MongoStore {
	public:
		explicit MongoStore(const std::string& connectionUri, const std::string& dbName);
		~MongoStore();
		
		bool createLobby(const std::string& sessionId, const PlayerInfo& host);
		bool addPlayerToLobby(const std::string& sessionId, const PlayerInfo& player);
		bool removePlayerFromLobby(const std::string& sessionId, const std::string& playerId);
		bool updatePhase(const std::string& sessionId, GamePhase newPhase);
		std::optional<LobbyInfo> getLobby(const std::string& sessionId) const;
		bool sessionExists(const std::string& sessionId) const;

		std::optional<BugCase> getCase(const std::string& caseId) const;
		std::optional<GameState> getGameState(const std::string& sessionId) const;
		std::vector<CaseSummary> listAvailableCases() const;

		bool saveGameState(const GameState& state);
		bool deleteSession(const std::string& sessionId);

	private:
		class Impl;
		std::unique_ptr<Impl> pImpl;
	};
}