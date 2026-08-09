#include "PathTool.h"
#include <QPainter>
#include <QPainterPathStroker>
#include <QStyleOptionGraphicsItem>

namespace Editor::Tools {

PathTool::PathTool(QGraphicsItem *parent)
    : QGraphicsObject(parent)
{
    setFlags(QGraphicsItem::ItemIsSelectable |
             QGraphicsItem::ItemIsMovable |
             QGraphicsItem::ItemSendsGeometryChanges |
             QGraphicsItem::ItemIsFocusable);

    setAcceptHoverEvents(true);
    setCursor(Qt::SizeAllCursor);
}

void PathTool::addPoint(const QPointF &point)
{
    m_points.append(point);
    if (m_points.size() == 1)
        m_path.moveTo(point);
    else
        m_path.lineTo(point);
    updateGeometry();
}

void PathTool::addPoints(const QList<QPointF> &points)
{
    for (const QPointF &p : points)
        addPoint(p);
    finishPath();
}

void PathTool::updateGeometry()
{
    prepareGeometryChange();

    m_strokePath = QPainterPath();
    if (!m_path.isEmpty()) {
        const QPen pen = strokePen();
        QPainterPathStroker stroker;
        stroker.setCapStyle(pen.capStyle());
        stroker.setJoinStyle(pen.joinStyle());
        stroker.setWidth(pen.widthF());
        m_strokePath = stroker.createStroke(m_path);
    }

    const qreal padding = boundsPadding();
    m_boundingRect = m_strokePath.boundingRect().adjusted(-padding, -padding, padding, padding);

    update();
}

void PathTool::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *)
{
    if (m_path.isEmpty())
        return;

    painter->setRenderHint(QPainter::Antialiasing);
    paintPath(painter);

    if (option->state & QStyle::State_Selected) {
        painter->setPen(QPen(Qt::blue, 1.0, Qt::DashLine));
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(m_path.boundingRect());
    }
}

} // namespace Editor::Tools
