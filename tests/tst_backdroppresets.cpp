#include <QtTest>
#include <QStandardPaths>
#include <QSettings>
#include <QVariantMap>
#include <QPixmap>

#include "editor/image/BackdropPresets.h"
#include "editor/image/BackdropItem.h"

using namespace Editor::Image;

class tst_BackdropPresets : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        QCoreApplication::setOrganizationName("SnimTest");
        QCoreApplication::setApplicationName("tst_backdroppresets");
        QStandardPaths::setTestModeEnabled(true);
        QSettings().clear();   // fresh store each test (no leftover user presets)
    }

    void builtinsPresentAndFirst()
    {
        const QVector<BackdropPreset> all = BackdropPresets::all();
        QVERIFY(all.size() >= 4);
        for (int i = 0; i < 4; ++i)
            QVERIFY(all[i].builtin);   // built-ins come first
        QVERIFY(BackdropPresets::isBuiltin("Indigo"));
        QVERIFY(!BackdropPresets::isBuiltin("Nope"));
    }

    void saveAndConfigForRoundTrip()
    {
        QVariantMap cfg{{"fill", "Solid"}, {"solidColor", "#101010"}, {"padding", 33}};
        BackdropPresets::save("My Preset", cfg);

        QVERIFY(!BackdropPresets::isBuiltin("My Preset"));
        QCOMPARE(BackdropPresets::configFor("My Preset").value("padding").toInt(), 33);

        bool found = false;
        for (const BackdropPreset &p : BackdropPresets::all())
            if (p.name == "My Preset") { found = true; QVERIFY(!p.builtin); }
        QVERIFY(found);
    }

    void saveDoesNotOverwriteBuiltin()
    {
        const int origPadding = BackdropPresets::configFor("Indigo").value("padding").toInt();
        BackdropPresets::save("Indigo", {{"padding", 9999}});   // must be ignored
        QCOMPARE(BackdropPresets::configFor("Indigo").value("padding").toInt(), origPadding);
    }

    void removeUserPreset()
    {
        BackdropPresets::save("Temp", {{"padding", 5}});
        QVERIFY(!BackdropPresets::configFor("Temp").isEmpty());
        BackdropPresets::remove("Temp");
        QVERIFY(BackdropPresets::configFor("Temp").isEmpty());
    }

    void defaultGetSetAndClearedOnRemove()
    {
        QVERIFY(BackdropPresets::defaultName().isEmpty());
        BackdropPresets::setDefault("Indigo");
        QCOMPARE(BackdropPresets::defaultName(), QStringLiteral("Indigo"));
        QCOMPARE(BackdropPresets::defaultConfig(), BackdropPresets::configFor("Indigo"));

        BackdropPresets::save("Dft", {{"padding", 7}});
        BackdropPresets::setDefault("Dft");
        BackdropPresets::remove("Dft");                 // removing the default clears it
        QVERIFY(BackdropPresets::defaultName().isEmpty());
    }

    void backdropItem_configRoundTrip()
    {
        const QVariantMap indigo = BackdropPresets::configFor("Indigo");
        BackdropItem item;
        item.applyConfig(indigo);
        const QVariantMap out = item.toConfig();
        // The fields Indigo specifies must survive the round-trip.
        QCOMPARE(out.value("fill").toString(), QStringLiteral("Gradient"));
        QCOMPARE(out.value("gradientIndex").toInt(), indigo.value("gradientIndex").toInt());
        QCOMPARE(out.value("padding").toInt(), indigo.value("padding").toInt());
        QCOMPARE(out.value("cornerRadius").toInt(), indigo.value("cornerRadius").toInt());
        QCOMPARE(out.value("shadowStrength").toInt(), indigo.value("shadowStrength").toInt());
    }

    void configPreview_rendersRequestedSize()
    {
        const QPixmap pm = BackdropItem::configPreview(
            BackdropPresets::configFor("Indigo"), QSize(58, 40));
        QVERIFY(!pm.isNull());
        QCOMPARE(pm.size(), QSize(58, 40));
    }
};

QTEST_MAIN(tst_BackdropPresets)
#include "tst_backdroppresets.moc"
