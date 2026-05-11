#pragma once

#include <QDialog>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QTimer>
#include "steam_manager.h"

class NESConsole;

class NetworkLobbyWindow : public QDialog {
    Q_OBJECT
public:
    explicit NetworkLobbyWindow(NESConsole* console, QWidget* parent = nullptr);
    ~NetworkLobbyWindow();

    // Called by MainWindow when host loads a ROM
    void hostLoadROM(const QString& romPath);

    // Get the synced ROM path (for client after sync)
    QString getSyncedROMPath() const { return syncedROMPath_; }
    bool isROMSynced() const { return romSynced_; }

    // Expose SteamManager for frame sync
    SteamManager* steamManager() const { return steam_; }

    // Handle non-frame-sync network messages (ROM sync, chat, etc.)
    void handleNetMessage(const QByteArray& data);

signals:
    void romSyncComplete(const QString& romPath);
    void netPlayEnded(const QString& reason);
    void playerJoined(uint64_t steamId, const QString& name);

private slots:
    void onCreateLobby();
    void onRefreshLobbies();
    void onJoinLobby();
    void onLeaveLobby();
    void onLobbyCreated(const LobbyCreatedResult& result);
    void onLobbyJoined(const LobbyJoinedResult& result);
    void onLobbyListUpdated(const LobbyListResult& result);
    void onPlayerJoined(uint64_t steamId, const QString& name);
    void onPlayerLeft(uint64_t steamId);
    void onPeerDisconnected(uint64_t steamId);
    void onLiveUpdate();
    void onSendChat();
    void onLobbySelected(int row);

private:
    void setupUI();
    void updateMemberListWithLatency();
    void updateMemberList();
    void updateUIState();

    // ROM sync (host side)
    void sendROMInfo(const QString& romPath);
    void sendROMChunk(int chunkIndex);

    // ROM sync (client side)
    void handleROMInfo(const QByteArray& payload);
    void handleROMChunk(const QByteArray& payload);
    void requestROMTransfer();

    // Utility
    static QByteArray computeFileSHA256(const QString& path);
    QString tryFindLocalROM(const QString& filename, const QString& sha256);

    NESConsole* console_ = nullptr;
    SteamManager* steam_ = nullptr;

    // UI
    QLineEdit* lobbyNameEdit_ = nullptr;
    QPushButton* createBtn_ = nullptr;
    QPushButton* refreshBtn_ = nullptr;
    QTableWidget* lobbyTable_ = nullptr;
    QPushButton* joinBtn_ = nullptr;
    QPushButton* leaveBtn_ = nullptr;
    QListWidget* memberList_ = nullptr;
    QLineEdit* chatInput_ = nullptr;
    QListWidget* chatList_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    QLabel* playerLabel_ = nullptr;

    QTimer* liveTimer_ = nullptr;
    bool inLobby_ = false;
    int selectedLobbyRow_ = -1;
    int lobbyRefreshCounter_ = 0;  // Auto-refresh lobby list every 5 seconds

    // ROM sync state
    bool romSynced_ = false;
    QString syncedROMPath_;

    // Host ROM transfer state
    QByteArray hostROMData_;
    QString hostROMFilename_;
    QString hostROMSHA256_;
    int hostROMChunkIndex_ = 0;
    static constexpr int CHUNK_SIZE = 4096;

    // Client ROM receive state
    QByteArray clientROMBuffer_;
    QString clientROMFilename_;
    QString clientROMSHA256_;
    int clientROMTotalChunks_ = 0;
    int clientROMReceivedChunks_ = 0;
};
