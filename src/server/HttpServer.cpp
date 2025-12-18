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
    sessionManager->leaveLobby(&conn);
}

void HttpServer::sendJsonResponse(crow::websocket::connection& conn,
    const std::string& type,
    const crow::json::wvalue& data) {
 
    crow::json::wvalue response = data;
    response["type"] = type;

    conn.send_text(response.dump());
}

void HttpServer::handleCreateLobby(crow::websocket::connection& conn,
    const crow::json::rvalue& msg) {
 
	std::string playerName = msg["playerName"].s();

    if (playerName.empty()) {
        sendJsonResponse(conn, "ERROR", { {"message", "Nome do jogador não pode ser vazio."} });
        return;
    }

    std::string sessionId = sessionManager->createLobby(playerName, &conn);

    if (!sessionId.empty()) {
        sendJsonResponse(conn, "LOBBY_CREATED", {
            {"sessionId", sessionId},
            {"playerName", playerName},
            {"role", static_cast<int>(PlayerRole::Host)}
            });
    }
    else sendJsonResponse(conn, "ERROR", { {"message", "Falha ao criar lobby."} });
}

void HttpServer::handleJoinAsPlayer(crow::websocket::connection& conn,
    const crow::json::rvalue& msg) {
    
    std::string sessionId = msg["sessionId"].s();
    std::string playerName = msg["playerName"].s();

    if (playerName.empty()) {
        sendJsonResponse(conn, "ERROR", { {"message", "Nome do jogador não pode ser vazio."} });
        return;
    }
    
    bool success = sessionManager->joinAsPlayer(sessionId, playerName, &conn);

    if (success) {
        sendJsonResponse(conn, "JOINED_LOBBY", {
            {"sessionId", sessionId},
            {"playerName", playerName},
            {"role", static_cast<int>(PlayerRole::Player)}
            });
    }
    else sendJsonResponse(conn, "ERROR", {
        {"message", "Não foi possível entrar no lobbu (código inválido, nome já em uso ou lobby cheio.)"}
        });

}

void HttpServer::handleJoinAsMaster(crow::websocket::connection& conn,
    const crow::json::rvalue& msg) {
    
    std::string sessionId = msg["sessionId"].s();
    std::string masterName = msg["masterName"].s();

    if (masterName.empty()) {
        sendJsonResponse(conn, "ERROR", { {
                "message", "Nome do mestre não pode ser vazio."
} });
        return;
    }

    bool success = sessionManager->joinAsMaster(sessionId, masterName, &conn);

    if (success)
        sendJsonResponse(conn, "JOINED_LOBBY", {
            {"sessionId", sessionId},
            {"playerName", masterName},
            {"role", static_cast<int>(PlayerRole::Master)}
            });
    else
        sendJsonResponse(conn, "ERROR", {
            {"message", "Não foi possível entrar como mestre (código inválido ou já há um mestre)"}
            });

}

void HttpServer::handleLeaveLobby(crow::websocket::connection& conn,
    const crow::json::rvalue& msg) {

    sessionManager->leaveLobby(&conn);
    sendJsonResponse(conn, "LEFT_LOBBY");
}

void HttpServer::handleStartGame(crow::websocket::connection& conn,
    const crow::json::rvalue& msg) {
    
    std::string sessionId = msg["sessionId"].s();
    std::optional<std::string> caseId;

    if (msg.has("caseId")) caseId = msg["caseId"].s();

    auto lobby = sessionManager->getLobby(sessionId);
    if(!lobby){
        sendJsonResponse(conn, "ERROR", {
            {"message", "Lobby não encontrado."}
            });
        return;
    }

    auto* host = lobby->getHost();
    if (!host || host->connection != &conn) {
        sendJsonResponse(conn, "ERROR", {
            {"message", "Apenas o host pode iniciar o jogo."}
            });
        return;
    }

    if (!lobby->canStartGame()) {
        sendJsonResponse(conn, "ERROR", {
            {"message", "Não é possível iniciar o jogo (necessário pelo menos 2 jogadores e um mestre)."}
            });
        return;
    }

    std::string actualCaseId = caseId.value_or("case_robotics_001");

    std::vector<std::string> playerNames;
    std::string hostPlayerId;

    for (const auto& player : lobby->players) {
        if (player.role == PlayerRole::Master) continue;

        playerNames.push_back(player.name);
        if (player.role == PlayerRole::Host) hostPlayerId = player.name;
    }

    bool initialized = engine->initializeGameFromLobby(
        sessionId,
        actualCaseId,
        playerNames,
        hostPlayerId
    );

    if (!initialized) {
        sendJsonResponse(conn, "ERROR", {
            {"message", "Falha ao inicializar o jogo. Verifique se o caso existe no banco de dados."}
            });
        return;
    }

    bool phaseUpdated = sessionManager->updateLobbyPhase(sessionId, GamePhase::Investigation);

    if (!phaseUpdated) {
        sendJsonResponse(conn, "ERROR", {
            {"message", "Falha ao atualizar fase do lobby."}
            });
        return;
    }

    crow::json::wvalue notification;
    notification["type"] = "GAME_STARTED";
    notification["sessionId"] = sessionId;
    notification["caseId"] = actualCaseId;
    notification["players"] = crow::json::wvalue::list();

    for (size_t i = 0; i < playerNames.size(); ++i) notification["players"][i] = playerNames[i];

    auto players = lobby->players;
    std::string notificationStr = notification.dump();

    for (const auto& player : players)
        if (player.connection)
            player.connection->send_text(notificationStr);

    sendJsonResponse(conn, "GAME_STARTED", {
        {"sessionId", sessionId},
        {"caseId", actualCaseId},
        {"initialized", true},
        {"phaseUpdated", phaseUpdated}
        });
}

