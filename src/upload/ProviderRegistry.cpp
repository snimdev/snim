#include "upload/ProviderRegistry.h"
#include "upload/UploadConfig.h"
#include "upload/strategies/S3Uploader.h"
#ifdef HAVE_LIBCURL
#include "upload/strategies/FtpUploader.h"
#endif
#ifdef HAVE_LIBSSH2
#include "upload/strategies/SftpUploader.h"
#endif

#include <QDir>

namespace Upload {

namespace {

bool sftpKeyAuth(const UploadProfile &p)
{
    return p.text(QStringLiteral("sftpAuthMode")) == QLatin1String("key");
}

} // namespace

const FieldSpec *ProviderSpec::field(const QString &key) const
{
    for (const FieldSpec &f : fields)
        if (f.kind != FieldKind::Secret && f.key == key)
            return &f;
    return nullptr;
}

const QList<ProviderSpec> &ProviderRegistry::providers()
{
    // The keychain ids changed in the Snim rebrand: secrets stored under the old
    // com.darkog.niceshot.* services are not migrated and must be re-entered.
    static const QList<ProviderSpec> specs = {
        {
            .id = QStringLiteral("s3"),
            .displayName = QStringLiteral("S3"),
            .addLabel = tr("S3-compatible…"),
            .namePlaceholder = tr("My S3 / R2 / MinIO"),
            .keychainService = QStringLiteral("dev.snim.s3"),
            .fields = {
                { .key = QStringLiteral("endpoint"), .label = tr("Endpoint:"),
                  .placeholder = tr("s3.amazonaws.com  ·  <acct>.r2.cloudflarestorage.com"),
                  .defaultValue = QStringLiteral("s3.amazonaws.com") },
                { .key = QStringLiteral("region"), .label = tr("Region:"),
                  .placeholder = tr("us-east-1  ·  auto (Cloudflare R2)"),
                  .defaultValue = QStringLiteral("us-east-1") },
                { .key = QStringLiteral("bucket"), .label = tr("Bucket:") },
                { .key = QStringLiteral("accessKeyId"), .label = tr("Access key ID:") },
                { .kind = FieldKind::Secret },
                { .key = QStringLiteral("keyPrefix"), .label = tr("Key prefix:"),
                  .placeholder = tr("e.g. screenshots/") },
                { .key = QStringLiteral("forcePathStyle"),
                  .label = tr("Force path-style URLs (MinIO / Wasabi)"),
                  .kind = FieldKind::Flag, .defaultValue = false },
            },
            .secretLabel = [](const UploadProfile &) { return tr("Secret key:"); },
            .secretHint = [](const UploadProfile &) { return tr("Secret access key"); },
            .isComplete = [](const UploadConfig &c) {
                return !c.text(QStringLiteral("endpoint")).isEmpty()
                       && !c.text(QStringLiteral("region")).isEmpty()
                       && !c.text(QStringLiteral("bucket")).isEmpty()
                       && !c.text(QStringLiteral("accessKeyId")).isEmpty()
                       && !c.secretKey.isEmpty();
            },
            .create = [](const UploadConfig &c, QObject *parent) -> std::unique_ptr<Uploader> {
                return std::make_unique<S3Uploader>(c, parent);
            },
        },
        {
            .id = QStringLiteral("sftp"),
            .displayName = QStringLiteral("SFTP"),
            .addLabel = tr("SFTP…"),
            .namePlaceholder = tr("My server"),
            .keychainService = QStringLiteral("dev.snim.sftp"),
#ifndef HAVE_LIBSSH2
            .missingDependency = tr("Requires libssh2, which was not included in this build."),
#endif
            .fields = {
                { .key = QStringLiteral("host"), .label = tr("Host:"),
                  .placeholder = QStringLiteral("sftp.example.com") },
                { .key = QStringLiteral("port"), .label = tr("Port:"), .kind = FieldKind::Port,
                  .placeholder = tr("22 (default)"), .defaultValue = 0 },
                { .key = QStringLiteral("username"), .label = tr("Username:") },
                { .key = QStringLiteral("sftpAuthMode"), .label = tr("Authentication:"),
                  .kind = FieldKind::Choice, .defaultValue = QStringLiteral("password"),
                  .choices = { { QStringLiteral("password"), tr("Password") },
                               { QStringLiteral("key"), tr("Private key file") } } },
                { .kind = FieldKind::Secret },
                { .key = QStringLiteral("privateKeyPath"), .label = tr("Private key:"),
                  .kind = FieldKind::FilePath,
                  .placeholder = QDir::homePath() + QStringLiteral("/.ssh/id_ed25519"),
                  .browseTitle = tr("Select private key file"), .visibleWhen = sftpKeyAuth },
                { .key = QStringLiteral("remoteDir"), .label = tr("Remote directory:"),
                  .placeholder = tr("e.g. /var/www/uploads - empty = login folder") },
            },
            .secretLabel = [](const UploadProfile &p) {
                return sftpKeyAuth(p) ? tr("Key passphrase (optional):") : tr("Password:");
            },
            .secretHint = [](const UploadProfile &p) {
                return sftpKeyAuth(p) ? tr("Key passphrase") : tr("Password");
            },
            .isComplete = [](const UploadConfig &c) {
                // Key auth needs no stored secret (the passphrase is optional), which also
                // keeps it usable on the stub-keychain platforms.
                return !c.text(QStringLiteral("host")).isEmpty()
                       && !c.text(QStringLiteral("username")).isEmpty()
                       && (sftpKeyAuth(c) ? !c.text(QStringLiteral("privateKeyPath")).isEmpty()
                                          : !c.secretKey.isEmpty());
            },
#ifdef HAVE_LIBSSH2
            .create = [](const UploadConfig &c, QObject *parent) -> std::unique_ptr<Uploader> {
                return std::make_unique<SftpUploader>(c, parent);
            },
#endif
        },
        {
            .id = QStringLiteral("ftp"),
            .displayName = QStringLiteral("FTP"),
            .addLabel = tr("FTP / FTPS…"),
            .namePlaceholder = tr("My server"),
            .keychainService = QStringLiteral("dev.snim.ftp"),
#ifndef HAVE_LIBCURL
            .missingDependency = tr("Requires libcurl, which was not included in this build."),
#endif
            .fields = {
                { .key = QStringLiteral("host"), .label = tr("Host:"),
                  .placeholder = QStringLiteral("ftp.example.com") },
                { .key = QStringLiteral("port"), .label = tr("Port:"), .kind = FieldKind::Port,
                  .placeholder = tr("21 (default)"), .defaultValue = 0 },
                { .key = QStringLiteral("username"), .label = tr("Username:"),
                  .placeholder = tr("empty = anonymous") },
                { .kind = FieldKind::Secret },
                // Leads with the secure default; a missing or unknown mode reads as it too.
                { .key = QStringLiteral("ftpsMode"), .label = tr("Encryption:"),
                  .kind = FieldKind::Choice, .defaultValue = QStringLiteral("explicit"),
                  .choices = { { QStringLiteral("explicit"), tr("Explicit TLS (FTPS)") },
                               { QStringLiteral("implicit"), tr("Implicit TLS (port 990)") },
                               { QStringLiteral("none"), tr("None (plain FTP - insecure)") } } },
                { .key = QStringLiteral("remoteDir"), .label = tr("Remote directory:"),
                  .placeholder = tr("e.g. /var/www/uploads - empty = login folder") },
            },
            .secretLabel = [](const UploadProfile &) { return tr("Password:"); },
            .secretHint = [](const UploadProfile &) { return tr("Password"); },
            .isComplete = [](const UploadConfig &c) {
                return !c.text(QStringLiteral("host")).isEmpty()
                       && (c.text(QStringLiteral("username")).isEmpty() || !c.secretKey.isEmpty());
            },
#ifdef HAVE_LIBCURL
            .create = [](const UploadConfig &c, QObject *parent) -> std::unique_ptr<Uploader> {
                return std::make_unique<FtpUploader>(c, parent);
            },
#endif
        },
    };
    return specs;
}

const ProviderSpec *ProviderRegistry::find(const QString &id)
{
    for (const ProviderSpec &spec : providers())
        if (spec.id == id)
            return &spec;
    return nullptr;
}

} // namespace Upload
