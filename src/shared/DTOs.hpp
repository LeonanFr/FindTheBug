#pragma once

#include "Enums.hpp"
#include <chrono>
#include <vector>
#include <string>
#include <unordered_set>
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
		std::string hostPlayerId;
		std::string masterPlayerId;

		// Turno
		std::vector<std::string> turnOrder;
		int currentTurnIndex;
		std::chrono::system_clock::time_point turnStartTime;

	};

	struct PlayerInfo {
		std::string id;
		std::string name;
		PlayerRole role{PlayerRole::Player};
		std::string connectionId;
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

		bool hasHost() const {
			return std::any_of(players.begin(), players.end(),
				[](const PlayerInfo& p) {
				return p.role == PlayerRole::Host;
				});
		}

		int playerCount() const {
			int count = 0;
			for (const auto& p : players) {
				if (p.role == PlayerRole::Player || p.role == PlayerRole::Host) {
					count++;
				}
			}
			return count;
		}

		bool canStartGame() const {
			return playerCount() >= 2 && 
				hasMaster() &&
				phase == GamePhase::Lobby;
		}

		PlayerInfo* findPlayerByConnection(crow::websocket::connection* conn) {
			auto it = std::find_if(players.begin(), players.end(),
				[conn](const PlayerInfo& p) { return p.connection == conn; });
			return it != players.end() ? &(*it) : nullptr;
		}

		PlayerInfo* getHost() {
			auto it = std::find_if(players.begin(), players.end(),
				[](const PlayerInfo& p) { return p.role == PlayerRole::Host; });
			return it != players.end() ? &(*it) : nullptr;
		}

		const PlayerInfo* getHost() const {
			auto it = std::find_if(players.begin(), players.end(),
				[](const PlayerInfo& p) { return p.role == PlayerRole::Host; });
			return it != players.end() ? &(*it) : nullptr;
		}

		PlayerInfo* getMaster() {
			auto it = std::find_if(players.begin(), players.end(),
				[](const PlayerInfo& p) { return p.role == PlayerRole::Master; });
			return it != players.end() ? &(*it) : nullptr;
		}

		const PlayerInfo* getMaster() const {
			for (const auto& player : players) {
				if (player.role == PlayerRole::Master) {
					return &player;
				}
			}
			return nullptr;
		}

		std::vector<PlayerInfo*> getPlayers() {
			std::vector<PlayerInfo*> result;
			for (auto& p : players) {
				if (p.role == PlayerRole::Player || p.role == PlayerRole::Host) {
					result.push_back(&p);
				}
			}
			return result;
		}

		std::vector<PlayerInfo*> getRegularPlayers() {
			std::vector<PlayerInfo*> regulars;
			for(auto& p : players) {
				if (p.role == PlayerRole::Player || p.role == PlayerRole::Host) {
					regulars.push_back(&p);
				}
			}
			return regulars;
		}
	};

}