#define NOMINMAX 

#include "HttpServer.hpp"
#include <functional>
#include <iostream>
#include <print>
#include <random>
#include <format>
#include <sstream>
#include <iomanip>

using namespace FindTheBug;

static std::string escapeJSON(const std::string& s) {
    std::ostringstream o;
    for (auto c : s) {
        if (c == '"') o << "\\\"";
        else if (c == '\\') o << "\\\\";
        else if ((unsigned char)c < 0x20) o << "\\u" << std::hex << std::setw(4) << std::setfill('0') << (int)c;
        else o << c;
    }
    return o.str();
}

HttpServer::HttpServer(
    std::shared_ptr<GameEngine> engine,
    std::shared_ptr<MongoStore> storage,
    std::shared_ptr<SessionManager> sessionManager,
    std::shared_ptr<TaskQueue> taskQueue
) : engine(std::move(engine)),
storage(std::move(storage)),
sessionManager(std::move(sessionManager)),
taskQueue(std::move(taskQueue))
{
    SessionManager::log("[INIT] HttpServer inicializado com fila de tarefas.");
}

void HttpServer::run(uint16_t port) {

    SessionManager::log("[DEBUG] HttpServer::run iniciando na porta " + std::to_string(port));

    crow::SimpleApp app;

    CROW_ROUTE(app, "/action").methods(crow::HTTPMethod::POST)
        ([this](const crow::request& req) {

        SessionManager::log("[HTTP] /action recebido");

        return this->handleAction(req);

            });

    CROW_ROUTE(app, "/cases").methods(crow::HTTPMethod::GET)
        ([this]() {
        auto cases = storage->listAvailableCases();
     
        std::vector<crow::json::wvalue> casesJson;
        for (const auto& c : cases) {
            crow::json::wvalue cv;
            cv["id"] = c.id;
            cv["title"] = c.title;
            cv["shortDescription"] = c.shortDescription;
            casesJson.push_back(cv);
        }

        crow::json::wvalue finalJson;
        finalJson["cases"] = std::move(casesJson);
        return finalJson;

            });

    CROW_ROUTE(app, "/cases/<string>").methods(crow::HTTPMethod::GET)
        ([this](std::string caseId) {
        auto caseOpt = storage->getCase(caseId);
        if (!caseOpt) return crow::response(404, "Caso nao encontrado");

        const auto& c = *caseOpt;
        crow::json::wvalue j;
        j["id"] = c.id;
        j["title"] = c.title;
        j["description"] = c.description;

        crow::json::wvalue topo;

        std::vector<crow::json::wvalue> mods;
        for (const auto& m : c.systemTopology.modules) {
            crow::json::wvalue mv; mv["name"] = m.name; mods.push_back(mv);
        }
        topo["modules"] = std::move(mods);

        std::vector<crow::json::wvalue> funcs;
        for (const auto& f : c.systemTopology.functions) {
            crow::json::wvalue fv; fv["name"] = f.name; fv["parentId"] = f.parentId; funcs.push_back(fv);
        }
        topo["functions"] = std::move(funcs);

        std::vector<crow::json::wvalue> conns;
        for (const auto& cn : c.systemTopology.connections) {
            crow::json::wvalue cv; cv["id"] = cn.id; cv["from"] = cn.from; cv["to"] = cn.to; conns.push_back(cv);
        }
        topo["connections"] = std::move(conns);

        j["systemTopology"] = std::move(topo);


        return crow::response(j);
            });

    auto wsOpenHandler = std::bind(&HttpServer::handleWebSocketOpen, this, std::placeholders::_1);
    auto wsCloseHandler = std::bind(&HttpServer::handleWebSocketClose, this,
        std::placeholders::_1, std::placeholders::_2);
    auto wsMessageHandler = std::bind(&HttpServer::handleWebSocketMessage, this,
        std::placeholders::_1, std::placeholders::_2,
        std::placeholders::_3);

    CROW_WEBSOCKET_ROUTE(app, "/ws")
        .onopen(wsOpenHandler)
        .onclose(wsCloseHandler)
        .onmessage(wsMessageHandler);

    SessionManager::log("[DEBUG] Servidor iniciando...");

    app.port(port).multithreaded().run();

}

// Handlers WebSocket

void HttpServer::handleWebSocketOpen(crow::websocket::connection& conn) {
    SessionManager::log("[WS] Nova conexao: " + std::to_string((uintptr_t)&conn));
}

