#pragma once

#include "crow.h"
#include "../shared/DTOs.hpp"
#include <unordered_map>
#include <vector>
#include <mutex>
#include <atomic>

namespace FindTheBug {

    class SessionManager {
    public:
        explicit SessionManager() = default;
        ~SessionManager() = default;

		void registerConnection(const std::string& sessionId, crow::websocket::connection* conn);
		void unregisterConnection(crow::websocket::connection* conn);
		void closeSession(const std::string& sessionId);

		void broadcastToSession(const std::string& sessionId, const std::string& message);

		static void sendTo(crow::websocket::connection* conn, const std::string& message);
		static void log(const std::string& message);

	private:
		std::mutex mutex_;

		std::unordered_map<std::string, std::unordered_set<crow::websocket::connection*>> sessionConnections_;
		std::unordered_map<crow::websocket::connection*, std::string> connectionToSession_;


    };

}