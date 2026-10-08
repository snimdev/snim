#include "hotkeys/ScreenshotKeySwap.h"

#include "core/Settings.h"
#include "hotkeys/HotkeyBindings.h"
#include "hotkeys/screenshotkey/UnsupportedScreenshotKey.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

namespace Hotkeys {

namespace {

const QString kSystem = QStringLiteral("system");
const QString kBindings = QStringLiteral("bindings");
const QString kAction = QStringLiteral("action");
const QString kSequence = QStringLiteral("sequence");

// The preset's value for action, or nullopt when the preset leaves it alone.
std::optional<QKeySequence> presetSequence(const QList<HotkeyBinding> &preset, HotkeyAction action)
{
    for (const HotkeyBinding &b : preset) {
        if (b.action == action)
            return HotkeyBindings::normalized(b.sequence);
    }
    return std::nullopt;
}

} // namespace

QString ScreenshotKeyMemento::toJson() const
{
    QJsonArray list;
    for (const HotkeyBinding &b : bindings) {
        list.append(QJsonObject{
            {kAction, hotkeyActionId(b.action)},
            {kSequence, b.sequence.toString(QKeySequence::PortableText)},
        });
    }
    const QJsonObject root{{kSystem, system}, {kBindings, list}};
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

std::optional<ScreenshotKeyMemento> ScreenshotKeyMemento::fromJson(const QString &json)
{
    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject())
        return std::nullopt;
    const QJsonObject root = doc.object();
    if (!root.value(kSystem).isString() || !root.value(kBindings).isArray())
        return std::nullopt;

    ScreenshotKeyMemento memento;
    memento.system = root.value(kSystem).toString();
    for (const QJsonValue &value : root.value(kBindings).toArray()) {
        const QJsonObject entry = value.toObject();
        const std::optional<HotkeyAction> action =
            hotkeyActionFromId(entry.value(kAction).toString());
        if (!action || !entry.value(kSequence).isString())
            return std::nullopt;
        memento.bindings.append(HotkeyBinding{
            *action,
            QKeySequence::fromString(entry.value(kSequence).toString(),
                                     QKeySequence::PortableText)});
    }
    return memento;
}

ScreenshotKeySwap::ScreenshotKeySwap(std::unique_ptr<ScreenshotKey> key,
                                     std::function<void()> reapply)
    : m_key(key ? std::move(key) : std::make_unique<UnsupportedScreenshotKey>()),
      m_reapply(std::move(reapply))
{
}

ScreenshotKey &ScreenshotKeySwap::key() const
{
    return *m_key;
}

bool ScreenshotKeySwap::isSwapped() const
{
    return !Core::Settings::screenshotKeyMemento().isEmpty();
}

ScreenshotKey::Result ScreenshotKeySwap::swap()
{
    if (isSwapped())
        return {false, {}, tr("Snim already uses %1.").arg(m_key->keyName())};

    // Only an action the preset leaves alone can end up sharing a key with it.
    const QList<HotkeyBinding> preset = m_key->preset();
    for (const HotkeyAction action : allHotkeyActions()) {
        if (presetSequence(preset, action))
            continue;
        const QKeySequence current = HotkeyBindings::normalized(HotkeyBindings::sequence(action));
        if (current.isEmpty())
            continue;
        for (const HotkeyBinding &b : preset) {
            if (HotkeyBindings::normalized(b.sequence) == current) {
                return {false, {},
                        tr("Snim's %1 hotkey already uses %2. Change it first, then try again.")
                            .arg(hotkeyActionDescription(action),
                                 current.toString(QKeySequence::NativeText))};
            }
        }
    }

    ScreenshotKeyMemento memento;
    for (const HotkeyBinding &b : preset)
        memento.bindings.append(HotkeyBinding{b.action, HotkeyBindings::sequence(b.action)});

    const ScreenshotKey::Result released = m_key->release();
    if (!released.ok)
        return released;

    memento.system = released.memento;
    Core::Settings::setScreenshotKeyMemento(memento.toJson());
    for (const HotkeyBinding &b : preset)
        HotkeyBindings::setSequence(b.action, b.sequence);
    reapply();
    return released;
}

ScreenshotKey::Result ScreenshotKeySwap::undo()
{
    const QString json = Core::Settings::screenshotKeyMemento();
    if (json.isEmpty())
        return {false, {}, tr("Snim does not use %1.").arg(m_key->keyName())};

    const std::optional<ScreenshotKeyMemento> memento = ScreenshotKeyMemento::fromJson(json);
    if (!memento) {
        // Unreadable, so nothing can be put back; forget it so the swap is offered again.
        Core::Settings::setScreenshotKeyMemento({});
        return {false, {},
                tr("Snim could not read what it changed. Give %1 back to %2 in your "
                   "system's keyboard settings.")
                    .arg(m_key->keyName(), m_key->ownerName())};
    }

    const ScreenshotKey::Result restored = m_key->restore(memento->system);
    if (!restored.ok)
        return restored;

    // A binding the user changed after the swap is theirs to keep.
    const QList<HotkeyBinding> preset = m_key->preset();
    for (const HotkeyBinding &before : memento->bindings) {
        const std::optional<QKeySequence> swapped = presetSequence(preset, before.action);
        const QKeySequence current =
            HotkeyBindings::normalized(HotkeyBindings::sequence(before.action));
        if (swapped && current == *swapped)
            HotkeyBindings::setSequence(before.action, before.sequence);
    }
    Core::Settings::setScreenshotKeyMemento({});
    reapply();
    return restored;
}

void ScreenshotKeySwap::reapply() const
{
    if (m_reapply)
        m_reapply();
}

} // namespace Hotkeys
