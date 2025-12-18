#include "SessionManager.hpp"

#include "crow.h"

using namespace FindTheBug;

std::string SessionManager::generateSessionId() const {
	static const char alphanum[] =
		"0123456789"
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	
	static std::random_device rd;
	static std::mt19937 gen(rd());
	static std::uniform_int_distribution<> dis(0, sizeof(alphanum) - 2);

	const int len = 6;

	std::string id;
	for (int i = 0; i < len; ++i) {
		id += alphanum[dis(gen)];
	}

	if (lobbyExists(id)) {
		return generateSessionId();
	}

	return id;
}

std::string SessionManager::createLobby(const std::string& hostName,
	crow::websocket::connection* conn) {

	std::lock_guard<std::mutex> lock(mutex_);
	
	std::string sessionId = generateSessionId();
	
	LobbyInfo lobby;
	lobby.sessionId = sessionId;
	lobby.createdAt = std::chrono::system_clock::now();
	lobby.lastActivity = lobby.createdAt;
	lobby.phase = GamePhase::Lobby;

	PlayerInfo host;
	host.id = "host_" + sessionId;
	host.name = hostName;
	host.role = PlayerRole::Host;
	host.connection = conn;
	host.joinedAt = std::chrono::system_clock::now();

	lobby.players.push_back(host);
	lobbies_[sessionId] = lobby;

	return sessionId;
}

bool SessionManager::joinAsPlayer(const std::string& sessionId,
		const std::string& playerName,
		crow::websocket::connection* conn)
{
	std::lock_guard<std::mutex> lock(mutex_);

	auto it = lobbies_.find(sessionId);

	if (it == lobbies_.end()) {
		return false;
	}

	LobbyInfo& lobby = it->second;

	if (lobby.playerCount() >= 5) {
		return false;
	}

	auto nameExists = [&](const std::string& name) {
		return std::any_of(lobby.players.begin(), lobby.players.end(),
			[&](const PlayerInfo& p) { return p.name == name; });
		};

	if (nameExists(playerName)) {
		return false;
	}

	PlayerInfo player;
	player.id = "player_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
	player.name = playerName;
	player.role = PlayerRole::Player;
	player.connection = conn;
	player.joinedAt = std::chrono::system_clock::now();

	lobby.players.push_back(player);
	lobby.lastActivity = std::chrono::system_clock::now();

	notifyLobbyUpdate(sessionId);
	return true;
}

bool SessionManager::joinAsMaster(const std::string& sessionId,
	const std::string& masterName,
	crow::websocket::connection* conn) {
	std::lock_guard lock(mutex_);

	auto it = lobbies_.find(sessionId);
	if (it == lobbies_.end()) return false;

	LobbyInfo& lobby = it->second;

	if (lobby.hasMaster()) return false;

	PlayerInfo master;
	master.id = "master_" + masterName;
	master.name = masterName;
	master.role = PlayerRole::Master;
	master.connection = conn;
	master.joinedAt = std::chrono::system_clock::now();

	lobby.players.push_back(master);
	lobby.lastActivity = std::chrono::system_clock::now();

	notifyLobbyUpdate(sessionId);
	return true;
}

void SessionManager::leaveLobby(crow::websocket::connection* conn) {
	
	std::lock_guard lock(mutex_);

	for (auto& [sessionId, lobby] : lobbies_) {
		auto player = lobby.findPlayerByConnection(conn);
		if (player) {
			PlayerRole role = player->role;

			lobby.players.erase(
				std::remove_if(lobby.players.begin(), lobby.players.end(),
					[conn](const PlayerInfo& p) { return p.connection == conn; }),
				lobby.players.end()
			);

			lobby.lastActivity = std::chrono::system_clock::now();

			if (role == PlayerRole::Host && lobby.playerCount() > 0) {
				auto* firstPlayer = lobby.getRegularPlayers().empty() ? nullptr : lobby.getRegularPlayers()[0];
				if (firstPlayer) firstPlayer->role = PlayerRole::Host;
			}

			notifyLobbyUpdate(sessionId);

			if (lobby.players.empty()) lobbies_.erase(sessionId);

			break;
		}
	}
}

std::optional<LobbyInfo> SessionManager::getLobby(const std::string& sessionId) const {
	
	std::lock_guard lock(mutex_);

	auto it = lobbies_.find(sessionId);
	if (it != lobbies_.end()) return it->second;

	return std::nullopt;
}

