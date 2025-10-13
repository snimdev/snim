#ifndef IMAGEEDITOR_LAYERPROPERTIES_H
#define IMAGEEDITOR_LAYERPROPERTIES_H

#include <QWidget>
#include <QGroupBox>
#include <QPushButton>
#include <QComboBox>
#include <QButtonGroup>
#include <QRadioButton>

namespace ImageEditor {

class Layer;

class LayerProperties : public QWidget
{
    Q_OBJECT

public:
    explicit LayerProperties(QWidget *parent = nullptr);
    void setLayer(Layer *layer);

private slots:
    void onBackgroundColorButtonClicked();
    void onTextColorButtonClicked();
    void onArrowColorButtonClicked();
    void onArrowSizeChanged(int index);
    void onArrowHeadTypeChanged();

private:
    void setupUI();
    void updatePropertiesForLayer();
    QPushButton* createColorButton(const QColor &color);

    Layer *m_currentLayer;

    // UI elements
    QGroupBox *m_backgroundGroup;
    QPushButton *m_backgroundColorButton;

    QGroupBox *m_textGroup;
    QPushButton *m_textColorButton;

    QGroupBox *m_arrowGroup;
    QPushButton *m_arrowColorButton;
    QComboBox *m_arrowSizeCombo;
    QButtonGroup *m_arrowHeadTypeGroup;
    QRadioButton *m_outlinedArrowHead;
    QRadioButton *m_filledArrowHead;
};

} // namespace ImageEditor

#endif // IMAGEEDITOR_LAYERPROPERTIES_H
