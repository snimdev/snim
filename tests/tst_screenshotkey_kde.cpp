#include <QtTest>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusVirtualObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QSet>
#include <QStandardPaths>
#include <QThread>

#include "hotkeys/screenshotkey/KdeScreenshotKey.h"

using namespace Hotkeys;

namespace {

const QString kService = QStringLiteral("org.kde.kglobalaccel");
const QString kPath = QStringLiteral("/kglobalaccel");
const QString kFakeConnection = QStringLiteral("snim-fake-kglobalaccel");
const QString kSpectacle = QStringLiteral("org.kde.spectacle.desktop");
const QString kSnim = QStringLiteral("dev.snim.Snim");

using Keys = QList<QList<int>>;

int combined(const char *text)
{
    return QKeySequence(QString::fromLatin1(text))[0].toCombined();
}

const int kPrint = Qt::Key_Print;
const int kMetaShiftS = combined("Meta+Shift+S");
const int kCtrlPrint = combined("Ctrl+Print");
const int kCtrlShiftPrint = combined("Ctrl+Shift+Print");

} // namespace

// A kglobalaccel that keeps its shortcuts in memory, on its own connection and thread so
// the strategy's blocking calls are answered while the test thread waits in them. Like
// the real daemon it ignores an id that is not four strings and drops a key another
// shortcut already holds.
class FakeKGlobalAccel : public QDBusVirtualObject
{
    Q_OBJECT

public:
    struct Shortcut {
        QString component;
        QString componentFriendly;
        QString action;
        QString actionFriendly;
        Keys keys;
    };

    void add(const Shortcut &shortcut)
    {
        QMutexLocker lock(&m_mutex);
        m_shortcuts.append(shortcut);
    }

    // Its setForeignShortcutKeys calls change nothing, like a daemon that refuses them.
    void freeze(const QString &component)
    {
        QMutexLocker lock(&m_mutex);
        m_frozen.insert(component);
    }

    Keys keys(const QString &component, const QString &action) const
    {
        QMutexLocker lock(&m_mutex);
        for (const Shortcut &s : m_shortcuts) {
            if (s.component == component && s.action == action)
                return s.keys;
        }
        return {};
    }

    QStringList sets() const
    {
        QMutexLocker lock(&m_mutex);
        return m_sets;
    }

    QString introspect(const QString &) const override { return {}; }

    bool handleMessage(const QDBusMessage &message, const QDBusConnection &connection) override
    {
        QMutexLocker lock(&m_mutex);
        const QVariantList args = message.arguments();
        const QString member = message.member();
        auto reply = [&](const QVariant &value) {
            connection.send(message.createReply(QVariantList{value}));
        };

        if (member == QLatin1String("getGlobalShortcutsByKey")) {
            const QList<int> key{args.value(0).toInt()};
            QList<KGlobalAccelShortcut> found;
            for (const Shortcut &s : std::as_const(m_shortcuts)) {
                if (!s.keys.contains(key))
                    continue;
                QList<int> firstChords;
                for (const QList<int> &k : s.keys)
                    firstChords.append(k.value(0));
                found.append({s.action, s.actionFriendly, s.component, s.componentFriendly,
                              QStringLiteral("default"), QStringLiteral("Default Context"),
                              firstChords, {}});
            }
            reply(QVariant::fromValue(found));
        } else if (member == QLatin1String("allActionsForComponent")) {
            const QString component = qdbus_cast<QStringList>(args.value(0)).value(0);
            QList<QStringList> ids;
            for (const Shortcut &s : std::as_const(m_shortcuts)) {
                if (s.component == component)
                    ids.append({s.component, s.action, s.componentFriendly, s.actionFriendly});
            }
            reply(QVariant::fromValue(ids));
        } else if (member == QLatin1String("shortcutKeys")) {
            QList<KGlobalAccelKey> wire;
            if (const Shortcut *s = find(qdbus_cast<QStringList>(args.value(0)))) {
                for (QList<int> key : s->keys) {
                    key.resize(4, 0);
                    wire.append({key});
                }
            }
            reply(QVariant::fromValue(wire));
        } else if (member == QLatin1String("setForeignShortcutKeys")) {
            Shortcut *s = find(qdbus_cast<QStringList>(args.value(0)));
            if (s && !m_frozen.contains(s->component)) {
                m_sets << s->component + QLatin1Char('/') + s->action;
                Keys kept;
                for (const KGlobalAccelKey &wire :
                     qdbus_cast<QList<KGlobalAccelKey>>(args.value(1))) {
                    QList<int> key = wire.chords;
                    key.removeAll(0);
                    if (!key.isEmpty() && !takenByAnother(key, s))
                        kept.append(key);
                }
                s->keys = kept;
            }
            connection.send(message.createReply());
        } else {
            connection.send(message.createErrorReply(
                QStringLiteral("org.freedesktop.DBus.Error.UnknownMethod"), member));
        }
        return true;
    }

private:
    Shortcut *find(const QStringList &id)
    {
        if (id.size() != 4)
            return nullptr;
        for (Shortcut &s : m_shortcuts) {
            if (s.component == id.at(0) && s.action == id.at(1))
                return &s;
        }
        return nullptr;
    }

