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


    bool GameEngine::initializeGameFromLobby(
        const std::string& sessionId,
        const std::string& caseId,
        const std::vector<std::string>& playerNames,
        const std::string& hostPlayerId
    ) {
		auto bugCase = impl->storage->getCase(caseId);
        if (!bugCase) return false;

		GameState initialState;
		initialState.sessionId = sessionId;
		initialState.currentCaseId = caseId;
		initialState.currentDay = 1;
		initialState.remainingPoints = 12;
		initialState.isCompleted = false;
		initialState.isSuddenDeath = false;
		initialState.playerIds = playerNames;
		initialState.hostPlayerId = hostPlayerId;

		return impl->storage->createSession(initialState);
    }

    ProcessResult GameEngine::processAction(
        const std::string& playerId,
        ActionType actionType,
        const std::string& targetId,
        const std::string& sessionId) {

        auto state = impl->storage->getSession(sessionId);
        if (!state) {
            return { .success = false, .message = "Erro: Sessão não encontrada." };
        }

		if (std::find(state->playerIds.begin(), state->playerIds.end(), playerId) == state->playerIds.end()) {
            return { .success = false,
                .newState = *state,
                .message = "Erro: Jogador não faz parte da sessão."
            };
        }

        auto bugCase = impl->storage->getCase(state->currentCaseId);
        if (!bugCase) {
            return { .success = false, .newState = *state, .message = "Erro: Caso não encontrado." };
        }

        if (state->isSuddenDeath && actionType != ActionType::SubmitSolution) {
            return { .success = false, .newState = *state,
                     .message = "Modo morte súbita! Apenas submissão de solução é permitida." };
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

        if (state->remainingPoints == 0) {
            if (state->currentDay == 5) {
				state->isSuddenDeath = true;
            }
            else {
                state->currentDay++;
				state->remainingPoints = 12;
            }
        }

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
        if (!state) return;

        if (victory) {
            state->isCompleted = true;
        }
        else {
            if(state->isSuddenDeath)
				state->isCompleted = true;
            else {
                if (state->currentDay != 5) {
                    state->currentDay = std::min(5, state->currentDay + 2);
				    state->remainingPoints = 12;
                }
                else {
                    state->remainingPoints = 0;
					state->isSuddenDeath = true;
                }
            }
        }

        impl->storage->updateSession(*state);
    }

}