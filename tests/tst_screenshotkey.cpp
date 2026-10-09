#include <QtTest>
#include <QSet>
#include <QSettings>
#include <QStandardPaths>

#include "core/Settings.h"
#include "hotkeys/HotkeyAction.h"
#include "hotkeys/HotkeyBindings.h"
#include "hotkeys/MacKeyMapping.h"
#include "hotkeys/PortalKeyMapping.h"
#include "hotkeys/ScreenshotKey.h"
#include "hotkeys/ScreenshotKeySwap.h"
#include "hotkeys/WinKeyMapping.h"
#include "hotkeys/screenshotkey/UnsupportedScreenshotKey.h"

#include <memory>

using namespace Hotkeys;

namespace {

// Two actions, on keys no platform's defaults use, so the swap reads the same everywhere.
QList<HotkeyBinding> fakePreset()
{
    return {{HotkeyAction::CaptureArea, QKeySequence(QStringLiteral("Print"))},
            {HotkeyAction::CaptureFullScreen, QKeySequence(QStringLiteral("Shift+Print"))}};
}

// Stands in for an OS: answers release/restore as told and records the calls.
class FakeScreenshotKey : public ScreenshotKey
{
public:
    Result releaseResult{true, QStringLiteral("os-before"), QStringLiteral("Freed.")};
    Result restoreResult{true, {}, QStringLiteral("Given back.")};
    int releases = 0;
    QStringList restored;

    Support support() const override { return Support::Automatic; }
    QString keyName() const override { return QStringLiteral("Print Screen"); }
    QString ownerName() const override { return QStringLiteral("Fake Tool"); }
    QString holderOf(const QKeySequence &) const override { return {}; }
    QList<HotkeyBinding> preset() const override { return fakePreset(); }

    Result release() override
    {
        ++releases;
        return releaseResult;
    }
    Result restore(const QString &memento) override
    {
        restored.append(memento);
        return restoreResult;
    }
};

} // namespace

// The screenshot key: which strategy each system gets, the bindings Snim takes once the
// key is free, the Null Object for systems that offer no way, and the undoable swap.
class tst_ScreenshotKey : public QObject
{
    Q_OBJECT

    // The swap owns its key; m_fake stays valid while m_swap lives.
    std::unique_ptr<ScreenshotKeySwap> m_swap;
    FakeScreenshotKey *m_fake = nullptr;
    int m_reapplies = 0;

    void makeSwap()
    {
        auto fake = std::make_unique<FakeScreenshotKey>();
        m_fake = fake.get();
        m_swap = std::make_unique<ScreenshotKeySwap>(std::move(fake), [this] { ++m_reapplies; });
    }

