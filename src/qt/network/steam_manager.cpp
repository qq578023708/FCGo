#include "steam_manager.h"
#include <QDebug>
#include <QFile>
#include <QCoreApplication>
#include <QDir>
#include <chrono>

// Steamworks support: STEAMWORKS_AVAILABLE should be defined by CMake
#ifdef STEAMWORKS_AVAILABLE

SteamManager::SteamManager(QObject* parent)
    : QObject(parent)
{
}

SteamManager::~SteamManager() {
    shutdown();
}

bool SteamManager::init() {
    if (initialized_) return true;

    // Ensure steam_appid.txt exists next to the executable
    QString exeDir = QCoreApplication::applicationDirPath();
    QString appIdPath = exeDir + "/steam_appid.txt";
    QFile appIdFile(appIdPath);
    if (!appIdFile.exists()) {
        if (appIdFile.open(QIODevice::WriteOnly)) {
            appIdFile.write(QByteArray::number(APP_ID));
            appIdFile.close();
            qDebug() << "SteamManager: Created" << appIdPath;
        }
    }

    // Check if Steam client is running before trying to init
    if (!SteamAPI_IsSteamRunning()) {
        qWarning() << "SteamManager: Steam client is not running";
        return false;
    }

    if (!SteamAPI_Init()) {
        qWarning() << "SteamManager: SteamAPI_Init failed";
        return false;
    }

    // Now register callbacks (after SteamAPI_Init)
    m_cbLobbyDataUpdate.Register(this, &SteamManager::onLobbyDataUpdate);
    m_cbLobbyChatUpdate.Register(this, &SteamManager::onLobbyChatUpdate);
    m_cbP2PSessionRequest.Register(this, &SteamManager::onP2PSessionRequest);
    m_cbP2PConnectionFail.Register(this, &SteamManager::onP2PConnectionFail);

    initialized_ = true;
    qDebug() << "SteamManager: Initialized with App ID" << APP_ID;
    qDebug() << "SteamManager: Player name:" << getPlayerName();
    return true;
}

void SteamManager::shutdown() {
    if (!initialized_) return;
    SteamAPI_Shutdown();
    initialized_ = false;
}

bool SteamManager::isSteamRunning() const {
    return SteamAPI_IsSteamRunning();
}

// ============================================================
// Lobby
// ============================================================

void SteamManager::createLobby(const QString& lobbyName, int maxPlayers) {
    if (!initialized_) return;
    currentLobbyName_ = lobbyName;

    SteamAPICall_t hCall = SteamMatchmaking()->CreateLobby(k_ELobbyTypePublic, maxPlayers);
    lobbyCreatedCall_.Set(hCall, this, &SteamManager::onLobbyCreated);
}

void SteamManager::searchLobbies(const QString& filter) {
    if (!initialized_) return;
    lobbyList_.clear();
    lobbyIds_.clear();

    // Filter: only FCGo lobbies (App ID 480 is shared, so we tag our lobbies)
    SteamMatchmaking()->AddRequestLobbyListStringFilter("app", "FCGo", k_ELobbyComparisonEqual);

    // Only filter by name if a non-empty filter is provided
    if (!filter.isEmpty()) {
        SteamMatchmaking()->AddRequestLobbyListStringFilter("name", filter.toUtf8().constData(), k_ELobbyComparisonEqual);
    }

    SteamMatchmaking()->AddRequestLobbyListResultCountFilter(50);

    SteamAPICall_t hCall = SteamMatchmaking()->RequestLobbyList();
    lobbyListCall_.Set(hCall, this, &SteamManager::onLobbyList);
}

void SteamManager::joinLobby(uint64_t lobbyId) {
    if (!initialized_) return;
    CSteamID steamLobbyId(static_cast<uint64>(lobbyId));
    SteamAPICall_t hCall = SteamMatchmaking()->JoinLobby(steamLobbyId);
    lobbyJoinedCall_.Set(hCall, this, &SteamManager::onLobbyJoined);
}

void SteamManager::leaveLobby() {
    if (!initialized_ || currentLobbyId_ == 0) return;
    CSteamID steamLobbyId(static_cast<uint64>(currentLobbyId_));
    SteamMatchmaking()->LeaveLobby(steamLobbyId);
    currentLobbyId_ = 0;
}

void SteamManager::setLobbyData(const QString& key, const QString& value) {
    if (!initialized_ || currentLobbyId_ == 0) return;
    CSteamID steamLobbyId(static_cast<uint64>(currentLobbyId_));
    SteamMatchmaking()->SetLobbyData(steamLobbyId, key.toUtf8().constData(), value.toUtf8().constData());
}

