#ifndef CAPTURE_AREASELECTOR_H
#define CAPTURE_AREASELECTOR_H

#include <QWidget>
#include <QRect>
#include <QPoint>
#include <QPixmap>
#include <QRubberBand>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QPaintEvent>

namespace Capture {

class AreaSelector : public QWidget
{
    Q_OBJECT

public:
    explicit AreaSelector(QWidget *parent = nullptr);
    QRect selectedArea() const { return m_selectedArea; }
    void setScreenshot(const QPixmap &screenshot) { m_screenshot = screenshot; }

signals:
    void areaSelected(const QRect &area);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    bool m_selecting;
    QPoint m_startPoint;
    QPoint m_endPoint;
    QRect m_selectedArea;
    QRubberBand *m_rubberBand;
    QPixmap m_screenshot;
};

} // namespace Capture

#endif // CAPTURE_AREASELECTOR_H
