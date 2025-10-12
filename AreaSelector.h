#ifndef AREASELECTOR_H
#define AREASELECTOR_H

#include <QWidget>
#include <QRect>
#include <QPoint>
#include <QPixmap>
#include <QRubberBand>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QPaintEvent>

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
    QPoint m_startPoint;
    QPoint m_endPoint;
    QRect m_selectedArea;
    bool m_selecting;
    QRubberBand *m_rubberBand;
    QPixmap m_screenshot;
};

#endif // AREASELECTOR_H
