#pragma once

// Steamworks integration wrapper
// App ID: 480 (Space War - Steamworks example game, used for development/testing)

#ifdef STEAMWORKS_AVAILABLE
#include "steam/steam_api.h"
#endif

#include <QObject>
#include <QString>
#include <functional>
#include <vector>
#include <cstdint>
#include <cstring>

struct LobbyData {
    uint64_t lobbyId = 0;
    QString name;
    QString hostName;
    int playerCount = 0;
    int maxPlayers = 2;
    QString romName;
    int ping = -1;  // -1 = unknown, 0-60 green, 60-100 yellow, 100+ red
};

struct NetPlayer {
    uint64_t steamId = 0;
    QString name;
    bool isHost = false;
};

// Steam callback results
struct LobbyCreatedResult {
    bool success = false;
    uint64_t lobbyId = 0;
    QString error;
};

struct LobbyJoinedResult {
    bool success = false;
    uint64_t lobbyId = 0;
    QString error;
};

struct LobbyListResult {
    bool success = false;
    std::vector<LobbyData> lobbies;
};

class SteamManager : public QObject {
    Q_OBJECT
public:
    static constexpr uint32_t APP_ID = 480u; // Space War (development)

    explicit SteamManager(QObject* parent = nullptr);
    ~SteamManager();

    bool init();
    void shutdown();
    bool isInitialized() const { return initialized_; }
    bool isSteamRunning() const;

    // Lobby
    void createLobby(const QString& lobbyName, int maxPlayers = 2);
    void searchLobbies(const QString& filter = "");
    void joinLobby(uint64_t lobbyId);
    void leaveLobby();
    std::vector<LobbyData> getLobbyList() const { return lobbyList_; }
    uint64_t getCurrentLobbyId() const { return currentLobbyId_; }

    // Lobby data
    void setLobbyData(const QString& key, const QString& value);
    QString getLobbyData(const QString& key) const;

    // Player info
    QString getPlayerName() const;
    uint64_t getPlayerSteamId() const;
    int getLobbyMemberCount() const;
    std::vector<NetPlayer> getLobbyMembers() const;

    // Networking (P2P)
    void sendNetData(const uint8_t* data, size_t len, bool reliable = true);
    bool receiveNetData(uint8_t* buffer, size_t* len, uint64_t* senderId = nullptr);

    // Convenience: send string message to all lobby members
    void sendNetMessage(const QByteArray& msg);

    // Frame sync: send/receive input state
    void sendFrameInput(uint32_t frameNum, uint8_t p1, uint8_t p2);
    void sendClientInput(uint8_t playerId, uint8_t buttons);  // Client sends input to host

    // State sync: send full state snapshot
    void sendStateSnapshot(const std::vector<uint8_t>& stateData);

    // Poll callbacks (call from main thread)
    void runCallbacks();

    bool isHost() const;

    // Kick a player from the lobby (ban temporarily)
    void kickPlayer(uint64_t steamId);

    // Latency measurement: send ping and get current latency
    void sendPing();
    int getLatency() const { return currentLatencyMs_; }

    // Handle ping/pong messages
    void handlePing(const uint8_t* data, size_t len, uint64_t senderId);
    void handlePong(const uint8_t* data, size_t len);

signals:
    void latencyUpdated(int latencyMs);
    void lobbyCreated(const LobbyCreatedResult& result);
    void lobbyJoined(const LobbyJoinedResult& result);
    void lobbyListUpdated(const LobbyListResult& result);
    void lobbyDataUpdated(uint64_t lobbyId);
    void playerJoined(uint64_t steamId, const QString& name);
    void playerLeft(uint64_t steamId);
    void peerDisconnected(uint64_t steamId);
    void netDataReceived(const QByteArray& data, uint64_t senderId);
    void chatMessage(uint64_t steamId, const QString& message);

private:
#ifdef STEAMWORKS_AVAILABLE
    void onLobbyCreated(LobbyCreated_t* callback, bool ioFailure);
    void onLobbyJoined(LobbyEnter_t* callback, bool ioFailure);
    void onLobbyList(LobbyMatchList_t* callback, bool ioFailure);
    void onLobbyDataUpdate(LobbyDataUpdate_t* callback);
    void onLobbyChatUpdate(LobbyChatUpdate_t* callback);
    void onP2PSessionRequest(P2PSessionRequest_t* callback);
    void onP2PConnectionFail(P2PSessionConnectFail_t* callback);

    CCallResult<SteamManager, LobbyCreated_t> lobbyCreatedCall_;
    CCallResult<SteamManager, LobbyMatchList_t> lobbyListCall_;
    CCallResult<SteamManager, LobbyEnter_t> lobbyJoinedCall_;
    CCallbackManual<SteamManager, LobbyDataUpdate_t> m_cbLobbyDataUpdate;
    CCallbackManual<SteamManager, LobbyChatUpdate_t> m_cbLobbyChatUpdate;
    CCallbackManual<SteamManager, P2PSessionRequest_t> m_cbP2PSessionRequest;
    CCallbackManual<SteamManager, P2PSessionConnectFail_t> m_cbP2PConnectionFail;
    CSteamID currentLobby_;
    std::vector<CSteamID> lobbyIds_;
#endif

    bool initialized_ = false;
    std::vector<LobbyData> lobbyList_;
    uint64_t currentLobbyId_ = 0;
    QString currentLobbyName_;

    // Latency measurement
    int currentLatencyMs_ = 0;
    std::chrono::steady_clock::time_point lastPingTime_;
    bool waitingForPong_ = false;
};
