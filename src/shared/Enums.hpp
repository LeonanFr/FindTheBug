#pragma once

namespace FindTheBug {

	enum class GameResult { Running, Victory, Defeat };

	enum class ActionType {
		ReadDocumentation,
		InsertLog,
		InvestigateFunction,
		SetBreakpoint,
		RunUnitTests,
		RunIntegrationTests,
		SubmitSolution,
		SkipTurn
	};

	enum class ClueType {
		Documentation = 0,
		Log = 1,
		Code = 2,
		Breakpoint = 3,
		UnitTestResult = 4,
		IntegrationTestResult = 5
	};

	enum class TargetType {
		Module = 0,
		Function = 1,
		Connection = 2
	};

	enum class PlayerRole {
		Player,
		Master,
		Host
	};

	enum class GamePhase {
		Lobby,
		Investigation,
		SuddenDeath,
		Review,
		Finished
	};
}