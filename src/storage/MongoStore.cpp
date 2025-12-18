#include "MongoStore.hpp"

#include <bsoncxx/json.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/pool.hpp>
#include <mongocxx/uri.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/builder/stream/helpers.hpp>
#include <bsoncxx/builder/stream/array.hpp>

#include <iostream>
#include <chrono>
#include <algorithm>
#include <print>

using namespace FindTheBug;
using namespace bsoncxx::builder::stream;

static mongocxx::instance instance{};

class MongoStore::Impl {
public:
    std::shared_ptr<mongocxx::pool> pool;
    std::string dbName;

    Impl(const std::string& uriString, const std::string& name)
        : dbName(name) {
        mongocxx::uri uri{ uriString };
        pool = std::make_shared<mongocxx::pool>(uri);
    }

    auto acquire() {
        return pool->acquire();
    }
};

MongoStore::MongoStore(const std::string& connectionUri, const std::string& dbName)
    : pImpl(std::make_unique<Impl>(connectionUri, dbName)) {
}

MongoStore::~MongoStore() = default;

// Operações de Lobby

bool MongoStore::createLobby(const std::string& sessionId, const PlayerInfo& host) {
    try {
        auto conn = pImpl->acquire();
        auto db = (*conn)[pImpl->dbName];
        auto collection = db["sessions"];

        auto doc = document{}
            << "sessionId" << sessionId
            << "phase" << static_cast<int>(GamePhase::Lobby)
            << "createdAt" << bsoncxx::types::b_date(std::chrono::system_clock::now())
            << "lastActivity" << bsoncxx::types::b_date(std::chrono::system_clock::now())
            << "players" << open_array
            << open_document
            << "name" << host.name
            << "role" << static_cast<int>(host.role)
            << "joinedAt" << bsoncxx::types::b_date(host.joinedAt)
            << close_document
            << close_array
            << finalize;

        collection.insert_one(doc.view());
        return true;
    }
    catch (const std::exception& e) {
        std::print("[MONGO] Error in createLobby: {}\n", e.what());
        return false;
    }
}

bool MongoStore::addPlayerToLobby(const std::string& sessionId, const PlayerInfo& player) {
    try {
        auto conn = pImpl->acquire();
        auto db = (*conn)[pImpl->dbName];
        auto collection = db["sessions"];

        auto update = document{}
            << "$push" << open_document
            << "players" << open_document
            << "name" << player.name
            << "role" << static_cast<int>(player.role)
            << "joinedAt" << bsoncxx::types::b_date(player.joinedAt)
            << close_document
            << close_document
            << "$set" << open_document
            << "lastActivity" << bsoncxx::types::b_date(std::chrono::system_clock::now())
            << close_document
            << finalize;

        auto result = collection.update_one(
            document{} << "sessionId" << sessionId << finalize,
            update.view()
        );

        return result && result->modified_count() > 0;
    }
    catch (const std::exception& e) {
        std::print("[MONGO] Error in addPlayerToLobby: {}\n", e.what());
        return false;
    }
}

bool MongoStore::removePlayerFromLobby(const std::string& sessionId, const std::string& playerName) {
    try {
        auto conn = pImpl->acquire();
        auto db = (*conn)[pImpl->dbName];
        auto collection = db["sessions"];

        auto update = document{}
            << "$pull" << open_document
            << "players" << open_document
            << "name" << playerName
            << close_document
            << close_document
            << "$set" << open_document
            << "lastActivity" << bsoncxx::types::b_date(std::chrono::system_clock::now())
            << close_document
            << finalize;

        auto result = collection.update_one(
            document{} << "sessionId" << sessionId << finalize,
            update.view()
        );

        return result && result->modified_count() > 0;
    }
    catch (const std::exception& e) {
        std::print("[MONGO] Error in removePlayerFromLobby: {}\n", e.what());
        return false;
    }
}

bool MongoStore::updatePhase(const std::string& sessionId, GamePhase newPhase) {
    try {
        auto conn = pImpl->acquire();
        auto db = (*conn)[pImpl->dbName];
        auto collection = db["sessions"];

        auto result = collection.update_one(
            document{} << "sessionId" << sessionId << finalize,
            document{} << "$set" << open_document
            << "phase" << static_cast<int>(newPhase)
            << "lastActivity" << bsoncxx::types::b_date(std::chrono::system_clock::now())
            << close_document << finalize
        );
        return result && result->modified_count() > 0;
    }
    catch (const std::exception& e) {
        std::print("[MONGO] Error in updatePhase: {}\n", e.what());
        return false;
    }
}