void HttpServer::handleWebSocketClose(crow::websocket::connection& conn, const std::string& reason) {
    sessionManager->unregisterConnection(&conn);
    SessionManager::log("[WS] Fechado (" + reason + ")");
}

void HttpServer::handleWebSocketMessage(crow::websocket::connection& conn, const std::string& data, bool is_binary) {
    if (is_binary) return;

    try {
        auto msg = crow::json::load(data);
        if (!msg || !msg.has("type")) {
            SessionManager::sendTo(&conn, "{\"type\":\"ERROR\",\"message\":\"Invalid JSON\"}");
            return;
        }

        std::string type = msg["type"].s();

        if (type == "CREATE_LOBBY") {
            if (msg.has("playerName"))
                processCreateLobby(&conn, msg["playerName"].s());
        }
        else if (type == "JOIN_AS_PLAYER") {
            if (msg.has("sessionId") && msg.has("playerName"))
                processJoinAsPlayer(&conn, msg["sessionId"].s(), msg["playerName"].s());
        }
        else if (type == "JOIN_AS_MASTER") {
            if (msg.has("sessionId") && msg.has("masterName"))
                processJoinAsMaster(&conn, msg["sessionId"].s(), msg["masterName"].s());
        }
        else if (type == "GET_LOBBY_INFO") {
            if (msg.has("sessionId"))
                processGetLobbyInfo(&conn, msg["sessionId"].s());
        }
        else if (type == "START_GAME") {

            std::string caseId = msg.has("caseId") ? msg["caseId"].s() : std::string("case_robotics_001");
            std::string pName = msg.has("playerName") ? msg["playerName"].s() : std::string("");

            if (msg.has("sessionId") && !pName.empty())
                processStartGame(&conn, msg["sessionId"].s(), pName, caseId);
            else
                SessionManager::sendTo(&conn, "{\"type\":\"ERROR\",\"message\":\"START_GAME requer sessionId e playerName\"}");
        }
        else if (type == "SUBMIT_SOLUTION") {
            if (msg.has("sessionId") && msg.has("answers")) {
                std::vector<std::string> answers;
                for (const auto& a : msg["answers"]) answers.push_back(a.s());
                processSubmitSolution(&conn, msg["sessionId"].s(), answers);
            }
        }
        else if (type == "VALIDATE_SOLUTION") {
            if (msg.has("sessionId") && msg.has("approved")) {
                processValidateSolution(&conn, msg["sessionId"].s(), msg["approved"].b());
            }
        }
        else if (type == "GAME_ACTION") {
            if (msg.has("sessionId") && msg.has("playerId") && msg.has("actionType") && msg.has("targetId")) {
                ActionType aType = static_cast<ActionType>(msg["actionType"].i());
                processGameAction(&conn, msg["sessionId"].s(), msg["playerId"].s(), aType, msg["targetId"].s());
            }
        }
        else if (type == "SAVE_NOTE") {
            if (msg.has("sessionId") && msg.has("playerId") && msg.has("clueId") && msg.has("content")) {
                processSaveNote(&conn, msg["sessionId"].s(), msg["playerId"].s(), msg["clueId"].s(), msg["content"].s());
            }
        }
        else if (type == "LEAVE_LOBBY") {
            sessionManager->unregisterConnection(&conn);
        }
    }
    catch (const std::exception& e) {
        SessionManager::log("[WS] Erro parsing: " + std::string(e.what()));
    }
}

// Lógica Assíncrona (TaskQueue)

void HttpServer::processCreateLobby(crow::websocket::connection* conn, const std::string& playerName) {
    std::string sessionId = generateSessionId();

    taskQueue->enqueue([this, conn, sessionId, playerName]() {
        PlayerInfo host;
        host.name = playerName;
        host.role = PlayerRole::Host;
        host.joinedAt = std::chrono::system_clock::now();

        if (storage->createLobby(sessionId, host)) {
            sessionManager->registerConnection(sessionId, conn);

            std::string resp = std::format(
                "{{\"type\":\"LOBBY_CREATED\",\"sessionId\":\"{}\",\"playerName\":\"{}\",\"role\":{}}}",
                sessionId, escapeJSON(playerName), (int)PlayerRole::Host
            );
            SessionManager::sendTo(conn, resp);

            SessionManager::log("[LOBBY] Criado: " + sessionId);
        }
        else {
            SessionManager::sendTo(conn, "{\"type\":\"ERROR\",\"message\":\"Falha ao criar lobby no DB\"}");
        }
        });
}

