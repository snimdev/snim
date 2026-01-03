#ifndef IMAGEEDITOR_BACKDROPMEMENTO_H
#define IMAGEEDITOR_BACKDROPMEMENTO_H

#include <QVariantMap>
#include <QString>
#include <utility>

namespace Editor::Image {

class BackdropItem;

/**
 * An opaque snapshot of a `BackdropItem`'s state.
 *
 * The public interface only lets callers hold, copy, and move the snapshot, with no
 * accessors to its contents. The state fields and constructor are private and
 * reachable only by `BackdropItem` (via `friend`), so nothing else can read or forge
 * one.
 *
 * It captures the complete state: the serialisable look plus the active preset name.
 * That is why it differs from `BackdropItem::toConfig()`, whose map is the look only
 * (used for presets/persistence/preview).
 */
class BackdropMemento
{
public:
    // Public interface: copy/move only. No way to read or mutate the state.
    BackdropMemento(const BackdropMemento &) = default;
    BackdropMemento(BackdropMemento &&) = default;
    BackdropMemento &operator=(const BackdropMemento &) = default;
    BackdropMemento &operator=(BackdropMemento &&) = default;
    ~BackdropMemento() = default;

private:
    friend class BackdropItem;   // only BackdropItem builds/reads the private state

    BackdropMemento(QVariantMap config, QString activePreset)
        : m_config(std::move(config)), m_activePreset(std::move(activePreset)) {}

    QVariantMap m_config;       // the backdrop look (== BackdropItem::toConfig())
    QString     m_activePreset; // which named preset was active, if any
};

} // namespace Editor::Image

#endif // IMAGEEDITOR_BACKDROPMEMENTO_H