QString SteamManager::getLobbyData(const QString& key) const {
    if (!initialized_ || currentLobbyId_ == 0) return {};
    CSteamID steamLobbyId(static_cast<uint64>(currentLobbyId_));
    const char* val = SteamMatchmaking()->GetLobbyData(steamLobbyId, key.toUtf8().constData());
    return val ? QString::fromUtf8(val) : QString();
}

// ============================================================
// Player info
// ============================================================

QString SteamManager::getPlayerName() const {
    if (!initialized_) return {};
    return QString::fromUtf8(SteamFriends()->GetPersonaName());
}

uint64_t SteamManager::getPlayerSteamId() const {
    if (!initialized_) return 0;
    return SteamUser()->GetSteamID().ConvertToUint64();
}

int SteamManager::getLobbyMemberCount() const {
    if (!initialized_ || currentLobbyId_ == 0) return 0;
    CSteamID steamLobbyId(static_cast<uint64>(currentLobbyId_));
    return SteamMatchmaking()->GetNumLobbyMembers(steamLobbyId);
}

std::vector<NetPlayer> SteamManager::getLobbyMembers() const {
    std::vector<NetPlayer> members;
    if (!initialized_ || currentLobbyId_ == 0) return members;

    CSteamID steamLobbyId(static_cast<uint64>(currentLobbyId_));
    CSteamID owner = SteamMatchmaking()->GetLobbyOwner(steamLobbyId);
    int count = SteamMatchmaking()->GetNumLobbyMembers(steamLobbyId);
    for (int i = 0; i < count; i++) {
        CSteamID memberId = SteamMatchmaking()->GetLobbyMemberByIndex(steamLobbyId, i);
        NetPlayer p;
        p.steamId = memberId.ConvertToUint64();
        p.name = QString::fromUtf8(SteamFriends()->GetFriendPersonaName(memberId));
        p.isHost = (memberId == owner);
        members.push_back(p);
    }
    return members;
}

// ============================================================
// Networking (P2P)
// ============================================================

void SteamManager::sendNetData(const uint8_t* data, size_t len, bool reliable) {
    if (!initialized_ || currentLobbyId_ == 0) return;

    CSteamID steamLobbyId(static_cast<uint64>(currentLobbyId_));
    int count = SteamMatchmaking()->GetNumLobbyMembers(steamLobbyId);
    for (int i = 0; i < count; i++) {
        CSteamID memberId = SteamMatchmaking()->GetLobbyMemberByIndex(steamLobbyId, i);
        if (memberId == SteamUser()->GetSteamID()) continue; // Skip self

        EP2PSend sendType = reliable ? k_EP2PSendReliable : k_EP2PSendUnreliable;
        SteamNetworking()->SendP2PPacket(memberId, data, static_cast<uint32>(len), sendType);
    }
}

bool SteamManager::receiveNetData(uint8_t* buffer, size_t* len, uint64_t* senderId) {
    if (!initialized_) return false;

    uint32 bytesRead = 0;
    CSteamID sender;
    if (SteamNetworking()->IsP2PPacketAvailable(&bytesRead)) {
        if (bytesRead <= static_cast<uint32>(*len)) {
            if (SteamNetworking()->ReadP2PPacket(buffer, static_cast<uint32>(*len), &bytesRead, &sender)) {
                *len = bytesRead;
                if (senderId) *senderId = sender.ConvertToUint64();
                return true;
            }
        }
    }
    return false;
}

void SteamManager::sendNetMessage(const QByteArray& msg) {
    sendNetData(reinterpret_cast<const uint8_t*>(msg.constData()), msg.size(), true);
}

void SteamManager::sendFrameInput(uint32_t frameNum, uint8_t p1, uint8_t p2) {
    // Format: 'I' + uint32_le frameNum + p1 + p2 = 7 bytes, reliable
    uint8_t data[7];
    data[0] = 'I';
    data[1] = frameNum & 0xFF;
    data[2] = (frameNum >> 8) & 0xFF;
    data[3] = (frameNum >> 16) & 0xFF;
    data[4] = (frameNum >> 24) & 0xFF;
    data[5] = p1;
    data[6] = p2;
    sendNetData(data, 7, true);
}

void SteamManager::sendClientInput(uint8_t playerId, uint8_t buttons) {
    // Format: 'C' + playerId(1) + buttons(1) = 3 bytes, unreliable
    uint8_t data[3];
    data[0] = 'C';
    data[1] = playerId;
    data[2] = buttons;
    sendNetData(data, 3, false);
}

