#pragma once

#include "Enums.hpp"
#include <chrono>
#include <vector>
#include <string>
#include <unordered_set>

namespace FindTheBug {

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

	struct BugCase {
		std::string id;
		std::string title;
		std::string description;

		std::vector<std::string> solutionQuestions;
		std::vector<std::string> correctAnswers;

		std::vector<Clue> availableClues;
	};

	struct GameState {
		std::string sessionId;
		std::string currentCaseId;
		int currentDay{ 1 };
		int remainingPoints{ 12 };
		bool isCompleted{ false };

		std::vector<Clue> discoveredClues;
		std::vector<PlayerAction> actionHistory;

		std::unordered_set<std::string> investigatedTargets;
		std::unordered_set<std::string> breakpointedTargets;
	};

}