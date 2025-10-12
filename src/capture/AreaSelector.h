// AreaSelector.h
#ifndef AREASELECTOR_H
#define AREASELECTOR_H

#include <QWidget>
#include <QRubberBand>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QPixmap>

namespace Capture {

    class AreaSelector : public QWidget
    {
        Q_OBJECT

    public:
        explicit AreaSelector(QWidget *parent = nullptr);

        void setScreenshot(const QPixmap &screenshot) {
            m_screenshot = screenshot;
            update();
        }

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

} // namespace Capture

#endif // AREASELECTOR_H