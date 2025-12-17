#define _CRT_SECURE_NO_WARNINGS 
#define NOMINMAX 

#include "crow.h"
#include <iostream>
#include <string>
#include <cstdlib>

#include "../storage/MongoStore.hpp"
#include "HttpServer.hpp"

using namespace FindTheBug;

std::string getEnvVar(const char* key, const char* defaultValue = "") {
    char* val = std::getenv(key);
    return val ? std::string(val) : std::string(defaultValue);
}

int main()
{
    try
    {
        std::string mongoUri = getEnvVar("MONGO_URI");
        std::string dbName = getEnvVar("DB_NAME");
        int port = std::stoi(getEnvVar("PORT", "8080"));

        if (mongoUri.empty()) {
            std::cerr << "\n[ERRO FATAL] Variavel de ambiente 'MONGO_URI' nao definida.\n";
            std::cerr << "Configure nas variaveis do Windows ou no launch.vs.json do VS.\n";
            return -1;
        }

        std::cout << "[SYSTEM] Inicializando MongoStore...\n";

        auto storage = std::make_shared<MongoStore>(mongoUri, dbName);
        auto engine = std::make_shared<GameEngine>(storage);

        HttpServer server(engine);

        std::cout << "[SYSTEM] Servidor iniciando na porta " << port << "...\n";
        server.run(static_cast<uint16_t>(port));
    }
    catch (const std::exception& e) {
        std::cerr << "[CRASH] Erro nao tratado na main: " << e.what() << "\n";
        return -1;
    }

    return 0;
}
