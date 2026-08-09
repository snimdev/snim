#ifndef IMAGEEDITOR_PATHTOOL_H
#define IMAGEEDITOR_PATHTOOL_H

#include "ToolItem.h"
#include <QList>
#include <QPainterPath>
#include <QPen>
#include <QPointF>

namespace Editor::Tools {

// A stroke painted point by point (freehand, highlight, blur): the points, their path
// and its widened hit area. A sibling base, so a FreehandTool cast never matches the others.
class PathTool : public ToolItem
{
    Q_OBJECT

public:
    QRectF boundingRect() const override { return m_boundingRect; }
    QPainterPath shape() const override { return m_strokePath; }
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) override;

    virtual void addPoint(const QPointF &point);
    virtual void finishPath() { updateGeometry(); }
    // Adds every point, then finishes the path.
    void addPoints(const QList<QPointF> &points);
    [[nodiscard]] bool isEmpty() const { return m_points.isEmpty(); }
    [[nodiscard]] QList<QPointF> points() const { return m_points; }

protected:
    explicit PathTool(QGraphicsItem *parent);

    // Its width, cap and join shape the hit area.
    [[nodiscard]] virtual QPen strokePen() const = 0;
    [[nodiscard]] virtual qreal boundsPadding() const { return 10.0; }
    // Called with antialiasing on and a non-empty path.
    virtual void paintPath(QPainter *painter) = 0;
    void updateGeometry();

    QPainterPath m_path;
    QPainterPath m_strokePath;

private:
    QList<QPointF> m_points;
    QRectF m_boundingRect;
};

} // namespace Editor::Tools

#endif // IMAGEEDITOR_PATHTOOL_H
