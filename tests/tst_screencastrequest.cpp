#include <QtTest>

#include "screen/ScreenCastPortalSession.h"

using Screen::ScreenCastPortalSession;
using Source = ScreenCastPortalSession::Source;

// What Snim asks the ScreenCast portal for. The picker itself is the desktop's, so the
// request is all a test can pin down; choosing a window there is checked by hand.
class tst_ScreencastRequest : public QObject
{
    Q_OBJECT

private slots:
    void asksForAMonitorByDefault()
    {
        const QVariantMap selection =
            ScreenCastPortalSession::sourceSelection({}, {}, QString());
        QCOMPARE(selection.value(QStringLiteral("types")).toUInt(), 1u);
        QCOMPARE(selection.value(QStringLiteral("multiple")).toBool(), false);
        QCOMPARE(selection.value(QStringLiteral("cursor_mode")).toUInt(), 1u);   // hidden
        QVERIFY(!selection.contains(QStringLiteral("persist_mode")));
    }

    void asksForAWindowAlone()
    {
        ScreenCastPortalSession::Options options;
        options.source = Source::Window;
        options.captureCursor = true;
        const QVariantMap selection =
            ScreenCastPortalSession::sourceSelection(options, {5, 3, 7}, QString());
        QCOMPARE(selection.value(QStringLiteral("types")).toUInt(), 2u);
        QCOMPARE(selection.value(QStringLiteral("cursor_mode")).toUInt(), 2u);   // embedded
    }

    void aRecordingNeverPersistsItsChoice()
    {
        // No token key: the picker shows every time, whatever the portal could restore.
        ScreenCastPortalSession::Options options;
        options.source = Source::Window;
        const QVariantMap selection = ScreenCastPortalSession::sourceSelection(
            options, {5, 3, 7}, QStringLiteral("stale"));
        QVERIFY(!selection.contains(QStringLiteral("persist_mode")));
        QVERIFY(!selection.contains(QStringLiteral("restore_token")));
    }

    void aCaptureSessionRestoresItsMonitors()
    {
        ScreenCastPortalSession::Options options;
        options.multiple = true;
        options.restoreTokenKey = QStringLiteral("capture/screencastRestoreToken");
        const QVariantMap selection = ScreenCastPortalSession::sourceSelection(
            options, {4, 1, 1}, QStringLiteral("abc"));
        QCOMPARE(selection.value(QStringLiteral("types")).toUInt(), 1u);
        QCOMPARE(selection.value(QStringLiteral("persist_mode")).toUInt(), 2u);
        QCOMPARE(selection.value(QStringLiteral("restore_token")).toString(),
                 QStringLiteral("abc"));
        // Before version 4 the portal knows neither option.
        QVERIFY(!ScreenCastPortalSession::sourceSelection(options, {3, 1, 1},
                                                          QStringLiteral("abc"))
                     .contains(QStringLiteral("persist_mode")));
    }

    void anUnavailableCursorModeFallsBackToHidden()
    {
        ScreenCastPortalSession::Options options;
        options.captureCursor = true;
        const QVariantMap selection =
            ScreenCastPortalSession::sourceSelection(options, {5, 1, 1}, QString());
        QCOMPARE(selection.value(QStringLiteral("cursor_mode")).toUInt(), 1u);
    }

    void onlyAPortalThatOffersWindowsSharesOne()
    {
        QVERIFY(ScreenCastPortalSession::offers(Source::Window, {5, 3, 0}));
        QVERIFY(ScreenCastPortalSession::offers(Source::Window, {5, 2, 0}));
        QVERIFY(!ScreenCastPortalSession::offers(Source::Window, {5, 1, 0}));
        QVERIFY(ScreenCastPortalSession::offers(Source::Monitor, {5, 1, 0}));
        // A portal that does not say is asked anyway.
        QVERIFY(ScreenCastPortalSession::offers(Source::Window, {}));
    }
};

QTEST_MAIN(tst_ScreencastRequest)
#include "tst_screencastrequest.moc"
