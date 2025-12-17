#include "GameEngine.hpp"
#include "ActionSystem.hpp"
#include "ValidationSystem.hpp"

#include <memory>
#include <algorithm>
#include <chrono>

namespace FindTheBug {

    class GameEngine::Impl {
    public:
        std::shared_ptr<MongoStore> storage;
        ActionSystem actionSystem;
        ValidationSystem validationSystem;
    };

    GameEngine::GameEngine(std::shared_ptr<MongoStore> storage)
        : impl(std::make_unique<Impl>()) {
        impl->storage = std::move(storage);
    }

    GameEngine::~GameEngine() = default;

    ProcessResult GameEngine::processAction(
        const std::string& playerId,
        ActionType actionType,
        const std::string& targetId,
        const std::string& sessionId) {

        auto state = impl->storage->getSession(sessionId);
        if (!state) {
            return { .success = false, .message = "Erro: Sessão não encontrada." };
        }

        auto bugCase = impl->storage->getCase(state->currentCaseId);
        if (!bugCase) {
            return { .success = false, .newState = *state, .message = "Erro: Caso não encontrado." };
        }

        auto actionResult = impl->actionSystem.execute(
            actionType,
            targetId,
            *bugCase,
            *state
        );

        if (!actionResult.success) {
            return { .success = false, .newState = *state, .message = actionResult.message };
        }

        state->remainingPoints -= actionResult.pointsSpent;

        if (actionResult.unlockedClue) {
            state->discoveredClues.push_back(*actionResult.unlockedClue);
        }

        if (actionType == ActionType::InvestigateFunction) {
            state->investigatedTargets.insert(targetId);
        }
        else if (actionType == ActionType::SetBreakpoint) {
            state->breakpointedTargets.insert(targetId);
        }

        state->actionHistory.push_back({
            .playerId = playerId,
            .actionType = actionType,
            .targetId = targetId,
            .timestamp = std::chrono::system_clock::now()
            });

        // 6. Persistir
        impl->storage->updateSession(*state);

        return { .success = true, .newState = *state, .message = actionResult.message };
    }

    ValidationResult GameEngine::submitToMaster(
        const std::string& sessionId,
        const std::vector<std::string>& answers) {

        auto state = impl->storage->getSession(sessionId);
        if (!state) return { .isCorrect = false, .score = 0, .generalMessage = "Sessão inválida" };

        auto bugCase = impl->storage->getCase(state->currentCaseId);
        if (!bugCase) return { .isCorrect = false, .score = 0, .generalMessage = "Caso inválido" };

        return impl->validationSystem.prepareForMaster(answers, *bugCase);
    }

    void GameEngine::finalizeSession(const std::string& sessionId, bool victory) {
        auto state = impl->storage->getSession(sessionId);
        if (!state) return; // Proteção contra crash

        if (victory) {
            state->isCompleted = true;
        }
        else {
            state->currentDay = std::min(5, state->currentDay + 2);
            state->remainingPoints = 12;
        }

        impl->storage->updateSession(*state);
    }

}