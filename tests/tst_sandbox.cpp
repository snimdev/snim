#include <QtTest>
#include <QFile>
#include <QTemporaryDir>

#include "core/Sandbox.h"

using namespace Core;

// The Flatpak check, driven through FLATPAK_ID and a stand-in for /.flatpak-info.
class tst_Sandbox : public QObject
{
    Q_OBJECT

    QTemporaryDir m_dir;

private slots:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
    }

    void init()
    {
        qunsetenv("FLATPAK_ID");   // a test run inside a Flatpak must not skew the results
    }

    void cleanup()
    {
        qunsetenv("FLATPAK_ID");
    }

    void notSandboxed_withoutEitherMarker()
    {
        QVERIFY(!Sandbox::isFlatpak(m_dir.filePath("missing-flatpak-info")));
    }

    void flatpak_whenTheAppIdIsSet()
    {
        qputenv("FLATPAK_ID", "dev.snim.Snim");
        QVERIFY(Sandbox::isFlatpak(m_dir.filePath("missing-flatpak-info")));
        QVERIFY(Sandbox::isFlatpak());
    }

    void flatpak_whenTheInfoFileExists()
    {
        const QString info = m_dir.filePath(".flatpak-info");
        QFile file(info);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("[Application]\nname=dev.snim.Snim\n");
        file.close();
        QVERIFY(Sandbox::isFlatpak(info));
    }
};

QTEST_MAIN(tst_Sandbox)
#include "tst_sandbox.moc"