std::optional<LobbyInfo> MongoStore::getLobby(const std::string& sessionId) const {
    try {
        auto conn = pImpl->acquire();
        auto db = (*conn)[pImpl->dbName];
        auto collection = db["sessions"];

        auto result = collection.find_one(
            document{} << "sessionId" << sessionId << finalize
        );

        if (!result) return std::nullopt;

        auto view = result->view();
        LobbyInfo lobby;

        if (view["sessionId"])
            lobby.sessionId = std::string(view["sessionId"].get_string().value);

        if (view["phase"])
            lobby.phase = static_cast<GamePhase>(view["phase"].get_int32().value);

        if (view["players"] && view["players"].type() == bsoncxx::type::k_array) {
            for (const auto& elem : view["players"].get_array().value) {
                auto doc = elem.get_document().view();
                PlayerInfo p;
                if (doc["name"]) p.name = std::string(doc["name"].get_string().value);
                if (doc["role"]) p.role = static_cast<PlayerRole>(doc["role"].get_int32().value);

                lobby.players.push_back(p);
            }
        }

        return lobby;
    }
    catch (const std::exception& e) {
        std::print("[MONGO] Error in getLobby: {}\n", e.what());
        return std::nullopt;
    }
}

bool MongoStore::sessionExists(const std::string& sessionId) const {
    try {
        auto conn = pImpl->acquire();
        auto db = (*conn)[pImpl->dbName];
        auto collection = db["sessions"];
        return collection.count_documents(document{} << "sessionId" << sessionId << finalize) > 0;
    }
    catch (...) { return false; }
}

// Operações de Jogo

std::optional<BugCase> MongoStore::getCase(const std::string& caseId) const {
    try {
        auto conn = pImpl->acquire();
        auto db = (*conn)[pImpl->dbName];
        auto collection = db["cases"];

        auto result = collection.find_one(document{} << "id" << caseId << finalize);
        if (!result) return std::nullopt;

        auto view = result->view();
        BugCase bc;

        if (view["id"]) bc.id = std::string(view["id"].get_string().value);
        if (view["title"]) bc.title = std::string(view["title"].get_string().value);
        if (view["description"]) bc.description = std::string(view["description"].get_string().value);

        if (view["systemTopology"] && view["systemTopology"].type() == bsoncxx::type::k_document) {
            auto topoView = view["systemTopology"].get_document().view();

            if (topoView["modules"] && topoView["modules"].type() == bsoncxx::type::k_array) {
                for (const auto& el : topoView["modules"].get_array().value) {
                    auto doc = el.get_document().view();
                    ModuleNode m;
                    if (doc["name"]) m.name = std::string(doc["name"].get_string().value);
                    bc.systemTopology.modules.push_back(m);
                }
            }

            if (topoView["functions"] && topoView["functions"].type() == bsoncxx::type::k_array) {
                for (const auto& el : topoView["functions"].get_array().value) {
                    auto doc = el.get_document().view();
                    FunctionNode f;
                    if (doc["name"]) f.name = std::string(doc["name"].get_string().value);
                    if (doc["parentId"]) f.parentId = std::string(doc["parentId"].get_string().value);
                    bc.systemTopology.functions.push_back(f);
                }
            }

            if (topoView["connections"] && topoView["connections"].type() == bsoncxx::type::k_array) {
                for (const auto& el : topoView["connections"].get_array().value) {
                    auto doc = el.get_document().view();
                    ConnectionNode c;
                    if (doc["id"]) c.id = std::string(doc["id"].get_string().value);
                    if (doc["from"]) c.from = std::string(doc["from"].get_string().value);
                    if (doc["to"]) c.to = std::string(doc["to"].get_string().value);
                    bc.systemTopology.connections.push_back(c);
                }
            }
        }

        if (view["availableClues"] && view["availableClues"].type() == bsoncxx::type::k_array) {
            for (const auto& elem : view["availableClues"].get_array().value) {
                Clue c;
                auto doc = elem.get_document().view();
                if (doc["id"]) c.id = std::string(doc["id"].get_string().value);
                if (doc["targetId"]) c.targetId = std::string(doc["targetId"].get_string().value);
                if (doc["targetType"]) c.targetType = static_cast<TargetType>(doc["targetType"].get_int32().value);
                if (doc["content"]) c.content = std::string(doc["content"].get_string().value);
                if (doc["cost"]) c.cost = doc["cost"].get_int32().value;
                if (doc["type"]) c.type = static_cast<ClueType>(doc["type"].get_int32().value);
                bc.availableClues.push_back(c);
            }
        }

        if (view["solutionQuestions"] && view["solutionQuestions"].type() == bsoncxx::type::k_array) {
            for (const auto& elem : view["solutionQuestions"].get_array().value) {
                bc.solutionQuestions.push_back(std::string(elem.get_string().value));
            }
        }

        if (view["correctAnswers"] && view["correctAnswers"].type() == bsoncxx::type::k_array) {
            for (const auto& elem : view["correctAnswers"].get_array().value) {
                bc.correctAnswers.push_back(std::string(elem.get_string().value));
            }
        }

        return bc;
    }
    catch (...) {
        return std::nullopt;
    }
}

