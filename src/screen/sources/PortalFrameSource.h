#ifndef SCREEN_PORTALFRAMESOURCE_H
#define SCREEN_PORTALFRAMESOURCE_H

#include "DesktopFrameSource.h"

#include <QVariantMap>

class QTimer;

namespace Screen {

/**
 * One org.freedesktop.portal.Screenshot request. Silent first; a portal that refuses
 * the silent request (GNOME for a host app with no stored permission) is asked again
 * with its dialog, and every later request in this run goes straight to the dialog.
 */
class PortalFrameSource : public DesktopFrameSource
{
    Q_OBJECT

public:
    explicit PortalFrameSource(QObject *parent = nullptr);
    ~PortalFrameSource() override;

    [[nodiscard]] QString name() const override { return QStringLiteral("Screenshot portal"); }
    void grab() override;

private slots:
    void handleResponse(uint status, const QVariantMap &results);

private:
    bool request(bool interactive, QString *error);
    void disconnectResponse();
    void finishFailed(const QString &reason, bool cancelled);

    QTimer *m_timeout = nullptr;
    QString m_requestPath;   // the Request object we are subscribed to
    bool m_interactive = false;
    bool m_busy = false;
};

} // namespace Screen

#endif // SCREEN_PORTALFRAMESOURCE_H
