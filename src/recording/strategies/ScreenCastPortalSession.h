#ifndef RECORDING_SCREENCASTPORTALSESSION_H
#define RECORDING_SCREENCASTPORTALSESSION_H

#include <QList>
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

    struct Options {
        bool captureCursor = false;
        bool multiple = false;
        // QSettings key holding the restore token; empty means the picker shows every time.
        QString restoreTokenKey;
    };

    struct Stream {
        quint32 nodeId = 0;
        QRect rectLogical;   // null when the portal sent no geometry
    };

    // Probe: reads the interface version, creating no session.
    [[nodiscard]] static bool isPortalAvailable();

    // The ScreenCast interface version, 0 when the portal cannot be reached.
    [[nodiscard]] static uint portalVersion();

    // Restore tokens are single use: Start hands back the one to present next time.
    [[nodiscard]] static QString restoreToken(const QString &key);
    static void storeRestoreToken(const QString &key, const QVariantMap &startResults);

    void open(bool captureCursor);
    void open(const Options &options);
    void close();

    // Every stream Start returned, in portal order; valid once ready() fired.
    [[nodiscard]] const QList<Stream> &streams() const { return m_streams; }

signals:
    // The first stream. The receiver owns pipewireFd and must close() it.
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
    void fail(const QString &error);
    void failResponse(const char *step, uint response);
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
    Options m_options;
    QList<Stream> m_streams;
};

} // namespace Recording

#endif // RECORDING_SCREENCASTPORTALSESSION_H
