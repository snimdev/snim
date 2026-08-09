#include "DrawingInteractions.h"
#include "../tools/ArrowTool.h"
#include "../tools/PathTool.h"
#include <QGraphicsEllipseItem>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QPen>
#include <QTransform>
#include <cmath>

namespace Editor::Interactions {

namespace {
constexpr int kMinRectSize = 5;          // smaller rectangles and ellipses are dropped
constexpr qreal kMinArrowLength = 10.0;  // shorter arrows are dropped
}

bool PointerInteraction::onMousePress(const QPointF &scenePos, QGraphicsScene *scene)
{
    if (!scene)
        return false;
    if (QGraphicsItem *item = scene->itemAt(scenePos, QTransform()))
        emit itemClicked(item->topLevelItem());
    // Only observes: the scene still does its own selection and dragging.
    return false;
}

ClickInteraction::ClickInteraction(Qt::CursorShape cursor, QObject *parent)
    : QObject(parent)
    , m_cursor(cursor)
{
}

bool ClickInteraction::onMousePress(const QPointF &scenePos, QGraphicsScene *)
{
    emit clicked(scenePos);
    return true;
}

bool DragInteraction::onMousePress(const QPointF &scenePos, QGraphicsScene *scene)
{
    if (!scene)
        return false;
    m_startPoint = scenePos;
    m_endPoint = scenePos;
    m_isDrawing = true;
    m_previewItem = createPreview(scenePos);
    if (m_previewItem)
        scene->addItem(m_previewItem);
    return true;
}

bool DragInteraction::onMouseMove(const QPointF &scenePos, QGraphicsScene *scene)
{
    if (!m_isDrawing || !scene)
        return false;
    m_endPoint = clampToImageBounds(scenePos);
    if (m_previewItem)
        updatePreview(m_endPoint);
    return true;
}

bool DragInteraction::onMouseRelease(const QPointF &scenePos, QGraphicsScene *scene)
{
    if (!m_isDrawing || !scene)
        return false;
    m_isDrawing = false;
    m_endPoint = clampToImageBounds(scenePos);
    const bool kept = finish();
    cleanup(scene);
    return kept;
}

void DragInteraction::cleanup(QGraphicsScene *scene)
{
    if (m_previewItem && scene) {
        scene->removeItem(m_previewItem);
        delete m_previewItem;
        m_previewItem = nullptr;
    }
    m_isDrawing = false;
}

QPointF DragInteraction::clampToImageBounds(const QPointF &point) const
{
    if (m_imageBounds.isNull())
        return point;
    return QPointF(qBound(qreal(m_imageBounds.left()), point.x(), qreal(m_imageBounds.right())),
                   qBound(qreal(m_imageBounds.top()), point.y(), qreal(m_imageBounds.bottom())));
}

RectDragInteraction::RectDragInteraction(Shape shape, QObject *parent)
    : DragInteraction(parent)
    , m_shape(shape)
{
}

QGraphicsItem *RectDragInteraction::createPreview(const QPointF &startPos)
{
    const QRectF rect(startPos, startPos);
    QAbstractGraphicsShapeItem *preview = m_shape == Ellipse
        ? static_cast<QAbstractGraphicsShapeItem*>(new QGraphicsEllipseItem(rect))
        : new QGraphicsRectItem(rect);
    preview->setPen(QPen(Qt::red, 2, Qt::DashLine));
    preview->setBrush(Qt::NoBrush);
    return preview;
}

void RectDragInteraction::updatePreview(const QPointF &pos)
{
    const QRectF rect = QRectF(m_startPoint, pos).normalized();
    if (auto *ellipse = qgraphicsitem_cast<QGraphicsEllipseItem*>(m_previewItem))
        ellipse->setRect(rect);
    else if (auto *box = qgraphicsitem_cast<QGraphicsRectItem*>(m_previewItem))
        box->setRect(rect);
}

bool RectDragInteraction::finish()
{
    const QRect rect = QRect(m_startPoint.toPoint(), m_endPoint.toPoint()).normalized();
    if (rect.width() < kMinRectSize || rect.height() < kMinRectSize)
        return false;
    emit rectDrawn(rect);
    return true;
}

QGraphicsItem *PathDragInteraction::createPreview(const QPointF &startPos)
{
    // Prototype: an empty clone of the template is already styled like the result.
    auto *path = dynamic_cast<Tools::PathTool*>(m_template ? m_template->clone() : nullptr);
    if (path)
        path->addPoint(startPos);
    return path;
}

void PathDragInteraction::updatePreview(const QPointF &pos)
{
    static_cast<Tools::PathTool*>(m_previewItem)->addPoint(pos);
}

bool PathDragInteraction::finish()
{
    // Never finishPath() the preview: for blur that runs the full blur on an item that
    // is about to be deleted. The committed item is built from the points instead.
    const auto *path = static_cast<Tools::PathTool*>(m_previewItem);
    if (!path || path->isEmpty())
        return false;
    emit pathDrawn(path->points());
    return true;
}

QGraphicsItem *ArrowInteraction::createPreview(const QPointF &startPos)
{
    auto *arrow = new Tools::ArrowTool(startPos.toPoint(), startPos.toPoint());
    arrow->applyStyleFrom(m_template);
    return arrow;
}

void ArrowInteraction::updatePreview(const QPointF &pos)
{
    static_cast<Tools::ArrowTool*>(m_previewItem)->setEndPoint(pos.toPoint());
}

bool ArrowInteraction::finish()
{
    const qreal dx = m_endPoint.x() - m_startPoint.x();
    const qreal dy = m_endPoint.y() - m_startPoint.y();
    if (std::sqrt(dx * dx + dy * dy) < kMinArrowLength)
        return false;
    emit arrowDrawn(m_startPoint.toPoint(), m_endPoint.toPoint());
    return true;
}

} // namespace Editor::Interactions
