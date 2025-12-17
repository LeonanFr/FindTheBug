#define NOMINMAX 

#include <functional>
#include "crow.h"
#include "HttpServer.hpp"

using namespace FindTheBug;

crow::json::wvalue stateToJSON(const GameState& state) {
    crow::json::wvalue json;

    json["sessionId"] = state.sessionId;
    json["currentCaseId"] = state.currentCaseId;
    json["currentDay"] = state.currentDay;
    json["remainingPoints"] = state.remainingPoints;
    json["isCompleted"] = state.isCompleted;

    std::vector<crow::json::wvalue> cluesJson;
    for (const auto& clue : state.discoveredClues) {
        crow::json::wvalue c;
        c["id"] = clue.id;
        c["targetId"] = clue.targetId;
        c["content"] = clue.content;
        c["type"] = static_cast<int>(clue.type);
        cluesJson.push_back(std::move(c));
    }
    json["discoveredClues"] = std::move(cluesJson);

    std::vector<crow::json::wvalue> historyJson;
    for (const auto& action : state.actionHistory) {
        crow::json::wvalue a;
        a["playerId"] = action.playerId;
        a["type"] = static_cast<int>(action.actionType);
        a["targetId"] = action.targetId;
        historyJson.push_back(std::move(a));
    }
    json["actionHistory"] = std::move(historyJson);

    return json;
}

HttpServer::HttpServer(std::shared_ptr<GameEngine> engine)
    : engine{ std::move(engine) },
    sessionManager{ std::make_shared<SessionManager>() } {
}

void HttpServer::run(uint16_t port) {
    crow::SimpleApp app;

    auto actionHandler = std::bind(&HttpServer::handleAction, this, std::placeholders::_1);

    CROW_ROUTE(app, "/action")
        .methods(crow::HTTPMethod::POST)
        (actionHandler);

    auto wsOpenHandler = std::bind(&HttpServer::handleWebSocketOpen, this, std::placeholders::_1);
    auto wsCloseHandler = std::bind(&HttpServer::handleWebSocketClose, this, std::placeholders::_1, std::placeholders::_2);
    auto wsMessageHandler = std::bind(&HttpServer::handleWebSocketMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);

    CROW_WEBSOCKET_ROUTE(app, "/ws")
        .onopen(wsOpenHandler)
        .onclose(wsCloseHandler)
        .onmessage(wsMessageHandler);

    app.port(port).multithreaded().run();
}

crow::response HttpServer::handleAction(const crow::request& req) {
    auto body = crow::json::load(req.body);
    if (!body) return crow::response(400, "Invalid JSON");

    auto result = engine->processAction(
        body["playerId"].s(),
        static_cast<ActionType>(body["actionType"].i()),
        body["targetId"].s(),
        body["sessionId"].s()
    );

    crow::json::wvalue response;
    response["success"] = result.success;
    response["message"] = result.message;
    response["state"] = stateToJSON(result.newState);

    return crow::response(200, response);
}

void HttpServer::handleWebSocketOpen(crow::websocket::connection& conn) {
    CROW_LOG_INFO << "Conexão WebSocket aberta";
}

void HttpServer::handleWebSocketClose(crow::websocket::connection& conn, const std::string& reason) {
    sessionManager->removeConnection(&conn);
}

void HttpServer::handleWebSocketMessage(crow::websocket::connection& conn, const std::string& data, bool is_binary) {
    auto msg = crow::json::load(data);
    if (!msg) return;

    std::string type = msg["type"].s();
    std::string sessionId = msg["sessionId"].s();

    if (type == "REGISTER") {
        std::string role = msg["role"].s();
        sessionManager->registerConnection(sessionId, role, &conn);
    }
    else if (type == "SUBMIT_SOLUTION") {
        std::vector<std::string> answers;
        std::vector<crow::json::wvalue> answersJson;

        for (const auto& a : msg["answers"]) {
            std::string s = a.s();
            answers.push_back(s);
            answersJson.push_back(s);
        }

        auto validation = engine->submitToMaster(sessionId, answers);

        auto* masterConn = sessionManager->getMaster(sessionId);
        if (masterConn) {
            crow::json::wvalue toMaster;
            toMaster["type"] = "SOLUTION_SUBMITTED";
            toMaster["answers"] = std::move(answersJson);

            std::vector<crow::json::wvalue> feedbackJson;
            for (const auto& f : validation.feedbackPerQuestion) {
                feedbackJson.push_back(f);
            }
            toMaster["feedbackPerQuestion"] = std::move(feedbackJson);

            masterConn->send_text(toMaster.dump());
        }
    }
    else if (type == "MASTER_DECISION") {
        bool victory = msg["victory"].b();
        engine->finalizeSession(sessionId, victory);

        crow::json::wvalue response;
        response["type"] = "DECISION_RESULT";
        response["victory"] = victory;

        auto players = sessionManager->getPlayers(sessionId);
        for (auto* p_conn : players) {
            p_conn->send_text(response.dump());
        }
    }
}
