#ifndef LAYERPROPERTIES_H
#define LAYERPROPERTIES_H

#include <QWidget>
#include <QGroupBox>
#include <QPushButton>

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
};

#endif // LAYERPROPERTIES_H