    bool takenByAnother(const QList<int> &key, const Shortcut *self) const
    {
        for (const Shortcut &s : m_shortcuts) {
            if (&s != self && s.keys.contains(key))
                return true;
        }
        return false;
    }

    mutable QMutex m_mutex;
    QList<Shortcut> m_shortcuts;
    QSet<QString> m_frozen;
    QStringList m_sets;
};

// The KDE screenshot key against a fake kglobalaccel on a private session bus (ctest wraps
// this in dbus-run-session with nothing activatable), never the desktop's own shortcuts.
class tst_ScreenshotKeyKde : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        if (qEnvironmentVariable("SNIM_TEST_PRIVATE_BUS") != QLatin1String("1"))
            QSKIP("needs the private session bus ctest starts (SNIM_TEST_PRIVATE_BUS=1)");
        QDBusConnectionInterface *bus = QDBusConnection::sessionBus().interface();
        if (!bus || bus->isServiceRegistered(kService))
            QSKIP("no private session bus, or it already has a kglobalaccel; refusing to touch it");
        QStandardPaths::setTestModeEnabled(true);
        KdeScreenshotKey::registerMetaTypes();
    }

    void cleanup()
    {
        if (!m_fake)
            return;
        QMetaObject::invokeMethod(m_fake, [] {
            QDBusConnection fake(kFakeConnection);
            fake.unregisterService(kService);
            fake.unregisterObject(kPath);
            QDBusConnection::disconnectFromBus(kFakeConnection);
        }, Qt::BlockingQueuedConnection);
        m_thread->quit();
        m_thread->wait();
        delete m_fake;
        delete m_thread;
        m_fake = nullptr;
        m_thread = nullptr;
        QTRY_VERIFY(!QDBusConnection::sessionBus().interface()->isServiceRegistered(kService));
    }

    void noServiceIsUnsupported()
    {
        KdeScreenshotKey key;
        QCOMPARE(key.support(), ScreenshotKey::Support::Unsupported);
        QVERIFY(key.holderOf(QKeySequence(Qt::Key_Print)).isEmpty());
        QCOMPARE(key.ownerName(), QStringLiteral("Spectacle"));

        const ScreenshotKey::Result released = key.release();
        QVERIFY(!released.ok);
        QVERIFY(released.memento.isEmpty());
        QVERIFY(!released.message.isEmpty());
        QVERIFY(!key.restore(QStringLiteral(R"({"holders":[],"snim":null})")).ok);
    }

    void holderOfAnyKey()
    {
        startFake();
        addSpectacle();
        addSnim();
        m_fake->add({QStringLiteral("kwin"), QStringLiteral("KWin"), QStringLiteral("Overview"),
                     QStringLiteral("Toggle Overview"), {{combined("Meta+W")}}});

        KdeScreenshotKey key;
        QCOMPARE(key.support(), ScreenshotKey::Support::Automatic);
        QCOMPARE(key.keyName(), QStringLiteral("Print Screen"));
        QCOMPARE(key.holderOf(QKeySequence(Qt::Key_Print)), QStringLiteral("Spectacle"));
        QCOMPARE(key.holderOf(QKeySequence(QStringLiteral("Meta+Shift+S"))),
                 QStringLiteral("Spectacle"));
        QCOMPARE(key.holderOf(QKeySequence(QStringLiteral("Meta+W"))), QStringLiteral("KWin"));
        // Snim's own shortcut is no clash, and neither is a free key.
        QVERIFY(key.holderOf(QKeySequence(QStringLiteral("Ctrl+Print"))).isEmpty());
        QVERIFY(key.holderOf(QKeySequence(QStringLiteral("Ctrl+F12"))).isEmpty());
        QVERIFY(key.holderOf(QKeySequence(QStringLiteral("Meta+W, Meta+X"))).isEmpty());
        QCOMPARE(key.ownerName(), QStringLiteral("Spectacle"));

        const QList<HotkeyBinding> preset = key.preset();
        QCOMPARE(preset.size(), 1);
        QCOMPARE(preset.at(0).sequence, QKeySequence(Qt::Key_Print));
    }

    void releaseThenRestoreRoundTrips()
    {
        startFake();
        addSpectacle();
        addSnim();
        KdeScreenshotKey key;
        QCOMPARE(key.holderOf(QKeySequence(Qt::Key_Print)), QStringLiteral("Spectacle"));

        const ScreenshotKey::Result released = key.release();
        QVERIFY2(released.ok, qPrintable(released.message));
        QVERIFY(released.message.isEmpty());
        // Spectacle keeps its other key; only Snim's Capture Area changes.
        QCOMPARE(m_fake->keys(kSpectacle, QStringLiteral("_launch")), (Keys{{kMetaShiftS}}));
        QCOMPARE(m_fake->keys(kSnim, QStringLiteral("captureArea")), (Keys{{kPrint}}));
        QCOMPARE(m_fake->keys(kSnim, QStringLiteral("recordArea")), (Keys{{kCtrlShiftPrint}}));
        QCOMPARE(m_fake->sets(), (QStringList{kSpectacle + "/_launch", kSnim + "/captureArea"}));
        // Read live again, not from before the release.
        QVERIFY(key.holderOf(QKeySequence(Qt::Key_Print)).isEmpty());

        const QJsonObject memento = QJsonDocument::fromJson(released.memento.toUtf8()).object();
        const QJsonArray holders = memento.value("holders").toArray();
        QCOMPARE(holders.size(), 1);
        QCOMPARE(holders.at(0)["id"].toArray(),
                 QJsonArray({kSpectacle, "_launch", "Spectacle", "Launch Spectacle"}));
        QCOMPARE(holders.at(0)["keys"].toArray(), QJsonArray({QJsonArray{kPrint},
                                                              QJsonArray{kMetaShiftS}}));
        QCOMPARE(memento.value("snim")["id"].toArray(),
                 QJsonArray({kSnim, "captureArea", "Snim", "Capture Area"}));
        QCOMPARE(memento.value("snim")["keys"].toArray(), QJsonArray({QJsonArray{kCtrlPrint}}));

        const ScreenshotKey::Result restored = key.restore(released.memento);
        QVERIFY2(restored.ok, qPrintable(restored.message));
        QVERIFY(restored.message.isEmpty());
        QCOMPARE(m_fake->keys(kSpectacle, QStringLiteral("_launch")),
                 (Keys{{kPrint}, {kMetaShiftS}}));
        QCOMPARE(m_fake->keys(kSnim, QStringLiteral("captureArea")), (Keys{{kCtrlPrint}}));
        QCOMPARE(m_fake->sets().mid(2),
                 (QStringList{kSnim + "/captureArea", kSpectacle + "/_launch"}));
        QCOMPARE(key.holderOf(QKeySequence(Qt::Key_Print)), QStringLiteral("Spectacle"));
    }

    void releaseWithNobodyOnPrint()
    {
        startFake();
        addSnim();
        KdeScreenshotKey key;

        const ScreenshotKey::Result released = key.release();
        QVERIFY2(released.ok, qPrintable(released.message));
        QVERIFY(released.message.isEmpty());
        QCOMPARE(m_fake->keys(kSnim, QStringLiteral("captureArea")), (Keys{{kPrint}}));
        const QJsonObject memento = QJsonDocument::fromJson(released.memento.toUtf8()).object();
        QVERIFY(memento.value("holders").toArray().isEmpty());

        QVERIFY(key.restore(released.memento).ok);
        QCOMPARE(m_fake->keys(kSnim, QStringLiteral("captureArea")), (Keys{{kCtrlPrint}}));
    }

    void missingSnimShortcutStillFreesPrint()
    {
        startFake();
        addSpectacle();
        KdeScreenshotKey key;

        const ScreenshotKey::Result released = key.release();
        QVERIFY(released.ok);
        QVERIFY(released.message.contains(QStringLiteral("Capture Area")));
        QCOMPARE(m_fake->keys(kSpectacle, QStringLiteral("_launch")), (Keys{{kMetaShiftS}}));
        const QJsonObject memento = QJsonDocument::fromJson(released.memento.toUtf8()).object();
        QVERIFY(memento.value("snim").isNull());

        QVERIFY(key.restore(released.memento).ok);
        QCOMPARE(m_fake->keys(kSpectacle, QStringLiteral("_launch")),
                 (Keys{{kPrint}, {kMetaShiftS}}));
    }

    void aHolderThatKeepsPrintFailsTheRelease()
    {
        startFake();
        m_fake->add({QStringLiteral("org.example.grabber"), QStringLiteral("Grabber"),
                     QStringLiteral("grab"), QStringLiteral("Grab"), {{kPrint}}});
        addSpectacle();
        addSnim();
        m_fake->freeze(kSpectacle);
        KdeScreenshotKey key;

        const ScreenshotKey::Result released = key.release();
        QVERIFY(!released.ok);
        QVERIFY(released.message.contains(QStringLiteral("Spectacle")));
        QVERIFY(released.memento.isEmpty());
        // The grabber that let go is put back; Snim's shortcut is never touched.
        const QString grabber = QStringLiteral("org.example.grabber/grab");
        QCOMPARE(m_fake->sets(), (QStringList{grabber, grabber}));
        QCOMPARE(m_fake->keys(kSnim, QStringLiteral("captureArea")), (Keys{{kCtrlPrint}}));
    }

    void partialRestoreIsReported()
    {
        startFake();
        addSpectacle();
        addSnim();
        KdeScreenshotKey key;
        const ScreenshotKey::Result released = key.release();
        QVERIFY(released.ok);
        // Another app takes Print while Snim has it, so Spectacle cannot get it back.
        m_fake->add({QStringLiteral("org.example.grabber"), QStringLiteral("Grabber"),
                     QStringLiteral("grab"), QStringLiteral("Grab"), {{kPrint}}});

        const ScreenshotKey::Result restored = key.restore(released.memento);
        QVERIFY(restored.ok);
        QVERIFY(restored.message.contains(QStringLiteral("Spectacle")));
        QCOMPARE(m_fake->keys(kSpectacle, QStringLiteral("_launch")), (Keys{{kMetaShiftS}}));
        QCOMPARE(m_fake->keys(kSnim, QStringLiteral("captureArea")), (Keys{{kCtrlPrint}}));
    }

    void unreadableMementoOnlyGuides()
    {
        startFake();
        addSpectacle();
        addSnim();
        KdeScreenshotKey key;

        const ScreenshotKey::Result restored = key.restore(QStringLiteral("garbage"));
        QVERIFY(restored.ok);
        QVERIFY(!restored.message.isEmpty());
        QVERIFY(m_fake->sets().isEmpty());
    }

    void refusesOutsideThePrivateBus()
    {
        startFake();
        addSpectacle();
        addSnim();
        KdeScreenshotKey key;

        qunsetenv("SNIM_TEST_PRIVATE_BUS");
        const ScreenshotKey::Result released = key.release();
        const ScreenshotKey::Result restored =
            key.restore(QStringLiteral(R"({"holders":[],"snim":null})"));
        qputenv("SNIM_TEST_PRIVATE_BUS", "1");

        QVERIFY(!released.ok);
        QVERIFY(!restored.ok);
        QVERIFY(m_fake->sets().isEmpty());
    }

