#include <QtTest>

#include "screen/WinWindowFilter.h"

using namespace Screen;
using WinWindowFilter::Candidate;

// Which top-level windows the Windows picker offers. The Win32 reads stay in
// WindowEnumerator_win; the decision is plain data, so it runs on every host.
class tst_WinWindowFilter : public QObject
{
    Q_OBJECT

    static constexpr quint32 kOwnPid = 100;

    static Candidate appWindow()
    {
        Candidate w;
        w.visible = true;
        w.className = QStringLiteral("Notepad");
        w.processId = 200;
        w.bounds = QRect(100, 100, 800, 600);
        return w;
    }

private slots:
    void offersAnOrdinaryAppWindow()
    {
        QVERIFY(WinWindowFilter::isPickable(appWindow(), kOwnPid));
    }

    void skipsHiddenMinimisedAndCloaked()
    {
        Candidate hidden = appWindow();
        hidden.visible = false;
        QVERIFY(!WinWindowFilter::isPickable(hidden, kOwnPid));

        Candidate minimised = appWindow();
        minimised.minimised = true;
        QVERIFY(!WinWindowFilter::isPickable(minimised, kOwnPid));

        // Visible to Win32 yet not drawn: another virtual desktop, a suspended UWP app.
        Candidate cloaked = appWindow();
        cloaked.cloaked = true;
        QVERIFY(!WinWindowFilter::isPickable(cloaked, kOwnPid));
    }

    void skipsToolWindows()
    {
        Candidate tool = appWindow();
        tool.toolWindow = true;
        QVERIFY(!WinWindowFilter::isPickable(tool, kOwnPid));
    }

    void skipsTheDesktop()
    {
        for (const char *cls : { "Progman", "WorkerW" }) {
            Candidate desktop = appWindow();
            desktop.className = QString::fromLatin1(cls);
            QVERIFY2(!WinWindowFilter::isPickable(desktop, kOwnPid), cls);
        }
    }

    void skipsOurOwnWindows()
    {
        Candidate own = appWindow();
        own.processId = kOwnPid;
        QVERIFY(!WinWindowFilter::isPickable(own, kOwnPid));
    }

    void skipsZeroSizeWindows()
    {
        Candidate empty = appWindow();
        empty.bounds = QRect(100, 100, 0, 600);
        QVERIFY(!WinWindowFilter::isPickable(empty, kOwnPid));
        empty.bounds = QRect();
        QVERIFY(!WinWindowFilter::isPickable(empty, kOwnPid));
    }
};

QTEST_MAIN(tst_WinWindowFilter)
#include "tst_winwindowfilter.moc"
