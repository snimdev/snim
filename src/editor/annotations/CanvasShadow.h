#ifndef EDITOR_CANVASSHADOW_H
#define EDITOR_CANVASSHADOW_H

#include <QList>
#include <QPixmap>
#include <QRect>

class QPainter;

namespace Editor {

/**
 * Soft drop shadow behind the editor canvas, the same size on screen at any zoom.
 * Flyweight: one small tile per device pixel ratio and theme, nine-sliced around
 * any canvas, so painting never blurs. View chrome only: exports never include it.
 */
class CanvasShadow
{
public:
    struct Slice
    {
        QRect target;   // device pixels
        QRect source;   // tile pixels
    };

    /// Pieces of a (4 * margin + 1) px tile, its canvas inset by margin, laid around
    /// `canvas`; corners reach margin px inside it (meeting halfway on a small one).
    static QList<Slice> slices(const QRect &canvas, int margin);

    void setDark(bool dark) { m_dark = dark; }

    /// canvas and exposed are in the painter's current (scene) coordinates.
    void paint(QPainter *painter, const QRectF &canvas, const QRectF &exposed);

private:
    void rebuild(qreal dpr);

    QPixmap m_tile;           // device pixels
    qreal m_tileDpr = 0;
    bool m_tileDark = false;
    int m_margin = 0;         // device pixels
    bool m_dark = false;
};

} // namespace Editor

#endif // EDITOR_CANVASSHADOW_H
