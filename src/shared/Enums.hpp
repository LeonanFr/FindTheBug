#pragma once

namespace FindTheBug {

	enum class ActionType {
		ReadDocumentation,
		InsertLog,
		InvestigateFunction,
		SetBreakpoint,
		RunUnitTests,
		RunIntegrationTests,
		SubmitSolution
	};

	enum class ClueType {
		Documentation,
		Log,
		Code,
		Breakpoint,
		UnitTestResult,
		IntegrationTestResult
	};

	enum class TargetType {
		Module,
		Function,
		Connection
	};

}