#pragma once

#include <string>
#include <optional>
#include "Types.hpp"
#include "../shared/DTOs.hpp"

namespace FindTheBug {

    class ActionSystem {
    public:
        int calculateCost(
            ActionType actionType,
            const std::string& targetId,
            const GameState& currentState
        ) const;

        ActionResult execute(
            ActionType actionType,
            const std::string& targetId,
            const BugCase& bugCase,
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