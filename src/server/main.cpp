#define _CRT_SECURE_NO_WARNINGS 
#define NOMINMAX 

#include "crow.h"
#include <iostream>
#include <string>
#include <cstdlib>
#include <memory>

#include "../storage/MongoStore.hpp"
#include "../engine/GameEngine.hpp"
#include "../infra/TaskQueue.hpp"
#include "SessionManager.hpp"
#include "HttpServer.hpp"

using namespace FindTheBug;

std::string getEnvVar(const char* key, const char* defaultValue = "") {
    char* val = std::getenv(key);
    return val ? std::string(val) : std::string(defaultValue);
}

int main()
{
    system("chcp 65001 > nul");

    try
    {
        std::string mongoUri = getEnvVar("MONGO_URI");
        std::string dbName = getEnvVar("DB_NAME", "FindTheBugDB");
        int port = std::stoi(getEnvVar("PORT", "8080"));

        if (mongoUri.empty()) {
            std::cerr << "[FATAL] MONGO_URI nao definida.\n";
            return -1;
        }

        auto taskQueue = std::make_shared<TaskQueue>(4);

        auto storage = std::make_shared<MongoStore>(mongoUri, dbName);

        auto sessionManager = std::make_shared<SessionManager>();

        auto engine = std::make_shared<GameEngine>(storage);

        HttpServer server(engine, storage, sessionManager, taskQueue);

        server.run(static_cast<uint16_t>(port));

    }
    catch (const std::exception& e) {
        std::cerr << "[CRASH] Main: " << e.what() << "\n";
        return -1;
    }

    return 0;
}