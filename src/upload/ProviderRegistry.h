#ifndef UPLOAD_PROVIDERREGISTRY_H
#define UPLOAD_PROVIDERREGISTRY_H

#include "upload/Uploader.h"

#include <QCoreApplication>
#include <QList>
#include <QPair>
#include <QString>
#include <QVariant>
#include <functional>
#include <memory>

namespace Upload {

struct UploadConfig;
struct UploadProfile;

// How a setting is edited and persisted: Text and FilePath as strings, Port as an int
// (0 = the protocol default), Flag as a bool, Choice as one of its stored values. Secret
// only places the write-only keychain secret among the rows; it is never in the profile.
enum class FieldKind { Text, Port, Flag, Choice, FilePath, Secret };

// One provider setting, as stored in UploadProfile::values and shown in its settings form.
struct FieldSpec {
    QString key;               // the JSON and values key, e.g. "endpoint", "ftpsMode"
    QString label;             // the row label; for a Flag the checkbox text (no row label)
    FieldKind kind = FieldKind::Text;
    QString placeholder;       // Port: the text shown at 0; FilePath: an example path
    QVariant defaultValue;     // a new profile's value, and what a missing key reads as
    QList<QPair<QString, QString>> choices;   // Choice: (stored value, label), display order
    QString browseTitle;       // FilePath: the file dialog's title
    std::function<bool(const UploadProfile &)> visibleWhen;   // null = always shown
};

/**
 * Declarative description of one upload provider: its persisted id and UI names, the
 * keychain service holding its secrets, its settings, when a destination is filled in and
 * how its uploader is built. Adding a provider is one ProviderSpec entry in
 * ProviderRegistry.cpp (plus its strategy's sources). Plain data, so it stays below the
 * UI layer.
 */
struct ProviderSpec {
    QString id;                  // persisted as the profile type: "s3", "sftp", "ftp"
    QString displayName;         // "S3", "SFTP", "FTP"
    QString addLabel;            // the Add menu entry
    QString namePlaceholder;     // hint in the profile's Name field
    // Core::KeychainStore service for this provider's secrets (account = the profile id).
    // Frozen: existing keychain items were written under it.
    QString keychainService;
    QString missingDependency;   // why the backend is not in this build; empty = it is
    QList<FieldSpec> fields;     // in form order

    // The secret row's label, and its placeholder while nothing is stored.
    std::function<QString(const UploadProfile &)> secretLabel;
    std::function<QString(const UploadProfile &)> secretHint;
    // Pure config, "is this destination filled in", never "is its backend built in".
    std::function<bool(const UploadConfig &)> isComplete;
    // A complete config, secret included; null when the backend is not in this build.
    std::function<std::unique_ptr<Uploader>(const UploadConfig &, QObject *)> create;

    [[nodiscard]] bool available() const { return missingDependency.isEmpty(); }
    [[nodiscard]] const FieldSpec *field(const QString &key) const;   // null when absent
};

// The built-in providers, in Add-menu order. One explicit list rather than
// self-registration: snim_lib is static, so the linker would drop unreferenced entries.
class ProviderRegistry
{
    Q_DECLARE_TR_FUNCTIONS(Upload::ProviderRegistry)

public:
    static const QList<ProviderSpec> &providers();
    static const ProviderSpec *find(const QString &id);   // null when no provider uses id
};

} // namespace Upload

#endif // UPLOAD_PROVIDERREGISTRY_H