void HttpServer::processJoinAsPlayer(crow::websocket::connection* conn, const std::string& sessionId, const std::string& playerName) {
    taskQueue->enqueue([this, conn, sessionId, playerName]() {
        if (!storage->sessionExists(sessionId)) {
            SessionManager::sendTo(conn, "{\"type\":\"ERROR\",\"message\":\"Lobby nao encontrado\"}");
            return;
        }

        PlayerInfo p;
        p.name = playerName;
        p.role = PlayerRole::Player;
        p.joinedAt = std::chrono::system_clock::now();

        if (storage->addPlayerToLobby(sessionId, p)) {
            sessionManager->registerConnection(sessionId, conn);

            std::string resp = std::format(
                "{{\"type\":\"JOINED_LOBBY\",\"sessionId\":\"{}\",\"playerName\":\"{}\",\"role\":{}}}",
                sessionId, escapeJSON(playerName), (int)PlayerRole::Player
            );
            SessionManager::sendTo(conn, resp);

            broadcastLobbyState(sessionId);
        }
        else {
            SessionManager::sendTo(conn, "{\"type\":\"ERROR\",\"message\":\"Nao foi possivel entrar (Nome em uso ou erro DB)\"}");
        }
        });
}

void HttpServer::processJoinAsMaster(crow::websocket::connection* conn, const std::string& sessionId, const std::string& masterName) {
    taskQueue->enqueue([this, conn, sessionId, masterName]() {
        if (!storage->sessionExists(sessionId)) {
            SessionManager::sendTo(conn, "{\"type\":\"ERROR\",\"message\":\"Lobby nao encontrado\"}");
            return;
        }

        PlayerInfo m;
        m.name = masterName;
        m.role = PlayerRole::Master;
        m.joinedAt = std::chrono::system_clock::now();

        if (storage->addPlayerToLobby(sessionId, m)) {
            sessionManager->registerConnection(sessionId, conn);

            std::string resp = std::format(
                "{{\"type\":\"JOINED_LOBBY\",\"sessionId\":\"{}\",\"playerName\":\"{}\",\"role\":{}}}",
                sessionId, escapeJSON(masterName), (int)PlayerRole::Master
            );
            SessionManager::sendTo(conn, resp);

            broadcastLobbyState(sessionId);
        }
        else {
            SessionManager::sendTo(conn, "{\"type\":\"ERROR\",\"message\":\"Mestre ja existe ou erro\"}");
        }
        });
}

void HttpServer::processGetLobbyInfo(crow::websocket::connection* conn, const std::string& sessionId) {
    taskQueue->enqueue([this, conn, sessionId]() {
        auto lobbyOpt = storage->getLobby(sessionId);
        if (lobbyOpt) {
            auto& lobby = *lobbyOpt;
            std::ostringstream oss;
            oss << "{\"type\":\"LOBBY_INFO\",\"exists\":true,\"sessionId\":\"" << lobby.sessionId << "\",";
            oss << "\"players\":[";
            bool first = true;
            for (const auto& p : lobby.players) {
                if (!first) oss << ",";
                oss << "{\"name\":\"" << escapeJSON(p.name) << "\",\"role\":" << (int)p.role << "}";
                first = false;
            }
            oss << "]}";
            SessionManager::sendTo(conn, oss.str());
        }
        else {
            SessionManager::sendTo(conn, "{\"type\":\"LOBBY_INFO\",\"exists\":false}");
        }
        });
}

