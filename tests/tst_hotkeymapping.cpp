#include <QtTest>
#include <QStandardPaths>

#include "hotkeys/MacKeyMapping.h"
#include "hotkeys/PortalKeyMapping.h"
#include "hotkeys/WinKeyMapping.h"

using namespace Hotkeys;

// The three key mappers. All pure and compiled everywhere, so the Windows and portal
// tables are checked on this macOS host too - a wrong constant here is a hotkey that
// silently fires the wrong action on a platform nobody develops on.
class tst_HotkeyMapping : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName("SnimTest");
        QCoreApplication::setApplicationName("tst_hotkeymapping");
        QStandardPaths::setTestModeEnabled(true);
    }

    // --- macOS / Carbon ------------------------------------------------------

    void macMapsQtControlToCommand()
    {
        const auto hk = toCarbonHotkey(QKeySequence("Ctrl+Shift+A"));
        QVERIFY(hk.has_value());
        QCOMPARE(hk->keyCode, quint32(0x00));                    // kVK_ANSI_A
        // Pins the deliberate swap: Qt maps Qt::ControlModifier to the COMMAND key on
        // macOS, so the app's "Ctrl+Shift+A" must register as cmdKey|shiftKey. Mapping
        // it to controlKey would register a hotkey the user can never press.
        QCOMPARE(hk->modifiers, quint32(0x0100 | 0x0200));
    }

    void macMapsMetaToTheControlKey()
    {
        const auto hk = toCarbonHotkey(QKeySequence("Meta+Z"));
        QVERIFY(hk.has_value());
        QCOMPARE(hk->keyCode, quint32(0x06));                    // kVK_ANSI_Z
        QCOMPARE(hk->modifiers, quint32(0x1000));                // controlKey
    }

    void macSpotChecksAcrossTheTable()
    {
        QCOMPARE(toCarbonHotkey(QKeySequence("Ctrl+5"))->keyCode, quint32(0x17));   // digits are not in order
        QCOMPARE(toCarbonHotkey(QKeySequence("Ctrl+F5"))->keyCode, quint32(0x60));
        QCOMPARE(toCarbonHotkey(QKeySequence("Ctrl+F1"))->keyCode, quint32(0x7A));
        QCOMPARE(toCarbonHotkey(QKeySequence("Ctrl+Left"))->keyCode, quint32(0x7B));
        QCOMPARE(toCarbonHotkey(QKeySequence("Ctrl+Up"))->keyCode, quint32(0x7E));
        QCOMPARE(toCarbonHotkey(QKeySequence("Ctrl+/"))->keyCode, quint32(0x2C));
        QCOMPARE(toCarbonHotkey(QKeySequence("Ctrl+Space"))->keyCode, quint32(0x31));
        // Backspace is kVK_Delete; the forward-delete key is a different code.
        QCOMPARE(toCarbonHotkey(QKeySequence("Ctrl+Backspace"))->keyCode, quint32(0x33));
        QCOMPARE(toCarbonHotkey(QKeySequence("Ctrl+Del"))->keyCode, quint32(0x75));

        // Every modifier at once.
        const auto all = toCarbonHotkey(QKeySequence("Ctrl+Alt+Shift+Meta+A"));
        QVERIFY(all.has_value());
        QCOMPARE(all->modifiers, quint32(0x0100 | 0x0200 | 0x0800 | 0x1000));
    }

    void macUnmappableIsNullopt()
    {
        QVERIFY(!toCarbonHotkey(QKeySequence()).has_value());
        QVERIFY(!toCarbonHotkey(QKeySequence("")).has_value());
        QVERIFY(!toCarbonHotkey(QKeySequence("Ctrl+Shift")).has_value());   // modifiers only
        QVERIFY(!toCarbonHotkey(QKeySequence("Ctrl+F30")).has_value());     // no kVK_ code
        QVERIFY(!toCarbonHotkey(QKeySequence("Ctrl+Insert")).has_value());
    }

    // --- Windows -------------------------------------------------------------

    void winMapsLettersAndModifiers()
    {
        const auto hk = toWinHotkey(QKeySequence("Ctrl+Shift+A"));
        QVERIFY(hk.has_value());
        QCOMPARE(hk->virtualKey, quint32(0x41));                      // 'A'
        QCOMPARE(hk->modifiers, quint32(0x2 | 0x4));                  // MOD_CONTROL|MOD_SHIFT
        // MOD_NOREPEAT is the backend's business, so it must not appear here.
        QCOMPARE(hk->modifiers & 0x4000u, quint32(0));

        const auto meta = toWinHotkey(QKeySequence("Meta+Alt+Z"));
        QVERIFY(meta.has_value());
        QCOMPARE(meta->virtualKey, quint32(0x5A));                    // 'Z'
        QCOMPARE(meta->modifiers, quint32(0x8 | 0x1));                // MOD_WIN|MOD_ALT
    }

    void winSpotChecksAcrossTheTable()
    {
        QCOMPARE(toWinHotkey(QKeySequence("Ctrl+5"))->virtualKey, quint32(0x35));
        QCOMPARE(toWinHotkey(QKeySequence("Ctrl+F5"))->virtualKey, quint32(0x74));   // VK_F5
        QCOMPARE(toWinHotkey(QKeySequence("Ctrl+F12"))->virtualKey, quint32(0x7B));  // VK_F12
        QCOMPARE(toWinHotkey(QKeySequence("Ctrl+Left"))->virtualKey, quint32(0x25));
        QCOMPARE(toWinHotkey(QKeySequence("Ctrl+Up"))->virtualKey, quint32(0x26));
        QCOMPARE(toWinHotkey(QKeySequence("Ctrl+Right"))->virtualKey, quint32(0x27));
        QCOMPARE(toWinHotkey(QKeySequence("Ctrl+Down"))->virtualKey, quint32(0x28));
        QCOMPARE(toWinHotkey(QKeySequence("Ctrl+Space"))->virtualKey, quint32(0x20));
        QCOMPARE(toWinHotkey(QKeySequence("Ctrl+/"))->virtualKey, quint32(0xBF));    // VK_OEM_2
        QCOMPARE(toWinHotkey(QKeySequence("Ctrl+,"))->virtualKey, quint32(0xBC));    // VK_OEM_COMMA
    }

    void winMapsPrintScreenAndFriends()
    {
        // Bare Print Screen is the classic screenshot key, so no modifiers must survive.
        const auto print = toWinHotkey(QKeySequence(Qt::Key_Print));
        QVERIFY(print.has_value());
        QCOMPARE(print->virtualKey, quint32(0x2C));                   // VK_SNAPSHOT
        QCOMPARE(print->modifiers, quint32(0));

        QCOMPARE(toWinHotkey(QKeySequence(Qt::ShiftModifier | Qt::Key_Print))->modifiers,
                 quint32(0x4));
        QCOMPARE(toWinHotkey(QKeySequence("Ctrl+Insert"))->virtualKey, quint32(0x2D));   // VK_INSERT
        QCOMPARE(toWinHotkey(QKeySequence(Qt::Key_Pause))->virtualKey, quint32(0x13));  // VK_PAUSE
        QCOMPARE(toWinHotkey(QKeySequence(Qt::Key_ScrollLock))->virtualKey, quint32(0x91));  // VK_SCROLL
    }

    void winMapsKeypadKeysApart()
    {
        const auto kp5 = toWinHotkey(QKeySequence(Qt::ControlModifier | Qt::KeypadModifier | Qt::Key_5));
        QVERIFY(kp5.has_value());
        QCOMPARE(kp5->virtualKey, quint32(0x65));                     // VK_NUMPAD5
        // KeypadModifier picks the key; it is not a RegisterHotKey modifier.
        QCOMPARE(kp5->modifiers, quint32(0x2));

        QCOMPARE(toWinHotkey(QKeySequence(Qt::KeypadModifier | Qt::Key_0))->virtualKey, quint32(0x60));
        QCOMPARE(toWinHotkey(QKeySequence(Qt::KeypadModifier | Qt::Key_9))->virtualKey, quint32(0x69));
        QCOMPARE(toWinHotkey(QKeySequence(Qt::KeypadModifier | Qt::Key_Asterisk))->virtualKey, quint32(0x6A));
        QCOMPARE(toWinHotkey(QKeySequence(Qt::KeypadModifier | Qt::Key_Plus))->virtualKey, quint32(0x6B));
        QCOMPARE(toWinHotkey(QKeySequence(Qt::KeypadModifier | Qt::Key_Minus))->virtualKey, quint32(0x6D));
        QCOMPARE(toWinHotkey(QKeySequence(Qt::KeypadModifier | Qt::Key_Period))->virtualKey, quint32(0x6E));
        QCOMPARE(toWinHotkey(QKeySequence(Qt::KeypadModifier | Qt::Key_Slash))->virtualKey, quint32(0x6F));
        // Keypad Enter and Num Lock off navigation share the main-block VK.
        QCOMPARE(toWinHotkey(QKeySequence(Qt::KeypadModifier | Qt::Key_Enter))->virtualKey, quint32(0x0D));
        QCOMPARE(toWinHotkey(QKeySequence(Qt::KeypadModifier | Qt::Key_Home))->virtualKey, quint32(0x24));

        // The main-row keys stay where they were.
        QCOMPARE(toWinHotkey(QKeySequence("Ctrl+5"))->virtualKey, quint32(0x35));
        QCOMPARE(toWinHotkey(QKeySequence("Ctrl+-"))->virtualKey, quint32(0xBD));    // VK_OEM_MINUS
    }

    void winUnmappableIsNullopt()
    {
        QVERIFY(!toWinHotkey(QKeySequence()).has_value());
        QVERIFY(!toWinHotkey(QKeySequence("")).has_value());
        QVERIFY(!toWinHotkey(QKeySequence("Ctrl+Shift")).has_value());   // modifiers only
        QVERIFY(!toWinHotkey(QKeySequence(Qt::ControlModifier | Qt::Key_F25)).has_value());   // past VK_F24
    }

    // --- Linux / xdg-desktop-portal -----------------------------------------

    void portalTriggerSpelling()
    {
        QCOMPARE(toPortalTrigger(QKeySequence("Ctrl+Shift+A")), QStringLiteral("CTRL+SHIFT+a"));
        QCOMPARE(toPortalTrigger(QKeySequence("Ctrl+F5")), QStringLiteral("CTRL+F5"));
        QCOMPARE(toPortalTrigger(QKeySequence("Ctrl+7")), QStringLiteral("CTRL+7"));
        QCOMPARE(toPortalTrigger(QKeySequence("Meta+Left")), QStringLiteral("LOGO+Left"));
        // Fixed modifier order regardless of how the sequence was spelled.
        QCOMPARE(toPortalTrigger(QKeySequence("Alt+Shift+Ctrl+B")),
                 QStringLiteral("CTRL+SHIFT+ALT+b"));
    }

    void portalUnmappableIsEmpty()
    {
        QVERIFY(toPortalTrigger(QKeySequence()).isEmpty());
        QVERIFY(toPortalTrigger(QKeySequence("")).isEmpty());
        QVERIFY(toPortalTrigger(QKeySequence("Ctrl+Insert")).isEmpty());
    }
};

QTEST_MAIN(tst_HotkeyMapping)
#include "tst_hotkeymapping.moc"
