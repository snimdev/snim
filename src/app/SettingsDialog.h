#ifndef APP_SETTINGSDIALOG_H
#define APP_SETTINGSDIALOG_H

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
#include <QMenu>
#include <QSpinBox>
#include <QStackedWidget>
#include <QVector>
#include <QHash>
#include <QKeySequenceEdit>
#include <QPointer>

#include "hotkeys/HotkeyAction.h"
#include "upload/UploadProfiles.h"

namespace Hotkeys { class GlobalHotkeyManager; }
namespace Upload { class Uploader; }

namespace App {

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    // hotkeyManager, when given, lets each hotkey row show why its binding did not register.
    explicit SettingsDialog(Hotkeys::GlobalHotkeyManager *hotkeyManager = nullptr,
                            QWidget *parent = nullptr);

signals:
    // Emitted after Apply has persisted everything, before the dialog closes.
    void settingsApplied();

private slots:
    void applySettings();
    void resetSettings();
    void validateHotkeys();
    void refreshHotkeyNotices();

private:
    void setupUI();
    void setupGeneralTab();
    void setupRecordingTab();
    void setupUploadTab();
    void setupHotkeysTab();
    // One page of the provider stack; each owns the fields of a single provider type.
    QWidget *createUploadS3Page();
    QWidget *createUploadSftpPage();
    QWidget *createUploadFtpPage();
    void loadSettings();
    // Empty when every staged secret reached the keychain; otherwise what to tell the user.
    QString saveSettings();
    // Upload profiles UI helpers (work on the in-memory working copy).
    void refreshUploadList();                  // rebuild the list from m_uploadWorking
    void bindUploadForm(int row);              // load working[row] into the field widgets
    void flushUploadForm(int row);             // write the field widgets back into working[row]
    void clearUploadPages();                   // blank every provider page's fields
    void onUploadSelectionChanged(int row);
    // The provider type is fixed at creation, so it is an argument of Add, not a field.
    void onUploadAddProfile(Upload::ProviderType type);
    void onUploadRemoveProfile();
    void onUploadSetDefault();
    // Test the destination as currently typed (saved or not) by uploading a probe file
    // and deleting it again. Never writes to the keychain - only Apply does.
    void onUploadTestConnection();
    // Auth-mode combo -> secret row label + private-key row visibility. Pure UI: it never
    // touches m_uploadWorking (the working copy is flushed on selection change / Apply).
    void updateSftpAuthMode();
    [[nodiscard]] QLineEdit *secretEditFor(Upload::ProviderType type) const;
    void pickColor(QColor &color, const QString &title, QColorDialog::ColorDialogOptions options);
    void updateColorButtons();                 // paint both swatches from the two colors

    // UI Components
    QTabWidget *m_tabWidget = nullptr;

    // General Tab
    QWidget *m_generalTab = nullptr;
    QLineEdit *m_screenshotFolderEdit = nullptr;
    QPushButton *m_browseButton = nullptr;
    QComboBox *m_imageFormatCombo = nullptr;
    QPushButton *m_foregroundColorButton = nullptr;
    QPushButton *m_backgroundColorButton = nullptr;
    QColor m_foregroundColor = Qt::red;
    QColor m_backgroundColor = Qt::transparent;

    // Recording Tab
    QWidget *m_recordingTab = nullptr;
    QCheckBox *m_cameraEnabledCheck = nullptr;
    QComboBox *m_cameraCombo = nullptr;
    QCheckBox *m_micEnabledCheck = nullptr;
    QComboBox *m_micCombo = nullptr;
    QCheckBox *m_systemAudioCheck = nullptr;
    QCheckBox *m_frameCheck = nullptr;   // "Highlight recorded area while recording"