void HttpServer::processStartGame(crow::websocket::connection* conn, const std::string& sessionId, const std::string& playerName, const std::string& caseId) {
    taskQueue->enqueue([this, conn, sessionId, playerName, caseId]() {
        auto lobbyOpt = storage->getLobby(sessionId);
        if (!lobbyOpt) {
            SessionManager::sendTo(conn, "{\"type\":\"ERROR\",\"message\":\"Lobby nao encontrado\"}");
            return;
        }

        const auto& lobby = *lobbyOpt;

        bool isHost = false;
        for (const auto& p : lobby.players) {
            if (p.name == playerName && p.role == PlayerRole::Host) {
                isHost = true;
                break;
            }
        }

        if (!isHost) {
            SessionManager::log("[WARN] Tentativa de inicio por nao-host: " + playerName);
            SessionManager::sendTo(conn, "{\"type\":\"ERROR\",\"message\":\"Permissao negada. Apenas o Host pode iniciar.\"}");
            return;
        }

        if (!lobby.canStartGame()) {
            SessionManager::sendTo(conn, "{\"type\":\"ERROR\",\"message\":\"Nao e possivel iniciar. Aguardando Mestre ou Jogadores.\"}");
            return;
        }

        std::vector<std::string> allParticipants;
        std::string hostPlayerId;
        std::string masterPlayerId;

        for (const auto& p : lobby.players) {
            if (p.role == PlayerRole::Player || p.role == PlayerRole::Host || p.role == PlayerRole::Master) {
                allParticipants.push_back(p.name);
            }
            if (p.role == PlayerRole::Host) {
                hostPlayerId = p.name;
            }
            if (p.role == PlayerRole::Master) {
                masterPlayerId = p.name;
            }
        }

        if (!engine->initializeGameFromLobby(sessionId, caseId, allParticipants, hostPlayerId, masterPlayerId)) {
            SessionManager::sendTo(conn, "{\"type\":\"ERROR\",\"message\":\"Erro ao criar sessao de jogo no banco.\"}");
            return;
        }

        if (storage->updatePhase(sessionId, GamePhase::Investigation)) {
            std::string msg = std::format(
                "{{\"type\":\"GAME_STARTED\",\"sessionId\":\"{}\",\"caseId\":\"{}\"}}",
                sessionId, escapeJSON(caseId)
            );
            sessionManager->broadcastToSession(sessionId, msg);
            SessionManager::log("[GAME] Jogo iniciado pelo Host " + playerName + " na sessao " + sessionId);
        }
        });
}

void HttpServer::processSubmitSolution(crow::websocket::connection* conn, const std::string& sessionId, const std::vector<std::string>& answers) {
    taskQueue->enqueue([this, conn, sessionId, answers]() {
        auto gameStateOpt = storage->getGameState(sessionId);
        if (!gameStateOpt) {
            SessionManager::sendTo(conn, "{\"type\":\"ERROR\",\"message\":\"Sessao de jogo nao encontrada.\"}");
            return;
        }

        auto bugCaseOpt = storage->getCase(gameStateOpt->currentCaseId);
        if (!bugCaseOpt) {
            SessionManager::sendTo(conn, "{\"type\":\"ERROR\",\"message\":\"Caso nao encontrado no banco.\"}");
            return;
        }

        const auto& bugCase = *bugCaseOpt;

        std::ostringstream oss;
        oss << "{\"type\":\"SOLUTION_FOR_REVIEW\",";
        oss << "\"sessionId\":\"" << sessionId << "\",";

        oss << "\"teamAnswers\":[";
        for (size_t i = 0; i < answers.size(); ++i) {
            if (i > 0) oss << ",";
            oss << "\"" << escapeJSON(answers[i]) << "\"";
        }
        oss << "],";

        oss << "\"questions\":[";
        for (size_t i = 0; i < bugCase.solutionQuestions.size(); ++i) {
            if (i > 0) oss << ",";
            oss << "\"" << escapeJSON(bugCase.solutionQuestions[i]) << "\"";
        }
        oss << "],";

        oss << "\"correctAnswers\":[";
        for (size_t i = 0; i < bugCase.correctAnswers.size(); ++i) {
            if (i > 0) oss << ",";
            oss << "\"" << escapeJSON(bugCase.correctAnswers[i]) << "\"";
        }
        oss << "]";

        oss << "}";

        sessionManager->broadcastToSession(sessionId, oss.str());

        SessionManager::log("[GAME] Solucao enviada para revisao do Mestre na sessao: " + sessionId);
        });
}

void FindTheBug::HttpServer::processValidateSolution(crow::websocket::connection* conn, const std::string& sessionId, bool approved)
{
    taskQueue->enqueue([this, sessionId, approved]() {

        GameResult result = engine->finalizeSession(sessionId, approved);

        if (result == GameResult::Victory) {
            sessionManager->broadcastToSession(sessionId, "{\"type\":\"GAME_VICTORY\"}");
            SessionManager::log("[GAME] Vitoria na sessao " + sessionId + ". Encerrando.");

            storage->deleteSession(sessionId);
            sessionManager->closeSession(sessionId);
        }
        else if (result == GameResult::Defeat) {
            sessionManager->broadcastToSession(sessionId, "{\"type\":\"GAME_OVER\"}");
            SessionManager::log("[GAME] Derrota na sessao " + sessionId + ". Encerrando.");

            storage->deleteSession(sessionId);
            sessionManager->closeSession(sessionId);
        }
        else {
            broadcastLobbyState(sessionId);

            sessionManager->broadcastToSession(sessionId, "{\"type\":\"SOLUTION_REJECTED\",\"message\":\"Solucao incorreta. Penalidade aplicada.\"}");
        }
        });
}

