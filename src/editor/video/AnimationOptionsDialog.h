#ifndef EDITOR_VIDEO_ANIMATIONOPTIONSDIALOG_H
#define EDITOR_VIDEO_ANIMATIONOPTIONSDIALOG_H

#include "editor/video/AnimationParams.h"

#include <QDialog>

class QCheckBox;
class QSpinBox;

namespace Editor::Video {

/**
 * The knobs of one GIF or WebP export: frame rate, size cap, quality, and looping.
 * Fields it does not show (the WebP effort and size passes) carry over from the
 * initial params untouched.
 */
class AnimationOptionsDialog : public QDialog
{
    Q_OBJECT

public:
    AnimationOptionsDialog(AnimationFormat format, const AnimationParams &initial,
                           QWidget *parent = nullptr);

    [[nodiscard]] AnimationParams params() const;    // clamped
    [[nodiscard]] bool skipNextTime() const;         // "Don't ask again"

private:
    AnimationFormat m_format;
    AnimationParams m_initial;
    QSpinBox *m_fps = nullptr;
    QSpinBox *m_maxWidth = nullptr;
    QSpinBox *m_quality = nullptr;
    QSpinBox *m_loop = nullptr;
    QCheckBox *m_lossless = nullptr;
    QCheckBox *m_skip = nullptr;
};

} // namespace Editor::Video

#endif // EDITOR_VIDEO_ANIMATIONOPTIONSDIALOG_H
