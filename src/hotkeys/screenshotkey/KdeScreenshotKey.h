#ifndef HOTKEYS_KDESCREENSHOTKEY_H
#define HOTKEYS_KDESCREENSHOTKEY_H

#include "hotkeys/ScreenshotKey.h"

#include <QDBusConnection>
#include <QElapsedTimer>
#include <QHash>
#include <QList>
#include <QMetaType>
#include <QString>

class QDBusArgument;

namespace Hotkeys {

// kglobalaccel's (ai): one QKeySequence as its combined chords, padded to four with 0.
struct KGlobalAccelKey {
    QList<int> chords;
};

// kglobalaccel's KGlobalShortcutInfo, (ssssssaiai); each key travels as its first chord.
struct KGlobalAccelShortcut {
    QString uniqueName;
    QString friendlyName;
    QString componentUniqueName;
    QString componentFriendlyName;
    QString contextUniqueName;
    QString contextFriendlyName;
    QList<int> keys;
    QList<int> defaultKeys;
};

/**
 * KDE Plasma: kglobalaccel gives Print to Spectacle and ignores the portal's preferred
 * trigger for a shortcut it already knows, so release() edits both shortcuts over raw
 * QtDBus (org.kde.KGlobalAccel, no KF6 link). Print leaves every other holder first and
 * only then goes to Snim's portal shortcut dev.snim.Snim/captureArea, because
 * kglobalaccel silently drops a key that is still taken. The memento is JSON with each
 * changed shortcut's id and previous keys.
 */
class KdeScreenshotKey : public ScreenshotKey
{
public:
    // The bus is the test seam; nothing is asked before the first query.
    explicit KdeScreenshotKey(const QDBusConnection &bus = QDBusConnection::sessionBus());

    [[nodiscard]] Support support() const override;
    [[nodiscard]] QString keyName() const override;
    // Whoever holds Print now, else Spectacle (its owner on a stock Plasma).
    [[nodiscard]] QString ownerName() const override;
    // Any single key, since KDE drops a taken trigger without a word; Snim's own excluded.
    [[nodiscard]] QString holderOf(const QKeySequence &seq) const override;
    [[nodiscard]] QList<HotkeyBinding> preset() const override;

    Result release() override;
    Result restore(const QString &memento) override;

    // Registers the wire types above with Qt DBus; safe to call any number of times.
    static void registerMetaTypes();

private:
    [[nodiscard]] bool hasService() const;
    [[nodiscard]] QString refusal() const;
    void forget() const;

    QDBusConnection m_bus;
    mutable bool m_service = false;   // a yes holds for the run
    mutable QElapsedTimer m_serviceAsked;
    mutable QHash<int, QString> m_holders;   // combined key -> holder, empty for nobody
    mutable QElapsedTimer m_holdersAsked;
};

QDBusArgument &operator<<(QDBusArgument &arg, const KGlobalAccelKey &key);
const QDBusArgument &operator>>(const QDBusArgument &arg, KGlobalAccelKey &key);
QDBusArgument &operator<<(QDBusArgument &arg, const KGlobalAccelShortcut &shortcut);
const QDBusArgument &operator>>(const QDBusArgument &arg, KGlobalAccelShortcut &shortcut);

} // namespace Hotkeys

Q_DECLARE_METATYPE(Hotkeys::KGlobalAccelKey)
Q_DECLARE_METATYPE(Hotkeys::KGlobalAccelShortcut)

#endif // HOTKEYS_KDESCREENSHOTKEY_H
