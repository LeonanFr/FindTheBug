#include "MongoStore.hpp"

#define NOMINMAX 

#include <bsoncxx/json.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/uri.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/builder/stream/helpers.hpp>
#include <bsoncxx/builder/stream/array.hpp>

#include <iostream>
#include <vector>
#include <optional>
#include <algorithm>

using namespace FindTheBug;

class MongoStore::Impl {
public:
    static mongocxx::instance instance;
    mongocxx::client client;
    mongocxx::database db;

    Impl(const std::string& uri, const std::string& name)
        : client{ mongocxx::uri{uri} }, db{ client[name] } {
    }
};

mongocxx::instance MongoStore::Impl::instance{};

MongoStore::MongoStore(const std::string& connectionUri, const std::string& dbName)
    : impl{ std::make_unique<Impl>(connectionUri, dbName) } {
}

MongoStore::~MongoStore() = default;

std::optional<BugCase> MongoStore::getCase(const std::string& caseId) const {
    try {
        auto collection = impl->db["cases"];
        auto result = collection.find_one(bsoncxx::builder::stream::document{} << "id" << caseId << bsoncxx::builder::stream::finalize);

        if (!result) return std::nullopt;

        auto view = result->view();
        BugCase bc;

        if (view["id"]) bc.id = std::string(view["id"].get_string().value);
        if (view["title"]) bc.title = std::string(view["title"].get_string().value);
        if (view["description"]) bc.description = std::string(view["description"].get_string().value);

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

        return bc;
    }
    catch (const std::exception& e) {
        std::cerr << "Erro getCase: " << e.what() << std::endl;
        return std::nullopt;
    }
}
std::optional<GameState> MongoStore::getSession(const std::string& sessionId) const {
    try {
        auto collection = impl->db["sessions"];
        auto result = collection.find_one(bsoncxx::builder::stream::document{}
        << "sessionId" << sessionId << bsoncxx::builder::stream::finalize);

        if (!result) return std::nullopt;

        auto view = result->view();
        GameState gs;

        if (view["sessionId"]) gs.sessionId = std::string(view["sessionId"].get_string().value);
        if (view["currentCaseId"]) gs.currentCaseId = std::string(view["currentCaseId"].get_string().value);
        if (view["currentDay"]) gs.currentDay = view["currentDay"].get_int32().value;
        if (view["remainingPoints"]) gs.remainingPoints = view["remainingPoints"].get_int32().value;
        if (view["isCompleted"]) gs.isCompleted = view["isCompleted"].get_bool().value;
        if (view["isSuddenDeath"]) gs.isSuddenDeath = view["isSuddenDeath"].get_bool().value;

        // playerIds
        if (view["playerIds"] && view["playerIds"].type() == bsoncxx::type::k_array) {
            for (const auto& elem : view["playerIds"].get_array().value) {
                gs.playerIds.push_back(std::string(elem.get_string().value));
            }
        }

        if (view["hostPlayerId"]) {
            gs.hostPlayerId = std::string(view["hostPlayerId"].get_string().value);
        }

        if (view["discoveredClues"] && view["discoveredClues"].type() == bsoncxx::type::k_array) {
            for (const auto& elem : view["discoveredClues"].get_array().value) {
                Clue c;
                auto doc = elem.get_document().view();
                if (doc["id"]) c.id = std::string(doc["id"].get_string().value);
                if (doc["targetId"]) c.targetId = std::string(doc["targetId"].get_string().value);
                if (doc["targetType"]) c.targetType = static_cast<TargetType>(doc["targetType"].get_int32().value);
                if (doc["type"]) c.type = static_cast<ClueType>(doc["type"].get_int32().value);
                if (doc["content"]) c.content = std::string(doc["content"].get_string().value);
                if (doc["cost"]) c.cost = doc["cost"].get_int32().value;
                gs.discoveredClues.push_back(c);
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

        return gs;
    }
    catch (const std::exception& e) {
        std::cerr << "Erro getSession: " << e.what() << std::endl;
        return std::nullopt;
    }
}

bool MongoStore::createSession(const GameState& state) {
    try {
        auto collection = impl->db["sessions"];

        using bsoncxx::builder::stream::document;
        using bsoncxx::builder::stream::finalize;

        bsoncxx::builder::stream::array inv_array;
        for (const auto& t : state.investigatedTargets) inv_array << t;

        bsoncxx::builder::stream::array bp_array;
        for (const auto& t : state.breakpointedTargets) bp_array << t;

        bsoncxx::builder::stream::array player_ids_array;
        for (const auto& playerId : state.playerIds) player_ids_array << playerId;

        bsoncxx::builder::stream::array clues_array;
        for (const auto& clue : state.discoveredClues) {
            clues_array << bsoncxx::builder::stream::open_document
                << "id" << clue.id
                << "targetId" << clue.targetId
                << "targetType" << static_cast<int>(clue.targetType)
                << "type" << static_cast<int>(clue.type)
                << "content" << clue.content
                << "cost" << clue.cost
                << bsoncxx::builder::stream::close_document;
        }

        auto doc = document{}
            << "sessionId" << state.sessionId
            << "currentCaseId" << state.currentCaseId
            << "currentDay" << state.currentDay
            << "remainingPoints" << state.remainingPoints
            << "isCompleted" << state.isCompleted
            << "isSuddenDeath" << state.isSuddenDeath
            << "playerIds" << player_ids_array
            << "hostPlayerId" << state.hostPlayerId
            << "discoveredClues" << clues_array
            << "investigatedTargets" << inv_array
            << "breakpointedTargets" << bp_array
            << finalize;

        auto result = collection.insert_one(doc.view());
        return result ? true : false;
    }
    catch (const std::exception& e) {
        std::cerr << "Erro createSession: " << e.what() << std::endl;
        return false;
    }
}

bool MongoStore::updateSession(const GameState& state) {
    try {
        auto collection = impl->db["sessions"];

        using bsoncxx::builder::stream::document;
        using bsoncxx::builder::stream::open_document;
        using bsoncxx::builder::stream::close_document;
        using bsoncxx::builder::stream::finalize;

        bsoncxx::builder::stream::array inv_array;
        for (const auto& t : state.investigatedTargets) inv_array << t;

        bsoncxx::builder::stream::array bp_array;
        for (const auto& t : state.breakpointedTargets) bp_array << t;

        bsoncxx::builder::stream::array player_ids_array;
        for (const auto& playerId : state.playerIds) player_ids_array << playerId;

        bsoncxx::builder::stream::array clues_array;
        for (const auto& clue : state.discoveredClues) {
            clues_array << bsoncxx::builder::stream::open_document
                << "id" << clue.id
                << "targetId" << clue.targetId
                << "targetType" << static_cast<int>(clue.targetType)
                << "type" << static_cast<int>(clue.type)
                << "content" << clue.content
                << "cost" << clue.cost
                << bsoncxx::builder::stream::close_document;
        }

        auto update_doc = document{} << "$set" << open_document
            << "currentDay" << state.currentDay
            << "remainingPoints" << state.remainingPoints
            << "isCompleted" << state.isCompleted
            << "isSuddenDeath" << state.isSuddenDeath
            << "playerIds" << player_ids_array
            << "hostPlayerId" << state.hostPlayerId
            << "discoveredClues" << clues_array
            << "investigatedTargets" << inv_array
            << "breakpointedTargets" << bp_array
            << close_document << finalize;

        auto result = collection.update_one(
            document{} << "sessionId" << state.sessionId << finalize,
            update_doc.view()
        );

        return result && result->modified_count() > 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Erro updateSession: " << e.what() << std::endl;
        return false;
    }
}