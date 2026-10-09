#include "hotkeys/screenshotkey/GnomeScreenshotKey.h"

#include "hotkeys/HotkeyBindings.h"

#include <QProcess>
#include <QStandardPaths>

#include <utility>

namespace Hotkeys {

namespace {

const QString kSchema = QStringLiteral("org.gnome.shell.keybindings");
const QString kKey = QStringLiteral("show-screenshot-ui");
const QString kPrint = QStringLiteral("Print");
// GNOME's default for the key, which reset restores.
const QStringList kDefault{kPrint};
// The real runner blocks the GUI thread, so a stuck gsettings may only hold it this long.
constexpr int kGSettingsTimeoutMs = 3000;
// Settings asks on every keystroke; older answers are read again.
constexpr qint64 kCacheMs = 2000;

QString appShortcutsPage()
{
    return ScreenshotKey::tr("Settings > Apps > Snim > Global Shortcuts");
}

} // namespace

std::optional<QStringList> parseGSettingsList(const QString &text)
{
    QString value = text.trimmed();
    if (value.startsWith(QLatin1String("@as ")))
        value = value.mid(4).trimmed();
    if (!value.startsWith(QLatin1Char('[')) || !value.endsWith(QLatin1Char(']')))
        return std::nullopt;

    QStringList list;
    const QString inner = value.mid(1, value.size() - 2);
    qsizetype i = 0;
    auto skipSpaces = [&] {
        while (i < inner.size() && inner.at(i).isSpace())
            ++i;
    };
    skipSpaces();
    while (i < inner.size()) {
        const QChar quote = inner.at(i);
        if (quote != QLatin1Char('\'') && quote != QLatin1Char('"'))
            return std::nullopt;
        QString item;
        bool closed = false;
        for (++i; i < inner.size(); ++i) {
            const QChar c = inner.at(i);
            if (c == QLatin1Char('\\') && i + 1 < inner.size()) {
                item.append(inner.at(++i));
            } else if (c == quote) {
                closed = true;
                ++i;
                break;
            } else {
                item.append(c);
            }
        }
        if (!closed)
            return std::nullopt;
        list.append(item);

        skipSpaces();
        if (i == inner.size())
            break;
        if (inner.at(i) != QLatin1Char(','))
            return std::nullopt;
        ++i;
        skipSpaces();
        // A comma must be followed by another item.
        if (i == inner.size())
            return std::nullopt;
    }
    return list;
}

QString formatGSettingsList(const QStringList &list)
{
    if (list.isEmpty())
        return QStringLiteral("@as []");
    QStringList quoted;
    for (QString item : list) {
        item.replace(QLatin1Char('\\'), QLatin1String("\\\\"));
        item.replace(QLatin1Char('\''), QLatin1String("\\'"));
        quoted.append(QLatin1Char('\'') + item + QLatin1Char('\''));
    }
    return QLatin1Char('[') + quoted.join(QLatin1String(", ")) + QLatin1Char(']');
}

GnomeScreenshotKey::GnomeScreenshotKey(GSettings gsettings, bool flatpak)
    : m_gsettings(std::move(gsettings)), m_flatpak(flatpak)
{
}

ScreenshotKey::Support GnomeScreenshotKey::support() const
{
    // The factory only picks GNOME with the GlobalShortcuts portal, so this is 48 or later.
    return m_flatpak ? Support::Manual : Support::Assisted;
}

QString GnomeScreenshotKey::keyName() const
{
    return tr("Print Screen");
}

QString GnomeScreenshotKey::ownerName() const
{
    return tr("GNOME");
}

QString GnomeScreenshotKey::holderOf(const QKeySequence &seq) const
{
    if (HotkeyBindings::normalized(seq) != QKeySequence(Qt::Key_Print))
        return {};
    // The sandbox cannot read GNOME's settings; Print is GNOME's unless the user moved it.
    if (m_flatpak)
        return ownerName();
    const std::optional<QStringList> keys = screenshotKeys();
    return keys && keys->contains(kPrint) ? ownerName() : QString();
}

QList<HotkeyBinding> GnomeScreenshotKey::preset() const
{
    return screenshotKeyPreset(HotkeyPlatform::Linux);
}

QString GnomeScreenshotKey::guidance() const
{
    if (m_flatpak) {
        return tr("In Settings > Keyboard > View and Customize Shortcuts > Screenshots, "
                  "remove Print Screen from \"Take a screenshot interactively\". Then open "
                  "%1 and set Capture Area to Print Screen.")
            .arg(appShortcutsPage());
    }
    return tr("Now open %1 and set Capture Area to Print Screen.").arg(appShortcutsPage());
}

ScreenshotKey::Result GnomeScreenshotKey::release()
{
    if (m_flatpak)
        return {false, {}, guidance()};
    forget();

    const std::optional<QString> text = m_gsettings({QStringLiteral("get"), kSchema, kKey});
    const std::optional<QStringList> before = text ? parseGSettingsList(*text) : std::nullopt;
    if (!before)
        return {false, {}, tr("Snim could not read GNOME's screenshot shortcut.")};

    QStringList without = *before;
    without.removeAll(kPrint);
    if (!m_gsettings({QStringLiteral("set"), kSchema, kKey, formatGSettingsList(without)}))
        return {false, {}, tr("GNOME did not let Snim change its screenshot shortcut.")};
    // Print is free, but only GNOME's own dialog can put it on Snim's shortcut.
    return {true, formatGSettingsList(*before), guidance()};
}

ScreenshotKey::Result GnomeScreenshotKey::restore(const QString &memento)
{
    if (m_flatpak)
        return {false, {}, guidance()};
    forget();

    // An unreadable memento falls back to GNOME's default, which is Print.
    const std::optional<QStringList> before = parseGSettingsList(memento);
    const QStringList args = !before || *before == kDefault
                                 ? QStringList{QStringLiteral("reset"), kSchema, kKey}
                                 : QStringList{QStringLiteral("set"), kSchema, kKey,
                                               formatGSettingsList(*before)};
    if (!m_gsettings(args))
        return {false, {}, tr("GNOME did not let Snim change its screenshot shortcut.")};
    return {true, {},
            tr("If Snim's Capture Area still uses Print Screen, change it in %1.")
                .arg(appShortcutsPage())};
}

std::optional<QString> GnomeScreenshotKey::runGSettings(const QStringList &args)
{
    // Tests run on developers' own desktops: never their real settings.
    if (QStandardPaths::isTestModeEnabled())
        return std::nullopt;
    const QString program = QStandardPaths::findExecutable(QStringLiteral("gsettings"));
    if (program.isEmpty())
        return std::nullopt;

    QProcess process;
    process.start(program, args);
    if (!process.waitForFinished(kGSettingsTimeoutMs)) {
        process.kill();
        process.waitForFinished();
        return std::nullopt;
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
        return std::nullopt;
    return QString::fromUtf8(process.readAllStandardOutput());
}

std::optional<QStringList> GnomeScreenshotKey::screenshotKeys() const
{
    if (!m_keysRead.isValid() || m_keysRead.hasExpired(kCacheMs)) {
        const std::optional<QString> text = m_gsettings({QStringLiteral("get"), kSchema, kKey});
        m_keys = text ? parseGSettingsList(*text) : std::nullopt;
        m_keysRead.start();
    }
    return m_keys;
}

void GnomeScreenshotKey::forget() const
{
    m_keysRead.invalidate();
}

} // namespace Hotkeys
