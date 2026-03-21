#include <QtTest>
#include <QUuid>

#include "core/KeychainStore.h"

#include <windows.h>
#include <wincred.h>

using namespace Core;

// The Credential Manager backing, against the real store (Windows only, see CMakeLists).
// Every entry lives under a test-only service and a per-run account, and cleanup() erases
// them whether or not the test got that far.
class tst_KeychainStore : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanup();

    void roundTripsASecret();
    void usesTheServiceSlashAccountTarget();
    void keepsAccountsApart();
    void rejectsEmptyIds();

private:
    QString secretOf(const QString &account) const
    {
        return KeychainStore::retrieve(m_service, account).value_or(QStringLiteral("<none>"));
    }

    const QString m_service = QStringLiteral("dev.snim.test.keychainstore");
    QString m_account;
    QString m_otherAccount;
};

void tst_KeychainStore::initTestCase()
{
    const QString run = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_account = QStringLiteral("account-") + run;
    m_otherAccount = QStringLiteral("other-") + run;
}

void tst_KeychainStore::cleanup()
{
    KeychainStore::erase(m_service, m_account);
    KeychainStore::erase(m_service, m_otherAccount);
}

void tst_KeychainStore::roundTripsASecret()
{
    QVERIFY(!KeychainStore::retrieve(m_service, m_account).has_value());

    QVERIFY(KeychainStore::store(m_service, m_account, QStringLiteral("hunter2")));
    QCOMPARE(secretOf(m_account), QStringLiteral("hunter2"));

    // Overwrite in place, with non-ASCII to prove the UTF-8 round trip.
    const QString replaced = QStringLiteral("päss wörd ✓");
    QVERIFY(KeychainStore::store(m_service, m_account, replaced));
    QCOMPARE(secretOf(m_account), replaced);

    QVERIFY(KeychainStore::erase(m_service, m_account));
    QVERIFY(!KeychainStore::retrieve(m_service, m_account).has_value());
    // Erasing what is already gone still counts as success, as on macOS.
    QVERIFY(KeychainStore::erase(m_service, m_account));
}

void tst_KeychainStore::usesTheServiceSlashAccountTarget()
{
    QVERIFY(KeychainStore::store(m_service, m_account, QStringLiteral("hunter2")));

    const std::wstring target = (m_service + QLatin1Char('/') + m_account).toStdWString();
    PCREDENTIALW cred = nullptr;
    QVERIFY(CredReadW(target.c_str(), CRED_TYPE_GENERIC, 0, &cred));
    QCOMPARE(QString::fromWCharArray(cred->UserName), m_account);
    QCOMPARE(QByteArray(reinterpret_cast<const char *>(cred->CredentialBlob),
                        static_cast<qsizetype>(cred->CredentialBlobSize)),
             QByteArrayLiteral("hunter2"));
    CredFree(cred);
}

void tst_KeychainStore::keepsAccountsApart()
{
    QVERIFY(KeychainStore::store(m_service, m_account, QStringLiteral("first")));
    QVERIFY(KeychainStore::store(m_service, m_otherAccount, QStringLiteral("second")));

    QCOMPARE(secretOf(m_account), QStringLiteral("first"));
    QCOMPARE(secretOf(m_otherAccount), QStringLiteral("second"));

    QVERIFY(KeychainStore::erase(m_service, m_account));
    QCOMPARE(secretOf(m_otherAccount), QStringLiteral("second"));
}

void tst_KeychainStore::rejectsEmptyIds()
{
    QVERIFY(!KeychainStore::store(QString(), m_account, QStringLiteral("x")));
    QVERIFY(!KeychainStore::store(m_service, QString(), QStringLiteral("x")));
    QVERIFY(!KeychainStore::retrieve(QString(), m_account).has_value());
    QVERIFY(!KeychainStore::erase(m_service, QString()));
}

QTEST_MAIN(tst_KeychainStore)
#include "tst_keychainstore.moc"
