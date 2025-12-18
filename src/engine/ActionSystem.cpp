#include "ActionSystem.hpp"

#include <iostream>

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
	default:
		std::cerr << "Unknown action type for cost calculation.\n";
		return 0;
	}
}

ActionResult ActionSystem::execute(
	ActionType actionType,
	const std::string& targetId,
	const BugCase& bugCase,
	const GameState& currentState) const {

	ActionResult result{ .success = false, .pointsSpent = 0 };

	int cost = calculateCost(actionType, targetId, currentState);
	if (currentState.remainingPoints < cost) {
		result.message = std::format("Pontos de Função insuficientes. Necessário: {}, Disponível: {}",
			cost, currentState.remainingPoints);
		return result;
	}

	ClueType expectedClueType = ClueType::Documentation;
	switch (actionType) {
	case ActionType::ReadDocumentation:
		expectedClueType = ClueType::Documentation;
		break;
	case ActionType::InsertLog:
		expectedClueType = ClueType::Log;
		break;
	case ActionType::InvestigateFunction:
		expectedClueType = ClueType::Code;
		break;
	case ActionType::SetBreakpoint:
		expectedClueType = ClueType::Breakpoint;
		break;
	case ActionType::RunUnitTests:
		expectedClueType = ClueType::UnitTestResult;
		break;
	case ActionType::RunIntegrationTests:
		expectedClueType = ClueType::IntegrationTestResult;
		break;
	default:
		result.message = "Tipo de ação não suportado.";
		return result;
	}

	auto clue = findClueInCase(bugCase, targetId, expectedClueType);

	if (!clue) {
		result.message = "A análise não revelou comportamentos anômalos neste alvo.";
		return result;
	}

	result.success = true;
	result.pointsSpent = cost;
	result.unlockedClue = clue;
	result.message = "Análise bem-sucedida! Uma nova pista foi descoberta.";

	return result;
}

std::optional<Clue> ActionSystem::findClueInCase(
	const BugCase& bugCase,
	const std::string& targetId,
	ClueType type) const {

	auto it = std::find_if(bugCase.availableClues.begin(), bugCase.availableClues.end(),
		[&](const Clue& clue) {
			return clue.targetId == targetId && clue.type == type;
		});

	if (it != bugCase.availableClues.end()) {
		return *it;
	}

	return std::nullopt;
}