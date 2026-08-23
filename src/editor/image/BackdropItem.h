#ifndef IMAGEEDITOR_BACKDROPITEM_H
#define IMAGEEDITOR_BACKDROPITEM_H

#include <QGraphicsItem>
#include <QColor>
#include <QPixmap>
#include <QRectF>
#include <functional>
#include "editor/annotations/tools/ITool.h"
#include "BackdropMemento.h"

namespace Editor::Image {

/**
 * Full-canvas backdrop drawn BEHIND the screenshot (z below it). Provides the
 * CleanShot-style "beautify background": solid color / gradient / wallpaper fill.
 * Padding, corner radius, and shadow are stored here (so they appear in the
 * Properties panel) but APPLIED by ImageEditor (which owns the screenshot item,
 * its rounded pixmap, the drop-shadow effect, and the scene rect).
 *
 * The editor sets the rect to fill via setCanvasRect(); a change callback lets the
 * editor re-layout when a geometry/appearance property changes.
 */
class BackdropItem : public QGraphicsItem, public Tools::ITool
{
public:
    enum class Fill { Solid, Gradient, Wallpaper };

    BackdropItem();

    // Editor-driven layout
    void setCanvasRect(const QRectF &rect);
    void setOnChanged(std::function<void(const QString &propertyId)> cb) { m_onChanged = std::move(cb); }

    // Values read by ImageEditor when applying the backdrop
    int    padding() const { return m_padding; }
    int    cornerRadius() const { return m_cornerRadius; }
    qreal  shadowBlur() const { return m_shadowStrength * 0.9; }       // 0..90 px
    qreal  shadowOpacity() const { return m_shadowStrength / 100.0; }  // 0..1

    // QGraphicsItem
    QRectF boundingRect() const override { return m_canvasRect; }
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    // ITool (drives the Properties panel)
    [[nodiscard]] QList<Tools::ToolProperty> getProperties() const override;
    void setProperty(const QString &propertyId, const QVariant &value) override;

    static QStringList gradientNames();
    static QStringList wallpaperNames();
    static QPixmap gradientPreview(int index, QSize size);
    static QPixmap wallpaperPreview(int index, QSize size);

    // Full visual config (for presets). Colors are stored as #rrggbb strings so
    // the map is JSON/QSettings-friendly.
    [[nodiscard]] QVariantMap toConfig() const;
    void applyConfig(const QVariantMap &config);
    static QPixmap configPreview(const QVariantMap &config, QSize size);

    // createMemento() snapshots the COMPLETE state (look + active preset); restore()
    // puts it back. Distinct from toConfig()/applyConfig() (the serialisable look, for
    // presets), which drive undoable backdrop edits via Commands::BackdropChangeCommand.
    [[nodiscard]] BackdropMemento createMemento() const;
    void restore(const BackdropMemento &memento);

    // Name of the preset currently in effect ("" once the user diverges via a
    // manual edit). Single source of truth shared by the popover grid and the
    // in-panel preset selector.
    [[nodiscard]] QString activePreset() const { return m_activePreset; }
    void setActivePreset(const QString &name) { m_activePreset = name; }

private:
    void paintFill(QPainter *painter, const QRectF &rect) const;   // the fill alone

    QRectF m_canvasRect;
    Fill   m_fill = Fill::Gradient;
    QColor m_solidColor = QColor("#1e1e2e");
    int    m_gradientIndex = 0;
    bool   m_customGradient = false;          // manual gradient colors override the preset
    QColor m_gradStart = QColor("#4f46e5");
    QColor m_gradEnd   = QColor("#9333ea");
    int    m_wallpaperIndex = 0;
    int    m_padding = 64;
    int    m_cornerRadius = 14;
    int    m_shadowStrength = 45;   // 0..100 (drives blur + opacity)
    QString m_activePreset;         // name of the applied preset, "" when diverged

    std::function<void(const QString &)> m_onChanged;
};

} // namespace Editor::Image

#endif // IMAGEEDITOR_BACKDROPITEM_H
