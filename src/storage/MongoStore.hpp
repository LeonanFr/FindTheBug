#pragma once

#include "../shared/DTOs.hpp"
#include <optional>
#include <memory>

namespace FindTheBug {
	class MongoStore {
	public:
		explicit MongoStore(const std::string& connectionUri, const std::string& dbName);
		~MongoStore();

		std::optional<BugCase> getCase(const std::string& caseId) const;
		std::optional<GameState> getSession(const std::string& sessionId) const;

		bool updateSession(const GameState& gameState);
		bool createSession(const GameState& gameState);

	private:
		class Impl;
		std::unique_ptr<Impl> impl;
	};
}