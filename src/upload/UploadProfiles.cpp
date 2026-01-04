#include "upload/UploadProfiles.h"

#include "core/KeychainStore.h"
#include "core/Settings.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>
#include <algorithm>

namespace Upload {

QString providerTypeToString(ProviderType t)
{
    switch (t) {
    case ProviderType::Sftp: return QStringLiteral("sftp");
    case ProviderType::Ftp:  return QStringLiteral("ftp");
    case ProviderType::S3:   break;
    }
    return QStringLiteral("s3");
}

ProviderType providerTypeFromString(const QString &s)
{
    if (s == QLatin1String("sftp")) return ProviderType::Sftp;
    if (s == QLatin1String("ftp"))  return ProviderType::Ftp;
    return ProviderType::S3;   // also the missing/unknown case (back-compat)
}

QString providerDisplayName(ProviderType t)
{
    switch (t) {
    case ProviderType::Sftp: return QStringLiteral("SFTP");
    case ProviderType::Ftp:  return QStringLiteral("FTP");
    case ProviderType::S3:   break;
    }
    return QStringLiteral("S3");
}

QString sftpAuthModeToString(SftpAuthMode m)
{
    return m == SftpAuthMode::PrivateKey ? QStringLiteral("key") : QStringLiteral("password");
}

SftpAuthMode sftpAuthModeFromString(const QString &s)
{
    return s == QLatin1String("key") ? SftpAuthMode::PrivateKey : SftpAuthMode::Password;
}

QString ftpEncryptionToString(FtpEncryption e)
{
    switch (e) {
    case FtpEncryption::None:     return QStringLiteral("none");
    case FtpEncryption::Implicit: return QStringLiteral("implicit");
    case FtpEncryption::Explicit: break;
    }
    return QStringLiteral("explicit");
}

FtpEncryption ftpEncryptionFromString(const QString &s)
{
    if (s == QLatin1String("none"))     return FtpEncryption::None;
    if (s == QLatin1String("implicit")) return FtpEncryption::Implicit;
    return FtpEncryption::Explicit;   // secure default for missing/unknown
}

QString keychainServiceFor(ProviderType t)
{
    switch (t) {
    case ProviderType::Sftp: return Core::KeychainStore::sftpService();
    case ProviderType::Ftp:  return Core::KeychainStore::ftpService();
    case ProviderType::S3:   break;
    }
    return Core::KeychainStore::s3Service();
}

namespace {

UploadProfile fromJson(const QJsonObject &o)
{
    UploadProfile p;
    p.id = o.value("id").toString();
    p.name = o.value("name").toString();
    p.type = providerTypeFromString(o.value("type").toString());   // absent -> S3
    p.endpoint = o.value("endpoint").toString();
    p.region = o.value("region").toString();
    p.bucket = o.value("bucket").toString();
    p.accessKeyId = o.value("accessKeyId").toString();
    p.keyPrefix = o.value("keyPrefix").toString();
    p.forcePathStyle = o.value("forcePathStyle").toBool();
    p.host = o.value("host").toString();
    p.port = o.value("port").toInt();
    p.username = o.value("username").toString();
    p.remoteDir = o.value("remoteDir").toString();
    p.sftpAuth = sftpAuthModeFromString(o.value("sftpAuthMode").toString());
    p.privateKeyPath = o.value("privateKeyPath").toString();
    p.ftpEncryption = ftpEncryptionFromString(o.value("ftpsMode").toString());
    p.publicBaseUrl = o.value("publicBaseUrl").toString();
    return p;
}

QJsonObject toJson(const UploadProfile &p)
{
    QJsonObject o;
    o["id"] = p.id;
    o["name"] = p.name;
    o["type"] = providerTypeToString(p.type);
    o["endpoint"] = p.endpoint;
    o["region"] = p.region;
    o["bucket"] = p.bucket;
    o["accessKeyId"] = p.accessKeyId;
    o["keyPrefix"] = p.keyPrefix;
    o["forcePathStyle"] = p.forcePathStyle;
    o["host"] = p.host;
    o["port"] = p.port;
    o["username"] = p.username;
    o["remoteDir"] = p.remoteDir;
    o["sftpAuthMode"] = sftpAuthModeToString(p.sftpAuth);
    o["privateKeyPath"] = p.privateKeyPath;
    o["ftpsMode"] = ftpEncryptionToString(p.ftpEncryption);
    o["publicBaseUrl"] = p.publicBaseUrl;
    return o;
}

void write(const QVector<UploadProfile> &v)
{
    QJsonArray arr;
    for (const UploadProfile &p : v)
        arr.append(toJson(p));
    Core::Settings::setUploadProfilesJson(
        QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact)));
}

