#include "GameEngine.hpp"
#include "ActionSystem.hpp"
#include "ValidationSystem.hpp"

#include <memory>
#include <algorithm>
#include <chrono>
#include <print> // Moderno C++23

namespace FindTheBug {

    class GameEngine::Impl {
    public:
        std::shared_ptr<MongoStore> storage;
        ActionSystem actionSystem;
        ValidationSystem validationSystem;
    };

    GameEngine::GameEngine(std::shared_ptr<MongoStore> storage)
        : pImpl(std::make_unique<Impl>()) {
        pImpl->storage = std::move(storage);
    }

    GameEngine::~GameEngine() = default;

    std::shared_ptr<MongoStore> GameEngine::getStorage() const {
        return pImpl->storage;
    }

    bool GameEngine::initializeGameFromLobby(
        const std::string& sessionId,
        const std::string& caseId,
        const std::vector<std::string>& playerNames,
        const std::string& hostPlayerId,
        const std::string& masterPlayerId
    ) {
        auto bugCase = pImpl->storage->getCase(caseId);
        if (!bugCase) {
            std::print("[ENGINE] Erro: CaseID {} nao encontrado.\n", caseId);
            return false;
        }

        GameState initialState;
        initialState.sessionId = sessionId;
        initialState.currentCaseId = caseId;
        initialState.currentDay = 1;
        initialState.remainingPoints = 12;
        initialState.isCompleted = false;
        initialState.isSuddenDeath = false;
        initialState.playerIds = playerNames;
        initialState.hostPlayerId = hostPlayerId;
        initialState.masterPlayerId = masterPlayerId;

        return pImpl->storage->saveGameState(initialState);
    }

    ProcessResult GameEngine::processAction(
        const std::string& playerId,
        ActionType actionType,
        const std::string& targetId,
        const std::string& sessionId) {

        auto stateOpt = pImpl->storage->getGameState(sessionId);
        if (!stateOpt) {
            return { .success = false, .newState = {}, .message = "Erro: Sessao nao encontrada." };
        }
        auto& state = *stateOpt;

        bool isPlayerInGame = std::find(state.playerIds.begin(), state.playerIds.end(), playerId) != state.playerIds.end();
        if (!isPlayerInGame && playerId != state.hostPlayerId) {
            return { .success = false, .newState = state, .message = "Erro: Jogador nao faz parte da sessao." };
        }

        if (playerId == state.masterPlayerId) {
            return { .success = false, .newState = state, .message = "O Mestre não pode realizar ações de investigação." };
        }

        auto bugCaseOpt = pImpl->storage->getCase(state.currentCaseId);
        if (!bugCaseOpt) {
            return { .success = false, .newState = state, .message = "Erro: Caso corrompido ou inexistente." };
        }
        const auto& bugCase = *bugCaseOpt;

        if (state.isSuddenDeath && actionType != ActionType::SubmitSolution) {
            return { .success = false, .newState = state,
                     .message = "MODO MORTE SUBITA: Apenas submissao de solucao e permitida!" };
        }

        auto actionResult = pImpl->actionSystem.execute(
            actionType,
            targetId,
            bugCase,
            state
        );

        if (!actionResult.success) {
            return { .success = false, .newState = state, .message = actionResult.message };
        }

        state.remainingPoints -= actionResult.pointsSpent;

        if (actionResult.unlockedClue) {
            state.discoveredClues.push_back(*actionResult.unlockedClue);
        }

        if (actionType == ActionType::InvestigateFunction) {
            state.investigatedTargets.insert(targetId);
        }
        else if (actionType == ActionType::SetBreakpoint) {
            state.breakpointedTargets.insert(targetId);
        }

        state.actionHistory.push_back({
            .playerId = playerId,
            .actionType = actionType,
            .targetId = targetId,
            .timestamp = std::chrono::system_clock::now()
            });

        if (state.remainingPoints <= 0) {
            if (state.currentDay >= 5) {
                state.isSuddenDeath = true;
                state.remainingPoints = 0;
            }
            else {
                state.currentDay++;
                state.remainingPoints = 12;
            }
        }

        if (pImpl->storage->saveGameState(state)) {
            return { .success = true, .newState = state, .message = actionResult.message };
        }
        else {
            return { .success = false, .newState = state, .message = "Erro critico ao salvar estado no banco." };
        }
    }

    ValidationResult GameEngine::submitToMaster(
        const std::string& sessionId,
        const std::vector<std::string>& answers) {

        auto stateOpt = pImpl->storage->getGameState(sessionId);
        if (!stateOpt) return { .isCorrect = false, .score = 0, .generalMessage = "Sessão inválida" };

        auto bugCaseOpt = pImpl->storage->getCase(stateOpt->currentCaseId);
        if (!bugCaseOpt) return { .isCorrect = false, .score = 0, .generalMessage = "Caso inválido" };

        return pImpl->validationSystem.prepareForMaster(answers, *bugCaseOpt);
    }

    GameResult GameEngine::finalizeSession(const std::string& sessionId, bool approvedByMaster) {
        auto stateOpt = pImpl->storage->getGameState(sessionId);
        if (!stateOpt) return GameResult::Running;

        auto state = *stateOpt;

        if (approvedByMaster) {
            state.isCompleted = true;
            pImpl->storage->saveGameState(state);
            return GameResult::Victory;
        }

        if (state.isSuddenDeath) {
            state.isCompleted = true;
            pImpl->storage->saveGameState(state);
            return GameResult::Defeat;
        }

        state.currentDay += 2;

        if (state.currentDay > 5) {
            state.currentDay = 5;
            state.isSuddenDeath = true;
            state.remainingPoints = 0;
            return GameResult::Running;
        }

        state.remainingPoints = 12;

        pImpl->storage->saveGameState(state);
        return GameResult::Running;
    }
}