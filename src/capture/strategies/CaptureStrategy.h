#ifndef CAPTURE_CAPTURESTRATEGY_H
#define CAPTURE_CAPTURESTRATEGY_H

#include <QList>
#include <QObject>
#include <QPixmap>
#include <QRect>
#include <QSharedPointer>
#include "editor/annotations/AnnotationSet.h"

namespace Screen {
class AreaSelector;
} // namespace Screen

namespace Capture {

class OverlayAnnotations;

/**
 * Abstract base class for different screenshot capture strategies
 */
class CaptureStrategy : public QObject
{
    Q_OBJECT

public:
    explicit CaptureStrategy(QObject *parent = nullptr) : QObject(parent) {}
    virtual ~CaptureStrategy() = default;

    // Pure virtual methods that each strategy must implement
    virtual void captureFullScreen() = 0;
    virtual void captureArea() = 0;
    virtual void captureWindow() = 0;

    // Get strategy name for debugging/logging
    virtual QString name() const = 0;

    // Whether the area selector should offer quick actions (Edit/Copy/Save toolbar).
    // On for normal capture; the OCR text-snip path turns it off.
    void setQuickActionsEnabled(bool enabled) { m_quickActionsEnabled = enabled; }
    [[nodiscard]] bool quickActionsEnabled() const { return m_quickActionsEnabled; }

signals:
    // annotations: overlay drawings the editor adds as layers over the plain pixmap.
    void screenshotReady(const QPixmap &pixmap, const Editor::AnnotationSet &annotations = {});
    void screenshotFailed(const QString &error);
    // The user backed out (Escape on the selector, a dismissed system dialog): no error.
    void screenshotCancelled();

protected:
    // Terminal actions for the selection toolbar, shared by every strategy: crop
    // the frozen frame to the selection, then copy it or save it. Callers own the
    // frame, so they pass it in; the overlays must be closed BEFORE saveAreaToFile,
    // or the file dialog opens behind a fullscreen overlay.
    void copyAreaToClipboard(const QPixmap &shot, const QRect &virtualGeometry, const QRect &area,
                             const QSharedPointer<OverlayAnnotations> &annotations = {});
    void saveAreaToFile(const QPixmap &shot, const QRect &virtualGeometry, const QRect &area,
                        const QSharedPointer<OverlayAnnotations> &annotations = {});

    // Edit: the plain crop, with the overlay's annotations handed over as editable layers.
    void emitSelection(const QPixmap &shot, const QRect &virtualGeometry, const QRect &area,
                       const QSharedPointer<OverlayAnnotations> &annotations);

    // The selection crop, with the overlay's annotations burned in when there are any.
    [[nodiscard]] static QPixmap cropWithAnnotations(const QPixmap &shot, const QRect &virtualGeometry,
                                                     const QRect &area,
                                                     const QSharedPointer<OverlayAnnotations> &annotations);

    // Area selection only: one session shared by all the selectors, null when quick
    // actions are off. Terminal handlers keep a copy until they have used it.
    QSharedPointer<OverlayAnnotations> attachAnnotations(const QList<Screen::AreaSelector*> &selectors,
                                                         const QPixmap &frame,
                                                         const QRect &virtualGeometry);

    // One selector per screen over the frozen frame; the pick is emitted like any capture.
    void showAreaSelector(const QPixmap &frame, const QRect &virtualGeometry);

    // A frame source's desktop: the selector over it, or straight out. A partial pick
    // made in a system dialog (a region, one monitor) goes out as it is, never stretched.
    void deliverFrame(const QPixmap &frame, const QRect &virtualGeometry, bool showSelector);

    // Every terminal action closes ALL the per-screen overlays first, so nothing is left
    // covering the screen or the save dialog.
    static void tearDownSelectors(QList<Screen::AreaSelector*> *selectors);

    bool m_quickActionsEnabled = true;
};

} // namespace Capture

#endif // CAPTURE_CAPTURESTRATEGY_H
