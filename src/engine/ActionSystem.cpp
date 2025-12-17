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

	auto clue = findClueInCase(bugCase, targetId, actionType);

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
	ActionType type) const {

	auto it = std::ranges::find_if(bugCase.availableClues,
		[&](const Clue& clue) {
			bool idMatch = (clue.targetId == targetId);
			bool typeMatches = false;

			switch (type) {
			case ActionType::ReadDocumentation:
				typeMatches = (clue.type == ClueType::Documentation);
				break;
			case ActionType::InsertLog:
				typeMatches = (clue.type == ClueType::Log);
				break;
			case ActionType::InvestigateFunction:
				typeMatches = (clue.type == ClueType::Code);
				break;
			case ActionType::SetBreakpoint:
				typeMatches = (clue.type == ClueType::Breakpoint);
				break;
			case ActionType::RunUnitTests:
				typeMatches = (clue.type == ClueType::UnitTestResult);
				break;
			case ActionType::RunIntegrationTests:
				typeMatches = (clue.type == ClueType::IntegrationTestResult);
				break;
			default:
				typeMatches = false;
			}
			return typeMatches && idMatch;
		});

	if (it != bugCase.availableClues.end()) {
		return *it;
	}

	return std::nullopt;

}