std::optional<GameState> MongoStore::getGameState(const std::string& sessionId) const {
    try {
        auto conn = pImpl->acquire();
        auto db = (*conn)[pImpl->dbName];
        auto collection = db["sessions"];

        auto result = collection.find_one(document{} << "sessionId" << sessionId << finalize);
        if (!result) return std::nullopt;

        auto view = result->view();
        GameState gs;

        if (view["sessionId"]) gs.sessionId = std::string(view["sessionId"].get_string().value);
        if (view["currentCaseId"]) gs.currentCaseId = std::string(view["currentCaseId"].get_string().value);
        if (view["currentDay"]) gs.currentDay = view["currentDay"].get_int32().value;
        if (view["remainingPoints"]) gs.remainingPoints = view["remainingPoints"].get_int32().value;
        if (view["isCompleted"]) gs.isCompleted = view["isCompleted"].get_bool().value;
        if (view["isSuddenDeath"]) gs.isSuddenDeath = view["isSuddenDeath"].get_bool().value;
        if (view["hostPlayerId"]) gs.hostPlayerId = std::string(view["hostPlayerId"].get_string().value);
        if (view["masterPlayerId"]) gs.masterPlayerId = std::string(view["masterPlayerId"].get_string().value);

        if (view["currentTurnIndex"]) gs.currentTurnIndex = view["currentTurnIndex"].get_int32().value;

        if (view["turnOrder"] && view["turnOrder"].type() == bsoncxx::type::k_array) {
            for (const auto& elem : view["turnOrder"].get_array().value) {
                gs.turnOrder.push_back(std::string(elem.get_string().value));
            }
        }

        if (view["playerIds"] && view["playerIds"].type() == bsoncxx::type::k_array) {
            for (const auto& elem : view["playerIds"].get_array().value) {
                gs.playerIds.push_back(std::string(elem.get_string().value));
            }
        }

        if (view["investigatedTargets"] && view["investigatedTargets"].type() == bsoncxx::type::k_array) {
            for (const auto& elem : view["investigatedTargets"].get_array().value) {
                gs.investigatedTargets.insert(std::string(elem.get_string().value));
            }
        }

        if (view["breakpointedTargets"] && view["breakpointedTargets"].type() == bsoncxx::type::k_array) {
            for (const auto& elem : view["breakpointedTargets"].get_array().value) {
                gs.breakpointedTargets.insert(std::string(elem.get_string().value));
            }
        }

        if (view["discoveredClues"] && view["discoveredClues"].type() == bsoncxx::type::k_array) {
            for (const auto& elem : view["discoveredClues"].get_array().value) {
                DiscoveredClue c; // Note: Usando DiscoveredClue, não Clue
                auto doc = elem.get_document().view();

                if (doc["id"]) c.id = std::string(doc["id"].get_string().value);
                if (doc["targetId"]) c.targetId = std::string(doc["targetId"].get_string().value);
                if (doc["targetType"]) c.targetType = static_cast<TargetType>(doc["targetType"].get_int32().value);
                if (doc["type"]) c.type = static_cast<ClueType>(doc["type"].get_int32().value);
                if (doc["content"]) c.content = std::string(doc["content"].get_string().value);

                if (doc["playerNotes"] && doc["playerNotes"].type() == bsoncxx::type::k_document) {
                    auto notesView = doc["playerNotes"].get_document().view();
                    for (auto note : notesView) {
                        std::string pId(note.key());
                        std::string txt(note.get_string().value);
                        c.playerNotes[pId] = txt;
                    }
                }

                gs.discoveredClues.push_back(c);
            }
        }

        return gs;
    }
    catch (const std::exception& e) {
        std::print("[MONGO] Error in getGameState: {}\n", e.what());
        return std::nullopt;
    }
}

