#include "AreaSelector.h"
#include <QPainter>
#include <QPainterPath>
#include <QApplication>
#include <QScreen>

namespace Capture {
    AreaSelector::AreaSelector(QWidget *parent)
        : QWidget(parent)
          , m_selecting(false)
          , m_rubberBand(nullptr) {
        setWindowFlags(Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint |
                       Qt::Tool | Qt::BypassWindowManagerHint);
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_DeleteOnClose);
        setCursor(Qt::CrossCursor);
        setMouseTracking(true);

        m_rubberBand = new QRubberBand(QRubberBand::Rectangle, this);

        // Style the rubber band
        QPalette palette;
        palette.setBrush(QPalette::Highlight, QBrush(QColor(0, 120, 215, 100)));
        m_rubberBand->setPalette(palette);
    }

    void AreaSelector::mousePressEvent(QMouseEvent *event) {
        if (event->button() == Qt::LeftButton) {
            // Convert local position to global virtual desktop coordinates
            m_startPoint = event->pos() + m_screenOffset;
            m_selecting = true;

            // Convert back to local widget coordinates for rubber band
            QPoint localStart = m_startPoint - m_screenOffset;
            m_rubberBand->setGeometry(QRect(localStart, QSize()));
            m_rubberBand->show();
        }
    }

    void AreaSelector::mouseMoveEvent(QMouseEvent *event) {
        if (m_selecting) {
            // Convert local position to global virtual desktop coordinates
            m_endPoint = event->pos() + m_screenOffset;

            // Convert back to local widget coordinates for rubber band display
            QPoint localStart = m_startPoint - m_screenOffset;
            QPoint localEnd = m_endPoint - m_screenOffset;
            QRect rect = QRect(localStart, localEnd).normalized();
            m_rubberBand->setGeometry(rect);
            update(); // Trigger repaint to show the selection
        }
    }

    void AreaSelector::mouseReleaseEvent(QMouseEvent *event) {
        if (event->button() == Qt::LeftButton && m_selecting) {
            m_selecting = false;
            // Convert local position to global virtual desktop coordinates
            m_endPoint = event->pos() + m_screenOffset;
            // Selected area is in global virtual desktop coordinates
            m_selectedArea = QRect(m_startPoint, m_endPoint).normalized();
            m_rubberBand->hide();

            emit areaSelected(m_selectedArea);
            close(); // Close the selector
        }
    }

    void AreaSelector::paintEvent(QPaintEvent *event) {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        if (!m_screenshot.isNull()) {
            // Draw the screenshot as background
            // The screenshot spans the entire virtual desktop
            qreal dpr = m_screenshot.devicePixelRatio();

            // Calculate which portion of the screenshot to draw for this screen
            // m_screenOffset is in logical coordinates, relative to virtual desktop
            // m_virtualGeometry is the full virtual desktop bounds
            QRect sourceRect;
            if (!m_virtualGeometry.isNull()) {
                // Convert screen offset to physical screenshot coordinates
                int srcX = (m_screenOffset.x() - m_virtualGeometry.x()) * dpr;
                int srcY = (m_screenOffset.y() - m_virtualGeometry.y()) * dpr;
                int srcW = width() * dpr;
                int srcH = height() * dpr;
                sourceRect = QRect(srcX, srcY, srcW, srcH);

                // Clamp to screenshot bounds
                sourceRect = sourceRect.intersected(m_screenshot.rect());
            } else {
                sourceRect = m_screenshot.rect();
            }

            // Draw this portion of the screenshot to fill the widget
            painter.drawPixmap(rect(), m_screenshot, sourceRect);

            // Add semi-transparent dark overlay
            painter.fillRect(rect(), QColor(0, 0, 0, 100));

            // Clear the selected area to show the original screenshot
            if (m_selecting && !m_rubberBand->geometry().isEmpty()) {
                QPainterPath path;
                path.addRect(rect());
                path.addRect(m_rubberBand->geometry());

                painter.setCompositionMode(QPainter::CompositionMode_Clear);
                painter.fillPath(path, Qt::transparent);
                painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

                // Redraw the selected area without overlay
                QRect selected = m_rubberBand->geometry();

                // Calculate source rect for the selected area
                int selSrcX = (m_screenOffset.x() - m_virtualGeometry.x() + selected.x()) * dpr;
                int selSrcY = (m_screenOffset.y() - m_virtualGeometry.y() + selected.y()) * dpr;
                int selSrcW = selected.width() * dpr;
                int selSrcH = selected.height() * dpr;
                QRect selectedSourceRect = QRect(selSrcX, selSrcY, selSrcW, selSrcH);
                selectedSourceRect = selectedSourceRect.intersected(m_screenshot.rect());

                painter.drawPixmap(selected, m_screenshot, selectedSourceRect);

                // Draw selection border
                painter.setPen(QPen(QColor(0, 120, 215), 2));
                painter.drawRect(selected);
            }
        } else {
            painter.fillRect(rect(), QColor(0, 0, 0, 150));
        }

        // Draw instructions
        painter.setPen(Qt::white);
        QFont font = painter.font();
        font.setPointSize(12);
        painter.setFont(font);

        QString instructions = m_selecting
                                   ? "Release to capture selected area"
                                   : "Click and drag to select area (ESC to cancel)";

        QRect textRect = painter.fontMetrics().boundingRect(instructions);
        textRect.moveCenter(QPoint(width() / 2, 30));

        // Draw text background
        painter.fillRect(textRect.adjusted(-10, -5, 10, 5),
                         QColor(0, 0, 0, 180));
        painter.drawText(textRect, Qt::AlignCenter, instructions);
    }

    void AreaSelector::keyPressEvent(QKeyEvent *event) {
        if (event->key() == Qt::Key_Escape) {
            emit areaSelected(QRect()); // Empty rect signals cancellation
            close();
        }
    }
} // namespace Capture
