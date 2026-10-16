#include <QtTest>
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>

#include "app/ProviderForm.h"
#include "upload/ProviderRegistry.h"

using namespace Upload;

// The settings page a ProviderSpec builds: values in, values out, and the rows that
// follow what is on screen. Offscreen widgets only; nothing is saved.
class tst_ProviderForm : public QObject
{
    Q_OBJECT

    static QLineEdit *editWithPlaceholder(QWidget *form, const QString &suffix)
    {
        for (QLineEdit *edit : form->findChildren<QLineEdit *>())
            if (edit->placeholderText().endsWith(suffix))
                return edit;
        return nullptr;
    }

    static QLineEdit *secretEdit(QWidget *form)
    {
        for (QLineEdit *edit : form->findChildren<QLineEdit *>())
            if (edit->echoMode() == QLineEdit::Password)
                return edit;
        return nullptr;
    }

private slots:
    void bindThenFlushRoundTrips_data()
    {
        QTest::addColumn<QString>("id");
        for (const ProviderSpec &spec : ProviderRegistry::providers())
            QTest::newRow(qPrintable(spec.id)) << spec.id;
    }

    void bindThenFlushRoundTrips()
    {
        QFETCH(QString, id);
        const ProviderSpec &spec = *ProviderRegistry::find(id);
        UploadProfile p;
        p.type = id;
        for (const FieldSpec &f : spec.fields) {
            switch (f.kind) {
            case FieldKind::Port:     p.values.insert(f.key, 2121); break;
            case FieldKind::Flag:     p.values.insert(f.key, true); break;
            case FieldKind::Choice:   p.values.insert(f.key, f.choices.last().first); break;
            case FieldKind::Text:
            case FieldKind::FilePath: p.values.insert(f.key, "v-" + f.key); break;
            case FieldKind::Secret:   break;
            }
        }

        App::ProviderForm form(spec);
        form.bind(p, false);
        UploadProfile out;
        out.type = id;
        form.flush(out);
        QCOMPARE(out.values, p.values);
        QVERIFY(form.secret().isEmpty());   // write-only: binding never fills it

        // Cleared, text and ports blank and choices fall back to their defaults.
        form.clear();
        form.flush(out);
        for (const FieldSpec &f : spec.fields) {
            if (f.kind == FieldKind::Choice)
                QCOMPARE(out.values.value(f.key), f.defaultValue);
            else if (f.kind == FieldKind::Port)
                QCOMPARE(out.values.value(f.key).toInt(), 0);
            else if (f.kind == FieldKind::Text)
                QVERIFY(out.values.value(f.key).toString().isEmpty());
        }
    }

    void flushTrimsText()
    {
        App::ProviderForm form(*ProviderRegistry::find("ftp"));
        UploadProfile p;
        p.type = "ftp";
        p.values.insert("host", "  ftp.example.com ");
        form.bind(p, false);
        UploadProfile out;
        out.type = "ftp";
        form.flush(out);
        QCOMPARE(out.text("host"), QStringLiteral("ftp.example.com"));
    }

    void sftpKeyRowFollowsTheAuthMode()
    {
        App::ProviderForm form(*ProviderRegistry::find("sftp"));
        auto *layout = form.findChild<QFormLayout *>();
        auto *combo = form.findChild<QComboBox *>();
        QLineEdit *keyPath = editWithPlaceholder(&form, "/.ssh/id_ed25519");
        QLineEdit *secret = secretEdit(&form);
        QVERIFY(layout && combo && keyPath && secret);
        QWidget *keyRow = keyPath->parentWidget();   // path + Browse share one row widget
        auto *secretLabel = qobject_cast<QLabel *>(layout->labelForField(secret));
        QVERIFY(secretLabel);

        UploadProfile p;
        p.type = "sftp";
        p.values.insert("sftpAuthMode", "password");
        form.bind(p, false);
        QVERIFY(!layout->isRowVisible(keyRow));
        QCOMPARE(secretLabel->text(), QStringLiteral("Password:"));
        QCOMPARE(secret->placeholderText(), QStringLiteral("Password"));

        // Switching on screen, before any flush, re-evaluates the rows.
        combo->setCurrentIndex(combo->findData(QStringLiteral("key")));
        QVERIFY(layout->isRowVisible(keyRow));
        QCOMPARE(secretLabel->text(), QStringLiteral("Key passphrase (optional):"));
        QCOMPARE(secret->placeholderText(), QStringLiteral("Key passphrase"));

        // A stored secret keeps its marker whatever the mode.
        p.values.insert("sftpAuthMode", "key");
        form.bind(p, true);
        QVERIFY(layout->isRowVisible(keyRow));
        combo->setCurrentIndex(combo->findData(QStringLiteral("password")));
        QVERIFY(!layout->isRowVisible(keyRow));
        QCOMPARE(secret->placeholderText(), QStringLiteral("•••••••• (stored)"));
    }

    void ftpEncryptionListsTheSecureDefaultFirst()
    {
        App::ProviderForm form(*ProviderRegistry::find("ftp"));
        auto *combo = form.findChild<QComboBox *>();
        QVERIFY(combo);
        QCOMPARE(combo->count(), 3);
        QCOMPARE(combo->itemData(0).toString(), QStringLiteral("explicit"));
        QCOMPARE(combo->itemText(2), QStringLiteral("None (plain FTP - insecure)"));
        form.clear();
        QCOMPARE(combo->currentData().toString(), QStringLiteral("explicit"));
    }
};

QTEST_MAIN(tst_ProviderForm)
#include "tst_providerform.moc"
