#ifndef SCREEN_SELECTORGROUP_H
#define SCREEN_SELECTORGROUP_H

#include "screen/AreaSelector.h"
#include "screen/WindowEnumerator.h"

#include <QList>
#include <QObject>
#include <QPixmap>
#include <QRect>
#include <QVector>

namespace Screen {

/**
 * One AreaSelector per screen over one frozen frame, for screenshots and recordings alike.
 * A Mediator between the overlays: the live selection on one is mirrored on the others, so
 * a selection spanning monitors shows on all of them, and whichever ends the selection
 * closes them all before the group passes its signal on, then the group deletes itself.
 */
class SelectorGroup : public QObject
{
    Q_OBJECT

public:
    struct Options {
        AreaSelector::Mode mode = AreaSelector::Mode::AreaSelect;
        QVector<WindowInfo> windows;   // WindowPick candidates, front to back
        bool actions = false;          // the Edit/Copy/Save toolbar
        bool layerSurface = false;     // a Wayland overlay layer over the panels, holding the keyboard
    };

    // Shows the overlays at once, in QGuiApplication::screens() order.
    SelectorGroup(const QPixmap &frame, const QRect &virtualGeometry, const Options &options,
                  QObject *parent = nullptr);
    ~SelectorGroup() override;

    [[nodiscard]] const QList<AreaSelector *> &selectors() const { return m_selectors; }

signals:
    // Sent once every overlay is closed; a window pick sends windowPicked, never areaSelected.
    void areaSelected(const QRect &area);
    void copyRequested(const QRect &area);
    void saveRequested(const QRect &area);
    void windowPicked(const QRect &area, quint64 windowId);
    // From the overlay the user works on, already mirrored on the others.
    void liveStateChanged(Screen::AreaSelector *selector, const QRect &selection, int phase);

private:
    void close();
    void tearDown();

    QList<AreaSelector *> m_selectors;
};

} // namespace Screen

#endif // SCREEN_SELECTORGROUP_H
