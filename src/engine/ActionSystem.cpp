#include "ActionSystem.hpp"
#include <print>
#include <format>

using namespace FindTheBug;

int ActionSystem::calculateCost(
    ActionType actionType,
    const std::string& targetId,
    const GameState& currentState
) const {
    switch (actionType) {
    case ActionType::ReadDocumentation:
    case ActionType::InsertLog:
        return 1;

    case ActionType::RunUnitTests:
        return 2;

    case ActionType::RunIntegrationTests:
        return 3;

    case ActionType::InvestigateFunction:
        if (currentState.breakpointedTargets.contains(targetId)) return 1;
        return 2;

    case ActionType::SetBreakpoint:
        if (currentState.investigatedTargets.contains(targetId)) return 1;
        return 2;

    case ActionType::SubmitSolution:
        return 0;

    default:
        std::println(stderr, "[ActionSystem] Unknown action type for cost calculation.");
        return 0;
    }
}

std::optional<Clue> ActionSystem::findClueInCase(
    const BugCase& bugCase,
    const std::string& targetId,
    ClueType type
) const {
    for (const auto& clue : bugCase.availableClues) {
        if (clue.targetId == targetId && clue.type == type) {
            return clue;
        }
    }
    return std::nullopt;
}

ActionResult ActionSystem::execute(
    ActionType actionType,
    const std::string& targetId,
    const BugCase& bugCase,
    const GameState& currentState) const {

    ActionResult result{ .success = false, .pointsSpent = 0 };

    int cost = calculateCost(actionType, targetId, currentState);

    if (currentState.remainingPoints < cost) {
        result.message = std::format("Pontos insuficientes. Necessario: {}, Disponivel: {}", cost, currentState.remainingPoints);
        return result;
    }

    if (actionType == ActionType::SubmitSolution) {
        result.success = true;
        result.pointsSpent = 0;
        result.message = "Solucao enviada para analise.";
        return result;
    }

    ClueType expectedClueType;
    switch (actionType) {
    case ActionType::ReadDocumentation:   expectedClueType = ClueType::Documentation; break;
    case ActionType::InsertLog:           expectedClueType = ClueType::Log; break;
    case ActionType::InvestigateFunction: expectedClueType = ClueType::Code; break;
    case ActionType::SetBreakpoint:       expectedClueType = ClueType::Breakpoint; break;
    case ActionType::RunUnitTests:        expectedClueType = ClueType::UnitTestResult; break;
    case ActionType::RunIntegrationTests: expectedClueType = ClueType::IntegrationTestResult; break;
    default:
        result.message = "Tipo de acao nao suportado.";
        return result;
    }

    auto clue = findClueInCase(bugCase, targetId, expectedClueType);

    if (!clue) {
        result.success = true;
        result.pointsSpent = cost;
        result.message = "A analise nao revelou comportamentos anomalos neste alvo.";
        return result;
    }

    result.success = true;
    result.pointsSpent = cost;
    result.unlockedClue = clue;
    result.message = "Analise bem-sucedida! Uma nova pista foi descoberta.";

    return result;
}