void SteamManager::sendPing() {
    if (!initialized_ || currentLobbyId_ == 0) return;

    // If still waiting for pong, check timeout (2 seconds)
    if (waitingForPong_) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - lastPingTime_).count();
        if (elapsed < 2000) return;  // Still waiting
        // Timeout - reset and send new ping
        waitingForPong_ = false;
    }

    // Send ping: 'P' + timestamp (8 bytes)
    uint8_t data[9];
    data[0] = 'P';
    auto now = std::chrono::steady_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    memcpy(data + 1, &timestamp, 8);

    sendNetData(data, 9, true);
    lastPingTime_ = now;
    waitingForPong_ = true;
}

void SteamManager::handlePing(const uint8_t* data, size_t len, uint64_t senderId) {
    if (len < 9 || data[0] != 'P') return;

    // Reply with pong: 'O' + original timestamp (8 bytes)
    uint8_t pong[9];
    pong[0] = 'O';
    memcpy(pong + 1, data + 1, 8);

    CSteamID targetId(senderId);
    SteamNetworking()->SendP2PPacket(targetId, pong, 9, k_EP2PSendReliable);
}

void SteamManager::handlePong(const uint8_t* data, size_t len) {
    if (len < 9 || data[0] != 'O' || !waitingForPong_) return;

    auto now = std::chrono::steady_clock::now();
    int64_t sentTime;
    memcpy(&sentTime, data + 1, 8);

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count() - sentTime;
    currentLatencyMs_ = static_cast<int>(elapsed / 2);  // RTT/2 = one-way latency
    waitingForPong_ = false;

    emit latencyUpdated(currentLatencyMs_);
}

void SteamManager::sendStateSnapshot(const std::vector<uint8_t>& stateData) {
    uint32_t totalChunks = (stateData.size() + 999) / 1000;
    for (uint32_t i = 0; i < totalChunks; i++) {
        uint32_t offset = i * 1000;
        uint32_t chunkSize = static_cast<uint32_t>(std::min(static_cast<size_t>(1000), stateData.size() - offset));
        std::vector<uint8_t> packet(1 + 4 + 4 + 4 + chunkSize);
        packet[0] = 'S';
        memcpy(packet.data() + 1, &i, 4);
        memcpy(packet.data() + 5, &totalChunks, 4);
        memcpy(packet.data() + 9, &chunkSize, 4);
        memcpy(packet.data() + 13, stateData.data() + offset, chunkSize);
        sendNetData(packet.data(), packet.size(), true);
    }
}

bool SteamManager::isHost() const {
    if (!initialized_ || currentLobbyId_ == 0) return false;
    CSteamID lobbyId(static_cast<uint64>(currentLobbyId_));
    CSteamID owner = SteamMatchmaking()->GetLobbyOwner(lobbyId);
    return owner == SteamUser()->GetSteamID();
}

void SteamManager::kickPlayer(uint64_t steamId) {
    if (!initialized_ || !isHost()) return;

    // Record kick time in lobby data for cooldown (5 minutes)
    CSteamID lobbyId(static_cast<uint64>(currentLobbyId_));
    auto now = std::chrono::steady_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()).count();
    QString key = QString("kicked_%1").arg(steamId);
    setLobbyData(key, QString::number(timestamp));

    // Send kick notification via reliable P2P message
    uint8_t kickMsg = 'K';
    CSteamID targetId(steamId);
    SteamNetworking()->SendP2PPacket(targetId, &kickMsg, 1, k_EP2PSendReliable);

    qDebug() << "SteamManager: Kicked player" << steamId;
}

void SteamManager::runCallbacks() {
    if (!initialized_) return;
    SteamAPI_RunCallbacks();
}

// ============================================================
// CCallResult callbacks
// ============================================================

void SteamManager::onLobbyCreated(LobbyCreated_t* callback, bool ioFailure) {
    LobbyCreatedResult result;
    result.success = !ioFailure && callback->m_eResult == k_EResultOK;
    result.lobbyId = callback->m_ulSteamIDLobby;

    if (result.success) {
        currentLobbyId_ = callback->m_ulSteamIDLobby;
        CSteamID lobbyId(callback->m_ulSteamIDLobby);

        // Set lobby metadata: tag as FCGo lobby (App ID 480 is shared)
        SteamMatchmaking()->SetLobbyData(lobbyId, "app", "FCGo");
        SteamMatchmaking()->SetLobbyData(lobbyId, "name", currentLobbyName_.toUtf8().constData());
        SteamMatchmaking()->SetLobbyData(lobbyId, "version", "1.0");
        SteamMatchmaking()->SetLobbyData(lobbyId, "host", SteamFriends()->GetPersonaName());

        qDebug() << "SteamManager: Lobby created:" << currentLobbyName_;
    } else {
        result.error = tr("Failed to create lobby");
        qWarning() << "SteamManager: Failed to create lobby";
    }

    emit lobbyCreated(result);
}

