#include "GameEngine.hpp"
#include "ActionSystem.hpp"
#include "ValidationSystem.hpp"

#include <memory>
#include <algorithm>
#include <chrono>
#include <print>

namespace FindTheBug {

    class GameEngine::Impl {
    public:
        std::shared_ptr<MongoStore> storage;
        ActionSystem actionSystem;
        ValidationSystem validationSystem;
        void advanceTurn(GameState& state) {
            if (state.turnOrder.empty()) return;
            state.currentTurnIndex = (state.currentTurnIndex + 1) % state.turnOrder.size();
            state.turnStartTime = std::chrono::system_clock::now();
        }
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
        const std::vector<std::string>& allParticipants,
        const std::string& hostPlayerId,
        const std::string& masterPlayerId
    ) {
        auto bugCase = pImpl->storage->getCase(caseId);
        if (!bugCase) {
            std::print("[ENGINE] Erro: CaseID {} nao encontrado.\n", caseId);
            return false;
        }



        GameState initialState;

        for (const auto& pid : allParticipants) {
            if (pid != masterPlayerId) {
                initialState.turnOrder.push_back(pid);
            }
        }
        initialState.currentTurnIndex = 0;
        initialState.turnStartTime = std::chrono::system_clock::now();
        initialState.lastActivity = std::chrono::system_clock::now();
        initialState.sessionId = sessionId;
        initialState.currentCaseId = caseId;
        initialState.currentDay = 1;
        initialState.remainingPoints = 12;
        initialState.isCompleted = false;
        initialState.isSuddenDeath = false;
        initialState.playerIds = allParticipants;
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
            return { .success = false, .newState = state, .message = "O Mestre nao pode realizar acoes de investigacao." };
        }

        auto bugCaseOpt = pImpl->storage->getCase(state.currentCaseId);
        if (!bugCaseOpt) {
            return { .success = false, .newState = state, .message = "Erro: Caso corrompido ou inexistente." };
        }
        const auto& bugCase = *bugCaseOpt;

        if (state.isSuddenDeath && actionType != ActionType::SubmitSolution) {
            return { .success = false, .newState = state,
                     .message = "MODO MORTE SUBITA: Apenas submissao de solucao permitida!" };
        }

        if (actionType != ActionType::SubmitSolution) {
            if (!state.turnOrder.empty()) {
                std::string currentPlayer = state.turnOrder[state.currentTurnIndex];
                if (playerId != currentPlayer) {
                    return { .success = false, .newState = state, .message = "Nao e seu turno. Vez de: " + currentPlayer };
                }
            }
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
            bool alreadyExists = false;
            for (const auto& existing : state.discoveredClues) {
                if (existing.id == actionResult.unlockedClue->id) {
                    alreadyExists = true;
                    break;
                }
            }

            if (!alreadyExists) {
                DiscoveredClue dc;
                dc.id = actionResult.unlockedClue->id;
                dc.targetId = actionResult.unlockedClue->targetId;
                dc.type = actionResult.unlockedClue->type;
                dc.targetType = actionResult.unlockedClue->targetType;
                dc.content = actionResult.unlockedClue->content;
                dc.discoveredBy = playerId;
                state.discoveredClues.push_back(dc);
            }
        }

        if (actionType == ActionType::InvestigateFunction) {
            state.investigatedTargets.insert(targetId);
        }
        else if (actionType == ActionType::SetBreakpoint) {
            state.breakpointedTargets.insert(targetId);
        }

        state.lastActivity = std::chrono::system_clock::now();

        state.actionHistory.push_back({
            .playerId = playerId,
            .actionType = actionType,
            .targetId = targetId,
            .timestamp = std::chrono::system_clock::now()
            });

        if (actionResult.pointsSpent > 0 && !state.turnOrder.empty()) {
            state.currentTurnIndex = (state.currentTurnIndex + 1) % state.turnOrder.size();
            state.turnStartTime = std::chrono::system_clock::now();

            if (actionResult.unlockedClue) {
                state.turnStartTime += std::chrono::seconds(30);
            }
        }

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
            return {
                .success = true,
                .newState = state,
                .message = actionResult.message,
                .revealedClue = actionResult.unlockedClue
            };
        }
        else {
            return { .success = false, .newState = state, .message = "Erro critico ao salvar estado no banco." };
        }
    }

    bool GameEngine::savePlayerNote(
        const std::string& sessionId,
        const std::string& playerId,
        const std::string& clueId,
        const std::string& content
    ) {
        auto stateOpt = pImpl->storage->getGameState(sessionId);
        if (!stateOpt) return false;
        auto state = *stateOpt;

        bool found = false;
        for (auto& dc : state.discoveredClues) {
            if (dc.id == clueId) {
                if (content.empty()) {
                    if (dc.discoveredBy == playerId) {
                        return false;
                    }
                    else {
                        dc.playerNotes.erase(playerId);
                    }
                }
                else {
                    dc.playerNotes[playerId] = content;
                }
                found = true;
                break;
            }
        }

        if (!found) return false;

        state.lastActivity = std::chrono::system_clock::now();

        return pImpl->storage->saveGameState(state);
    }

    ValidationResult GameEngine::submitToMaster(
        const std::string& sessionId,
        const std::vector<std::string>& answers) {

        auto stateOpt = pImpl->storage->getGameState(sessionId);
        if (!stateOpt) return { .isCorrect = false, .score = 0, .generalMessage = "Sessão inválida" };

        stateOpt->lastActivity = std::chrono::system_clock::now();
        pImpl->storage->saveGameState(*stateOpt);

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


    GameResult GameEngine::removePlayer(const std::string& sessionId, const std::string& playerId) {
        auto stateOpt = pImpl->storage->getGameState(sessionId);
        if (!stateOpt) return GameResult::Running;
        auto state = *stateOpt;

        auto it = std::remove(state.playerIds.begin(), state.playerIds.end(), playerId);
        state.playerIds.erase(it, state.playerIds.end());

        auto itTurn = std::find(state.turnOrder.begin(), state.turnOrder.end(), playerId);
        if (itTurn != state.turnOrder.end()) {
            int indexRemoved = std::distance(state.turnOrder.begin(), itTurn);
            state.turnOrder.erase(itTurn);

            if (state.currentTurnIndex >= state.turnOrder.size()) {
                state.currentTurnIndex = 0;
            }
            else if (indexRemoved < state.currentTurnIndex) {
                state.currentTurnIndex--;
            }

            if (indexRemoved == state.currentTurnIndex) {
                state.turnStartTime = std::chrono::system_clock::now();
            }
        }

    
        if (state.turnOrder.size() < 2) {
            state.isCompleted = true;
            pImpl->storage->saveGameState(state);
            return GameResult::Defeat;
        }

        pImpl->storage->saveGameState(state);
        return GameResult::Running;
    }
}