#include <QtTest>

#include "screen/ScreenCastPortalSession.h"
#include "screen/sources/ScreencastFrameSource.h"

using Screen::ScreenCastPortalSession;
using Screen::ScreencastFrameSource;

// Screenshot consent lives under its own key; a session without one persists nothing.
class tst_ScreencastToken : public QObject
{
    Q_OBJECT

    const QString m_captureKey = QString::fromLatin1(ScreencastFrameSource::kRestoreTokenKey);

private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("SnimTest"));
        QCoreApplication::setApplicationName(QStringLiteral("tst_screencasttoken"));
        QStandardPaths::setTestModeEnabled(true);
    }

    void init()
    {
        QSettings().clear();
    }

    void theScreenshotKeyIsUnderCapture()
    {
        QVERIFY(m_captureKey.startsWith(QStringLiteral("capture/")));
    }

    void storesTheTokenStartHandsBack()
    {
        ScreenCastPortalSession::storeRestoreToken(
            m_captureKey, {{QStringLiteral("restore_token"), QStringLiteral("abc")}});
        QCOMPARE(ScreenCastPortalSession::restoreToken(m_captureKey), QStringLiteral("abc"));
        QVERIFY(ScreencastFrameSource::hasRestoreToken());
    }

    void aStartWithoutTokenForgetsTheOldOne()
    {
        QSettings().setValue(m_captureKey, QStringLiteral("old"));
        ScreenCastPortalSession::storeRestoreToken(m_captureKey, {});
        QVERIFY(!ScreencastFrameSource::hasRestoreToken());
    }

    void theSettingsResetForgetsThePick()
    {
        QSettings().setValue(m_captureKey, QStringLiteral("screens"));
        QVERIFY(ScreencastFrameSource::remembersScreenPick());
        ScreencastFrameSource::forgetScreenPicks();
        QVERIFY(!ScreencastFrameSource::remembersScreenPick());
        QVERIFY(!QSettings().contains(m_captureKey));
    }

    void anEmptyKeyPersistsNothing()
    {
        ScreenCastPortalSession::storeRestoreToken(
            QString(), {{QStringLiteral("restore_token"), QStringLiteral("abc")}});
        QVERIFY(QSettings().allKeys().isEmpty());
        QVERIFY(ScreenCastPortalSession::restoreToken(QString()).isEmpty());
    }
};

QTEST_MAIN(tst_ScreencastToken)
#include "tst_screencasttoken.moc"
