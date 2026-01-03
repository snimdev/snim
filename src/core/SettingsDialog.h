#ifndef CORE_SETTINGSDIALOG_H
#define CORE_SETTINGSDIALOG_H

#include <QDialog>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QFileDialog>
#include <QStandardPaths>
#include <QColorDialog>
#include <QColor>
#include <QCheckBox>

namespace Core {

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);

private slots:
    void browseScreenshotFolder();
    void chooseForegroundColor();
    void chooseBackgroundColor();
    void applySettings();
    void resetSettings();

private:
    void setupUI();
    void setupGeneralTab();
    void setupRecordingTab();
    void setupUploadTab();
    void setupHotkeysTab();
    void loadSettings();
    void saveSettings();
    void updateForegroundButtonStyle();
    void updateBackgroundButtonStyle();

    // UI Components
    QTabWidget *m_tabWidget;

    // General Tab
    QWidget *m_generalTab;
    QLineEdit *m_screenshotFolderEdit;
    QPushButton *m_browseButton;
    QComboBox *m_imageFormatCombo;
    QPushButton *m_foregroundColorButton;
    QPushButton *m_backgroundColorButton;
    QColor m_foregroundColor;
    QColor m_backgroundColor;

    // Recording Tab
    QWidget *m_recordingTab;
    QCheckBox *m_cameraEnabledCheck;
    QComboBox *m_cameraCombo;
    QCheckBox *m_micEnabledCheck;
    QComboBox *m_micCombo;
    QCheckBox *m_systemAudioCheck;
    QCheckBox *m_frameCheck;   // "Highlight recorded area while recording"

    // Upload Tab
    QWidget *m_uploadTab;

    // Hotkeys Tab
    QWidget *m_hotkeysTab;

    // Dialog buttons
    QPushButton *m_applyButton;
    QPushButton *m_cancelButton;
    QPushButton *m_resetButton;
};

} // namespace Core

#endif // CORE_SETTINGSDIALOG_H
