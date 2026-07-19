#include <QtTest>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMetaType>

#include "core/SecretService.h"

using namespace Core;
using SecretService::Attributes;

// The pure pieces of the Secret Service client, plus how it fails with no bus at all.
// Nothing here reaches a session bus (tst_keychainstore_linux runs on a private one).
class tst_SecretService : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void attributesNameTheEntryAndTheSchema();
    void labelNamesServiceAndAccount();
    void itemPropertiesCarryLabelAndAttributes();
    void textSecretIsPlainUtf8();
    void rootPathMeansNoObject();
    void errorNamesMapToFailures_data();
    void errorNamesMapToFailures();
    void typesMarshalToTheApiSignatures();
    void failsFastWithoutABus();
};

void tst_SecretService::initTestCase()
{
    SecretService::registerMetaTypes();
}

void tst_SecretService::attributesNameTheEntryAndTheSchema()
{
    const Attributes attributes = SecretService::attributesFor(QStringLiteral("dev.snim.s3"),
                                                               QStringLiteral("profile-1"));
    const Attributes expected{
        {QStringLiteral("service"), QStringLiteral("dev.snim.s3")},
        {QStringLiteral("account"), QStringLiteral("profile-1")},
        {QStringLiteral("xdg:schema"), QStringLiteral("dev.snim.Snim.Secret")},
    };
    QCOMPARE(attributes, expected);
}

void tst_SecretService::labelNamesServiceAndAccount()
{
    QCOMPARE(SecretService::labelFor(QStringLiteral("dev.snim.sftp"), QStringLiteral("abc")),
             QStringLiteral("Snim upload secret (dev.snim.sftp, abc)"));
}

void tst_SecretService::itemPropertiesCarryLabelAndAttributes()
{
    const QVariantMap properties = SecretService::itemProperties(QStringLiteral("dev.snim.ftp"),
                                                                 QStringLiteral("abc"));
    QCOMPARE(properties.size(), 2);
    QCOMPARE(properties.value(QStringLiteral("org.freedesktop.Secret.Item.Label")).toString(),
             QStringLiteral("Snim upload secret (dev.snim.ftp, abc)"));
    const QVariant attributes = properties.value(QStringLiteral("org.freedesktop.Secret.Item.Attributes"));
    QCOMPARE(attributes.metaType(), QMetaType::fromType<Attributes>());
    QCOMPARE(attributes.value<Attributes>(),
             SecretService::attributesFor(QStringLiteral("dev.snim.ftp"), QStringLiteral("abc")));
}

void tst_SecretService::textSecretIsPlainUtf8()
{
    const QDBusObjectPath session(QStringLiteral("/org/freedesktop/secrets/session/s1"));
    const SecretService::Secret secret = SecretService::textSecret(session, QStringLiteral("päss ✓"));
    QCOMPARE(secret.session, session);
    QVERIFY(secret.parameters.isEmpty());
    QCOMPARE(secret.value, QByteArray("p\xc3\xa4ss \xe2\x9c\x93"));
    QCOMPARE(secret.contentType, QStringLiteral("text/plain; charset=utf8"));
}

void tst_SecretService::rootPathMeansNoObject()
{
    QVERIFY(SecretService::isNoObject(QDBusObjectPath(QStringLiteral("/"))));
    QVERIFY(SecretService::isNoObject(QDBusObjectPath()));
    QVERIFY(!SecretService::isNoObject(QDBusObjectPath(QStringLiteral("/org/freedesktop/secrets/prompt/u1"))));
    QVERIFY(!SecretService::isNoObject(
        QDBusObjectPath(QStringLiteral("/org/freedesktop/secrets/collection/login"))));
}

void tst_SecretService::errorNamesMapToFailures_data()
{
    QTest::addColumn<QString>("error");
    QTest::addColumn<int>("failure");
    const auto add = [](const char *error, KeychainStore::Failure failure) {
        QTest::newRow(error) << QString::fromLatin1(error) << int(failure);
    };
    add("org.freedesktop.DBus.Error.ServiceUnknown", KeychainStore::Failure::NoService);
    add("org.freedesktop.DBus.Error.NameHasNoOwner", KeychainStore::Failure::NoService);
    add("org.freedesktop.DBus.Error.Disconnected", KeychainStore::Failure::NoService);
    add("org.freedesktop.DBus.Error.NoReply", KeychainStore::Failure::NoService);
    add("org.freedesktop.DBus.Error.Timeout", KeychainStore::Failure::NoService);
    add("org.freedesktop.DBus.Error.Spawn.ChildExited", KeychainStore::Failure::NoService);
    add("org.freedesktop.Secret.Error.IsLocked", KeychainStore::Failure::Locked);
    add("org.freedesktop.Secret.Error.NoSuchObject", KeychainStore::Failure::Other);
    add("org.freedesktop.DBus.Error.AccessDenied", KeychainStore::Failure::Other);
    add("org.freedesktop.DBus.Error.UnknownMethod", KeychainStore::Failure::Other);
}

void tst_SecretService::errorNamesMapToFailures()
{
    QFETCH(QString, error);
    QFETCH(int, failure);
    QCOMPARE(int(SecretService::failureForError(error)), failure);
}

void tst_SecretService::typesMarshalToTheApiSignatures()
{
    QCOMPARE(QString::fromLatin1(QDBusMetaType::typeToSignature(QMetaType::fromType<SecretService::Secret>())),
             QStringLiteral("(oayays)"));
    QCOMPARE(QString::fromLatin1(QDBusMetaType::typeToSignature(QMetaType::fromType<Attributes>())),
             QStringLiteral("a{ss}"));

    QDBusArgument arg;
    arg << SecretService::textSecret(QDBusObjectPath(QStringLiteral("/s")), QStringLiteral("x"));
    QCOMPARE(arg.currentSignature(), QStringLiteral("(oayays)"));
}

void tst_SecretService::failsFastWithoutABus()
{
    // Named but never connected, so every call fails the way a missing session bus does.
    const QDBusConnection none(QStringLiteral("snim-test-no-bus"));
    QVERIFY(!none.isConnected());

    QElapsedTimer timer;
    timer.start();
    SecretService::Client client(none);
    QVERIFY(!client.session().has_value());
    QCOMPARE(client.failure(), KeychainStore::Failure::NoService);
    QList<QDBusObjectPath> unlocked;
    QList<QDBusObjectPath> locked;
    QVERIFY(!client.search(SecretService::attributesFor(QStringLiteral("s"), QStringLiteral("a")),
                           &unlocked, &locked));
    QVERIFY(!client.defaultCollection().has_value());

    QString detail;
    QCOMPARE(SecretService::probe(&detail, none), KeychainStore::Failure::NoService);
    QCOMPARE(detail, QStringLiteral("no Secret Service on the session bus"));
    QVERIFY2(timer.elapsed() < 1000, qPrintable(QStringLiteral("took %1 ms").arg(timer.elapsed())));
}

QTEST_MAIN(tst_SecretService)
#include "tst_secretservice.moc"
