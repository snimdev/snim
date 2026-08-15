#ifndef SCREEN_OVERLAYSTYLE_H
#define SCREEN_OVERLAYSTYLE_H

#include <QColor>

// The look the selection overlay and the recording frame share.
namespace Screen::OverlayStyle {

inline constexpr int kDimAlpha = 120;           // darkening outside the selection
inline const QColor kAccent{0, 150, 255};       // selection border and handles

} // namespace Screen::OverlayStyle

#endif // SCREEN_OVERLAYSTYLE_H
