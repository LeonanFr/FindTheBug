#pragma once

#include "../shared/DTOS.hpp"
#include <optional>
#include <vector>

namespace FindTheBug {
	struct ActionResult {
		bool success{ false };
		int pointsSpent{ 0 };
		std::optional<Clue> unlockedClue;
		std::string message;
	};

	struct ProcessResult {
		bool success{ false };
		GameState newState;
		std::string message;
	};

	struct ValidationResult {
		bool isCorrect{ false };
		int score{ 0 };
		std::vector<std::string> feedbackPerQuestion;
		std::string generalMessage;
	};

}