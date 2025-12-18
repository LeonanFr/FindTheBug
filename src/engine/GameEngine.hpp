#pragma once

#include <memory>
#include <string>
#include <vector>
#include "../storage/MongoStore.hpp"
#include "Types.hpp"
#include "../shared/DTOs.hpp"

namespace FindTheBug {

    class GameEngine {
    public:
        explicit GameEngine(std::shared_ptr<MongoStore> storage);
        ~GameEngine();

        bool initializeGameFromLobby(
            const std::string& sessionId,
            const std::string& caseId,
            const std::vector<std::string>& playerNames,
            const std::string& hostPlayerId,
            const std::string& masterPlayerId
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

        GameResult finalizeSession(const std::string& sessionId, bool approvedByMaster);
        GameResult removePlayer(const std::string& sessionId, const std::string& playerId);

        bool savePlayerNote(
            const std::string& sessionId,
            const std::string& playerId,
            const std::string& clueId,
            const std::string& content
        );

        std::shared_ptr<MongoStore> getStorage() const;

    private:
        class Impl;
        std::unique_ptr<Impl> pImpl;
    };
}