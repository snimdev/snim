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
#include <QListWidget>
#include <QVector>
#include <QHash>

#include "upload/UploadProfiles.h"

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
    // Upload profiles UI helpers (work on the in-memory working copy).
    void refreshUploadList();                  // rebuild the list from m_uploadWorking
    void bindUploadForm(int row);              // load working[row] into the field widgets
    void flushUploadForm(int row);             // write the field widgets back into working[row]
    void onUploadSelectionChanged(int row);
    void onUploadAddProfile();
    void onUploadRemoveProfile();
    void onUploadSetDefault();
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

    // Upload Tab (S3-compatible, multi-destination). The secret access key is NOT
    // persisted to QSettings — it goes to the OS keychain (keyed by profile id) on Apply.
    // The form binds to the selected profile in an in-memory working copy; nothing is
    // persisted until Apply.
    QWidget *m_uploadTab;
    QCheckBox *m_uploadEnabledCheck = nullptr;
    QListWidget *m_uploadList = nullptr;
    QPushButton *m_uploadAddButton = nullptr;
    QPushButton *m_uploadRemoveButton = nullptr;
    QPushButton *m_uploadDefaultButton = nullptr;
    QWidget *m_uploadForm = nullptr;             // the field group; disabled when no selection
    QLineEdit *m_uploadNameEdit = nullptr;
    QLineEdit *m_uploadEndpointEdit = nullptr;
    QLineEdit *m_uploadRegionEdit = nullptr;
    QLineEdit *m_uploadBucketEdit = nullptr;
    QLineEdit *m_uploadAccessKeyEdit = nullptr;
    QLineEdit *m_uploadSecretEdit = nullptr;     // write-only into the keychain
    QLineEdit *m_uploadPrefixEdit = nullptr;
    QLineEdit *m_uploadPublicUrlEdit = nullptr;
    QCheckBox *m_uploadPathStyleCheck = nullptr;

    QVector<Upload::UploadProfile> m_uploadWorking;   // edited copy; committed on Apply
    QString m_uploadDefaultId;                        // working default profile id
    QHash<QString, QString> m_uploadNewSecrets;       // profile id -> newly entered secret
    int m_uploadCurrentRow = -1;                      // row currently bound to the form

    // Hotkeys Tab
    QWidget *m_hotkeysTab;

    // Dialog buttons
    QPushButton *m_applyButton;
    QPushButton *m_cancelButton;
    QPushButton *m_resetButton;
};

} // namespace Core

#endif // CORE_SETTINGSDIALOG_H
