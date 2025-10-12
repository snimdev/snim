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

namespace Core {

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);

private slots:
    void browseScreenshotFolder();
    void applySettings();
    void resetSettings();

private:
    void setupUI();
    void setupGeneralTab();
    void setupUploadTab();
    void setupHotkeysTab();
    void loadSettings();
    void saveSettings();

    // UI Components
    QTabWidget *m_tabWidget;

    // General Tab
    QWidget *m_generalTab;
    QLineEdit *m_screenshotFolderEdit;
    QPushButton *m_browseButton;
    QComboBox *m_imageFormatCombo;

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
