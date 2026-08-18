#include "openmvbridgeserver.h"
#include <QHostAddress>
#include <QRegularExpression>

OpenMVBridgeServer *OpenMVBridgeServer::s_instance = nullptr;

OpenMVBridgeServer *OpenMVBridgeServer::instance()
{
    if (!s_instance) {
        s_instance = new OpenMVBridgeServer();
    }
    return s_instance;
}

OpenMVBridgeServer::OpenMVBridgeServer(QObject *parent)
    : QObject(parent)
{
    s_instance = this;
    m_server = new QTcpServer(this);
    connect(m_server, &QTcpServer::newConnection, this, &OpenMVBridgeServer::onNewConnection);
}

OpenMVBridgeServer::~OpenMVBridgeServer()
{
    stopServer();
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

bool OpenMVBridgeServer::startServer(quint16 port)
{
    m_port = port;
    if (m_server->isListening()) {
        m_server->close();
    }

    if (!m_server->listen(QHostAddress::LocalHost, m_port)) {
        qWarning() << "[OpenMV Bridge Server] Failed to bind port" << m_port << ":" << m_server->errorString();
        return false;
    }

    qInfo() << "[OpenMV Bridge Server] Listening for VS Code on ws://127.0.0.1:" << m_port;
    return true;
}

void OpenMVBridgeServer::stopServer()
{
    for (QTcpSocket *socket : m_clients) {
        socket->disconnect(this);
        socket->close();
        socket->deleteLater();
    }
    m_clients.clear();
    m_clientStates.clear();

    if (m_server && m_server->isListening()) {
        m_server->close();
    }
}

bool OpenMVBridgeServer::isRunning() const
{
    return m_server && m_server->isListening();
}

void OpenMVBridgeServer::onNewConnection()
{
    while (m_server->hasPendingConnections()) {
        QTcpSocket *socket = m_server->nextPendingConnection();
        if (!socket) continue;

        m_clients.append(socket);
        m_clientStates[socket] = ClientState();

        connect(socket, &QTcpSocket::readyRead, this, &OpenMVBridgeServer::onClientReadyRead);
        connect(socket, &QTcpSocket::disconnected, this, &OpenMVBridgeServer::onClientDisconnected);

        qDebug() << "[OpenMV Bridge Server] Client connected from" << socket->peerAddress().toString() << ":" << socket->peerPort();
        emit clientConnected(socket->peerAddress().toString());
    }
}

void OpenMVBridgeServer::onClientDisconnected()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    qDebug() << "[OpenMV Bridge Server] Client disconnected:" << socket->peerAddress().toString();
    emit clientDisconnected(socket->peerAddress().toString());

    m_clients.removeAll(socket);
    m_clientStates.remove(socket);
    socket->deleteLater();
}

void OpenMVBridgeServer::onClientReadyRead()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    QByteArray incoming = socket->readAll();
    ClientState &state = m_clientStates[socket];
    state.buffer.append(incoming);

    if (!state.isWebSocketHandshakeDone) {
        handleHandshake(socket, state.buffer);
    } else {
        handleWebSocketFrames(socket);
    }
}

