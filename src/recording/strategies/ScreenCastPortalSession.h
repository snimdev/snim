#ifndef RECORDING_SCREENCASTPORTALSESSION_H
#define RECORDING_SCREENCASTPORTALSESSION_H

#include <QObject>
#include <QRect>
#include <QString>
#include <QVariantMap>

namespace Recording {

/**
 * One xdg-desktop-portal ScreenCast handshake: CreateSession, SelectSources, Start and
 * OpenPipeWireRemote, ending in a PipeWire node the recorder can pull frames from.
 * GUI thread only. Reusable: open() after a failure or a close() starts from scratch.
 */
class ScreenCastPortalSession : public QObject
{
    Q_OBJECT

public:
    explicit ScreenCastPortalSession(QObject *parent = nullptr);
    ~ScreenCastPortalSession() override;

    // Probe: reads the interface version, creating no session.
    [[nodiscard]] static bool isPortalAvailable();

    void open(bool captureCursor);
    void close();

signals:
    // The receiver owns pipewireFd and must close() it.
    void ready(quint32 nodeId, const QRect &streamRectLogical, int pipewireFd);
    void failed(const QString &error);
    void sessionClosed();

private slots:
    // org.freedesktop.portal.Request Response handlers, connected old-style on the
    // precomputed request path before each call goes out.
    void handleCreateSessionResponse(uint response, const QVariantMap &results);
    void handleSelectSourcesResponse(uint response, const QVariantMap &results);
    void handleStartResponse(uint response, const QVariantMap &results);
    void handleSessionClosed();
    void handleSessionClosedWithDetails(const QVariantMap &details);

private:
    void createSession();
    void selectSources();
    void start();
    void openPipeWireRemote();
    void failStart(const QString &error);
    void fail(const QString &error);
    void reset();

    bool connectResponse(const QString &path, const char *slot);
    void disconnectResponse(const QString &path, const char *slot);

    // Token and the Request path the portal will derive from it.
    [[nodiscard]] static QString newToken();
    [[nodiscard]] static QString requestPath(const QString &token);

    // 0 when the property cannot be read.
    [[nodiscard]] static uint readUintProperty(const QString &name);

    QString m_sessionPath;
    QString m_createRequestPath;
    QString m_selectRequestPath;
    QString m_startRequestPath;
    QString m_sentRestoreToken;
    bool m_captureCursor = false;
    bool m_restoreRetried = false;   // stale restore_token recovery runs at most once
    quint32 m_nodeId = 0;
    QRect m_streamRect;
};

} // namespace Recording

#endif // RECORDING_SCREENCASTPORTALSESSION_H