    static QKeySequence seq(const char *text) { return QKeySequence(QString::fromLatin1(text)); }

private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName("SnimTest");
        QCoreApplication::setApplicationName("tst_screenshotkey");
        QStandardPaths::setTestModeEnabled(true);   // throwaway store, never the real prefs
    }

    void init()
    {
        QSettings().clear();
        m_reapplies = 0;
        makeSwap();
    }

    void chooser_data()
    {
        QTest::addColumn<HotkeyPlatform>("platform");
        QTest::addColumn<QString>("desktop");
        QTest::addColumn<bool>("portal");
        QTest::addColumn<ScreenshotKeyKind>("expected");

        const HotkeyPlatform onLinux = HotkeyPlatform::Linux;
        // The desktop and portal only matter on Linux.
        QTest::newRow("windows") << HotkeyPlatform::Windows << QString() << false
                                 << ScreenshotKeyKind::Windows;
        QTest::newRow("windows/kde-env") << HotkeyPlatform::Windows << "KDE" << true
                                         << ScreenshotKeyKind::Windows;
        QTest::newRow("mac") << HotkeyPlatform::Mac << QString() << false
                             << ScreenshotKeyKind::Mac;
        QTest::newRow("kde") << onLinux << "KDE" << true << ScreenshotKeyKind::Kde;
        QTest::newRow("kde/lowercase") << onLinux << "kde" << true << ScreenshotKeyKind::Kde;
        QTest::newRow("gnome") << onLinux << "GNOME" << true << ScreenshotKeyKind::Gnome;
        QTest::newRow("ubuntu:GNOME") << onLinux << "ubuntu:GNOME" << true
                                      << ScreenshotKeyKind::Gnome;
        QTest::newRow("cinnamon") << onLinux << "X-Cinnamon" << true
                                  << ScreenshotKeyKind::Unsupported;
        QTest::newRow("empty") << onLinux << QString() << true << ScreenshotKeyKind::Unsupported;
        QTest::newRow("kde/no-portal") << onLinux << "KDE" << false
                                       << ScreenshotKeyKind::Unsupported;
        QTest::newRow("gnome/no-portal") << onLinux << "GNOME" << false
                                         << ScreenshotKeyKind::Unsupported;
    }

    void chooser()
    {
        QFETCH(HotkeyPlatform, platform);
        QFETCH(QString, desktop);
        QFETCH(bool, portal);
        QFETCH(ScreenshotKeyKind, expected);
        QCOMPARE(chooseScreenshotKey(platform, desktop, portal), expected);
    }

    void presetOnWindowsAndLinuxIsPrint()
    {
        for (const HotkeyPlatform p : {HotkeyPlatform::Windows, HotkeyPlatform::Linux}) {
            const QList<HotkeyBinding> preset = screenshotKeyPreset(p);
            QCOMPARE(preset.size(), 1);
            QCOMPARE(preset.at(0).action, HotkeyAction::CaptureArea);
            QCOMPARE(preset.at(0).sequence, QKeySequence(Qt::Key_Print));
        }
    }

    void presetOnMacIsTheSystemKeys()
    {
        // Qt's Ctrl is Command: Shift+Command 4/3/5, as the system binds them.
        const QList<HotkeyBinding> preset = screenshotKeyPreset(HotkeyPlatform::Mac);
        QCOMPARE(preset.size(), 3);
        QCOMPARE(preset.at(0).action, HotkeyAction::CaptureArea);
        QCOMPARE(preset.at(0).sequence, QKeySequence(QStringLiteral("Ctrl+Shift+4")));
        QCOMPARE(preset.at(1).action, HotkeyAction::CaptureFullScreen);
        QCOMPARE(preset.at(1).sequence, QKeySequence(QStringLiteral("Ctrl+Shift+3")));
        QCOMPARE(preset.at(2).action, HotkeyAction::RecordArea);
        QCOMPARE(preset.at(2).sequence, QKeySequence(QStringLiteral("Ctrl+Shift+5")));
    }

    void presetMapsOnItsPlatform()
    {
        // A preset its own backend cannot register would free the key for nothing.
        for (const HotkeyBinding &b : screenshotKeyPreset(HotkeyPlatform::Windows))
            QVERIFY2(toWinHotkey(b.sequence).has_value(), qPrintable(b.sequence.toString()));
        for (const HotkeyBinding &b : screenshotKeyPreset(HotkeyPlatform::Mac))
            QVERIFY2(toCarbonHotkey(b.sequence).has_value(), qPrintable(b.sequence.toString()));
        for (const HotkeyBinding &b : screenshotKeyPreset(HotkeyPlatform::Linux))
            QVERIFY2(!toPortalTrigger(b.sequence).isEmpty(), qPrintable(b.sequence.toString()));
    }

    void presetAvoidsEveryDefaultOnItsPlatform()
    {
        // Otherwise the swap would collide with Snim's own untouched defaults.
        for (const HotkeyPlatform p :
             {HotkeyPlatform::Windows, HotkeyPlatform::Mac, HotkeyPlatform::Linux}) {
            QSet<QString> defaults;
            for (const HotkeyAction a : allHotkeyActions())
                defaults.insert(hotkeyActionDefault(a, p).toString(QKeySequence::PortableText));
            QSet<HotkeyAction> actions;
            for (const HotkeyBinding &b : screenshotKeyPreset(p)) {
                const QString text = b.sequence.toString(QKeySequence::PortableText);
                QVERIFY2(!defaults.contains(text), qPrintable(text));
                QVERIFY(!actions.contains(b.action));
                actions.insert(b.action);
            }
        }
    }

    void unsupportedIsANullObject()
    {
        UnsupportedScreenshotKey key;
        QCOMPARE(key.support(), ScreenshotKey::Support::Unsupported);
        QVERIFY(!key.keyName().isEmpty());
        QVERIFY(key.holderOf(QKeySequence(Qt::Key_Print)).isEmpty());
        QVERIFY(key.preset().isEmpty());
        QVERIFY(key.guidance().isEmpty());
        QVERIFY(!key.settingsPage().isValid());

        const ScreenshotKey::Result released = key.release();
        QVERIFY(!released.ok);
        QVERIFY(released.memento.isEmpty());
        QVERIFY(!released.message.isEmpty());

        const ScreenshotKey::Result restored = key.restore(QStringLiteral("anything"));
        QVERIFY(!restored.ok);
        QVERIFY(!restored.message.isEmpty());
    }

    void mementoJsonRoundTrips()
    {
        ScreenshotKeyMemento memento;
        memento.system = QStringLiteral("{\"nested\":1}");
        memento.bindings = {{HotkeyAction::CaptureArea, seq("Ctrl+Print")},
                            {HotkeyAction::CaptureFullScreen, QKeySequence()}};

        const auto back = ScreenshotKeyMemento::fromJson(memento.toJson());
        QVERIFY(back.has_value());
        QCOMPARE(back->system, memento.system);
        QCOMPARE(back->bindings.size(), 2);
        QCOMPARE(back->bindings.at(0).action, HotkeyAction::CaptureArea);
        QCOMPARE(back->bindings.at(0).sequence, seq("Ctrl+Print"));
        QCOMPARE(back->bindings.at(1).action, HotkeyAction::CaptureFullScreen);
        QVERIFY(back->bindings.at(1).sequence.isEmpty());
    }

    void badMementoJsonIsNullopt()
    {
        QVERIFY(!ScreenshotKeyMemento::fromJson(QString()).has_value());
        QVERIFY(!ScreenshotKeyMemento::fromJson("not json").has_value());
        QVERIFY(!ScreenshotKeyMemento::fromJson("[]").has_value());
        QVERIFY(!ScreenshotKeyMemento::fromJson(R"({"system":1,"bindings":[]})").has_value());
        QVERIFY(!ScreenshotKeyMemento::fromJson(R"({"system":""})").has_value());
        QVERIFY(!ScreenshotKeyMemento::fromJson(
                     R"({"system":"","bindings":[{"action":"nonsense","sequence":""}]})")
                     .has_value());
    }

    void swapWritesThePresetAndTheMemento()
    {
        const ScreenshotKey::Result result = m_swap->swap();
        QVERIFY(result.ok);
        QCOMPARE(result.message, QStringLiteral("Freed."));
        QCOMPARE(m_fake->releases, 1);
        QCOMPARE(m_reapplies, 1);
        QVERIFY(m_swap->isSwapped());

        QCOMPARE(HotkeyBindings::sequence(HotkeyAction::CaptureArea), seq("Print"));
        QCOMPARE(HotkeyBindings::sequence(HotkeyAction::CaptureFullScreen), seq("Shift+Print"));

        const auto memento = ScreenshotKeyMemento::fromJson(Core::Settings::screenshotKeyMemento());
        QVERIFY(memento.has_value());
        QCOMPARE(memento->system, QStringLiteral("os-before"));
        QCOMPARE(memento->bindings.size(), 2);
        QCOMPARE(memento->bindings.at(0).action, HotkeyAction::CaptureArea);
        QCOMPARE(memento->bindings.at(0).sequence, hotkeyActionDefault(HotkeyAction::CaptureArea));
        QCOMPARE(memento->bindings.at(1).action, HotkeyAction::CaptureFullScreen);
        QCOMPARE(memento->bindings.at(1).sequence,
                 hotkeyActionDefault(HotkeyAction::CaptureFullScreen));
    }

    void secondSwapIsRefused()
    {
        QVERIFY(m_swap->swap().ok);
        const QString memento = Core::Settings::screenshotKeyMemento();

        const ScreenshotKey::Result again = m_swap->swap();
        QVERIFY(!again.ok);
        QVERIFY(!again.message.isEmpty());
        QCOMPARE(m_fake->releases, 1);
        QCOMPARE(Core::Settings::screenshotKeyMemento(), memento);
    }

    void failedReleaseChangesNothing()
    {
        m_fake->releaseResult = {false, {}, QStringLiteral("Denied.")};

        const ScreenshotKey::Result result = m_swap->swap();
        QVERIFY(!result.ok);
        QCOMPARE(result.message, QStringLiteral("Denied."));
        QVERIFY(!m_swap->isSwapped());
        QCOMPARE(m_reapplies, 0);
        for (const HotkeyAction a : allHotkeyActions())
            QVERIFY(!HotkeyBindings::isCustomized(a));
    }

    void presetCollisionIsRefused()
    {
        // Record Window is not in the preset, so it would share Print with Capture Area.
        HotkeyBindings::setSequence(HotkeyAction::RecordWindow, seq("Print"));

        const ScreenshotKey::Result result = m_swap->swap();
        QVERIFY(!result.ok);
        QVERIFY(result.message.contains(hotkeyActionDescription(HotkeyAction::RecordWindow)));
        QCOMPARE(m_fake->releases, 0);
        QCOMPARE(m_reapplies, 0);
        QVERIFY(!m_swap->isSwapped());
        QCOMPARE(HotkeyBindings::sequence(HotkeyAction::CaptureArea),
                 hotkeyActionDefault(HotkeyAction::CaptureArea));
    }

    void collisionWithAReassignedActionIsFine()
    {
        // The preset moves Capture Full Screen off Print, so nothing ends up shared.
        HotkeyBindings::setSequence(HotkeyAction::CaptureFullScreen, seq("Print"));

        QVERIFY(m_swap->swap().ok);
        QCOMPARE(HotkeyBindings::sequence(HotkeyAction::CaptureArea), seq("Print"));
        QCOMPARE(HotkeyBindings::sequence(HotkeyAction::CaptureFullScreen), seq("Shift+Print"));

        QVERIFY(m_swap->undo().ok);
        QCOMPARE(HotkeyBindings::sequence(HotkeyAction::CaptureFullScreen), seq("Print"));
    }

    void undoRestoresTheSystemAndTheBindings()
    {
        HotkeyBindings::setSequence(HotkeyAction::CaptureArea, seq("Meta+F9"));
        QVERIFY(m_swap->swap().ok);

        const ScreenshotKey::Result result = m_swap->undo();
        QVERIFY(result.ok);
        QCOMPARE(result.message, QStringLiteral("Given back."));
        QCOMPARE(m_fake->restored, QStringList{QStringLiteral("os-before")});
        QCOMPARE(m_reapplies, 2);
        QVERIFY(!m_swap->isSwapped());

        // The exact storage state comes back: one override, the default left unstored.
        QCOMPARE(HotkeyBindings::sequence(HotkeyAction::CaptureArea), seq("Meta+F9"));
        QVERIFY(HotkeyBindings::isCustomized(HotkeyAction::CaptureArea));
        QVERIFY(!HotkeyBindings::isCustomized(HotkeyAction::CaptureFullScreen));
    }

    void bindingEditedAfterTheSwapSurvivesUndo()
    {
        QVERIFY(m_swap->swap().ok);
        HotkeyBindings::setSequence(HotkeyAction::CaptureArea, seq("Meta+F10"));

        QVERIFY(m_swap->undo().ok);
        QCOMPARE(HotkeyBindings::sequence(HotkeyAction::CaptureArea), seq("Meta+F10"));
        QCOMPARE(HotkeyBindings::sequence(HotkeyAction::CaptureFullScreen),
                 hotkeyActionDefault(HotkeyAction::CaptureFullScreen));
    }

    void swapOutlastsTheInstance()
    {
        QVERIFY(m_swap->swap().ok);
        makeSwap();   // as after a restart

        QVERIFY(m_swap->isSwapped());
        QVERIFY(m_swap->undo().ok);
        QCOMPARE(m_fake->restored, QStringList{QStringLiteral("os-before")});
        QVERIFY(!m_swap->isSwapped());
    }

    void failedRestoreKeepsTheMemento()
    {
        QVERIFY(m_swap->swap().ok);
        const QString memento = Core::Settings::screenshotKeyMemento();
        m_fake->restoreResult = {false, {}, QStringLiteral("Busy.")};

        const ScreenshotKey::Result result = m_swap->undo();
        QVERIFY(!result.ok);
        QCOMPARE(result.message, QStringLiteral("Busy."));
        QVERIFY(m_swap->isSwapped());
        QCOMPARE(Core::Settings::screenshotKeyMemento(), memento);
        QCOMPARE(HotkeyBindings::sequence(HotkeyAction::CaptureArea), seq("Print"));
        QCOMPARE(m_reapplies, 1);
    }

    void undoWithoutASwapIsRefused()
    {
        const ScreenshotKey::Result result = m_swap->undo();
        QVERIFY(!result.ok);
        QVERIFY(!result.message.isEmpty());
        QVERIFY(m_fake->restored.isEmpty());
        QCOMPARE(m_reapplies, 0);
    }

    void unreadableMementoIsDropped()
    {
        Core::Settings::setScreenshotKeyMemento(QStringLiteral("garbage"));
        QVERIFY(m_swap->isSwapped());

        const ScreenshotKey::Result result = m_swap->undo();
        QVERIFY(!result.ok);
        QVERIFY(m_fake->restored.isEmpty());
        QVERIFY(!m_swap->isSwapped());   // so the swap can be offered again
    }

    void windowsMementoRoundTrips()
    {
        QCOMPARE(encodeSnippingSetting(std::nullopt), QStringLiteral("absent"));
        QCOMPARE(encodeSnippingSetting(0u), QStringLiteral("0"));
        QCOMPARE(encodeSnippingSetting(1u), QStringLiteral("1"));

        const QList<std::optional<quint32>> settings{std::nullopt, 0u, 1u, 2u, 4294967295u};
        for (const std::optional<quint32> &setting : settings) {
            const auto back = decodeSnippingSetting(encodeSnippingSetting(setting));
            QVERIFY(back.has_value());
            QVERIFY(*back == setting);
        }
    }

    void windowsMementoRejectsForeignText()
    {
        // Undo falls back to the OS default for these, so none may pass for a value.
        for (const char *text : {"", " 1", "1 ", "-1", "+1", "0x1", "on", "Absent", "4294967296"})
            QVERIFY2(!decodeSnippingSetting(QString::fromLatin1(text)).has_value(), text);
    }

    void nullKeyFallsBackToTheNullObject()
    {
        ScreenshotKeySwap swap(nullptr, {});
        QCOMPARE(swap.key().support(), ScreenshotKey::Support::Unsupported);
        QVERIFY(!swap.swap().ok);
        QVERIFY(!swap.isSwapped());
    }
};

QTEST_MAIN(tst_ScreenshotKey)
#include "tst_screenshotkey.moc"
