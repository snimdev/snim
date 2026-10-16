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
#include <QStackedWidget>
#include <QVector>
#include <QHash>
#include <QKeySequenceEdit>
#include <QPointer>
#include <QGroupBox>
#include <QToolButton>

#include "hotkeys/HotkeyAction.h"
#include "upload/UploadProfiles.h"

namespace Hotkeys {
class GlobalHotkeyManager;
class ScreenshotKeySwap;
}
namespace Upload { class Uploader; }

namespace App {

class ProviderForm;

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    // hotkeyManager, when given, lets each hotkey row show why its binding did not register;
    // screenshotKey adds the section that lends the OS screenshot key to Snim.
    explicit SettingsDialog(Hotkeys::GlobalHotkeyManager *hotkeyManager = nullptr,
                            Hotkeys::ScreenshotKeySwap *screenshotKey = nullptr,
                            QWidget *parent = nullptr);

signals:
    // Emitted after Apply has persisted everything, before the dialog closes.
    void settingsApplied();

private slots:
    void applySettings();
    void resetSettings();
    void validateHotkeys();
    void refreshHotkeyNotices();
    void onScreenshotKeyClicked();

private:
    void setupUI();
    void setupGeneralTab();
    void setupRecordingTab();
    void setupUploadTab();
    void setupHotkeysTab();
    void setupScreenshotKeySection(QVBoxLayout *layout);
    // note: what the last swap or undo said, shown under the status.
    void refreshScreenshotKeySection(const QString &note = QString());
    // Runs the swap (take) or its undo, then shows the bindings it stored.
    void changeScreenshotKey(bool take);
    // A row offers the swap when the preset puts seq on its action.
    [[nodiscard]] bool screenshotKeyOffersSwap(Hotkeys::HotkeyAction action,
                                               const QKeySequence &seq) const;
    void loadSettings();
    // Empty when every staged secret reached the keychain; otherwise what to tell the user.
    QString saveSettings();
    // Upload profiles UI helpers (work on the in-memory working copy).
    void refreshUploadList();                  // rebuild the list from m_uploadWorking
    void bindUploadForm(int row);              // load working[row] into the field widgets
    void flushUploadForm(int row);             // write the field widgets back into working[row]
    [[nodiscard]] ProviderForm *uploadPageFor(const QString &type) const;   // null if none
    void onUploadSelectionChanged(int row);
    // The provider type is fixed at creation, so it is an argument of Add, not a field.
    void onUploadAddProfile(const QString &type);
    void onUploadRemoveProfile();
    void onUploadSetDefault();
    // Test the destination as currently typed (saved or not) by uploading a probe file
    // and deleting it again. Never writes to the keychain - only Apply does.
    void onUploadTestConnection();
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

    // Upload Tab (multi-destination, one page per Upload::ProviderSpec). The secret is NOT
    // persisted to QSettings - it goes to the OS keychain (service = the profile type's,
    // account = the profile id) on Apply. The form binds to the selected profile in an
    // in-memory working copy; nothing is persisted until Apply.
    QWidget *m_uploadTab = nullptr;
    QCheckBox *m_uploadEnabledCheck = nullptr;
    QListWidget *m_uploadList = nullptr;
    QPushButton *m_uploadAddButton = nullptr;
    QMenu *m_uploadAddMenu = nullptr;            // one entry per provider
    QPushButton *m_uploadRemoveButton = nullptr;
    QPushButton *m_uploadDefaultButton = nullptr;
    QWidget *m_uploadForm = nullptr;             // the field group; disabled when no selection
    QLineEdit *m_uploadNameEdit = nullptr;
    QLabel *m_uploadTypeLabel = nullptr;         // read-only: type is fixed at creation
    QLabel *m_uploadUnavailableLabel = nullptr;  // shown when the type wasn't built in
    QFormLayout *m_uploadTopForm = nullptr;
    int m_uploadWarningRow = -1;                 // row of m_uploadUnavailableLabel
    QStackedWidget *m_uploadStack = nullptr;     // m_uploadPages
    QVector<ProviderForm *> m_uploadPages;       // one per provider, in registry order

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
        QLabel *notice;   // under the edit, hidden unless the key is taken or failed
        QToolButton *swapButton;   // beside the notice, when the screenshot key swap frees it
    };

    QWidget *m_hotkeysTab = nullptr;
    QVector<HotkeyRow> m_hotkeyRows;
    QLabel *m_hotkeyConflictLabel = nullptr;   // hidden unless two rows collide
    QLabel *m_hotkeyFailureLabel = nullptr;    // replaces the row notices when none registered
    QPointer<Hotkeys::GlobalHotkeyManager> m_hotkeyManager;
    Hotkeys::ScreenshotKeySwap *m_screenshotKey = nullptr;   // owned by Application
    QGroupBox *m_screenshotKeyGroup = nullptr;
    QLabel *m_screenshotKeyStatus = nullptr;
    QPushButton *m_screenshotKeyButton = nullptr;

    // Dialog buttons
    QPushButton *m_applyButton = nullptr;
    QPushButton *m_cancelButton = nullptr;
    QPushButton *m_resetButton = nullptr;
};

} // namespace App

#endif // APP_SETTINGSDIALOG_H
