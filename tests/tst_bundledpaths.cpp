#include <QtTest>

#include "core/BundledPaths.h"

using namespace Core;

// Self-location of the bundled Tesseract data for a relocated install. Pure string work,
// so no bundle is needed.
class tst_BundledPaths : public QObject
{
    Q_OBJECT

private slots:
    void computesPathsRelativeToTheBinary();
    void claimsNothingForASystemInstall();
    void computesTheWindowsLayout();
    void picksTheLayoutForThisPlatform();
};

void tst_BundledPaths::computesPathsRelativeToTheBinary()
{
    QCOMPARE(BundledPaths::forBinaryDir(QStringLiteral("/opt/snim/usr/bin")).tessdataDir,
             QStringLiteral("/opt/snim/usr/share/tessdata"));
    // The Flatpak's /app is just another prefix.
    QCOMPARE(BundledPaths::forBinaryDir(QStringLiteral("/app/bin")).tessdataDir,
             QStringLiteral("/app/share/tessdata"));
}

void tst_BundledPaths::claimsNothingForASystemInstall()
{
    for (const QString &binDir : {QStringLiteral("/usr/bin"), QStringLiteral("/bin"),
                                  QStringLiteral("/usr/bin/")}) {
        QVERIFY2(BundledPaths::forBinaryDir(binDir).tessdataDir.isEmpty(), qPrintable(binDir));
    }
}

void tst_BundledPaths::computesTheWindowsLayout()
{
    QCOMPARE(BundledPaths::forWindowsBinaryDir(QStringLiteral("C:/Program Files/Snim")).tessdataDir,
             QStringLiteral("C:/Program Files/Snim/tessdata"));
}

void tst_BundledPaths::picksTheLayoutForThisPlatform()
{
    const QString binDir = QStringLiteral("/opt/snim/usr/bin");
    const BundledPaths::Paths paths = BundledPaths::forThisPlatform(binDir);
#ifdef Q_OS_WIN
    QCOMPARE(paths.tessdataDir, BundledPaths::forWindowsBinaryDir(binDir).tessdataDir);
#else
    QCOMPARE(paths.tessdataDir, BundledPaths::forBinaryDir(binDir).tessdataDir);
#endif
}

QTEST_MAIN(tst_BundledPaths)
#include "tst_bundledpaths.moc"