std::vector<CaseSummary> FindTheBug::MongoStore::listAvailableCases() const
{
    std::vector<CaseSummary> summaries;

    try {
        auto conn = pImpl->acquire();
        auto db = (*conn)[pImpl->dbName];
        auto collection = db["cases"];

        mongocxx::options::find opts;
        
        opts.projection(document{}
            << "id" << 1
            << "title" << 1
            << "shortDescription" << 1
            << "_id" << 0
            << finalize);

        auto cursor = collection.find({}, opts);

        for (auto&& doc : cursor) {
            CaseSummary s;
            if (doc["id"]) s.id = std::string(doc["id"].get_string().value);
            if (doc["title"]) s.id = std::string(doc["title"].get_string().value);
            if (doc["shortDescription"])
                s.shortDescription = std::string(doc["shortDescription"].get_string().value);
            else
                s.shortDescription = "Sem descricao disponivel.";

            summaries.push_back(s);
        }
    }
    catch (const std::exception& e) {
        std::print("[MONGO] Erro ao listar casos: {}\n", e.what());
    }
    return summaries;
}

bool MongoStore::saveGameState(const GameState& state) {
    try {
        auto conn = pImpl->acquire();
        auto db = (*conn)[pImpl->dbName];
        auto collection = db["sessions"];

        bsoncxx::builder::stream::array inv_array;
        for (const auto& t : state.investigatedTargets) inv_array << t;

        bsoncxx::builder::stream::array bp_array;
        for (const auto& t : state.breakpointedTargets) bp_array << t;

        bsoncxx::builder::stream::array player_ids_array;
        for (const auto& playerId : state.playerIds) player_ids_array << playerId;

        bsoncxx::builder::stream::array turn_order_array;
        for (const auto& pid : state.turnOrder) turn_order_array << pid;

        bsoncxx::builder::stream::array clues_array;
        for (const auto& clue : state.discoveredClues) {

            bsoncxx::builder::stream::document notes_doc;
            for (const auto& pair : clue.playerNotes) {
                notes_doc << pair.first << pair.second;
            }

            clues_array << bsoncxx::builder::stream::open_document
                << "id" << clue.id
                << "targetId" << clue.targetId
                << "targetType" << static_cast<int>(clue.targetType)
                << "type" << static_cast<int>(clue.type)
                << "content" << clue.content
                << "playerNotes" << notes_doc
                << bsoncxx::builder::stream::close_document;
        }

        auto update_doc = document{} << "$set" << open_document
            << "currentCaseId" << state.currentCaseId
            << "currentDay" << state.currentDay
            << "remainingPoints" << state.remainingPoints
            << "isCompleted" << state.isCompleted
            << "isSuddenDeath" << state.isSuddenDeath
            << "playerIds" << player_ids_array
            << "hostPlayerId" << state.hostPlayerId
            << "currentTurnIndex" << state.currentTurnIndex
            << "turnOrder" << turn_order_array

            << "discoveredClues" << clues_array
            << "investigatedTargets" << inv_array
            << "breakpointedTargets" << bp_array
            << "lastActivity" << bsoncxx::types::b_date(std::chrono::system_clock::now())
            << close_document << finalize;

        auto result = collection.update_one(
            document{} << "sessionId" << state.sessionId << finalize,
            update_doc.view()
        );

        return result && result->modified_count() > 0;
    }
    catch (const std::exception& e) {
        std::print("[MONGO] Error in saveGameState: {}\n", e.what());
        return false;
    }
}

bool MongoStore::deleteSession(const std::string& sessionId) {
    try {
        auto conn = pImpl->acquire();
        auto db = (*conn)[pImpl->dbName];
        auto collection = db["sessions"];

        std::print("[MONGO DEBUG] Iniciando remocao da sessao: {}\n", sessionId);

        auto count = collection.count_documents(document{} << "sessionId" << sessionId << finalize);
        std::print("[MONGO DEBUG] Documentos encontrados antes do delete: {}\n", count);

        if (count == 0) {
            std::print("[MONGO AVISO] A sessao {} nao foi encontrada no banco para deletar.\n", sessionId);
            return false;
        }

        auto result = collection.delete_one(
            document{} << "sessionId" << sessionId << finalize
        );

        if (result) {
            std::print("[MONGO DEBUG] Operacao delete concluida. Deletados: {}\n", result->deleted_count());
            return result->deleted_count() > 0;
        }
        else {
            std::print("[MONGO ERRO] O driver retornou um resultado vazio (std::nullopt).\n");
            return false;
        }
    }
    catch (const std::exception& e) {
        std::print("[MONGO CRITICO] Excecao ao deletar sessao: {}\n", e.what());
        return false;
    }
    catch (...) {
        std::print("[MONGO CRITICO] Erro desconhecido ao deletar sessao.\n");
        return false;
    }
}