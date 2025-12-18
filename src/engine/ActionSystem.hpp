#pragma once

#include <string>
#include "../shared/DTOS.hpp"
#include "Types.hpp"
#include <optional>

namespace FindTheBug {
	
	class ActionSystem {
	public:
		explicit ActionSystem() = default;

		ActionResult execute(
			ActionType actionType,
			const std::string& targetId,
			const BugCase& bugCase,
			const GameState& currentState
		) const;

		int calculateCost(
			ActionType actionType,
			const std::string& targetId,
			const GameState& currentState
		) const;

	private:
		std::optional<Clue> findClueInCase(
			const BugCase& bugCase,
			const std::string& targetId,
			ClueType type
		) const;

	};

}