#pragma once

#include "Enums.hpp"
#include <chrono>
#include <vector>
#include <string>
#include <unordered_set>
#include <map>
#include <algorithm>
#include "crow.h"

namespace FindTheBug {

    enum class SessionStatus {
        Lobby,
        InGame,
        SuddenDeath,
        Review,
        Finished
    };

    struct DiscoveredClue {
        std::string id;
        std::string targetId;
        ClueType type;
        TargetType targetType;
        std::string content;
        std::string discoveredBy;
        std::map<std::string, std::string> playerNotes;
    };

    struct PlayerAction {
        std::string playerId;
        ActionType actionType;
        std::string targetId;
        std::chrono::system_clock::time_point timestamp;
    };

    struct Clue {
        std::string id;
        std::string targetId;
        TargetType targetType;
        ClueType type;
        std::string content;
        int cost{ 0 };
    };

    struct ModuleNode {
        std::string name;
    };

    struct FunctionNode {
        std::string name;
        std::string parentId;
    };

    struct ConnectionNode {
        std::string id;
        std::string from;
        std::string to;
    };

    struct SystemTopology {
        std::vector<ModuleNode> modules;
        std::vector<FunctionNode> functions;
        std::vector<ConnectionNode> connections;
    };

    struct BugCase {
        std::string id;
        std::string title;
        std::string description;

        std::vector<std::string> solutionQuestions;
        std::vector<std::string> correctAnswers;

        std::vector<Clue> availableClues;
        SystemTopology systemTopology;
    };

    struct CaseSummary {
        std::string id;
        std::string title;
        std::string shortDescription;
    };

    struct GameState {
        std::string sessionId;
        std::string currentCaseId;
        std::chrono::system_clock::time_point lastActivity;
        int currentDay{ 1 };
        int remainingPoints{ 12 };
        bool isCompleted{ false };
        bool isSuddenDeath{ false };

        std::vector<DiscoveredClue> discoveredClues;
        std::vector<PlayerAction> actionHistory;

        std::unordered_set<std::string> investigatedTargets;
        std::unordered_set<std::string> breakpointedTargets;

        // Jogadores
        std::vector<std::string> playerIds;
        std::string masterPlayerId;

        // Turno
        std::vector<std::string> turnOrder;
        int currentTurnIndex;
        std::chrono::system_clock::time_point turnStartTime;
    };

    struct PlayerInfo {
        std::string id;
        std::string name;
        PlayerRole role{ PlayerRole::Player };
        std::chrono::system_clock::time_point joinedAt;

        crow::websocket::connection* connection{ nullptr };
    };

    struct LobbyInfo {
        std::string sessionId;
        std::vector<PlayerInfo> players;
        GamePhase phase{ GamePhase::Lobby };
        std::chrono::system_clock::time_point createdAt;
        std::chrono::system_clock::time_point lastActivity;

        bool hasMaster() const {
            return std::any_of(players.begin(), players.end(),
                [](const PlayerInfo& p) {
                    return p.role == PlayerRole::Master;
                });
        }

        int playerCount() const {
            int count = 0;
            for (const auto& p : players) {
                if (p.role == PlayerRole::Player) {
                    count++;
                }
            }
            return count;
        }

        bool canStartGame() const {
            return playerCount() >= 1 &&
                hasMaster() &&
                phase == GamePhase::Lobby;
        }

        const PlayerInfo* getMaster() const {
            for (const auto& p : players) {
                if (p.role == PlayerRole::Master) {
                    return &p;
                }
            }
            return nullptr;
        }

        std::vector<PlayerInfo*> getPlayers() {
            std::vector<PlayerInfo*> result;
            for (auto& p : players) {
                if (p.role == PlayerRole::Player) {
                    result.push_back(&p);
                }
            }
            return result;
        }
    };
}