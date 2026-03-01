#include "editor/video/AnimationOptionsDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QSpinBox>

namespace Editor::Video {

AnimationOptionsDialog::AnimationOptionsDialog(AnimationFormat format,
                                               const AnimationParams &initial, QWidget *parent)
    : QDialog(parent), m_format(format), m_initial(initial.clamped())
{
    setWindowTitle(format == AnimationFormat::WebP ? tr("WebP Options") : tr("GIF Options"));

    auto *form = new QFormLayout(this);

    m_fps = new QSpinBox(this);
    m_fps->setObjectName(QStringLiteral("fps"));
    m_fps->setRange(1, 50);
    m_fps->setSuffix(tr(" fps"));
    m_fps->setValue(m_initial.fps);
    form->addRow(tr("Frame rate:"), m_fps);

    m_maxWidth = new QSpinBox(this);
    m_maxWidth->setObjectName(QStringLiteral("maxWidth"));
    m_maxWidth->setRange(0, 7680);
    m_maxWidth->setSingleStep(10);
    m_maxWidth->setSuffix(tr(" px"));
    m_maxWidth->setSpecialValueText(tr("Original"));
    m_maxWidth->setValue(std::min(m_initial.maxWidth, 7680));
    form->addRow(tr("Max size:"), m_maxWidth);

    m_quality = new QSpinBox(this);
    m_quality->setObjectName(QStringLiteral("quality"));
    m_quality->setRange(0, 100);
    m_quality->setValue(m_initial.quality);
    form->addRow(tr("Quality:"), m_quality);
    form->setRowVisible(m_quality, format == AnimationFormat::WebP);   // the GIF encoders take no quality

    m_lossless = new QCheckBox(tr("Lossless"), this);
    m_lossless->setObjectName(QStringLiteral("lossless"));
    connect(m_lossless, &QCheckBox::toggled, m_quality, &QWidget::setDisabled);
    m_lossless->setChecked(format == AnimationFormat::WebP && m_initial.lossless);
    form->addRow(m_lossless);
    form->setRowVisible(m_lossless, format == AnimationFormat::WebP);

    m_loop = new QSpinBox(this);
    m_loop->setObjectName(QStringLiteral("loop"));
    m_loop->setRange(0, 1000);
    m_loop->setSpecialValueText(tr("Forever"));
    m_loop->setValue(std::min(m_initial.loopCount, 1000));
    form->addRow(tr("Loop:"), m_loop);

    m_skip = new QCheckBox(tr("Don't ask again"), this);
    m_skip->setObjectName(QStringLiteral("skip"));
    form->addRow(m_skip);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    form->addRow(buttons);
}

AnimationParams AnimationOptionsDialog::params() const
{
    AnimationParams p = m_initial;
    p.fps = m_fps->value();
    p.maxWidth = m_maxWidth->value();
    p.quality = m_quality->value();
    p.lossless = m_format == AnimationFormat::WebP && m_lossless->isChecked();
    p.loopCount = m_loop->value();
    return p.clamped();
}

bool AnimationOptionsDialog::skipNextTime() const
{
    return m_skip->isChecked();
}

} // namespace Editor::Video