bool SessionManager::lobbyExists(const std::string& sessionId) const {
	std::lock_guard lock(mutex_);
	return lobbies_.find(sessionId) != lobbies_.end();
}

crow::websocket::connection* SessionManager::getMasterConnection(const std::string& sessionId) {
	std::lock_guard lock(mutex_);

	auto it = lobbies_.find(sessionId);
	if(it!=lobbies_.end()){
		auto* master = it->second.getMaster();
		return master ? master->connection : nullptr;
	}
	return nullptr;
}

std::vector<crow::websocket::connection*> SessionManager::getPlayerConnections(const std::string& sessionId) {

	std::lock_guard lock(mutex_);

	std::vector<crow::websocket::connection*> connections;
	
	auto it = lobbies_.find(sessionId);
	
	if (it != lobbies_.end())
	{
		const LobbyInfo& lobby = it->second;
		for (const auto& player : lobby.players) {
			if (player.role == PlayerRole::Player || player.role == PlayerRole::Host) {
				connections.push_back(player.connection);
			}
		}
	}

	return connections;
}

bool SessionManager::updateLobbyPhase(const std::string& sessionId, GamePhase newPhase) {

	std::lock_guard lock(mutex_);

	auto it = lobbies_.find(sessionId);
	if (it == lobbies_.end()) return false;

	it->second.phase = newPhase;
	it->second.lastActivity = std::chrono::system_clock::now();

	notifyLobbyUpdate(sessionId);
	return true;
}

void SessionManager::notifyLobbyUpdate(const std::string& sessionId) {
	auto lobby = getLobby(sessionId);
	if (!lobby) return;

	crow::json::wvalue update;
	update["type"] = "LOBBY_UPDATE";
	update["sessionId"] = sessionId;
	update["canStart"] = lobby->canStartGame();

	crow::json::wvalue playersJson = crow::json::wvalue::list();
	int index = 0;

	for (const auto& player : lobby->players) {
		if (player.role == PlayerRole::Master) continue;

		crow::json::wvalue playerJson;
		playerJson["name"] = player.name;
		playerJson["role"] = static_cast<int>(player.role);
		playersJson[index++] = std::move(playerJson);
	}
	update["players"] = std::move(playersJson);

	broadcastToLobby(sessionId, update, false);
}

void SessionManager::broadcastToLobby(const std::string& sessionId,
	const crow::json::wvalue& message,
	bool excludeMaster) {
	auto it = lobbies_.find(sessionId);
	if (it == lobbies_.end()) return;

	const LobbyInfo& lobby = it->second;
	std::string messageStr = message.dump();

	for (const auto& player : lobby.players) {
		if (excludeMaster && player.role == PlayerRole::Master) continue;

		if (player.connection) player.connection->send_text(messageStr);
	}

}

void SessionManager::migrateHostIfNeeded(const std::string& sessionId) {
	std::lock_guard lock(mutex_);

	auto it = lobbies_.find(sessionId);
	if (it == lobbies_.end()) return;

	LobbyInfo& lobby = it->second;

	bool hasHost = std::any_of(lobby.players.begin(), lobby.players.end(),
		[](const PlayerInfo& p) { return p.role == PlayerRole::Host; });

	if (!hasHost && !lobby.players.empty()) {

		auto it = std::find_if(lobby.players.begin(), lobby.players.end(),
			[](const PlayerInfo& p) { return p.role == PlayerRole::Player; });

		if (it != lobby.players.end()) {
			it->role = PlayerRole::Host;

			crow::json::wvalue notification;
			notification["type"] = "HOST_MIGRATED";
			notification["newHost"] = it->name;

			broadcastToLobby(sessionId, notification, false);

			notifyLobbyUpdate(sessionId);
		}
	}
}

void SessionManager::cleanupInactiveLobbies(int maxInactiveMinutes) {
	std::lock_guard lock(mutex_);

	auto now = std::chrono::system_clock::now();
	auto threshold = now - std::chrono::minutes(maxInactiveMinutes);
	
	std::vector<std::string> toRemove;

	for (const auto& [sessionId, lobby] : lobbies_) {
		if(lobby.lastActivity < threshold) {
			toRemove.push_back(sessionId);
		}
	}

	for (const auto& sessionId : toRemove) {
		crow::json::wvalue notification;
		notification["type"] = "LOBBY_EXPIRED";
		notification["sessionId"] = sessionId;
		notification["reason"] = "Lobby inativo por muito tempo.";

		broadcastToLobby(sessionId, notification, false);

		lobbies_.erase(sessionId);
	}
}