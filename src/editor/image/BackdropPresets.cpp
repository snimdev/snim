#include "BackdropPresets.h"
#include "core/Settings.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <algorithm>

namespace Editor::Image {

namespace {
    QVector<BackdropPreset> builtins()
    {
        QVector<BackdropPreset> v;
        auto add = [&](const QString &n, const QVariantMap &c) { v.push_back({n, c, true}); };
        add("Clean Gray", {{"fill", "Solid"}, {"solidColor", "#e9ecf1"},
                           {"padding", 64}, {"cornerRadius", 14}, {"shadowStrength", 40}});
        add("Indigo",     {{"fill", "Gradient"}, {"gradientIndex", 0}, {"customGradient", false},
                           {"padding", 72}, {"cornerRadius", 16}, {"shadowStrength", 50}});
        add("Sunset",     {{"fill", "Gradient"}, {"gradientIndex", 1}, {"customGradient", false},
                           {"padding", 72}, {"cornerRadius", 16}, {"shadowStrength", 50}});
        add("Aurora",     {{"fill", "Wallpaper"}, {"wallpaperIndex", 0},
                           {"padding", 80}, {"cornerRadius", 18}, {"shadowStrength", 55}});
        return v;
    }

    QVector<BackdropPreset> userPresets()
    {
        QVector<BackdropPreset> v;
        const QByteArray json = Core::Settings::backdropPresetsJson().toUtf8();
        const QJsonArray arr = QJsonDocument::fromJson(json).array();
        for (const QJsonValue &e : arr) {
            const QJsonObject o = e.toObject();
            v.push_back({o.value("name").toString(), o.value("config").toObject().toVariantMap(), false});
        }
        return v;
    }

    void writeUser(const QVector<BackdropPreset> &v)
    {
        QJsonArray arr;
        for (const BackdropPreset &p : v) {
            QJsonObject o;
            o["name"] = p.name;
            o["config"] = QJsonObject::fromVariantMap(p.config);
            arr.append(o);
        }
        Core::Settings::setBackdropPresetsJson(
            QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact)));
    }
}

QVector<BackdropPreset> BackdropPresets::all()
{
    QVector<BackdropPreset> v = builtins();
    v += userPresets();
    return v;
}

void BackdropPresets::save(const QString &name, const QVariantMap &config)
{
    if (name.isEmpty() || isBuiltin(name))
        return; // don't overwrite built-ins
    QVector<BackdropPreset> v = userPresets();
    bool replaced = false;
    for (BackdropPreset &p : v) {
        if (p.name == name) { p.config = config; replaced = true; break; }
    }
    if (!replaced)
        v.push_back({name, config, false});
    writeUser(v);
}

void BackdropPresets::remove(const QString &name)
{
    QVector<BackdropPreset> v = userPresets();
    v.erase(std::remove_if(v.begin(), v.end(),
                           [&](const BackdropPreset &p) { return p.name == name; }), v.end());
    writeUser(v);
    if (defaultName() == name)
        setDefault(QString());
}

bool BackdropPresets::isBuiltin(const QString &name)
{
    for (const BackdropPreset &p : builtins())
        if (p.name == name) return true;
    return false;
}

QVariantMap BackdropPresets::configFor(const QString &name)
{
    for (const BackdropPreset &p : all())
        if (p.name == name) return p.config;
    return {};
}

void BackdropPresets::setDefault(const QString &name)
{
    Core::Settings::setBackdropDefaultName(name);
}

QString BackdropPresets::defaultName()
{
    return Core::Settings::backdropDefaultName();
}

QVariantMap BackdropPresets::defaultConfig()
{
    const QString n = defaultName();
    return n.isEmpty() ? QVariantMap{} : configFor(n);
}

} // namespace Editor::Image