// One-time: turn a pre-existing single Upload/* config into a "Default" profile and
// re-key its secret from (legacy accessKeyId) to (new profile id). Runs only when the
// profiles list is still empty but a legacy bucket exists; persisting a non-empty list
// makes the guard fail forever after.
QVector<UploadProfile> migrateLegacyIfNeeded()
{
    if (Core::Settings::uploadBucket().isEmpty())
        return {};

    UploadProfile p;
    p.id = UploadProfiles::newId();
    p.name = QStringLiteral("Default");
    p.endpoint = Core::Settings::uploadEndpoint();
    p.region = Core::Settings::uploadRegion();
    p.bucket = Core::Settings::uploadBucket();
    p.accessKeyId = Core::Settings::uploadAccessKeyId();
    p.keyPrefix = Core::Settings::uploadKeyPrefix();
    p.publicBaseUrl = Core::Settings::uploadPublicBaseUrl();
    p.forcePathStyle = Core::Settings::uploadForcePathStyle();

    // Move the secret from the old (accessKeyId-keyed) slot to the new (id-keyed) slot,
    // then erase the old slot so no stale credential copy lingers in the keychain.
    if (!p.accessKeyId.isEmpty()) {
        const auto secret = Core::KeychainStore::retrieve(
            Core::KeychainStore::s3Service(), p.accessKeyId);
        if (secret) {
            Core::KeychainStore::store(Core::KeychainStore::s3Service(), p.id, *secret);
            Core::KeychainStore::erase(Core::KeychainStore::s3Service(), p.accessKeyId);
        }
    }

    QVector<UploadProfile> v{p};
    write(v);
    Core::Settings::setUploadDefaultProfileId(p.id);
    return v;
}

} // namespace

QString UploadProfiles::newId()
{
    return QUuid::createUuid().toString(QUuid::Id128);
}

QVector<UploadProfile> UploadProfiles::all()
{
    const QByteArray json = Core::Settings::uploadProfilesJson().toUtf8();
    const QJsonArray arr = QJsonDocument::fromJson(json).array();
    if (arr.isEmpty())
        return migrateLegacyIfNeeded();   // empty list -> maybe a legacy single config

    QVector<UploadProfile> v;
    v.reserve(arr.size());
    for (const QJsonValue &e : arr)
        v.push_back(fromJson(e.toObject()));
    return v;
}

UploadProfile UploadProfiles::byId(const QString &id)
{
    if (id.isEmpty())
        return {};
    for (const UploadProfile &p : all())
        if (p.id == id)
            return p;
    return {};
}

void UploadProfiles::save(const UploadProfile &p)
{
    if (p.id.isEmpty())
        return;
    QVector<UploadProfile> v = all();
    bool replaced = false;
    for (UploadProfile &e : v) {
        if (e.id == p.id) { e = p; replaced = true; break; }
    }
    if (!replaced)
        v.push_back(p);
    write(v);
    // First profile saved becomes the default automatically.
    if (defaultId().isEmpty())
        setDefault(p.id);
}

void UploadProfiles::remove(const QString &id)
{
    if (id.isEmpty())
        return;
    QVector<UploadProfile> v = all();
    v.erase(std::remove_if(v.begin(), v.end(),
                           [&](const UploadProfile &p) { return p.id == id; }), v.end());
    write(v);
    // Erase from every service, not just the removed profile's own: erase is idempotent,
    // and this also sweeps a secret left behind if the type ever changed under the id.
    Core::KeychainStore::erase(Core::KeychainStore::s3Service(), id);
    Core::KeychainStore::erase(Core::KeychainStore::sftpService(), id);
    Core::KeychainStore::erase(Core::KeychainStore::ftpService(), id);
    // If the default was removed, hand it to the first remaining profile (or clear).
    if (defaultId() == id)
        setDefault(v.isEmpty() ? QString() : v.first().id);
}

void UploadProfiles::setAll(const QVector<UploadProfile> &profiles, const QString &defaultId)
{
    write(profiles);
    // Validate the default against the new list; fall back to the first (or none).
    QString def = defaultId;
    const bool present = std::any_of(profiles.begin(), profiles.end(),
                                     [&](const UploadProfile &p) { return p.id == def; });
    if (!present)
        def = profiles.isEmpty() ? QString() : profiles.first().id;
    setDefault(def);
}

QString UploadProfiles::defaultId()
{
    return Core::Settings::uploadDefaultProfileId();
}

void UploadProfiles::setDefault(const QString &id)
{
    Core::Settings::setUploadDefaultProfileId(id);
}

UploadProfile UploadProfiles::defaultProfile()
{
    return byId(defaultId());
}

} // namespace Upload