void SteamManager::onLobbyJoined(LobbyEnter_t* callback, bool ioFailure) {
    LobbyJoinedResult result;
    result.success = !ioFailure && callback->m_EChatRoomEnterResponse == k_EChatRoomEnterResponseSuccess;
    result.lobbyId = callback->m_ulSteamIDLobby;

    if (result.success) {
        currentLobbyId_ = callback->m_ulSteamIDLobby;
        CSteamID lobbyId(callback->m_ulSteamIDLobby);

        // Accept P2P sessions from all lobby members and send handshake
        int count = SteamMatchmaking()->GetNumLobbyMembers(lobbyId);
        for (int i = 0; i < count; i++) {
            CSteamID memberId = SteamMatchmaking()->GetLobbyMemberByIndex(lobbyId, i);
            if (memberId != SteamUser()->GetSteamID()) {
                SteamNetworking()->AcceptP2PSessionWithUser(memberId);
                // Send handshake to establish P2P channel
                const char* handshake = "HELLO";
                SteamNetworking()->SendP2PPacket(memberId, handshake, 6, k_EP2PSendReliable);
            }
        }

        qDebug() << "SteamManager: Joined lobby, members:" << count;
    } else {
        result.error = tr("Failed to join lobby");
        qWarning() << "SteamManager: Failed to join lobby";
    }

    emit lobbyJoined(result);
}

void SteamManager::onLobbyList(LobbyMatchList_t* callback, bool ioFailure) {
    LobbyListResult result;
    result.success = !ioFailure;

    if (result.success) {
        for (uint32 i = 0; i < callback->m_nLobbiesMatching; i++) {
            CSteamID lobbyId = SteamMatchmaking()->GetLobbyByIndex(i);
            lobbyIds_.push_back(lobbyId);

            LobbyData data;
            data.lobbyId = lobbyId.ConvertToUint64();
            data.name = QString::fromUtf8(SteamMatchmaking()->GetLobbyData(lobbyId, "name"));
            data.playerCount = SteamMatchmaking()->GetNumLobbyMembers(lobbyId);
            data.maxPlayers = SteamMatchmaking()->GetLobbyMemberLimit(lobbyId);
            data.romName = QString::fromUtf8(SteamMatchmaking()->GetLobbyData(lobbyId, "rom"));

            // Get host name from lobby data (more reliable than GetFriendPersonaName)
            QString hostFromData = QString::fromUtf8(SteamMatchmaking()->GetLobbyData(lobbyId, "host"));
            if (!hostFromData.isEmpty()) {
                data.hostName = hostFromData;
            } else {
                CSteamID host = SteamMatchmaking()->GetLobbyOwner(lobbyId);
                data.hostName = QString::fromUtf8(SteamFriends()->GetFriendPersonaName(host));
            }

            // Ping not directly available from Steam lobby search
            // Would need P2P connection to each host for accurate ping
            data.ping = -1;

            lobbyList_.push_back(data);
        }
        qDebug() << "SteamManager: Found" << lobbyList_.size() << "lobbies";
    }

    emit lobbyListUpdated(result);
}

// ============================================================
// CCallbackManual callbacks
// ============================================================

void SteamManager::onLobbyDataUpdate(LobbyDataUpdate_t* callback) {
    emit lobbyDataUpdated(callback->m_ulSteamIDLobby);
}