void HttpServer::handleGetLobbyInfo(crow::websocket::connection& conn,
    const crow::json::rvalue& msg) {

    std::string sessionId = msg["sessionId"].s();
    auto lobby = sessionManager->getLobby(sessionId);

    if (!lobby) {
        sendJsonResponse(conn, "LOBBY_INFO", {
            {"exists", false}
            });
        return;
    }

    std::vector<crow::json::wvalue> playersJson;
    for (const auto& player : lobby->players) {
        if (player.role != PlayerRole::Master) {
            crow::json::wvalue playerJson;
            playerJson["name"] = player.name;
            playerJson["role"] = static_cast<int>(player.role);
            playersJson.push_back(std::move(playerJson));
        }
    }

    std::string masterName;
    auto* master = lobby->getMaster();
    if (master) {
        masterName = master->name;
    }

    sendJsonResponse(conn, "LOBBY_INFO", {
        {"exists", true},
        {"sessionId", lobby->sessionId},
        {"phase", static_cast<int>(lobby->phase)},
        {"canStart", lobby->canStartGame()},
        {"playerCount", lobby->playerCount()},
        {"players", std::move(playersJson)},
        {"masterName", masterName}
        });

}

void HttpServer::handleWebSocketMessage(crow::websocket::connection& conn,
    const std::string& data, bool is_binary) {
    auto msg = crow::json::load(data);
    if (!msg) return;

    std::string type = msg["type"].s();


    if (type == "CREATE_LOBBY") {
        handleCreateLobby(conn, msg);
    }
    else if (type == "JOIN_AS_PLAYER") {
        handleJoinAsPlayer(conn, msg);
    }
    else if (type == "JOIN_AS_MASTER") {
        handleJoinAsMaster(conn, msg);
    }
    else if (type == "LEAVE_LOBBY") {
        handleLeaveLobby(conn, msg);
    }
    else if (type == "START_GAME") {
        handleStartGame(conn, msg);
    }
    else if (type == "GET_LOBBY_INFO") {
        handleGetLobbyInfo(conn, msg);
    }

    else if (type == "SUBMIT_SOLUTION") {
        std::string sessionId = msg["sessionId"].s();
        std::vector<std::string> answers;

        for (const auto& a : msg["answers"]) {
            answers.push_back(a.s());
        }

        auto validation = engine->submitToMaster(sessionId, answers);

        auto* masterConn = sessionManager->getMasterConnection(sessionId);
        if (masterConn) {
            crow::json::wvalue toMaster;
            toMaster["type"] = "SOLUTION_SUBMITTED";
            toMaster["answers"] = answers;
            toMaster["sessionId"] = sessionId;

            std::vector<crow::json::wvalue> feedbackJson;
            for (const auto& f : validation.feedbackPerQuestion) {
                feedbackJson.push_back(f);
            }
            toMaster["feedbackPerQuestion"] = std::move(feedbackJson);

            masterConn->send_text(toMaster.dump());
        }
    }
    else if (type == "MASTER_DECISION") {
        std::string sessionId = msg["sessionId"].s();
        bool victory = msg["victory"].b();

        engine->finalizeSession(sessionId, victory);

        crow::json::wvalue response;
        response["type"] = "DECISION_RESULT";
        response["victory"] = victory;

        auto players = sessionManager->getPlayerConnections(sessionId);
        for (auto* p_conn : players) {
            p_conn->send_text(response.dump());
        }
    }

    else {
        sendJsonResponse(conn, "ERROR", {
            {"message", "Tipo de mensagem não reconhecido: " + type}
            });
    }
}