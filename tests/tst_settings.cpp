#include <QtTest>
#include <QStandardPaths>
#include <QSettings>
#include <QColor>

#include "core/Settings.h"

using namespace Core;

// Validates both the Core::Settings wrapper and the test harness itself
// (offscreen QApplication via QTEST_MAIN, test-mode QSettings isolation).
class tst_Settings : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName("NiceshotTest");
        QCoreApplication::setApplicationName("tst_settings");
        QStandardPaths::setTestModeEnabled(true);   // redirect QSettings to a throwaway store
        QSettings().clear();                          // start from a clean store
    }

    void imageFormat_default_then_roundtrip()
    {
        QCOMPARE(Settings::imageFormat(), QStringLiteral("png"));  // default
        Settings::setImageFormat("jpg");
        QCOMPARE(Settings::imageFormat(), QStringLiteral("jpg"));
    }

    void screenshotFolder_roundtrip()
    {
        Settings::setScreenshotFolder("/tmp/niceshot-shots");
        QCOMPARE(Settings::screenshotFolder(), QStringLiteral("/tmp/niceshot-shots"));
    }

    void editorColors_default_then_roundtrip()
    {
        QCOMPARE(Settings::editorForeground(), QColor(Qt::red));          // default
        QCOMPARE(Settings::editorBackground(), QColor(Qt::transparent));  // default
        Settings::setEditorForeground(QColor("#112233"));
        Settings::setEditorBackground(QColor("#44556677"));
        QCOMPARE(Settings::editorForeground(), QColor("#112233"));
        QCOMPARE(Settings::editorBackground(), QColor("#44556677"));
    }

    void backdrop_roundtrip()
    {
        const QString json = QStringLiteral("[{\"name\":\"X\",\"config\":{}}]");
        Settings::setBackdropPresetsJson(json);
        QCOMPARE(Settings::backdropPresetsJson(), json);

        Settings::setBackdropDefaultName("Indigo");
        QCOMPARE(Settings::backdropDefaultName(), QStringLiteral("Indigo"));
    }
};

QTEST_MAIN(tst_Settings)
#include "tst_settings.moc"
