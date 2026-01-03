#ifndef IMAGEEDITOR_BACKDROPPRESETS_H
#define IMAGEEDITOR_BACKDROPPRESETS_H

#include <QString>
#include <QVariantMap>
#include <QVector>

namespace Editor::Image {

struct BackdropPreset {
    QString name;
    QVariantMap config;   // BackdropItem config (see BackdropItem::toConfig)
    bool builtin = false;
};

// Persisted store (QSettings) of named backdrop configurations, plus a few
// built-in starter presets. One preset may be marked the default, which is
// auto-applied to new captures.
namespace BackdropPresets {
    QVector<BackdropPreset> all();                              // built-ins first, then user presets
    void        save(const QString &name, const QVariantMap &config); // add/overwrite a user preset
    void        remove(const QString &name);                   // user presets only
    bool        isBuiltin(const QString &name);
    QVariantMap configFor(const QString &name);                // empty if not found
    void        setDefault(const QString &name);               // "" clears the default
    QString     defaultName();
    QVariantMap defaultConfig();                               // empty if no default set
}

} // namespace Editor::Image

#endif // IMAGEEDITOR_BACKDROPPRESETS_H
