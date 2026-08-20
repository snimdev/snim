#ifndef CORE_PORTAL_H
#define CORE_PORTAL_H

#include <QString>
#include <QVariant>
#include <QVariantMap>

#include <functional>

class QObject;

/**
 * What every xdg-desktop-portal client repeats: the portal object, request tokens and
 * paths, the Response subscription and the blocking property probes. Linux only, GUI
 * thread only (the probe cache is unguarded).
 */
namespace Core::Portal {

inline const QString kService = QStringLiteral("org.freedesktop.portal.Desktop");
inline const QString kPath = QStringLiteral("/org/freedesktop/portal/desktop");

// A handle token: it becomes an object-path element, so no braces or dashes.
[[nodiscard]] QString newToken();

// session_handle from a CreateSession response: a string by the spec, an object path
// from some backends; empty when missing.
[[nodiscard]] QString sessionHandle(const QVariantMap &results);

// One property of interface on the portal object; invalid when the portal does not
// answer within 2 s (the caller blocks the GUI thread meanwhile).
[[nodiscard]] QVariant property(const QString &interface, const QString &name);

// Whether interface answers (its version property). A yes is kept for the run; a no is
// asked again, the portal may still be starting.
[[nodiscard]] bool hasInterface(const QString &interface);

// Sends a method that answers with a Request. Its reply only acknowledges the request
// (the answer is the Response signal), so only a failed call reaches onError.
void sendRequest(const QString &interface, const QString &method, const QVariantList &args,
                 QObject *context, const std::function<void(const QString &error)> &onError);

// Ends a session; the portal drops whatever the session held.
void closeSession(const QString &sessionPath);

/**
 * One org.freedesktop.portal.Request. The portal derives its path from our unique name
 * and the handle token, so the Response is subscribed BEFORE the call goes out and the
 * answer can never be missed.
 */
class Request
{
public:
    // A fresh token and path with Response connected to slot; false when the bus refused.
    bool listen(QObject *receiver, const char *slot);
    // Disconnects; safe when not listening.
    void stop();

    [[nodiscard]] const QString &token() const { return m_token; }
    [[nodiscard]] const QString &path() const { return m_path; }

    // ":1.42" + token -> /org/freedesktop/portal/desktop/request/1_42/<token>
    [[nodiscard]] static QString pathFor(const QString &uniqueName, const QString &token);

private:
    QString m_token;
    QString m_path;
    QObject *m_receiver = nullptr;
    const char *m_slot = nullptr;
};

} // namespace Core::Portal

#endif // CORE_PORTAL_H
