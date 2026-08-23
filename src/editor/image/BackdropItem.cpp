#include "BackdropItem.h"
#include "editor/image/BackdropPresets.h"

#include <QPainter>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QPainterPath>
#include <QStyleOptionGraphicsItem>
#include <vector>

namespace Editor::Image {

using Tools::ToolProperty;   // ITool/ToolProperty live in the shared Editor::Tools

namespace {
    struct GradientPreset { const char *name; const char *c1; const char *c2; };
    // Diagonal two-stop gradients (top-left -> bottom-right).
    const GradientPreset kGradients[] = {
        {"Indigo",  "#4f46e5", "#9333ea"},
        {"Sunset",  "#ff6a00", "#ee0979"},
        {"Ocean",   "#2193b0", "#6dd5ed"},
        {"Mint",    "#11998e", "#38ef7d"},
        {"Slate",   "#232526", "#414345"},
        {"Peach",   "#ffecd2", "#fcb69f"},
    };
    constexpr int kGradientCount = int(sizeof(kGradients) / sizeof(kGradients[0]));

    // Procedural "mesh gradient" wallpapers: a base color with a few soft radial
    // color blobs layered on top (the modern CleanShot/macOS wallpaper look).
    struct Blob { qreal x, y, radius; const char *color; };
    struct WallpaperPreset {
        const char *name;
        const char *base;
        std::vector<Blob> blobs;
    };
    const WallpaperPreset kWallpapers[] = {
        {"Aurora", "#0b1020", {{0.20, 0.25, 0.55, "#3b82f6"}, {0.80, 0.20, 0.50, "#8b5cf6"},
                               {0.65, 0.85, 0.55, "#10b981"}}},
        {"Sunset", "#1a0a2e", {{0.18, 0.80, 0.55, "#f59e0b"}, {0.85, 0.30, 0.55, "#ec4899"},
                               {0.50, 0.10, 0.45, "#8b5cf6"}}},
        {"Lavender", "#e9e4ff", {{0.20, 0.25, 0.55, "#c4b5fd"}, {0.85, 0.75, 0.55, "#a5b4fc"},
                                 {0.70, 0.15, 0.40, "#f5d0fe"}}},
        {"Mint", "#04201c", {{0.25, 0.30, 0.55, "#14b8a6"}, {0.80, 0.75, 0.55, "#34d399"},
                             {0.60, 0.10, 0.40, "#22d3ee"}}},
        {"Coral", "#2a0d12", {{0.20, 0.25, 0.55, "#fb7185"}, {0.82, 0.30, 0.50, "#fdba74"},
                              {0.55, 0.85, 0.55, "#f43f5e"}}},
        {"Graphite", "#1c2128", {{0.25, 0.25, 0.60, "#475569"}, {0.80, 0.80, 0.55, "#334155"},
                                 {0.70, 0.20, 0.40, "#64748b"}}},
    };
    constexpr int kWallpaperCount = int(sizeof(kWallpapers) / sizeof(kWallpapers[0]));

    void paintMeshWallpaper(QPainter *p, const QRectF &rect, const WallpaperPreset &wp) {
        p->fillRect(rect, QColor(wp.base));
        const qreal span = qMax(rect.width(), rect.height());
        for (const Blob &b : wp.blobs) {
            const QPointF c(rect.left() + b.x * rect.width(), rect.top() + b.y * rect.height());
            QRadialGradient g(c, b.radius * span);
            QColor col(b.color);
            col.setAlpha(220);
            g.setColorAt(0.0, col);
            QColor edge(b.color);
            edge.setAlpha(0);
            g.setColorAt(1.0, edge);
            p->fillRect(rect, g);
        }
    }

    QString fillName(BackdropItem::Fill f) {
        switch (f) {
            case BackdropItem::Fill::Solid:     return QStringLiteral("Solid");
            case BackdropItem::Fill::Gradient:  return QStringLiteral("Gradient");
            case BackdropItem::Fill::Wallpaper: return QStringLiteral("Wallpaper");
        }
        return QStringLiteral("Gradient");
    }