void OpenMVBridgeServer::handleHandshake(QTcpSocket *socket, const QByteArray &data)
{
    // Check if full HTTP header received
    int headerEnd = data.indexOf("\r\n\r\n");
    if (headerEnd == -1) {
        return; // wait for more data
    }

    QString request = QString::fromUtf8(data.left(headerEnd));
    ClientState &state = m_clientStates[socket];
    state.buffer.remove(0, headerEnd + 4);

    // Look for Sec-WebSocket-Key
    QRegularExpression keyRegex(QStringLiteral("Sec-WebSocket-Key:\\s*([A-Za-z0-9+/=]+)"), QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch match = keyRegex.match(request);

    if (match.hasMatch()) {
        QString key = match.captured(1).trimmed();
        QString magic = QStringLiteral("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
        QByteArray acceptRaw = (key + magic).toUtf8();
        QByteArray acceptHash = QCryptographicHash::hash(acceptRaw, QCryptographicHash::Sha1);
        QString acceptBase64 = QString::fromLatin1(acceptHash.toBase64());

        QByteArray response;
        response.append("HTTP/1.1 101 Switching Protocols\r\n");
        response.append("Upgrade: websocket\r\n");
        response.append("Connection: Upgrade\r\n");
        response.append("Sec-WebSocket-Accept: " + acceptBase64.toUtf8() + "\r\n\r\n");

        socket->write(response);
        socket->flush();

        state.isWebSocketHandshakeDone = true;
        qDebug() << "[OpenMV Bridge Server] WebSocket Handshake completed with" << socket->peerAddress().toString();

        // Send initial welcome message
        broadcastNotification(QStringLiteral("info"), QStringLiteral("OpenMV IDE Bridge connected successfully."));

        // If there is leftover data in the buffer, process frames
        if (!state.buffer.isEmpty()) {
            handleWebSocketFrames(socket);
        }
    } else {
        // Not a websocket handshake
        socket->write("HTTP/1.1 400 Bad Request\r\n\r\nInvalid WebSocket Handshake");
        socket->disconnectFromHost();
    }
}

void OpenMVBridgeServer::handleWebSocketFrames(QTcpSocket *socket)
{
    ClientState &state = m_clientStates[socket];

    while (state.buffer.size() >= 2) {
        const quint8 *raw = reinterpret_cast<const quint8*>(state.buffer.constData());
        bool fin = (raw[0] & 0x80) != 0;
        quint8 opcode = raw[0] & 0x0F;
        bool masked = (raw[1] & 0x80) != 0;
        quint64 payloadLength = raw[1] & 0x7F;
        int headerSize = 2;

        if (payloadLength == 126) {
            if (state.buffer.size() < 4) return; // Need more data
            payloadLength = (quint64(raw[2]) << 8) | quint64(raw[3]);
            headerSize = 4;
        } else if (payloadLength == 127) {
            if (state.buffer.size() < 10) return; // Need more data
            payloadLength = 0;
            for (int i = 0; i < 8; ++i) {
                payloadLength = (payloadLength << 8) | quint64(raw[2 + i]);
            }
            headerSize = 10;
        }

        int maskSize = masked ? 4 : 0;
        if (state.buffer.size() < headerSize + maskSize + int(payloadLength)) {
            return; // Wait for full frame
        }

        quint8 mask[4] = {0, 0, 0, 0};
        if (masked) {
            mask[0] = raw[headerSize];
            mask[1] = raw[headerSize + 1];
            mask[2] = raw[headerSize + 2];
            mask[3] = raw[headerSize + 3];
        }

        const char *payloadRaw = state.buffer.constData() + headerSize + maskSize;
        QByteArray payload(payloadRaw, int(payloadLength));

        if (masked) {
            for (int i = 0; i < payload.size(); ++i) {
                payload[i] = payload[i] ^ mask[i % 4];
            }
        }

        state.buffer.remove(0, headerSize + maskSize + int(payloadLength));

        if (opcode == 0x01) { // Text frame
            QString text = QString::fromUtf8(payload);
            processClientJson(socket, text);
        } else if (opcode == 0x08) { // Close frame
            sendWebSocketFrame(socket, QByteArray(), 0x08);
            socket->disconnectFromHost();
            return;
        } else if (opcode == 0x09) { // Ping
            sendWebSocketFrame(socket, payload, 0x0A); // Pong
        }
    }
}

void OpenMVBridgeServer::processClientJson(QTcpSocket *socket, const QString &jsonString)
{
    Q_UNUSED(socket);
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(jsonString.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        return;
    }

    QJsonObject obj = doc.object();
    QString type = obj.value(QStringLiteral("type")).toString();

    if (type == QLatin1String("file_saved")) {
        QString file = obj.value(QStringLiteral("file")).toString();
        if (!file.isEmpty()) {
            emit requestReloadFile(file);
        }
    } else if (type == QLatin1String("serial_input")) {
        QString data = obj.value(QStringLiteral("data")).toString();
        emit serialInputReceived(data);
    } else {
        emit commandReceived(type, obj);
    }
}

void OpenMVBridgeServer::sendWebSocketFrame(QTcpSocket *socket, const QByteArray &payload, quint8 opcode)
{
    if (!socket || socket->state() != QAbstractSocket::ConnectedState) return;

    QByteArray frame;
    frame.append(char(0x80 | (opcode & 0x0F))); // FIN = 1

    quint64 len = payload.size();
    if (len <= 125) {
        frame.append(char(len));
    } else if (len <= 65535) {
        frame.append(char(126));
        frame.append(char((len >> 8) & 0xFF));
        frame.append(char(len & 0xFF));
    } else {
        frame.append(char(127));
        for (int i = 7; i >= 0; --i) {
            frame.append(char((len >> (i * 8)) & 0xFF));
        }
    }

    frame.append(payload);
    socket->write(frame);
    socket->flush();
}

void OpenMVBridgeServer::broadcastMessage(const QJsonObject &jsonObj)
{
    QJsonDocument doc(jsonObj);
    QByteArray payload = doc.toJson(QJsonDocument::Compact);

    for (QTcpSocket *socket : m_clients) {
        if (m_clientStates[socket].isWebSocketHandshakeDone) {
            sendWebSocketFrame(socket, payload, 0x01);
        }
    }
}

void OpenMVBridgeServer::broadcastDiagnostics(const QString &filePath, const QJsonArray &diagnosticItems)
{
    QJsonObject msg;
    msg[QStringLiteral("type")] = QStringLiteral("diagnostics");
    msg[QStringLiteral("file")] = filePath;
    msg[QStringLiteral("items")] = diagnosticItems;
    broadcastMessage(msg);
}

void OpenMVBridgeServer::broadcastSerialData(const QString &payload, const QString &port, bool connected)
{
    if (payload.isEmpty()) return;

    QJsonObject msg;
    msg[QStringLiteral("type")] = QStringLiteral("serial_data");
    msg[QStringLiteral("payload")] = payload;
    if (!port.isEmpty()) {
        msg[QStringLiteral("port")] = port;
    }
    msg[QStringLiteral("connected")] = connected;
    broadcastMessage(msg);
}

void OpenMVBridgeServer::broadcastDeviceStatus(bool connected, const QString &port, const QString &board)
{
    QJsonObject msg;
    msg[QStringLiteral("type")] = QStringLiteral("device_status");
    msg[QStringLiteral("connected")] = connected;
    if (!port.isEmpty()) msg[QStringLiteral("port")] = port;
    if (!board.isEmpty()) msg[QStringLiteral("board")] = board;
    broadcastMessage(msg);
}

void OpenMVBridgeServer::broadcastNotification(const QString &level, const QString &message)
{
    QJsonObject msg;
    msg[QStringLiteral("type")] = QStringLiteral("notification");
    msg[QStringLiteral("level")] = level;
    msg[QStringLiteral("message")] = message;
    broadcastMessage(msg);
}
