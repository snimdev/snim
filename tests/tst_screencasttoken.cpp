#include <QtTest>

#include "capture/strategies/ScreencastCaptureStrategy.h"
#include "screen/ScreenCastPortalSession.h"

using Screen::ScreenCastPortalSession;

// Screenshot consent must never restore, or be wiped by, the recorder's session.
class tst_ScreencastToken : public QObject
{
    Q_OBJECT

    const QString m_captureKey = QString::fromLatin1(Capture::ScreencastCaptureStrategy::kRestoreTokenKey);
    const QString m_recordingKey = QStringLiteral("recording/screencastRestoreToken");

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

    void captureAndRecordingUseDifferentKeys()
    {
        QVERIFY(m_captureKey != m_recordingKey);
        QVERIFY(m_captureKey.startsWith(QStringLiteral("capture/")));
    }

    void storesTheTokenStartHandsBack()
    {
        ScreenCastPortalSession::storeRestoreToken(
            m_captureKey, {{QStringLiteral("restore_token"), QStringLiteral("abc")}});
        QCOMPARE(ScreenCastPortalSession::restoreToken(m_captureKey), QStringLiteral("abc"));
        QVERIFY(Capture::ScreencastCaptureStrategy::hasRestoreToken());
        QVERIFY(ScreenCastPortalSession::restoreToken(m_recordingKey).isEmpty());
    }

    void aStartWithoutTokenForgetsTheOldOne()
    {
        QSettings().setValue(m_captureKey, QStringLiteral("old"));
        QSettings().setValue(m_recordingKey, QStringLiteral("recorder"));
        ScreenCastPortalSession::storeRestoreToken(m_captureKey, {});
        QVERIFY(!Capture::ScreencastCaptureStrategy::hasRestoreToken());
        QCOMPARE(QSettings().value(m_recordingKey).toString(), QStringLiteral("recorder"));
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
