#ifndef UPLOAD_UPLOADPROFILES_H
#define UPLOAD_UPLOADPROFILES_H

#include <QString>
#include <QVariant>
#include <QVariantMap>
#include <QVector>

namespace Upload {

/**
 * One saved upload destination. The non-secret config plus a stable `id` (the keychain
 * account for this profile's secret). Mirrors Editor::Image::BackdropPreset. `type` names
 * the ProviderSpec, whose fields say what `values` holds.
 */
struct UploadProfile {
    QString id;              // QUuid (Id128); also the keychain account for the secret
    QString name;            // user-facing label
    // The ProviderSpec id. Profiles saved before the key existed were all S3, so a
    // missing or unknown type reads as S3.
    QString type = QStringLiteral("s3");
    QString publicBaseUrl;   // every type: override for the returned link (required for R2)
    QVariantMap values;      // the provider's own settings, keyed by FieldSpec::key

    // A provider setting; a missing key reads as the field's defaultValue.
    [[nodiscard]] QVariant value(const QString &key) const;
    [[nodiscard]] QString text(const QString &key) const { return value(key).toString(); }
    [[nodiscard]] int number(const QString &key) const { return value(key).toInt(); }
    [[nodiscard]] bool flag(const QString &key) const { return value(key).toBool(); }
};

QString providerDisplayName(const QString &type);   // "S3" | "SFTP" | "FTP" (UI labels)
// The Core::KeychainStore service id holding this type's secret (account = profile id).
QString keychainServiceFor(const QString &type);

// "Name - SFTP", starred when it is the default: the transport matters when picking.
QString profileLabel(const UploadProfile &p, bool isDefault);

} // namespace Upload

/**
 * Persisted store of upload server profiles + which one is the default, kept as JSON in
 * QSettings (mirrors Editor::Image::BackdropPresets). Secrets are NOT here - each
 * profile's secret lives in the OS keychain under its `id`, in its type's service.
 */
namespace Upload::UploadProfiles {

QVector<UploadProfile> all();
UploadProfile          byId(const QString &id);        // empty id if not found
// Replace the whole list + default in one write (the Settings dialog's commit path).
// Does NOT touch the keychain - the caller reconciles secrets. A defaultId not present
// in the list is cleared (falls back to the first profile, or "" if empty).
void                   setAll(const QVector<UploadProfile> &profiles, const QString &defaultId);

QString       defaultId();
UploadProfile defaultProfile();                        // empty id if no/invalid default

// Make a fresh, unused profile id (QUuid Id128).
QString newId();

} // namespace Upload::UploadProfiles

#endif // UPLOAD_UPLOADPROFILES_H
