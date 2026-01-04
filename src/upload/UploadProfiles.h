#ifndef UPLOAD_UPLOADPROFILES_H
#define UPLOAD_UPLOADPROFILES_H

#include <QString>
#include <QVector>

namespace Upload {

/**
 * One saved upload destination. The non-secret config plus a stable `id` (the keychain
 * account for this profile's secret access key). Mirrors Editor::Image::BackdropPreset.
 */
struct UploadProfile {
    QString id;              // QUuid (Id128); also the keychain account for the secret
    QString name;            // user-facing label
    QString endpoint;        // host only, e.g. "s3.amazonaws.com"
    QString region;          // "us-east-1", "auto" (R2), ...
    QString bucket;
    QString accessKeyId;
    QString keyPrefix;       // e.g. "screenshots/"
    QString publicBaseUrl;   // override for the returned link (required for R2)
    bool forcePathStyle = false;

    [[nodiscard]] bool isNull() const { return id.isEmpty(); }
};

/**
 * Persisted store of upload server profiles + which one is the default, kept as JSON in
 * QSettings (mirrors Editor::Image::BackdropPresets). Secrets are NOT here — each
 * profile's secret access key lives in the OS keychain under its `id`.
 *
 * A lazy, one-time migration converts a pre-existing single `Upload/*` config into one
 * "Default" profile the first time the store is read.
 */
namespace UploadProfiles {

QVector<UploadProfile> all();
UploadProfile          byId(const QString &id);        // null profile if not found
void                   save(const UploadProfile &p);   // add or update (matched by id)
void                   remove(const QString &id);      // also erases the keychain secret
// Replace the whole list + default in one write (the Settings dialog's commit path).
// Does NOT touch the keychain — the caller reconciles secrets. A defaultId not present
// in the list is cleared (falls back to the first profile, or "" if empty).
void                   setAll(const QVector<UploadProfile> &profiles, const QString &defaultId);

QString       defaultId();
void          setDefault(const QString &id);           // "" clears
UploadProfile defaultProfile();                        // null if no/invalid default

// Make a fresh, unused profile id (QUuid Id128).
QString newId();

} // namespace UploadProfiles

} // namespace Upload

#endif // UPLOAD_UPLOADPROFILES_H