    BackdropItem::Fill fillFromName(const QString &name) {
        return name == QLatin1String("Solid")     ? BackdropItem::Fill::Solid
             : name == QLatin1String("Wallpaper") ? BackdropItem::Fill::Wallpaper
                                                  : BackdropItem::Fill::Gradient;
    }
}

BackdropItem::BackdropItem()
{
    setZValue(-100);                 // behind the screenshot and all annotations
    setFlag(QGraphicsItem::ItemIsSelectable, false);
}

QStringList BackdropItem::gradientNames()
{
    QStringList names;
    for (int i = 0; i < kGradientCount; ++i)
        names << QString::fromLatin1(kGradients[i].name);
    return names;
}

QStringList BackdropItem::wallpaperNames()
{
    QStringList names;
    for (int i = 0; i < kWallpaperCount; ++i)
        names << QString::fromLatin1(kWallpapers[i].name);
    return names;
}

QPixmap BackdropItem::gradientPreview(int index, QSize size)
{
    QPixmap pm(size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    const GradientPreset &g = kGradients[qBound(0, index, kGradientCount - 1)];
    QLinearGradient grad(0, 0, size.width(), size.height());
    grad.setColorAt(0.0, QColor(g.c1));
    grad.setColorAt(1.0, QColor(g.c2));
    QPainterPath path;
    path.addRoundedRect(QRectF(0.5, 0.5, size.width() - 1.0, size.height() - 1.0), 5, 5);
    p.fillPath(path, grad);
    return pm;
}

QPixmap BackdropItem::wallpaperPreview(int index, QSize size)
{
    QPixmap pm(size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    QPainterPath clip;
    clip.addRoundedRect(QRectF(0, 0, size.width(), size.height()), 5, 5);
    p.setClipPath(clip);
    paintMeshWallpaper(&p, QRectF(0, 0, size.width(), size.height()),
                       kWallpapers[qBound(0, index, kWallpaperCount - 1)]);
    return pm;
}

void BackdropItem::setCanvasRect(const QRectF &rect)
{
    if (rect == m_canvasRect)
        return;
    prepareGeometryChange();
    m_canvasRect = rect;
    update();
}

void BackdropItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    if (m_canvasRect.isEmpty())
        return;

    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(Qt::NoPen);
    paintFill(painter, m_canvasRect);
}

void BackdropItem::paintFill(QPainter *painter, const QRectF &rect) const
{
    switch (m_fill) {
        case Fill::Solid:
            painter->fillRect(rect, m_solidColor);
            return;
        case Fill::Wallpaper:
            paintMeshWallpaper(painter, rect,
                               kWallpapers[qBound(0, m_wallpaperIndex, kWallpaperCount - 1)]);
            return;
        case Fill::Gradient:
            break;
    }
    const GradientPreset &g = kGradients[qBound(0, m_gradientIndex, kGradientCount - 1)];
    QLinearGradient grad(rect.topLeft(), rect.bottomRight());
    grad.setColorAt(0.0, m_customGradient ? m_gradStart : QColor(g.c1));
    grad.setColorAt(1.0, m_customGradient ? m_gradEnd : QColor(g.c2));
    painter->fillRect(rect, grad);
}

QVariantMap BackdropItem::toConfig() const
{
    QVariantMap c;
    c["fill"] = fillName(m_fill);
    c["solidColor"] = m_solidColor.name();
    c["gradientIndex"] = m_gradientIndex;
    c["customGradient"] = m_customGradient;
    c["gradStart"] = m_gradStart.name();
    c["gradEnd"] = m_gradEnd.name();
    c["wallpaperIndex"] = m_wallpaperIndex;
    c["padding"] = m_padding;
    c["cornerRadius"] = m_cornerRadius;
    c["shadowStrength"] = m_shadowStrength;
    return c;
}

void BackdropItem::applyConfig(const QVariantMap &c)
{
    m_fill = fillFromName(c.value("fill", "Gradient").toString());
    m_solidColor     = QColor(c.value("solidColor", m_solidColor.name()).toString());
    m_gradientIndex  = c.value("gradientIndex", m_gradientIndex).toInt();
    m_customGradient = c.value("customGradient", m_customGradient).toBool();
    m_gradStart      = QColor(c.value("gradStart", m_gradStart.name()).toString());
    m_gradEnd        = QColor(c.value("gradEnd", m_gradEnd.name()).toString());
    m_wallpaperIndex = c.value("wallpaperIndex", m_wallpaperIndex).toInt();
    m_padding        = c.value("padding", m_padding).toInt();
    m_cornerRadius   = c.value("cornerRadius", m_cornerRadius).toInt();
    m_shadowStrength = c.value("shadowStrength", m_shadowStrength).toInt();
    update();
    if (m_onChanged)
        m_onChanged("config");
}

BackdropMemento BackdropItem::createMemento() const
{
    // Only BackdropItem may build a memento. Captures the look (toConfig) plus the
    // active-preset name = the complete restorable state.
    return BackdropMemento(toConfig(), m_activePreset);
}

void BackdropItem::restore(const BackdropMemento &memento)
{
    // applyConfig() fires m_onChanged("config"), so the geometry/shadow re-apply and
    // panel/popover refresh happen for free on undo/redo.
    applyConfig(memento.m_config);
    m_activePreset = memento.m_activePreset;
}

QPixmap BackdropItem::configPreview(const QVariantMap &c, QSize size)
{
    QPixmap pm(size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QRectF rect(0, 0, size.width(), size.height());

    QPainterPath clip;
    clip.addRoundedRect(rect, 5, 5);
    p.setClipPath(clip);

    BackdropItem look;   // a scratch item reads the config exactly as applyConfig() does
    look.applyConfig(c);
    look.paintFill(&p, rect);
    p.setClipping(false);

    // Mini "screenshot" with a soft shadow to convey the look.
    const qreal inset = size.width() * 0.22;
    const QRectF shot = rect.adjusted(inset, inset, -inset, -inset);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 60));
    p.drawRoundedRect(shot.translated(0, 1.5), 3, 3);
    p.setBrush(Qt::white);
    p.drawRoundedRect(shot, 3, 3);
    return pm;
}

QList<ToolProperty> BackdropItem::getProperties() const
{
    QList<ToolProperty> props;

    // Preset picker (apply a saved backdrop). Rendered as a thumbnail grid; the
    // active preset is highlighted, empty value = none (diverged via manual edit).
    {
        const QVector<BackdropPreset> presets = BackdropPresets::all();
        QStringList names;
        QVariantList previews;
        for (const BackdropPreset &p : presets) {
            names << p.name;
            previews << QVariant::fromValue(configPreview(p.config, QSize(58, 40)));
        }
        props << ToolProperty{"preset", "Preset", m_activePreset, "swatches",
                              {{"items", names}, {"previews", previews}}};
    }

    props << ToolProperty{"fill", "Background", fillName(m_fill), "dropdown",
                          {{"items", QStringList{"Solid", "Gradient", "Wallpaper"}}}};

    if (m_fill == Fill::Solid) {
        props << ToolProperty{"color", "Color", m_solidColor, "color"};
    } else if (m_fill == Fill::Gradient) {
        const QStringList names = gradientNames();
        QVariantList previews;
        for (int i = 0; i < names.size(); ++i)
            previews << QVariant::fromValue(gradientPreview(i, QSize(58, 40)));
        // No preset highlighted while using manual colors.
        props << ToolProperty{"gradient", "Gradient",
                              m_customGradient ? QString() : names.value(m_gradientIndex),
                              "swatches", {{"items", names}, {"previews", previews}}};

        const GradientPreset &cur = kGradients[qBound(0, m_gradientIndex, kGradientCount - 1)];
        // A colorpair is two swatches side by side (Start → End).
        props << ToolProperty{"gradColors", "Custom colors", {}, "colorpair",
                              {{"startId", "gradStart"}, {"startName", "Start"},
                               {"startValue", m_customGradient ? m_gradStart : QColor(cur.c1)},
                               {"endId", "gradEnd"}, {"endName", "End"},
                               {"endValue", m_customGradient ? m_gradEnd : QColor(cur.c2)}}};
    } else if (m_fill == Fill::Wallpaper) {
        const QStringList names = wallpaperNames();
        QVariantList previews;
        for (int i = 0; i < names.size(); ++i)
            previews << QVariant::fromValue(wallpaperPreview(i, QSize(58, 40)));
        props << ToolProperty{"wallpaper", "Wallpaper", names.value(m_wallpaperIndex), "swatches",
                              {{"items", names}, {"previews", previews}}};
    }

    props << ToolProperty{"padding", "Padding", m_padding, "slider", {{"min", 0}, {"max", 256}}}
          << ToolProperty{"radius", "Corner radius", m_cornerRadius, "slider", {{"min", 0}, {"max", 64}}}
          << ToolProperty{"shadow", "Shadow", m_shadowStrength, "slider", {{"min", 0}, {"max", 100}}};

    return props;
}

void BackdropItem::setProperty(const QString &propertyId, const QVariant &value)
{
    // Applying a preset replaces the whole config; applyConfig() fires the change
    // callback itself, so notify once and return.
    if (propertyId == "preset") {
        const QVariantMap cfg = BackdropPresets::configFor(value.toString());
        if (!cfg.isEmpty()) {
            applyConfig(cfg);
            m_activePreset = value.toString();
        }
        return;
    }

    // Any individual edit diverges from the active preset.
    m_activePreset.clear();

    if (propertyId == "fill") {
        m_fill = fillFromName(value.toString());
        update();
    } else if (propertyId == "color") {
        m_solidColor = value.value<QColor>();
        update();
    } else if (propertyId == "gradient") {
        m_gradientIndex = qMax(0, gradientNames().indexOf(value.toString()));
        m_customGradient = false;   // picking a preset clears manual colors
        update();
    } else if (propertyId == "gradStart") {
        m_gradStart = value.value<QColor>();
        m_customGradient = true;
        update();
    } else if (propertyId == "gradEnd") {
        m_gradEnd = value.value<QColor>();
        m_customGradient = true;
        update();
    } else if (propertyId == "wallpaper") {
        m_wallpaperIndex = qMax(0, wallpaperNames().indexOf(value.toString()));
        update();
    } else if (propertyId == "padding") {
        m_padding = value.toInt();
    } else if (propertyId == "radius") {
        m_cornerRadius = value.toInt();
    } else if (propertyId == "shadow") {
        m_shadowStrength = value.toInt();
    }

    if (m_onChanged)
        m_onChanged(propertyId);
}

} // namespace Editor::Image