void SteamManager::onLobbyChatUpdate(LobbyChatUpdate_t* callback) {
    qDebug() << "SteamManager: onLobbyChatUpdate - lobby:" << callback->m_ulSteamIDLobby
             << "current:" << currentLobbyId_
             << "user:" << callback->m_ulSteamIDUserChanged
             << "state:" << callback->m_rgfChatMemberStateChange;

    if (callback->m_ulSteamIDLobby != currentLobbyId_) {
        qDebug() << "SteamManager: Ignoring chat update for different lobby";
        return;
    }

    CSteamID userChanged(callback->m_ulSteamIDUserChanged);
    QString name = QString::fromUtf8(SteamFriends()->GetFriendPersonaName(userChanged));

    // EChatMemberStateChange: 1=joined, 2=left, 4=entered/exited, 8=banned, 16=kicked
    if (callback->m_rgfChatMemberStateChange & 0x01) {
        // Member joined - check cooldown for kicked players
        uint64_t joinerId = callback->m_ulSteamIDUserChanged;
        CSteamID lobbyId(static_cast<uint64>(currentLobbyId_));
        QString kickKey = QString("kicked_%1").arg(joinerId);
        const char* kickTime = SteamMatchmaking()->GetLobbyData(lobbyId, kickKey.toUtf8().constData());
        if (kickTime && kickTime[0] != '\0') {
            // Check if within 5-minute cooldown
            auto kickTimestamp = std::stoll(kickTime);
            auto now = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
            if (now - kickTimestamp < 300) {  // 5 minutes
                // Still in cooldown, send kick notification
                CSteamID targetId(joinerId);
                uint8_t kickMsg = 'K';
                SteamNetworking()->SendP2PPacket(targetId, &kickMsg, 1, k_EP2PSendReliable);
                qDebug() << "SteamManager: Rejected player" << joinerId << "(cooldown active)";
                return;
            } else {
                // Cooldown expired, clean up
                SteamMatchmaking()->SetLobbyData(lobbyId, kickKey.toUtf8().constData(), "");
            }
        }

        // Check if lobby is full (memberCount already includes the new joiner)
        int memberCount = SteamMatchmaking()->GetNumLobbyMembers(lobbyId);
        int maxPlayers = SteamMatchmaking()->GetLobbyMemberLimit(lobbyId);
        if (memberCount > maxPlayers) {
            CSteamID targetId(joinerId);
            uint8_t fullMsg = 'F';
            SteamNetworking()->SendP2PPacket(targetId, &fullMsg, 1, k_EP2PSendReliable);
            qDebug() << "SteamManager: Rejected player" << joinerId << "(lobby full:" << memberCount << ">" << maxPlayers << ")";
            return;
        }

        // Accept P2P session and send handshake
        SteamNetworking()->AcceptP2PSessionWithUser(userChanged);
        const char* handshake = "HELLO";
        SteamNetworking()->SendP2PPacket(userChanged, handshake, 6, k_EP2PSendReliable);

        emit playerJoined(joinerId, name);
        qDebug() << "SteamManager: Player joined:" << name;
    }
    if (callback->m_rgfChatMemberStateChange & 0x02) {
        // Member left
        emit playerLeft(callback->m_ulSteamIDUserChanged);
        qDebug() << "SteamManager: Player left:" << name;
    }
}

void SteamManager::onP2PSessionRequest(P2PSessionRequest_t* callback) {
    SteamNetworking()->AcceptP2PSessionWithUser(callback->m_steamIDRemote);
    qDebug() << "SteamManager: Accepted P2P session from" << callback->m_steamIDRemote.ConvertToUint64();
}

void SteamManager::onP2PConnectionFail(P2PSessionConnectFail_t* callback) {
    uint64_t peerId = callback->m_steamIDRemote.ConvertToUint64();
    qWarning() << "SteamManager: P2P connection failed with" << peerId;
    emit peerDisconnected(peerId);
}

#else // !STEAMWORKS_AVAILABLE

// Stub implementation when Steamworks SDK is not available
SteamManager::SteamManager(QObject* parent) : QObject(parent) {}
SteamManager::~SteamManager() {}
bool SteamManager::init() { qWarning() << "SteamManager: Steamworks SDK not available"; return false; }
void SteamManager::shutdown() {}
bool SteamManager::isSteamRunning() const { return false; }
void SteamManager::createLobby(const QString&, int) {}
void SteamManager::searchLobbies(const QString&) {}
void SteamManager::joinLobby(uint64_t) {}
void SteamManager::leaveLobby() {}
void SteamManager::setLobbyData(const QString&, const QString&) {}
QString SteamManager::getLobbyData(const QString&) const { return {}; }
QString SteamManager::getPlayerName() const { return {}; }
uint64_t SteamManager::getPlayerSteamId() const { return 0; }
int SteamManager::getLobbyMemberCount() const { return 0; }
std::vector<NetPlayer> SteamManager::getLobbyMembers() const { return {}; }
void SteamManager::sendNetData(const uint8_t*, size_t, bool) {}
bool SteamManager::receiveNetData(uint8_t*, size_t*, uint64_t*) { return false; }
void SteamManager::runCallbacks() {}

#endif
