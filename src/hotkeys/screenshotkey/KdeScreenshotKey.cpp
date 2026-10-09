#include "hotkeys/screenshotkey/KdeScreenshotKey.h"

#include <QDBusArgument>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QStandardPaths>
#include <QStringList>

#include <optional>

namespace Hotkeys {

namespace {

const QString kService = QStringLiteral("org.kde.kglobalaccel");
const QString kPath = QStringLiteral("/kglobalaccel");
const QString kInterface = QStringLiteral("org.kde.KGlobalAccel");
// Where KDE's portal files Snim's shortcuts: the app id, then hotkeyActionId().
const QString kSnimComponent = QStringLiteral("dev.snim.Snim");
// Calls run on the GUI thread, so a stuck daemon may only hold it this long.
constexpr int kCallTimeoutMs = 2000;
// Settings asks on every keystroke; older answers are asked again.
constexpr qint64 kCacheMs = 2000;
// kglobalaccel's maxSequenceLength.
constexpr int kMaxChords = 4;

const QString kHolders = QStringLiteral("holders");
const QString kSnim = QStringLiteral("snim");
const QString kId = QStringLiteral("id");
const QString kKeys = QStringLiteral("keys");

using Key = QList<int>;   // one key sequence, as its combined chords
using Keys = QList<Key>;

const Key kPrint{int(Qt::Key_Print)};

// A shortcut and its keys; id is componentUnique, actionUnique, componentFriendly,
// actionFriendly, as every KGlobalAccel method takes it.
struct Action {
    QStringList id;
    Keys keys;
};

struct Memento {
    QList<Action> holders;
    std::optional<Action> snim;
};

std::optional<QDBusMessage> call(const QDBusConnection &bus, const QString &method,
                                 const QVariantList &args)
{
    QDBusMessage message = QDBusMessage::createMethodCall(kService, kPath, kInterface, method);
    message.setArguments(args);
    // Only a running daemon is asked; a desktop without one is not Snim's to start.
    message.setAutoStartService(false);
    const QDBusMessage reply = bus.call(message, QDBus::Block, kCallTimeoutMs);
    if (reply.type() != QDBusMessage::ReplyMessage) {
        qWarning().noquote() << QStringLiteral("Screenshot key: kglobalaccel %1 failed: %2 %3")
                                    .arg(method, reply.errorName(), reply.errorMessage());
        return std::nullopt;
    }
    return reply;
}

Key fromWire(const KGlobalAccelKey &key)
{
    Key chords;
    for (const int chord : key.chords) {
        if (chord == 0)
            break;
        chords.append(chord);
    }
    return chords;
}

KGlobalAccelKey toWire(const Key &key)
{
    KGlobalAccelKey wire{key.mid(0, kMaxChords)};
    while (wire.chords.size() < kMaxChords)
        wire.chords.append(0);
    return wire;
}

std::optional<Keys> keysOf(const QDBusConnection &bus, const QStringList &id)
{
    const auto reply = call(bus, QStringLiteral("shortcutKeys"), {QVariant::fromValue(id)});
    if (!reply || reply->arguments().isEmpty())
        return std::nullopt;
    Keys keys;
    for (const KGlobalAccelKey &wire :
         qdbus_cast<QList<KGlobalAccelKey>>(reply->arguments().constFirst())) {
        const Key key = fromWire(wire);
        if (!key.isEmpty())
            keys.append(key);
    }
    return keys;
}

bool setKeys(const QDBusConnection &bus, const QStringList &id, const Keys &keys)
{
    QList<KGlobalAccelKey> wire;
    for (const Key &key : keys)
        wire.append(toWire(key));
    return call(bus, QStringLiteral("setForeignShortcutKeys"),
                {QVariant::fromValue(id), QVariant::fromValue(wire)})
        .has_value();
}

// Every shortcut on key except Snim's own; nullopt when kglobalaccel did not answer.
std::optional<QList<KGlobalAccelShortcut>> otherHolders(const QDBusConnection &bus, int key)
{
    const auto reply = call(bus, QStringLiteral("getGlobalShortcutsByKey"), {key});
    if (!reply || reply->arguments().isEmpty())
        return std::nullopt;
    QList<KGlobalAccelShortcut> others;
    for (const KGlobalAccelShortcut &shortcut :
         qdbus_cast<QList<KGlobalAccelShortcut>>(reply->arguments().constFirst())) {
        if (shortcut.componentUniqueName != kSnimComponent)
            others.append(shortcut);
    }
    return others;
}

QStringList actionIdOf(const KGlobalAccelShortcut &shortcut)
{
    // Outside the default context a shortcut is addressed as "component|context".
    QString component = shortcut.componentUniqueName;
    if (!shortcut.contextUniqueName.isEmpty()
        && shortcut.contextUniqueName != QLatin1String("default"))
        component += QLatin1Char('|') + shortcut.contextUniqueName;
    return {component, shortcut.uniqueName, shortcut.componentFriendlyName,
            shortcut.friendlyName};
}

QString displayName(const QString &friendly, const QString &unique)
{
    return friendly.isEmpty() ? unique : friendly;
}

// Snim's Capture Area shortcut, if KDE's portal filed it under Snim's app id.
std::optional<QStringList> snimCaptureArea(const QDBusConnection &bus)
{
    const auto reply = call(bus, QStringLiteral("allActionsForComponent"),
                            {QVariant::fromValue(QStringList{kSnimComponent})});
    if (!reply || reply->arguments().isEmpty())
        return std::nullopt;
    const QString captureArea = hotkeyActionId(HotkeyAction::CaptureArea);
    for (const QStringList &id : qdbus_cast<QList<QStringList>>(reply->arguments().constFirst())) {
        if (id.size() == 4 && id.at(1) == captureArea)
            return id;
    }
    return std::nullopt;
}

// Best effort, newest change first.
void putBack(const QDBusConnection &bus, const QList<Action> &actions)
{
    for (auto it = actions.crbegin(); it != actions.crend(); ++it)
        setKeys(bus, it->id, it->keys);
}

QJsonObject actionToJson(const Action &action)
{
    QJsonArray keys;
    for (const Key &key : action.keys) {
        QJsonArray chords;
        for (const int chord : key)
            chords.append(chord);
        keys.append(chords);
    }
    return {{kId, QJsonArray::fromStringList(action.id)}, {kKeys, keys}};
}

std::optional<Action> actionFromJson(const QJsonValue &value)
{
    const QJsonObject object = value.toObject();
    const QJsonArray id = object.value(kId).toArray();
    if (!value.isObject() || id.size() != 4 || !object.value(kKeys).isArray())
        return std::nullopt;
    Action action;
    for (const QJsonValue &part : id) {
        if (!part.isString())
            return std::nullopt;
        action.id.append(part.toString());
    }
    for (const QJsonValue &key : object.value(kKeys).toArray()) {
        if (!key.isArray())
            return std::nullopt;
        Key chords;
        for (const QJsonValue &chord : key.toArray()) {
            if (!chord.isDouble())
                return std::nullopt;
            chords.append(chord.toInt());
        }
        action.keys.append(chords);
    }
    return action;
}

QString mementoToJson(const Memento &memento)
{
    QJsonArray holders;
    for (const Action &holder : memento.holders)
        holders.append(actionToJson(holder));
    const QJsonObject root{
        {kHolders, holders},
        {kSnim, memento.snim ? QJsonValue(actionToJson(*memento.snim)) : QJsonValue()},
    };
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

std::optional<Memento> mementoFromJson(const QString &json)
{
    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject())
        return std::nullopt;
    const QJsonObject root = doc.object();
    if (!root.value(kHolders).isArray())
        return std::nullopt;

    Memento memento;
    for (const QJsonValue &value : root.value(kHolders).toArray()) {
        const std::optional<Action> holder = actionFromJson(value);
        if (!holder)
            return std::nullopt;
        memento.holders.append(*holder);
    }
    const QJsonValue snim = root.value(kSnim);
    if (!snim.isNull()) {
        memento.snim = actionFromJson(snim);
        if (!memento.snim)
            return std::nullopt;
    }
    return memento;
}

QString shortcutsPage()
{
    return ScreenshotKey::tr("System Settings > Keyboard > Shortcuts");
}

} // namespace

QDBusArgument &operator<<(QDBusArgument &arg, const KGlobalAccelKey &key)
{
    arg.beginStructure();
    arg << key.chords;
    arg.endStructure();
    return arg;
}

const QDBusArgument &operator>>(const QDBusArgument &arg, KGlobalAccelKey &key)
{
    arg.beginStructure();
    arg >> key.chords;
    arg.endStructure();
    return arg;
}

QDBusArgument &operator<<(QDBusArgument &arg, const KGlobalAccelShortcut &shortcut)
{
    arg.beginStructure();
    arg << shortcut.uniqueName << shortcut.friendlyName << shortcut.componentUniqueName
        << shortcut.componentFriendlyName << shortcut.contextUniqueName
        << shortcut.contextFriendlyName << shortcut.keys << shortcut.defaultKeys;
    arg.endStructure();
    return arg;
}

const QDBusArgument &operator>>(const QDBusArgument &arg, KGlobalAccelShortcut &shortcut)
{
    arg.beginStructure();
    arg >> shortcut.uniqueName >> shortcut.friendlyName >> shortcut.componentUniqueName
        >> shortcut.componentFriendlyName >> shortcut.contextUniqueName
        >> shortcut.contextFriendlyName >> shortcut.keys >> shortcut.defaultKeys;
    arg.endStructure();
    return arg;
}

void KdeScreenshotKey::registerMetaTypes()
{
    static const bool registered = [] {
        qDBusRegisterMetaType<KGlobalAccelKey>();
        qDBusRegisterMetaType<QList<KGlobalAccelKey>>();
        qDBusRegisterMetaType<KGlobalAccelShortcut>();
        qDBusRegisterMetaType<QList<KGlobalAccelShortcut>>();
        qDBusRegisterMetaType<QList<QStringList>>();
        return true;
    }();
    Q_UNUSED(registered);
}

KdeScreenshotKey::KdeScreenshotKey(const QDBusConnection &bus) : m_bus(bus)
{
    registerMetaTypes();
}

ScreenshotKey::Support KdeScreenshotKey::support() const
{
    return hasService() ? Support::Automatic : Support::Unsupported;
}

QString KdeScreenshotKey::keyName() const
{
    return tr("Print Screen");
}

QString KdeScreenshotKey::ownerName() const
{
    const QString holder = holderOf(QKeySequence(Qt::Key_Print));
    return holder.isEmpty() ? tr("Spectacle") : holder;
}

QString KdeScreenshotKey::holderOf(const QKeySequence &seq) const
{
    if (seq.count() != 1 || !hasService())
        return {};
    if (!m_holdersAsked.isValid() || m_holdersAsked.hasExpired(kCacheMs)) {
        m_holders.clear();
        m_holdersAsked.start();
    }
    const int key = seq[0].toCombined();
    const auto cached = m_holders.constFind(key);
    if (cached != m_holders.cend())
        return cached.value();

    QString holder;
    const auto others = otherHolders(m_bus, key);
    if (others && !others->isEmpty())
        holder = displayName(others->constFirst().componentFriendlyName,
                             others->constFirst().componentUniqueName);
    m_holders.insert(key, holder);
    return holder;
}

QList<HotkeyBinding> KdeScreenshotKey::preset() const
{
    return screenshotKeyPreset(HotkeyPlatform::Linux);
}

ScreenshotKey::Result KdeScreenshotKey::release()
{
    if (const QString refused = refusal(); !refused.isEmpty())
        return {false, {}, refused};
    forget();
    const QString unreachable = tr("Snim could not reach KDE's shortcut service.");

    // Order matters: kglobalaccel drops a key that is still taken, so Print leaves first.
    const auto holders = otherHolders(m_bus, kPrint.constFirst());
    if (!holders)
        return {false, {}, unreachable};
    Memento memento;
    for (const KGlobalAccelShortcut &holder : *holders) {
        Action before{actionIdOf(holder), {}};
        const std::optional<Keys> keys = keysOf(m_bus, before.id);
        if (!keys) {
            putBack(m_bus, memento.holders);
            return {false, {}, unreachable};
        }
        before.keys = *keys;
        Keys without = *keys;
        without.removeAll(kPrint);
        // Recorded before the call: a failed one may still have landed.
        memento.holders.append(before);
        if (!setKeys(m_bus, before.id, without)) {
            putBack(m_bus, memento.holders);
            return {false, {}, unreachable};
        }
    }

    const auto left = otherHolders(m_bus, kPrint.constFirst());
    if (!left || !left->isEmpty()) {
        putBack(m_bus, memento.holders);
        forget();
        if (!left)
            return {false, {}, unreachable};
        return {false, {},
                tr("KDE did not let Snim take Print Screen from %1.")
                    .arg(displayName(left->constFirst().componentFriendlyName,
                                     left->constFirst().componentUniqueName))};
    }

    // Started outside its app id (from an IDE, say), Snim's shortcut is filed elsewhere.
    const QString guidance =
        tr("Print Screen is free now. In %1, set Snim's Capture Area to Print Screen.")
            .arg(shortcutsPage());
    QString message;
    const std::optional<QStringList> snim = snimCaptureArea(m_bus);
    const std::optional<Keys> snimKeys = snim ? keysOf(m_bus, *snim) : std::nullopt;
    if (snim && snimKeys) {
        memento.snim = Action{*snim, *snimKeys};
        setKeys(m_bus, *snim, {kPrint});
        const std::optional<Keys> now = keysOf(m_bus, *snim);
        if (!now || !now->contains(kPrint))
            message = guidance;
    } else {
        message = guidance;
    }
    forget();
    return {true, mementoToJson(memento), message};
}

ScreenshotKey::Result KdeScreenshotKey::restore(const QString &memento)
{
    if (const QString refused = refusal(); !refused.isEmpty())
        return {false, {}, refused};
    forget();

    // Nothing to replay, but the swap must not stay stuck on it.
    const std::optional<Memento> before = mementoFromJson(memento);
    if (!before) {
        return {true, {},
                tr("Snim could not tell which shortcut had Print Screen. Give it back in %1.")
                    .arg(shortcutsPage())};
    }

    // Reverse order: Snim lets go of Print first, or the old holders would not get it back.
    if (before->snim)
        setKeys(m_bus, before->snim->id, before->snim->keys);
    QStringList missed;
    for (auto it = before->holders.crbegin(); it != before->holders.crend(); ++it) {
        setKeys(m_bus, it->id, it->keys);
        const std::optional<Keys> now = keysOf(m_bus, it->id);
        if (it->keys.contains(kPrint) && (!now || !now->contains(kPrint)))
            missed.prepend(displayName(it->id.at(2), it->id.at(0)));
    }
    forget();
    if (!missed.isEmpty()) {
        return {true, {},
                tr("%1 did not get Print Screen back. Set it in %2.")
                    .arg(missed.join(QStringLiteral(", ")), shortcutsPage())};
    }
    return {true, {}, {}};
}

bool KdeScreenshotKey::hasService() const
{
    // A no is asked again later: the daemon may still be starting.
    if (m_service || (m_serviceAsked.isValid() && !m_serviceAsked.hasExpired(kCacheMs)))
        return m_service;
    m_serviceAsked.start();
    QDBusMessage message = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.DBus"), QStringLiteral("/org/freedesktop/DBus"),
        QStringLiteral("org.freedesktop.DBus"), QStringLiteral("NameHasOwner"));
    message.setArguments({kService});
    const QDBusMessage reply = m_bus.call(message, QDBus::Block, kCallTimeoutMs);
    m_service = reply.type() == QDBusMessage::ReplyMessage && reply.arguments().value(0).toBool();
    return m_service;
}

QString KdeScreenshotKey::refusal() const
{
    // Tests only ever edit the fake kglobalaccel on the private bus ctest starts.
    if (QStandardPaths::isTestModeEnabled()
        && qEnvironmentVariable("SNIM_TEST_PRIVATE_BUS") != QLatin1String("1"))
        return tr("Snim leaves KDE's shortcuts alone while testing.");
    if (!hasService())
        return tr("KDE's shortcut service is not running.");
    return {};
}

void KdeScreenshotKey::forget() const
{
    m_holders.clear();
    m_holdersAsked.invalidate();
}

} // namespace Hotkeys
