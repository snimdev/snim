#include <QtTest>
#include <QSet>

#include "hotkeys/HotkeyAction.h"
#include "hotkeys/MacKeyMapping.h"
#include "hotkeys/PortalKeyMapping.h"
#include "hotkeys/ScreenshotKey.h"
#include "hotkeys/WinKeyMapping.h"
#include "hotkeys/screenshotkey/UnsupportedScreenshotKey.h"

using namespace Hotkeys;

// The screenshot key: which strategy each system gets, the bindings Snim takes once the
// key is free, and the Null Object for systems that offer no way. Pure, no OS involved.
class tst_ScreenshotKey : public QObject
{
    Q_OBJECT

private slots:
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
};

QTEST_MAIN(tst_ScreenshotKey)
#include "tst_screenshotkey.moc"
