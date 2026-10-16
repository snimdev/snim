#include "upload/UploadProfiles.h"

#include "core/Settings.h"
#include "upload/ProviderRegistry.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QUuid>
#include <algorithm>
#include <optional>

namespace Upload {

QVariant UploadProfile::value(const QString &key) const
{
    if (const auto it = values.constFind(key); it != values.cend())
        return *it;
    const ProviderSpec *spec = ProviderRegistry::find(type);
    const FieldSpec *field = spec ? spec->field(key) : nullptr;
    return field ? field->defaultValue : QVariant();
}

QString providerDisplayName(const QString &type)
{
    const ProviderSpec *spec = ProviderRegistry::find(type);
    return spec ? spec->displayName : QString();
}

QString keychainServiceFor(const QString &type)
{
    const ProviderSpec *spec = ProviderRegistry::find(type);
    return spec ? spec->keychainService : QString();
}

QString profileLabel(const UploadProfile &p, bool isDefault)
{
    const QString name = p.name.isEmpty() ? QObject::tr("(unnamed)") : p.name;
    const QString label = name + QStringLiteral(" - ") + providerDisplayName(p.type);
    return isDefault ? QStringLiteral("★ ") + label : label;
}

} // namespace Upload

namespace Upload::UploadProfiles {

namespace {

// A stored value in the field's JSON shape; a Choice outside its list reads as absent.
std::optional<QVariant> fieldFromJson(const FieldSpec &f, const QJsonValue &v)
{
    switch (f.kind) {
    case FieldKind::Port:
        return v.toInt();
    case FieldKind::Flag:
        return v.toBool();
    case FieldKind::Choice: {
        const QString s = v.toString();
        const bool known = std::any_of(f.choices.cbegin(), f.choices.cend(),
                                       [&s](const auto &c) { return c.first == s; });
        return known ? std::optional<QVariant>(s) : std::nullopt;
    }
    case FieldKind::Text:
    case FieldKind::FilePath:
    case FieldKind::Secret:
        break;
    }
    return v.toString();
}

QJsonValue fieldToJson(const FieldSpec &f, const QVariant &v)
{
    switch (f.kind) {
    case FieldKind::Port: return v.toInt();
    case FieldKind::Flag: return v.toBool();
    default:              return v.toString();
    }
}

// The provider's fields sit flat beside id/name/type, under the same keys and in the
// same value shapes as ever, so older files load and rewrite compatibly.
UploadProfile fromJson(const QJsonObject &o)
{
    UploadProfile p;
    p.id = o.value("id").toString();
    p.name = o.value("name").toString();
    const QString type = o.value("type").toString();
    const ProviderSpec *spec = ProviderRegistry::find(type);
    if (spec)
        p.type = type;
    else
        spec = ProviderRegistry::find(p.type);   // absent or unknown -> S3
    p.publicBaseUrl = o.value("publicBaseUrl").toString();
    for (const FieldSpec &f : spec->fields) {
        if (f.kind == FieldKind::Secret || !o.contains(f.key))
            continue;
        if (const auto v = fieldFromJson(f, o.value(f.key)))
            p.values.insert(f.key, *v);
    }
    return p;
}

QJsonObject toJson(const UploadProfile &p)
{
    QJsonObject o;
    o["id"] = p.id;
    o["name"] = p.name;
    o["type"] = p.type;
    o["publicBaseUrl"] = p.publicBaseUrl;
    if (const ProviderSpec *spec = ProviderRegistry::find(p.type))
        for (const FieldSpec &f : spec->fields)
            if (f.kind != FieldKind::Secret)
                o[f.key] = fieldToJson(f, p.value(f.key));
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

} // namespace

QString newId()
{
    return QUuid::createUuid().toString(QUuid::Id128);
}

QVector<UploadProfile> all()
{
    const QByteArray json = Core::Settings::uploadProfilesJson().toUtf8();
    const QJsonArray arr = QJsonDocument::fromJson(json).array();
    QVector<UploadProfile> v;
    v.reserve(arr.size());
    for (const QJsonValue &e : arr)
        v.push_back(fromJson(e.toObject()));
    return v;
}

UploadProfile byId(const QString &id)
{
    if (id.isEmpty())
        return {};
    for (const UploadProfile &p : all())
        if (p.id == id)
            return p;
    return {};
}

void setAll(const QVector<UploadProfile> &profiles, const QString &defaultId)
{
    write(profiles);
    // Validate the default against the new list; fall back to the first (or none).
    QString def = defaultId;
    const bool present = std::any_of(profiles.begin(), profiles.end(),
                                     [&](const UploadProfile &p) { return p.id == def; });
    if (!present)
        def = profiles.isEmpty() ? QString() : profiles.first().id;
    Core::Settings::setUploadDefaultProfileId(def);
}

QString defaultId()
{
    return Core::Settings::uploadDefaultProfileId();
}

UploadProfile defaultProfile()
{
    return byId(defaultId());
}

} // namespace Upload::UploadProfiles