void HttpServer::processGameAction(crow::websocket::connection* conn, const std::string& sessionId, const std::string& playerId, ActionType actionType, const std::string& targetId) {
    taskQueue->enqueue([this, conn, sessionId, playerId, actionType, targetId]() {

        auto result = engine->processAction(playerId, actionType, targetId, sessionId);

        if (!result.success) {
            std::string err = std::format("{{\"type\":\"ERROR\",\"message\":\"{}\"}}", escapeJSON(result.message));
            SessionManager::sendTo(conn, err);
            return;
        }

        broadcastGameState(sessionId);

        if (result.revealedClue) {
            std::string revealMsg = std::format(
                "{{\"type\":\"CLUE_REVEALED\",\"clueId\":\"{}\",\"content\":\"{}\",\"duration\":30,\"investigator\":\"{}\"}}",
                result.revealedClue->id,
                escapeJSON(result.revealedClue->content),
                playerId
            );
            sessionManager->broadcastToSession(sessionId, revealMsg);
        }
        });
}

void HttpServer::processSaveNote(crow::websocket::connection* conn, const std::string& sessionId, const std::string& playerId, const std::string& clueId, const std::string& content) {
    taskQueue->enqueue([this, conn, sessionId, playerId, clueId, content]() {

        bool success = engine->savePlayerNote(sessionId, playerId, clueId, content);

        if (success) {
            // Sucesso: Atualiza o estado para todos (a nota aparece no "caderno")
            broadcastGameState(sessionId);
        }
        else {
            SessionManager::sendTo(conn, "{\"type\":\"ERROR\",\"message\":\"Falha ao salvar nota (Pista nao encontrada?)\"}");
        }
        });
}

// Helpers

void HttpServer::broadcastGameState(const std::string& sessionId) {
    auto stateOpt = storage->getGameState(sessionId);
    if (!stateOpt) return;
    auto& state = *stateOpt;

    crow::json::wvalue json;
    json["type"] = "GAME_STATE_UPDATE";
    json["sessionId"] = state.sessionId;
    json["currentDay"] = state.currentDay;
    json["remainingPoints"] = state.remainingPoints;

    json["currentTurnIndex"] = state.currentTurnIndex;
    if (!state.turnOrder.empty()) {
        json["currentTurnPlayer"] = state.turnOrder[state.currentTurnIndex];
    }

    std::vector<crow::json::wvalue> cluesJson;
    for (const auto& c : state.discoveredClues) {
        crow::json::wvalue cj;
        cj["id"] = c.id;
        cj["targetId"] = c.targetId;
        cj["type"] = static_cast<int>(c.type);
        cj["content"] = c.content;

        crow::json::wvalue notesJson;
        for (const auto& pair : c.playerNotes) {
            notesJson[pair.first] = pair.second;
        }
        cj["playerNotes"] = std::move(notesJson);

        cluesJson.push_back(cj);
    }
    json["discoveredClues"] = std::move(cluesJson);

    sessionManager->broadcastToSession(sessionId, json.dump());
}

void HttpServer::broadcastLobbyState(const std::string& sessionId) {
    auto lobbyOpt = storage->getLobby(sessionId);
    if (!lobbyOpt) return;

    auto& lobby = *lobbyOpt;
    std::ostringstream oss;
    oss << "{\"type\":\"LOBBY_UPDATE\",\"sessionId\":\"" << lobby.sessionId << "\",";
    oss << "\"canStart\":" << (lobby.canStartGame() ? "true" : "false") << ",";
    oss << "\"players\":[";

    bool first = true;
    for (const auto& p : lobby.players) {
        if (p.role == PlayerRole::Master) continue;
        if (!first) oss << ",";
        oss << "{\"name\":\"" << escapeJSON(p.name) << "\",\"role\":" << (int)p.role << "}";
        first = false;
    }
    oss << "]}";

    sessionManager->broadcastToSession(sessionId, oss.str());
}

std::string HttpServer::generateSessionId() {
    static const char alphanum[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, sizeof(alphanum) - 2);
    std::string s;
    for (int i = 0; i < 6; ++i) s += alphanum[dis(gen)];
    return s;
}

crow::response HttpServer::handleAction(const crow::request& req) {
    return crow::response(200, "OK");
}