#pragma once

#include "../shared/DTOS.hpp"
#include "Types.hpp"
#include "../storage/MongoStore.hpp"
#include <memory>
#include <string>

namespace FindTheBug {
	
	class GameEngine {
	public:
		explicit GameEngine(std::shared_ptr<MongoStore> storage);
		~GameEngine();

		bool initializeGameFromLobby(
			const std::string& sessionId,
			const std::string& caseId,
			const std::vector<std::string>& playerNames,
			const std::string& hostPlayerId
			);

		ProcessResult processAction(
			const std::string& playerId,
			ActionType actionType,
			const std::string& targetId,
			const std::string& sessionId
		);

		ValidationResult submitToMaster(
			const std::string& sessionId,
			const std::vector<std::string>& answers
		);

		void finalizeSession(const std::string& sessionId, bool victory);

	private:
		class Impl;
		std::unique_ptr<Impl> impl;
	};
}