    // Upload Tab (multi-destination: S3-compatible / SFTP / FTP). The secret is NOT
    // persisted to QSettings - it goes to the OS keychain (service = the profile type's,
    // account = the profile id) on Apply. The form binds to the selected profile in an
    // in-memory working copy; nothing is persisted until Apply.
    QWidget *m_uploadTab = nullptr;
    QCheckBox *m_uploadEnabledCheck = nullptr;
    QListWidget *m_uploadList = nullptr;
    QPushButton *m_uploadAddButton = nullptr;
    QMenu *m_uploadAddMenu = nullptr;            // one entry per provider type
    QPushButton *m_uploadRemoveButton = nullptr;
    QPushButton *m_uploadDefaultButton = nullptr;
    QWidget *m_uploadForm = nullptr;             // the field group; disabled when no selection
    QLineEdit *m_uploadNameEdit = nullptr;
    QLabel *m_uploadTypeLabel = nullptr;         // read-only: type is fixed at creation
    QLabel *m_uploadUnavailableLabel = nullptr;  // shown when the type wasn't built in
    QFormLayout *m_uploadTopForm = nullptr;
    int m_uploadWarningRow = -1;                 // row of m_uploadUnavailableLabel
    QStackedWidget *m_uploadStack = nullptr;     // S3 / SFTP / FTP pages

    // S3 page
    QLineEdit *m_uploadEndpointEdit = nullptr;
    QLineEdit *m_uploadRegionEdit = nullptr;
    QLineEdit *m_uploadBucketEdit = nullptr;
    QLineEdit *m_uploadAccessKeyEdit = nullptr;
    QLineEdit *m_uploadSecretEdit = nullptr;     // write-only into the keychain
    QLineEdit *m_uploadPrefixEdit = nullptr;
    QCheckBox *m_uploadPathStyleCheck = nullptr;

    // SFTP page
    QFormLayout *m_sftpForm = nullptr;
    QLineEdit *m_sftpHostEdit = nullptr;
    QSpinBox *m_sftpPortSpin = nullptr;
    QLineEdit *m_sftpUserEdit = nullptr;
    QComboBox *m_sftpAuthCombo = nullptr;
    QLabel *m_sftpSecretLabel = nullptr;         // "Password:" / "Key passphrase (optional):"
    QLineEdit *m_sftpSecretEdit = nullptr;       // write-only into the keychain
    QLineEdit *m_sftpKeyPathEdit = nullptr;
    QPushButton *m_sftpKeyBrowseButton = nullptr;
    int m_sftpKeyPathRow = -1;                   // hidden in password auth mode
    QLineEdit *m_sftpRemoteDirEdit = nullptr;

    // FTP page
    QLineEdit *m_ftpHostEdit = nullptr;
    QSpinBox *m_ftpPortSpin = nullptr;
    QLineEdit *m_ftpUserEdit = nullptr;
    QLineEdit *m_ftpSecretEdit = nullptr;        // write-only into the keychain
    QComboBox *m_ftpEncryptionCombo = nullptr;
    QLineEdit *m_ftpRemoteDirEdit = nullptr;

    // Shared by every provider type
    QLineEdit *m_uploadPublicUrlEdit = nullptr;
    QPushButton *m_uploadTestButton = nullptr;
    QLabel *m_uploadTestStatusLabel = nullptr;    // result of the last test, colored
    // Alive only while a test runs; parented to the dialog, so closing Settings during a
    // test destroys it, which is what tells its worker to abort.
    Upload::Uploader *m_uploadTester = nullptr;

    QVector<Upload::UploadProfile> m_uploadWorking;   // edited copy; committed on Apply
    QString m_uploadDefaultId;                        // working default profile id
    QHash<QString, QString> m_uploadNewSecrets;       // profile id -> newly entered secret
    int m_uploadCurrentRow = -1;                      // row currently bound to the form

    // Hotkeys Tab - one row per Hotkeys::HotkeyAction, in enum order.
    struct HotkeyRow {
        Hotkeys::HotkeyAction action;
        QKeySequenceEdit *edit;
        QLabel *notice;   // under the edit, hidden unless the saved binding failed
    };

    QWidget *m_hotkeysTab = nullptr;
    QVector<HotkeyRow> m_hotkeyRows;
    QLabel *m_hotkeyConflictLabel = nullptr;   // hidden unless two rows collide
    QLabel *m_hotkeyFailureLabel = nullptr;    // replaces the row notices when none registered
    QPointer<Hotkeys::GlobalHotkeyManager> m_hotkeyManager;

    // Dialog buttons
    QPushButton *m_applyButton = nullptr;
    QPushButton *m_cancelButton = nullptr;
    QPushButton *m_resetButton = nullptr;
};

} // namespace App

#endif // APP_SETTINGSDIALOG_H
