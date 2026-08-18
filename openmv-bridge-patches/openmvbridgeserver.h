#pragma once

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QList>
#include <QMap>
#include <QCryptographicHash>
#include <QTimer>
#include <QDateTime>
#include <QDebug>

class OpenMVBridgeServer : public QObject
{
    Q_OBJECT

public:
    static OpenMVBridgeServer *instance();
    explicit OpenMVBridgeServer(QObject *parent = nullptr);
    ~OpenMVBridgeServer() override;

    bool startServer(quint16 port = 23888);
    void stopServer();
    bool isRunning() const;

    // Public Broadcast API
    void broadcastDiagnostics(const QString &filePath, const QJsonArray &diagnosticItems);
    void broadcastSerialData(const QString &payload, const QString &port = QString(), bool connected = true);
    void broadcastDeviceStatus(bool connected, const QString &port = QString(), const QString &board = QString());
    void broadcastNotification(const QString &level, const QString &message);
    void broadcastMessage(const QJsonObject &jsonObj);

signals:
    void clientConnected(const QString &peerAddress);
    void clientDisconnected(const QString &peerAddress);
    void requestReloadFile(const QString &filePath);
    void syncFileContentReceived(const QString &filePath, const QString &content);
    void serialInputReceived(const QString &data);
    void commandReceived(const QString &command, const QJsonObject &params);

private slots:
    void onNewConnection();
    void onClientReadyRead();
    void onClientDisconnected();

private:
    struct ClientState {
        bool isWebSocketHandshakeDone = false;
        QByteArray buffer;
    };

    void handleHandshake(QTcpSocket *socket, const QByteArray &data);
    void handleWebSocketFrames(QTcpSocket *socket);
    void processClientJson(QTcpSocket *socket, const QString &jsonString);
    void sendWebSocketFrame(QTcpSocket *socket, const QByteArray &payload, quint8 opcode = 0x01);

    static OpenMVBridgeServer *s_instance;
    QTcpServer *m_server = nullptr;
    QList<QTcpSocket*> m_clients;
    QMap<QTcpSocket*, ClientState> m_clientStates;
    quint16 m_port = 23888;
};