private:
    void startFake()
    {
        m_thread = new QThread;
        m_fake = new FakeKGlobalAccel;
        m_fake->moveToThread(m_thread);
        m_thread->start();
        bool ok = false;
        QMetaObject::invokeMethod(m_fake, [this, &ok] {
            QDBusConnection fake = QDBusConnection::connectToBus(QDBusConnection::SessionBus,
                                                                 kFakeConnection);
            ok = fake.registerVirtualObject(kPath, m_fake) && fake.registerService(kService);
        }, Qt::BlockingQueuedConnection);
        if (!ok)
            qFatal("could not start the fake kglobalaccel");
    }

    void addSpectacle()
    {
        m_fake->add({kSpectacle, QStringLiteral("Spectacle"), QStringLiteral("_launch"),
                     QStringLiteral("Launch Spectacle"), {{kPrint}, {kMetaShiftS}}});
    }

    void addSnim()
    {
        m_fake->add({kSnim, QStringLiteral("Snim"), QStringLiteral("captureArea"),
                     QStringLiteral("Capture Area"), {{kCtrlPrint}}});
        m_fake->add({kSnim, QStringLiteral("Snim"), QStringLiteral("recordArea"),
                     QStringLiteral("Record Area"), {{kCtrlShiftPrint}}});
    }

    QThread *m_thread = nullptr;
    FakeKGlobalAccel *m_fake = nullptr;
};

QTEST_MAIN(tst_ScreenshotKeyKde)
#include "tst_screenshotkey_kde.moc"
