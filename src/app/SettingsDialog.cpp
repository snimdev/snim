#include "app/SettingsDialog.h"
#include "core/Settings.h"
#include "core/KeychainStore.h"
#include "hotkeys/GlobalHotkeyManager.h"
#include "hotkeys/HotkeyBackendFactory.h"
#include "hotkeys/HotkeyBindings.h"
#include "upload/UploadProfiles.h"
#include "upload/UploadConfig.h"
#include "upload/Uploader.h"
#include "upload/UploaderFactory.h"
#if defined(Q_OS_LINUX) && defined(SNIM_HAVE_LINUX_RECORDER)
#include "screen/sources/ScreencastFrameSource.h"
#endif
#include <QAction>
#include <QGroupBox>
#include <QMessageBox>
#include <QSet>
#include <QToolButton>
#include <QSignalBlocker>
#include <QStandardPaths>
#include <QDir>
#include <QMediaDevices>
#include <QCameraDevice>
#include <QAudioDevice>
#include <QApplication>
#include <QPermissions>
#include <algorithm>

namespace App {

SettingsDialog::SettingsDialog(Hotkeys::GlobalHotkeyManager *hotkeyManager, QWidget *parent)
    : QDialog(parent), m_hotkeyManager(hotkeyManager)
{
    setWindowTitle("Settings");
    setModal(true);
    resize(500, 400);

    setupUI();
    loadSettings();

    // The dialog is the context: the connection dies with it, the manager lives on.
    if (m_hotkeyManager)
        connect(m_hotkeyManager, &Hotkeys::GlobalHotkeyManager::failuresChanged,
                this, &SettingsDialog::refreshHotkeyNotices);
}

void SettingsDialog::setupUI()
{
    auto *mainLayout = new QVBoxLayout(this);

    // Create tab widget
    m_tabWidget = new QTabWidget(this);

    // Setup tabs
    setupGeneralTab();
    setupRecordingTab();
    setupUploadTab();
    setupHotkeysTab();

    mainLayout->addWidget(m_tabWidget);

    // Dialog buttons
    auto *buttonLayout = new QHBoxLayout();

    m_resetButton = new QPushButton("Reset to Defaults", this);
    m_cancelButton = new QPushButton("Cancel", this);
    m_applyButton = new QPushButton("Apply", this);

    m_applyButton->setDefault(true);

    buttonLayout->addWidget(m_resetButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_cancelButton);
    buttonLayout->addWidget(m_applyButton);

    mainLayout->addLayout(buttonLayout);

    // Connect signals
    connect(m_applyButton, &QPushButton::clicked, this, &SettingsDialog::applySettings);
    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_resetButton, &QPushButton::clicked, this, &SettingsDialog::resetSettings);
}

void SettingsDialog::setupGeneralTab()
{
    m_generalTab = new QWidget();

    auto *layout = new QVBoxLayout(m_generalTab);

    // Screenshot Settings Group
    auto *screenshotGroup = new QGroupBox("Screenshot Settings", m_generalTab);
    auto *screenshotLayout = new QFormLayout(screenshotGroup);

    // Screenshot folder setting
    auto *folderLayout = new QHBoxLayout();
    m_screenshotFolderEdit = new QLineEdit(screenshotGroup);
    m_browseButton = new QPushButton("Browse...", screenshotGroup);
    m_browseButton->setMaximumWidth(80);

    folderLayout->addWidget(m_screenshotFolderEdit);
    folderLayout->addWidget(m_browseButton);
    connect(m_browseButton, &QPushButton::clicked, this, [this] {
        const QString current = m_screenshotFolderEdit->text();
        const QString folder = QFileDialog::getExistingDirectory(
            this, "Select Screenshot Folder",
            current.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)
                              : current,
            QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
        if (!folder.isEmpty())
            m_screenshotFolderEdit->setText(folder);
    });

    screenshotLayout->addRow("Screenshot Folder:", folderLayout);

    // Image format setting
    m_imageFormatCombo = new QComboBox(screenshotGroup);
    m_imageFormatCombo->addItem("PNG", "png");
    m_imageFormatCombo->addItem("JPG", "jpg");

    screenshotLayout->addRow("Image Format:", m_imageFormatCombo);

#if defined(Q_OS_LINUX) && defined(SNIM_HAVE_LINUX_RECORDER)
    // The ScreenCast portal remembers the screens picked once; this makes it ask again.
    auto *resetPickButton = new QPushButton(tr("Reset screen-sharing choice"), screenshotGroup);
    resetPickButton->setToolTip(tr("Forget which screens Snim may capture, so the next "
                                   "screenshot asks again."));
    resetPickButton->setEnabled(Screen::ScreencastFrameSource::remembersScreenPick());
    connect(resetPickButton, &QPushButton::clicked, this, [resetPickButton] {
        Screen::ScreencastFrameSource::forgetScreenPicks();
        resetPickButton->setEnabled(false);
        resetPickButton->setText(tr("Screen-sharing choice reset"));
    });
    screenshotLayout->addRow(tr("Screen sharing:"), resetPickButton);
#endif

    layout->addWidget(screenshotGroup);

    // Editor Settings Group
    auto *editorGroup = new QGroupBox("Editor Settings", m_generalTab);
    auto *editorLayout = new QFormLayout(editorGroup);

    // Swatch buttons for the stroke/text color and the fill color (which may be
    // transparent); loadSettings() paints them.
    const auto swatch = [editorGroup] {
        auto *button = new QPushButton(editorGroup);
        button->setMaximumWidth(100);
        button->setMinimumHeight(30);
        return button;
    };
    m_foregroundColorButton = swatch();
    editorLayout->addRow("Foreground Color:", m_foregroundColorButton);
    m_backgroundColorButton = swatch();
    editorLayout->addRow("Background Color:", m_backgroundColorButton);
    connect(m_foregroundColorButton, &QPushButton::clicked, this, [this] {
        pickColor(m_foregroundColor, "Choose Foreground Color", {});
    });
    connect(m_backgroundColorButton, &QPushButton::clicked, this, [this] {
        pickColor(m_backgroundColor, "Choose Background Color", QColorDialog::ShowAlphaChannel);
    });

    layout->addWidget(editorGroup);
    layout->addStretch();

    m_tabWidget->addTab(m_generalTab, "General");
}

void SettingsDialog::setupRecordingTab()
{
    // One entry per device, or a placeholder holding no id.
    const auto fillDevices = [](QComboBox *combo, const auto &devices, const char *none) {
        for (const auto &device : devices)
            combo->addItem(device.description(), device.id());
        if (combo->count() == 0)
            combo->addItem(none, QByteArray());
    };

    m_recordingTab = new QWidget();
    auto *layout = new QVBoxLayout(m_recordingTab);

    // Webcam group
    auto *camGroup = new QGroupBox("Webcam", m_recordingTab);
    auto *camLayout = new QFormLayout(camGroup);
    m_cameraEnabledCheck = new QCheckBox("Show webcam circle in recordings", camGroup);
    m_cameraCombo = new QComboBox(camGroup);
    fillDevices(m_cameraCombo, QMediaDevices::videoInputs(), "No camera found");
    camLayout->addRow(m_cameraEnabledCheck);
    camLayout->addRow("Camera:", m_cameraCombo);
    layout->addWidget(camGroup);

    // Audio group
    auto *audioGroup = new QGroupBox("Audio", m_recordingTab);
    auto *audioLayout = new QFormLayout(audioGroup);
    m_micEnabledCheck = new QCheckBox("Record microphone", audioGroup);
    m_micCombo = new QComboBox(audioGroup);
    fillDevices(m_micCombo, QMediaDevices::audioInputs(), "No microphone found");
    m_systemAudioCheck = new QCheckBox("Record system audio", audioGroup);
    audioLayout->addRow(m_micEnabledCheck);
    audioLayout->addRow("Microphone:", m_micCombo);
    audioLayout->addRow(m_systemAudioCheck);
    layout->addWidget(audioGroup);

    // Display group: the on-screen recording frame (border + dim around the region).
    auto *displayGroup = new QGroupBox("Display", m_recordingTab);
    auto *displayLayout = new QFormLayout(displayGroup);
    m_frameCheck = new QCheckBox("Highlight recorded area while recording", displayGroup);
    displayLayout->addRow(m_frameCheck);
    layout->addWidget(displayGroup);

    layout->addStretch();
    m_tabWidget->addTab(m_recordingTab, "Recording");

    // A device dropdown is only relevant when its input is enabled. The TCC permission is
    // asked for the moment an input is enabled, while a normal dialog has focus - at
    // recording time the full-screen selection overlays sit above system dialogs and
    // would hide the prompt.
    const auto wireInput = [this](QCheckBox *check, QComboBox *combo, const auto &permission) {
        connect(check, &QCheckBox::toggled, combo, &QWidget::setEnabled);
        connect(check, &QCheckBox::toggled, this, [this, permission](bool on) {
            if (on && qApp->checkPermission(permission) == Qt::PermissionStatus::Undetermined)
                qApp->requestPermission(permission, this, [](const QPermission &) {});
        });
    };
    wireInput(m_cameraEnabledCheck, m_cameraCombo, QCameraPermission());
    wireInput(m_micEnabledCheck, m_micCombo, QMicrophonePermission());
}

namespace {

// Selects the entry holding data; leaves the combo alone when there is none.
void selectData(QComboBox *combo, const QVariant &data)
{
    if (const int index = combo->findData(data); index != -1)
        combo->setCurrentIndex(index);
}

// The secret edit's placeholder while nothing is stored.
QString secretHint(Upload::ProviderType type, Upload::SftpAuthMode auth)
{
    if (type == Upload::ProviderType::S3)
        return SettingsDialog::tr("Secret access key");
    return type == Upload::ProviderType::Sftp && auth == Upload::SftpAuthMode::PrivateKey
               ? SettingsDialog::tr("Key passphrase")
               : SettingsDialog::tr("Password");
}

// Provider pages inside m_uploadStack. Fixed, explicit indices - nothing relies on the
// numeric value of ProviderType.
constexpr int kS3Page   = 0;
constexpr int kSftpPage = 1;
constexpr int kFtpPage  = 2;

int pageForType(Upload::ProviderType t)
{
    switch (t) {
    case Upload::ProviderType::Sftp: return kSftpPage;
    case Upload::ProviderType::Ftp:  return kFtpPage;
    case Upload::ProviderType::S3:   break;
    }
    return kS3Page;
}

// Combo index <-> enum maps. Written out rather than cast, so reordering a combo can
// never silently rewrite stored profiles.
constexpr int kSftpAuthPassword = 0;
constexpr int kSftpAuthKey      = 1;

Upload::SftpAuthMode sftpAuthForIndex(int index)
{
    return index == kSftpAuthKey ? Upload::SftpAuthMode::PrivateKey
                                 : Upload::SftpAuthMode::Password;
}

int indexForSftpAuth(Upload::SftpAuthMode mode)
{
    return mode == Upload::SftpAuthMode::PrivateKey ? kSftpAuthKey : kSftpAuthPassword;
}

// The encryption combo leads with the secure default, so index != enum value here.
constexpr int kFtpEncExplicit = 0;
constexpr int kFtpEncImplicit = 1;
constexpr int kFtpEncNone     = 2;

Upload::FtpEncryption ftpEncryptionForIndex(int index)
{
    switch (index) {
    case kFtpEncImplicit: return Upload::FtpEncryption::Implicit;
    case kFtpEncNone:     return Upload::FtpEncryption::None;
    default:              return Upload::FtpEncryption::Explicit;
    }
}

int indexForFtpEncryption(Upload::FtpEncryption enc)
{
    switch (enc) {
    case Upload::FtpEncryption::Implicit: return kFtpEncImplicit;
    case Upload::FtpEncryption::None:     return kFtpEncNone;
    case Upload::FtpEncryption::Explicit: break;
    }
    return kFtpEncExplicit;
}

// Why the keychain refused a secret and what to do about it, in this platform's words.
QString keychainProblem(Core::KeychainStore::Failure why)
{
    switch (why) {
    case Core::KeychainStore::Failure::NoService:
        return SettingsDialog::tr("no keyring service is running. Install or start GNOME Keyring "
                                  "or KWallet, then try again.");
    case Core::KeychainStore::Failure::Locked:
        return SettingsDialog::tr("the keyring stayed locked. Unlock it when asked, then try again.");
    case Core::KeychainStore::Failure::None:
    case Core::KeychainStore::Failure::Other:
        break;
    }
#if defined(Q_OS_MACOS)
    return SettingsDialog::tr("the macOS Keychain refused it.");
#elif defined(Q_OS_WIN)
    return SettingsDialog::tr("Windows Credential Manager refused it.");
#elif defined(Q_OS_LINUX)
    return SettingsDialog::tr("the keyring service reported an error.");
#else
    return SettingsDialog::tr("this build of Snim cannot keep passwords on this system.");
#endif
}

} // namespace

void SettingsDialog::setupUploadTab()
{
    m_uploadTab = new QWidget();
    auto *layout = new QVBoxLayout(m_uploadTab);

    m_uploadEnabledCheck = new QCheckBox(tr("Enable upload"), m_uploadTab);
    layout->addWidget(m_uploadEnabledCheck);

    // Left: the list of saved servers + Add/Remove/Set-default. Right: the field form.
    auto *split = new QHBoxLayout();

    auto *leftCol = new QVBoxLayout();
    m_uploadList = new QListWidget(m_uploadTab);
    m_uploadList->setMaximumWidth(200);
    leftCol->addWidget(m_uploadList, /*stretch=*/1);
    auto *listButtons = new QHBoxLayout();
    m_uploadAddButton = new QPushButton("Add", m_uploadTab);
    m_uploadRemoveButton = new QPushButton("Remove", m_uploadTab);
    listButtons->addWidget(m_uploadAddButton);
    listButtons->addWidget(m_uploadRemoveButton);
    leftCol->addLayout(listButtons);
    m_uploadDefaultButton = new QPushButton("Set as default", m_uploadTab);
    leftCol->addWidget(m_uploadDefaultButton);
    split->addLayout(leftCol);

    // The provider type is fixed at creation (switching one in place would mean remapping
    // fields and re-keying the keychain entry), so Add asks for it up front.
    m_uploadAddMenu = new QMenu(m_uploadAddButton);
    m_uploadAddMenu->setToolTipsVisible(true);
    struct AddEntry { Upload::ProviderType type; QString text; QString missingDep; };
    const AddEntry addEntries[] = {
        { Upload::ProviderType::S3, tr("S3-compatible…"), QString() },
        { Upload::ProviderType::Sftp, tr("SFTP…"),
          tr("Requires libssh2, which was not included in this build.") },
        { Upload::ProviderType::Ftp, tr("FTP / FTPS…"),
          tr("Requires libcurl, which was not included in this build.") },
    };
    for (const AddEntry &entry : addEntries) {
        QAction *action = m_uploadAddMenu->addAction(entry.text);
        const bool available = Upload::UploaderFactory::isAvailable(entry.type);
        action->setEnabled(available);
        if (!available)
            action->setToolTip(entry.missingDep);
        const Upload::ProviderType type = entry.type;
        connect(action, &QAction::triggered, this, [this, type] { onUploadAddProfile(type); });
    }
    m_uploadAddButton->setMenu(m_uploadAddMenu);   // a menu button: click opens the list

    m_uploadForm = new QGroupBox("Server", m_uploadTab);
    auto *formCol = new QVBoxLayout(m_uploadForm);

    // Shared header: name + the (read-only) type, plus a warning for a saved profile whose
    // transport this build lacks. The form stays editable - only uploading would fail.
    m_uploadTopForm = new QFormLayout();
    m_uploadTopForm->setContentsMargins(0, 0, 0, 0);

    m_uploadNameEdit = new QLineEdit(m_uploadForm);
    m_uploadNameEdit->setPlaceholderText(tr("e.g. My server"));   // per-type hint set on bind
    m_uploadTopForm->addRow(tr("Name:"), m_uploadNameEdit);

    m_uploadTypeLabel = new QLabel(m_uploadForm);
    m_uploadTopForm->addRow(tr("Type:"), m_uploadTypeLabel);

    m_uploadUnavailableLabel = new QLabel(
        tr("This provider isn't available in this build - uploads from this destination will fail."),
        m_uploadForm);
    m_uploadUnavailableLabel->setWordWrap(true);
    m_uploadUnavailableLabel->setStyleSheet("color: #c86400;");
    m_uploadWarningRow = m_uploadTopForm->rowCount();
    m_uploadTopForm->addRow(m_uploadUnavailableLabel);
    m_uploadTopForm->setRowVisible(m_uploadWarningRow, false);
    formCol->addLayout(m_uploadTopForm);

    // One page of fields per provider type; bind/flush only ever touch the current one.
    m_uploadStack = new QStackedWidget(m_uploadForm);
    m_uploadStack->addWidget(createUploadS3Page());     // kS3Page
    m_uploadStack->addWidget(createUploadSftpPage());   // kSftpPage
    m_uploadStack->addWidget(createUploadFtpPage());    // kFtpPage
    formCol->addWidget(m_uploadStack);

    auto *sharedForm = new QFormLayout();
    sharedForm->setContentsMargins(0, 0, 0, 0);
    m_uploadPublicUrlEdit = new QLineEdit(m_uploadForm);
    m_uploadPublicUrlEdit->setPlaceholderText(
        tr("https://cdn.example.com - optional; the link uses the server URL otherwise"));
    sharedForm->addRow(tr("Public base URL:"), m_uploadPublicUrlEdit);
    formCol->addLayout(sharedForm);

    // Test connection: a real probe upload through the real backend, reported inline.
    // No dialog and no spinner - the label carries the whole story.
    auto *testRow = new QHBoxLayout();
    m_uploadTestButton = new QPushButton(tr("Test connection"), m_uploadForm);
    m_uploadTestButton->setFixedWidth(140);
    m_uploadTestStatusLabel = new QLabel(m_uploadForm);
    m_uploadTestStatusLabel->setWordWrap(true);
    testRow->addWidget(m_uploadTestButton);
    testRow->addWidget(m_uploadTestStatusLabel, /*stretch=*/1);
    formCol->addLayout(testRow);

    formCol->addStretch();

    split->addWidget(m_uploadForm, /*stretch=*/1);
    layout->addLayout(split);

    auto *note = new QLabel(tr("Each destination's secret (S3 secret key, SFTP/FTP password or "
                               "key passphrase) is stored in your system keychain, not in "
                               "settings. Uploads use the default ★ server; the Upload button's "
                               "▾ menu lets you pick another per upload."), m_uploadTab);
    note->setStyleSheet("color: gray; font-style: italic;");
    note->setWordWrap(true);
    layout->addWidget(note);

    connect(m_uploadList, &QListWidget::currentRowChanged, this,
            &SettingsDialog::onUploadSelectionChanged);
    connect(m_uploadRemoveButton, &QPushButton::clicked, this, &SettingsDialog::onUploadRemoveProfile);
    connect(m_uploadDefaultButton, &QPushButton::clicked, this, &SettingsDialog::onUploadSetDefault);
    connect(m_uploadTestButton, &QPushButton::clicked, this, &SettingsDialog::onUploadTestConnection);
    // "Enable upload" is a runtime switch, not an edit lock: destinations can be set up
    // or fixed while uploads are off, so the form only cares about having a selection.

    m_tabWidget->addTab(m_uploadTab, "Upload");
}

QWidget *SettingsDialog::createUploadS3Page()
{
    auto *page = new QWidget(m_uploadStack);
    auto *form = new QFormLayout(page);
    form->setContentsMargins(0, 0, 0, 0);

    m_uploadEndpointEdit = new QLineEdit(page);
    m_uploadEndpointEdit->setPlaceholderText("s3.amazonaws.com  ·  <acct>.r2.cloudflarestorage.com");
    form->addRow("Endpoint:", m_uploadEndpointEdit);

    m_uploadRegionEdit = new QLineEdit(page);
    m_uploadRegionEdit->setPlaceholderText("us-east-1  ·  auto (Cloudflare R2)");
    form->addRow("Region:", m_uploadRegionEdit);

    m_uploadBucketEdit = new QLineEdit(page);
    form->addRow("Bucket:", m_uploadBucketEdit);

    m_uploadAccessKeyEdit = new QLineEdit(page);
    form->addRow("Access key ID:", m_uploadAccessKeyEdit);

    m_uploadSecretEdit = new QLineEdit(page);
    m_uploadSecretEdit->setEchoMode(QLineEdit::Password);
    form->addRow("Secret key:", m_uploadSecretEdit);

    m_uploadPrefixEdit = new QLineEdit(page);
    m_uploadPrefixEdit->setPlaceholderText("e.g. screenshots/");
    form->addRow("Key prefix:", m_uploadPrefixEdit);

    m_uploadPathStyleCheck = new QCheckBox("Force path-style URLs (MinIO / Wasabi)", page);
    form->addRow(QString(), m_uploadPathStyleCheck);

    return page;
}

QWidget *SettingsDialog::createUploadSftpPage()
{
    auto *page = new QWidget(m_uploadStack);
    m_sftpForm = new QFormLayout(page);
    m_sftpForm->setContentsMargins(0, 0, 0, 0);

    m_sftpHostEdit = new QLineEdit(page);
    m_sftpHostEdit->setPlaceholderText(QStringLiteral("sftp.example.com"));
    m_sftpForm->addRow(tr("Host:"), m_sftpHostEdit);

    m_sftpPortSpin = new QSpinBox(page);
    m_sftpPortSpin->setRange(0, 65535);
    m_sftpPortSpin->setSpecialValueText(tr("22 (default)"));   // shown at 0
    m_sftpForm->addRow(tr("Port:"), m_sftpPortSpin);

    m_sftpUserEdit = new QLineEdit(page);
    m_sftpForm->addRow(tr("Username:"), m_sftpUserEdit);

    m_sftpAuthCombo = new QComboBox(page);
    m_sftpAuthCombo->addItem(tr("Password"));            // kSftpAuthPassword
    m_sftpAuthCombo->addItem(tr("Private key file"));    // kSftpAuthKey
    m_sftpForm->addRow(tr("Authentication:"), m_sftpAuthCombo);

    // One secret edit for both modes - the label says which it is.
    m_sftpSecretLabel = new QLabel(tr("Password:"), page);
    m_sftpSecretEdit = new QLineEdit(page);
    m_sftpSecretEdit->setEchoMode(QLineEdit::Password);
    m_sftpForm->addRow(m_sftpSecretLabel, m_sftpSecretEdit);

    // The path + Browse pair lives in a container widget so hiding the row in password
    // mode (setRowVisible) hides both, not just the label.
    auto *keyRowWidget = new QWidget(page);
    auto *keyRow = new QHBoxLayout(keyRowWidget);
    keyRow->setContentsMargins(0, 0, 0, 0);
    m_sftpKeyPathEdit = new QLineEdit(keyRowWidget);
    m_sftpKeyPathEdit->setPlaceholderText(QDir::homePath() + QStringLiteral("/.ssh/id_ed25519"));
    m_sftpKeyBrowseButton = new QPushButton(tr("Browse…"), keyRowWidget);
    m_sftpKeyBrowseButton->setMaximumWidth(90);
    keyRow->addWidget(m_sftpKeyPathEdit);
    keyRow->addWidget(m_sftpKeyBrowseButton);
    m_sftpKeyPathRow = m_sftpForm->rowCount();
    m_sftpForm->addRow(tr("Private key:"), keyRowWidget);

    m_sftpRemoteDirEdit = new QLineEdit(page);
    m_sftpRemoteDirEdit->setPlaceholderText(tr("e.g. /var/www/uploads - empty = login folder"));
    m_sftpForm->addRow(tr("Remote directory:"), m_sftpRemoteDirEdit);

    connect(m_sftpAuthCombo, &QComboBox::currentIndexChanged,
            this, &SettingsDialog::updateSftpAuthMode);
    connect(m_sftpKeyBrowseButton, &QPushButton::clicked, this, [this] {
        const QString current = m_sftpKeyPathEdit->text().trimmed();
        const QString path = QFileDialog::getOpenFileName(
            this, tr("Select private key file"),
            current.isEmpty() ? QDir::homePath() + QStringLiteral("/.ssh") : current);
        if (!path.isEmpty())
            m_sftpKeyPathEdit->setText(path);
    });
    updateSftpAuthMode();

    return page;
}

QWidget *SettingsDialog::createUploadFtpPage()
{
    auto *page = new QWidget(m_uploadStack);
    auto *form = new QFormLayout(page);
    form->setContentsMargins(0, 0, 0, 0);

    m_ftpHostEdit = new QLineEdit(page);
    m_ftpHostEdit->setPlaceholderText(QStringLiteral("ftp.example.com"));
    form->addRow(tr("Host:"), m_ftpHostEdit);

    m_ftpPortSpin = new QSpinBox(page);
    m_ftpPortSpin->setRange(0, 65535);
    m_ftpPortSpin->setSpecialValueText(tr("21 (default)"));   // shown at 0
    form->addRow(tr("Port:"), m_ftpPortSpin);

    m_ftpUserEdit = new QLineEdit(page);
    m_ftpUserEdit->setPlaceholderText(tr("empty = anonymous"));
    form->addRow(tr("Username:"), m_ftpUserEdit);

    m_ftpSecretEdit = new QLineEdit(page);
    m_ftpSecretEdit->setEchoMode(QLineEdit::Password);
    form->addRow(tr("Password:"), m_ftpSecretEdit);

    m_ftpEncryptionCombo = new QComboBox(page);
    m_ftpEncryptionCombo->addItem(tr("Explicit TLS (FTPS)"));        // kFtpEncExplicit
    m_ftpEncryptionCombo->addItem(tr("Implicit TLS (port 990)"));    // kFtpEncImplicit
    m_ftpEncryptionCombo->addItem(tr("None (plain FTP - insecure)"));// kFtpEncNone
    form->addRow(tr("Encryption:"), m_ftpEncryptionCombo);

    m_ftpRemoteDirEdit = new QLineEdit(page);
    m_ftpRemoteDirEdit->setPlaceholderText(tr("e.g. /var/www/uploads - empty = login folder"));
    form->addRow(tr("Remote directory:"), m_ftpRemoteDirEdit);

    return page;
}

QLineEdit *SettingsDialog::secretEditFor(Upload::ProviderType type) const
{
    switch (type) {
    case Upload::ProviderType::Sftp: return m_sftpSecretEdit;
    case Upload::ProviderType::Ftp:  return m_ftpSecretEdit;
    case Upload::ProviderType::S3:   break;
    }
    return m_uploadSecretEdit;
}

void SettingsDialog::updateSftpAuthMode()
{
    const Upload::SftpAuthMode mode = sftpAuthForIndex(m_sftpAuthCombo->currentIndex());
    const bool keyAuth = mode == Upload::SftpAuthMode::PrivateKey;
    m_sftpSecretLabel->setText(keyAuth ? tr("Key passphrase (optional):") : tr("Password:"));
    m_sftpForm->setRowVisible(m_sftpKeyPathRow, keyAuth);
    // Keep the hint in step with the mode - but never clobber the "stored" marker.
    if (m_sftpSecretEdit->placeholderText() != tr("•••••••• (stored)"))
        m_sftpSecretEdit->setPlaceholderText(secretHint(Upload::ProviderType::Sftp, mode));
}

void SettingsDialog::refreshUploadList()
{
    const QSignalBlocker block(m_uploadList);   // don't fire selection changes while rebuilding
    m_uploadList->clear();
    for (const Upload::UploadProfile &p : m_uploadWorking)
        m_uploadList->addItem(Upload::profileLabel(p, p.id == m_uploadDefaultId));
    const bool any = !m_uploadWorking.isEmpty();
    m_uploadRemoveButton->setEnabled(any);
    m_uploadDefaultButton->setEnabled(any);
    m_uploadForm->setEnabled(any);
    // Same condition as the form, minus a test that is still running (its own handler
    // re-enables the button when it reports back).
    m_uploadTestButton->setEnabled(any && !m_uploadTester);
}

void SettingsDialog::clearUploadPages()
{
    m_uploadEndpointEdit->clear();
    m_uploadRegionEdit->clear();
    m_uploadBucketEdit->clear();
    m_uploadAccessKeyEdit->clear();
    m_uploadSecretEdit->clear();
    m_uploadPrefixEdit->clear();
    m_uploadPathStyleCheck->setChecked(false);

    m_sftpHostEdit->clear();
    m_sftpPortSpin->setValue(0);
    m_sftpUserEdit->clear();
    {
        const QSignalBlocker block(m_sftpAuthCombo);
        m_sftpAuthCombo->setCurrentIndex(kSftpAuthPassword);
    }
    updateSftpAuthMode();
    m_sftpSecretEdit->clear();
    m_sftpKeyPathEdit->clear();
    m_sftpRemoteDirEdit->clear();

    m_ftpHostEdit->clear();
    m_ftpPortSpin->setValue(0);
    m_ftpUserEdit->clear();
    m_ftpSecretEdit->clear();
    m_ftpEncryptionCombo->setCurrentIndex(kFtpEncExplicit);
    m_ftpRemoteDirEdit->clear();
}

void SettingsDialog::bindUploadForm(int row)
{
    const bool valid = row >= 0 && row < m_uploadWorking.size();
    m_uploadCurrentRow = valid ? row : -1;
    const Upload::UploadProfile p = valid ? m_uploadWorking[row] : Upload::UploadProfile{};

    // Shared fields + the read-only type, and the "not built in" warning for a saved
    // profile whose transport this build lacks (the form stays editable).
    m_uploadNameEdit->setText(p.name);
    m_uploadNameEdit->setPlaceholderText(p.type == Upload::ProviderType::S3
                                             ? tr("My S3 / R2 / MinIO")
                                             : tr("My server"));
    // A result from the previously bound profile would read as this one's - drop it.
    m_uploadTestStatusLabel->clear();
    m_uploadTestStatusLabel->setStyleSheet(QString());
    m_uploadTypeLabel->setText(valid ? Upload::providerDisplayName(p.type) : QString());
    // No selection = no type to show; a visible "Type:" with an empty value reads broken.
    m_uploadTopForm->setRowVisible(m_uploadTypeLabel, valid);
    m_uploadPublicUrlEdit->setText(p.publicBaseUrl);
    m_uploadTopForm->setRowVisible(m_uploadWarningRow,
                                   valid && !Upload::UploaderFactory::isAvailable(p.type));

    // Only the current page is ever read back, but blanking the others keeps a previous
    // profile's values from lingering behind the stack (and clears the row when invalid).
    clearUploadPages();
    m_uploadStack->setCurrentIndex(pageForType(p.type));
    if (!valid)
        return;

    switch (p.type) {
    case Upload::ProviderType::S3:
        m_uploadEndpointEdit->setText(p.endpoint);
        m_uploadRegionEdit->setText(p.region);
        m_uploadBucketEdit->setText(p.bucket);
        m_uploadAccessKeyEdit->setText(p.accessKeyId);
        m_uploadPrefixEdit->setText(p.keyPrefix);
        m_uploadPathStyleCheck->setChecked(p.forcePathStyle);
        break;
    case Upload::ProviderType::Sftp:
        m_sftpHostEdit->setText(p.host);
        m_sftpPortSpin->setValue(p.port);
        m_sftpUserEdit->setText(p.username);
        {
            const QSignalBlocker block(m_sftpAuthCombo);
            m_sftpAuthCombo->setCurrentIndex(indexForSftpAuth(p.sftpAuth));
        }
        updateSftpAuthMode();   // secret row label + private-key row visibility
        m_sftpKeyPathEdit->setText(p.privateKeyPath);
        m_sftpRemoteDirEdit->setText(p.remoteDir);
        break;
    case Upload::ProviderType::Ftp:
        m_ftpHostEdit->setText(p.host);
        m_ftpPortSpin->setValue(p.port);
        m_ftpUserEdit->setText(p.username);
        m_ftpEncryptionCombo->setCurrentIndex(indexForFtpEncryption(p.ftpEncryption));
        m_ftpRemoteDirEdit->setText(p.remoteDir);
        break;
    }

    // The secret edit is write-only: it stays empty and its placeholder says whether one
    // is already stored (in this type's keychain service) or staged for this Apply.
    QLineEdit *secret = secretEditFor(p.type);
    const bool hasSecret = m_uploadNewSecrets.contains(p.id) ||
        Core::KeychainStore::retrieve(Upload::keychainServiceFor(p.type), p.id).has_value();
    secret->setPlaceholderText(hasSecret ? tr("•••••••• (stored)") : secretHint(p.type, p.sftpAuth));
}

void SettingsDialog::flushUploadForm(int row)
{
    if (row < 0 || row >= m_uploadWorking.size())
        return;
    Upload::UploadProfile &p = m_uploadWorking[row];
    p.name = m_uploadNameEdit->text().trimmed();
    p.publicBaseUrl = m_uploadPublicUrlEdit->text().trimmed();
    // The type never changes, so the bound page is still the one this profile owns.
    switch (p.type) {
    case Upload::ProviderType::S3:
        p.endpoint = m_uploadEndpointEdit->text().trimmed();
        p.region = m_uploadRegionEdit->text().trimmed();
        p.bucket = m_uploadBucketEdit->text().trimmed();
        p.accessKeyId = m_uploadAccessKeyEdit->text().trimmed();
        p.keyPrefix = m_uploadPrefixEdit->text().trimmed();
        p.forcePathStyle = m_uploadPathStyleCheck->isChecked();
        break;
    case Upload::ProviderType::Sftp:
        p.host = m_sftpHostEdit->text().trimmed();
        p.port = m_sftpPortSpin->value();          // 0 = the protocol default
        p.username = m_sftpUserEdit->text().trimmed();
        p.sftpAuth = sftpAuthForIndex(m_sftpAuthCombo->currentIndex());
        p.privateKeyPath = m_sftpKeyPathEdit->text().trimmed();
        p.remoteDir = m_sftpRemoteDirEdit->text().trimmed();
        break;
    case Upload::ProviderType::Ftp:
        p.host = m_ftpHostEdit->text().trimmed();
        p.port = m_ftpPortSpin->value();           // 0 = the protocol default
        p.username = m_ftpUserEdit->text().trimmed();
        p.ftpEncryption = ftpEncryptionForIndex(m_ftpEncryptionCombo->currentIndex());
        p.remoteDir = m_ftpRemoteDirEdit->text().trimmed();
        break;
    }
    const QString secret = secretEditFor(p.type)->text();
    if (!secret.isEmpty())
        m_uploadNewSecrets.insert(p.id, secret);   // staged; written to keychain on Apply
}

void SettingsDialog::onUploadSelectionChanged(int row)
{
    flushUploadForm(m_uploadCurrentRow);   // don't lose edits on the outgoing row
    // The list label may have changed (name edit) - refresh without re-entrancy.
    if (m_uploadCurrentRow >= 0 && m_uploadCurrentRow < m_uploadWorking.size()) {
        const Upload::UploadProfile &p = m_uploadWorking[m_uploadCurrentRow];
        if (auto *it = m_uploadList->item(m_uploadCurrentRow))
            it->setText(Upload::profileLabel(p, p.id == m_uploadDefaultId));
    }
    bindUploadForm(row);
}

void SettingsDialog::onUploadAddProfile(Upload::ProviderType type)
{
    flushUploadForm(m_uploadCurrentRow);
    Upload::UploadProfile p;
    p.id = Upload::UploadProfiles::newId();
    p.name = tr("New server");
    p.type = type;
    if (type == Upload::ProviderType::S3) {
        p.endpoint = QStringLiteral("s3.amazonaws.com");
        p.region = QStringLiteral("us-east-1");
    }
    // SFTP/FTP keep the struct defaults: port 0 (protocol default) and explicit FTPS.
    m_uploadWorking.push_back(p);
    if (m_uploadDefaultId.isEmpty())
        m_uploadDefaultId = p.id;          // first one is the default
    refreshUploadList();
    m_uploadList->setCurrentRow(m_uploadWorking.size() - 1);   // selects + binds via signal
}

void SettingsDialog::onUploadRemoveProfile()
{
    const int row = m_uploadList->currentRow();
    if (row < 0 || row >= m_uploadWorking.size())
        return;
    const QString removedId = m_uploadWorking[row].id;
    m_uploadWorking.remove(row);
    m_uploadNewSecrets.remove(removedId);
    if (m_uploadDefaultId == removedId)
        m_uploadDefaultId = m_uploadWorking.isEmpty() ? QString() : m_uploadWorking.first().id;
    m_uploadCurrentRow = -1;   // the bound row is gone; avoid flushing into a shifted index
    refreshUploadList();
    if (!m_uploadWorking.isEmpty())
        m_uploadList->setCurrentRow(qMin(row, m_uploadWorking.size() - 1));
    else
        bindUploadForm(-1);
}

void SettingsDialog::onUploadSetDefault()
{
    const int row = m_uploadList->currentRow();
    if (row < 0 || row >= m_uploadWorking.size())
        return;
    m_uploadDefaultId = m_uploadWorking[row].id;
    flushUploadForm(m_uploadCurrentRow);
    refreshUploadList();
    m_uploadList->setCurrentRow(row);
}

void SettingsDialog::onUploadTestConnection()
{
    if (m_uploadTester)
        return;                            // a test is already running
    flushUploadForm(m_uploadCurrentRow);   // test what is on screen, not what was bound
    if (m_uploadCurrentRow < 0 || m_uploadCurrentRow >= m_uploadWorking.size())
        return;
    const Upload::UploadProfile &p = m_uploadWorking[m_uploadCurrentRow];

    auto setStatus = [this](const QString &text, const char *color) {
        m_uploadTestStatusLabel->setStyleSheet(color ? QString("color: %1;").arg(color)
                                                     : QString());
        m_uploadTestStatusLabel->setText(text);
    };

    // A destination whose transport was never compiled in can't be tested at all; say so
    // here rather than spinning up the stub just to hear the same sentence back.
    if (!Upload::UploaderFactory::isAvailable(p.type)) {
        setStatus(tr("%1 support is not included in this build.")
                      .arg(Upload::providerDisplayName(p.type)), "#c62828");
        return;
    }

    // Build the config straight from the working copy: the point is to test values the
    // user has typed but not saved. The secret comes from this session's staged entry if
    // there is one, else from the keychain - and is never written back here (only Apply
    // does that). `enabled` is forced on: the global upload switch says whether the app
    // uploads, not whether a destination may be checked while it is being set up.
    Upload::UploadConfig cfg{p};
    cfg.enabled = true;
    const QString staged = m_uploadNewSecrets.value(p.id);
    auto readFailure = Core::KeychainStore::Failure::None;
    if (!staged.isEmpty()) {
        cfg.secretKey = staged;
    } else if (const auto stored = Core::KeychainStore::retrieve(
                   Upload::keychainServiceFor(p.type), p.id, &readFailure)) {
        cfg.secretKey = *stored;
    }

    if (!cfg.isComplete()) {
        // An unreadable keychain explains a missing password better than the generic hint.
        if (readFailure != Core::KeychainStore::Failure::None)
            setStatus(tr("Snim could not read the password: %1").arg(keychainProblem(readFailure)),
                      "#c62828");
        else
            setStatus(tr("Fill in the required fields first."), nullptr);
        return;
    }

    m_uploadTestButton->setEnabled(false);
    setStatus(tr("Testing..."), nullptr);

    // Parented to the dialog: closing Settings mid-test destroys the tester, whose
    // destructor tells the worker to give up and whose queued replies are then dropped.
    m_uploadTester = Upload::UploaderFactory::createForConfig(cfg, this).release();
    connect(m_uploadTester, &Upload::Uploader::testFinished, this,
            [this, setStatus](bool ok, const QString &message) {
                setStatus(message, ok ? "#2e7d32" : "#c62828");
                if (m_uploadTester) {
                    m_uploadTester->deleteLater();
                    m_uploadTester = nullptr;
                }
                m_uploadTestButton->setEnabled(!m_uploadWorking.isEmpty());
            });
    m_uploadTester->testConnection();
}

void SettingsDialog::setupHotkeysTab()
{
    m_hotkeysTab = new QWidget();

    auto *layout = new QVBoxLayout(m_hotkeysTab);

    // Built first: the rows below connect validateHotkeys, which writes to it.
    m_hotkeyConflictLabel = new QLabel(m_hotkeysTab);
    m_hotkeyConflictLabel->setWordWrap(true);
    m_hotkeyConflictLabel->setStyleSheet("color: #c62828;");
    m_hotkeyConflictLabel->hide();

    // Separate from the conflict label, whose text blocks Apply.
    m_hotkeyFailureLabel = new QLabel(m_hotkeysTab);
    m_hotkeyFailureLabel->setWordWrap(true);
    m_hotkeyFailureLabel->setStyleSheet("color: #c62828;");
    m_hotkeyFailureLabel->hide();

    auto *group = new QGroupBox(tr("Global Hotkeys"), m_hotkeysTab);
    auto *form = new QFormLayout(group);

    for (const Hotkeys::HotkeyAction action : Hotkeys::allHotkeyActions()) {
        auto *edit = new QKeySequenceEdit(group);
        edit->setMaximumSequenceLength(1);   // no backend can register a second chord
        edit->setClearButtonEnabled(true);

        auto *defaultButton = new QToolButton(group);
        defaultButton->setText(tr("Default"));
        defaultButton->setToolTip(tr("Restore the factory hotkey"));
        connect(defaultButton, &QToolButton::clicked, this, [edit, action] {
            edit->setKeySequence(Hotkeys::hotkeyActionDefault(action));
        });

        auto *notice = new QLabel(group);
        notice->setWordWrap(true);
        notice->setStyleSheet("color: #c62828;");
        notice->hide();

        auto *row = new QHBoxLayout();
        row->addWidget(edit);
        row->addWidget(defaultButton);
        auto *field = new QVBoxLayout();
        field->setSpacing(2);
        field->addLayout(row);
        field->addWidget(notice);
        form->addRow(Hotkeys::hotkeyActionDescription(action) + ":", field);

        connect(edit, &QKeySequenceEdit::keySequenceChanged, this, &SettingsDialog::validateHotkeys);
        connect(edit, &QKeySequenceEdit::keySequenceChanged,
                this, &SettingsDialog::refreshHotkeyNotices);
        m_hotkeyRows.append(HotkeyRow{action, edit, notice});
    }

    layout->addWidget(group);

    // The factory answers for the platform without creating a backend, so no Carbon
    // handler is installed and no portal session is opened by opening Settings.
    QString note;
    if (!Hotkeys::HotkeyBackendFactory::isAvailable()) {
        note = tr("Global hotkeys are not supported on this system.");
        group->setEnabled(false);
    } else if (!Hotkeys::HotkeyBackendFactory::userConfiguresKeys()) {
        note = tr("Your desktop manages global shortcut keys. The combinations below are "
                  "suggestions; the system's own shortcut dialog decides the final bindings.");
    }
    if (!note.isEmpty()) {
        auto *info = new QLabel(note, m_hotkeysTab);
        info->setWordWrap(true);
        info->setStyleSheet("color: gray;");
        layout->addWidget(info);
    }

    layout->addWidget(m_hotkeyFailureLabel);
    layout->addWidget(m_hotkeyConflictLabel);
    layout->addStretch();

    m_tabWidget->addTab(m_hotkeysTab, "Hotkeys");
}

void SettingsDialog::validateHotkeys()
{
    QHash<QString, QVector<int>> bySequence;   // portable text -> row indices
    for (int i = 0; i < m_hotkeyRows.size(); ++i) {
        const QKeySequence seq = m_hotkeyRows[i].edit->keySequence();
        if (!seq.isEmpty())
            bySequence[seq.toString(QKeySequence::PortableText)].append(i);
    }

    QSet<int> clashing;
    QStringList messages;
    for (auto it = bySequence.cbegin(); it != bySequence.cend(); ++it) {
        if (it.value().size() < 2)
            continue;
        QStringList names;
        for (const int row : it.value()) {
            clashing.insert(row);
            names << Hotkeys::hotkeyActionDescription(m_hotkeyRows[row].action);
        }
        const QString last = names.takeLast();
        messages << tr("%1 and %2 use the same hotkey").arg(names.join(", "), last);
    }

    // The :focus arm too, or the focused edit keeps the platform focus frame instead.
    static const QString kClashStyle =
        QStringLiteral("QLineEdit { border: 1px solid #c62828; }"
                       "QLineEdit:focus { border: 1px solid #c62828; }");
    for (int i = 0; i < m_hotkeyRows.size(); ++i)
        m_hotkeyRows[i].edit->setStyleSheet(clashing.contains(i) ? kClashStyle : QString());
    m_hotkeyConflictLabel->setText(messages.join(QStringLiteral("\n")));
    m_hotkeyConflictLabel->setVisible(!messages.isEmpty());
}

void SettingsDialog::refreshHotkeyNotices()
{
    const bool allFailed = m_hotkeyManager && m_hotkeyManager->allFailed();
    QString firstReason;
    for (const HotkeyRow &row : m_hotkeyRows) {
        const QString reason = m_hotkeyManager ? m_hotkeyManager->failureReason(row.action)
                                               : QString();
        if (firstReason.isEmpty())
            firstReason = reason;
        // An edited row has not been registered yet, so the last pass says nothing about it.
        const bool saved = Hotkeys::HotkeyBindings::normalized(row.edit->keySequence())
                           == Hotkeys::HotkeyBindings::normalized(
                               Hotkeys::HotkeyBindings::sequence(row.action));
        const bool show = !allFailed && saved && !reason.isEmpty();
        row.notice->setText(show ? tr("Not registered: %1").arg(reason) : QString());
        row.notice->setVisible(show);
    }

    // One line instead of the same notice on every row.
    m_hotkeyFailureLabel->setText(
        allFailed ? tr("No global hotkey could be registered: %1").arg(firstReason) : QString());
    m_hotkeyFailureLabel->setVisible(allFailed);
}

void SettingsDialog::pickColor(QColor &color, const QString &title,
                               QColorDialog::ColorDialogOptions options)
{
    const QColor picked = QColorDialog::getColor(color, this, title, options);
    if (picked.isValid()) {
        color = picked;
        updateColorButtons();
    }
}

void SettingsDialog::updateColorButtons()
{
    const auto paint = [](QPushButton *button, const QString &color) {
        button->setStyleSheet(QString("QPushButton {"
                                      "  background-color: %1;"
                                      "  border: 2px solid #555;"
                                      "  border-radius: 4px;"
                                      "}"
                                      "QPushButton:hover {"
                                      "  border: 2px solid #777;"
                                      "}").arg(color));
    };
    paint(m_foregroundColorButton, m_foregroundColor.name());
    paint(m_backgroundColorButton, m_backgroundColor.name(QColor::HexArgb));
    // A fully transparent fill shows no swatch, so say it in words.
    m_backgroundColorButton->setText(m_backgroundColor.alpha() == 0 ? "Transparent" : "");
}

void SettingsDialog::loadSettings()
{
    m_screenshotFolderEdit->setText(Core::Settings::screenshotFolder());

    selectData(m_imageFormatCombo, Core::Settings::imageFormat());

    m_foregroundColor = Core::Settings::editorForeground();
    m_backgroundColor = Core::Settings::editorBackground();
    updateColorButtons();

    m_cameraEnabledCheck->setChecked(Core::Settings::cameraEnabled());
    selectData(m_cameraCombo, Core::Settings::cameraDeviceId());
    m_cameraCombo->setEnabled(m_cameraEnabledCheck->isChecked());

    m_micEnabledCheck->setChecked(Core::Settings::micEnabled());
    selectData(m_micCombo, Core::Settings::micDeviceId());
    m_micCombo->setEnabled(m_micEnabledCheck->isChecked());

    m_systemAudioCheck->setChecked(Core::Settings::systemAudioEnabled());
    m_frameCheck->setChecked(Core::Settings::recordingFrameEnabled());

    for (const HotkeyRow &row : m_hotkeyRows)
        row.edit->setKeySequence(Hotkeys::HotkeyBindings::sequence(row.action));
    validateHotkeys();
    refreshHotkeyNotices();

    // Upload - load the working copy of all profiles. Secrets stay in the keychain.
    m_uploadEnabledCheck->setChecked(Core::Settings::uploadEnabled());
    m_uploadWorking = Upload::UploadProfiles::all();
    m_uploadDefaultId = Upload::UploadProfiles::defaultId();
    m_uploadNewSecrets.clear();
    m_uploadCurrentRow = -1;
    refreshUploadList();
    if (!m_uploadWorking.isEmpty())
        m_uploadList->setCurrentRow(0);
    else
        bindUploadForm(-1);
}

QString SettingsDialog::saveSettings()
{
    Core::Settings::setScreenshotFolder(m_screenshotFolderEdit->text());
    Core::Settings::setImageFormat(m_imageFormatCombo->currentData().toString());
    Core::Settings::setEditorForeground(m_foregroundColor);
    Core::Settings::setEditorBackground(m_backgroundColor);

    Core::Settings::setCameraEnabled(m_cameraEnabledCheck->isChecked());
    Core::Settings::setCameraDeviceId(m_cameraCombo->currentData().toByteArray());
    Core::Settings::setMicEnabled(m_micEnabledCheck->isChecked());
    Core::Settings::setMicDeviceId(m_micCombo->currentData().toByteArray());
    Core::Settings::setSystemAudioEnabled(m_systemAudioCheck->isChecked());
    Core::Settings::setRecordingFrameEnabled(m_frameCheck->isChecked());

    for (const HotkeyRow &row : m_hotkeyRows)
        Hotkeys::HotkeyBindings::setSequence(
            row.action, Hotkeys::HotkeyBindings::normalized(row.edit->keySequence()));

    // Upload - commit the working profiles. Secrets go to the keychain (keyed by
    // profile id), never to QSettings.
    flushUploadForm(m_uploadCurrentRow);
    Core::Settings::setUploadEnabled(m_uploadEnabledCheck->isChecked());

    // Profiles dropped this session (present originally, absent now): erase their secrets
    // from the service their type used.
    QSet<QString> workingIds;
    for (const Upload::UploadProfile &p : m_uploadWorking)
        workingIds.insert(p.id);
    for (const Upload::UploadProfile &o : Upload::UploadProfiles::all())
        if (!workingIds.contains(o.id))
            Core::KeychainStore::erase(Upload::keychainServiceFor(o.type), o.id);

    // Newly-entered secrets for surviving profiles. The service depends on the profile's
    // type, so resolve it in the working copy (a removed one is skipped by workingIds).
    QHash<QString, QString> refused;
    QStringList refusedNames;
    auto why = Core::KeychainStore::Failure::None;
    for (auto it = m_uploadNewSecrets.cbegin(); it != m_uploadNewSecrets.cend(); ++it) {
        if (!workingIds.contains(it.key()) || it.value().isEmpty())
            continue;
        const auto profile = std::find_if(m_uploadWorking.cbegin(), m_uploadWorking.cend(),
                                          [&it](const Upload::UploadProfile &p) {
                                              return p.id == it.key();
                                          });
        if (profile == m_uploadWorking.cend())
            continue;
        auto failure = Core::KeychainStore::Failure::None;
        if (!Core::KeychainStore::store(Upload::keychainServiceFor(profile->type), it.key(),
                                        it.value(), &failure)) {
            refused.insert(it.key(), it.value());
            refusedNames.append(QStringLiteral("\"%1\"").arg(
                profile->name.isEmpty() ? tr("(unnamed)") : profile->name));
            why = failure;
        }
    }

    Upload::UploadProfiles::setAll(m_uploadWorking, m_uploadDefaultId);
    // A refused secret stays staged and typed, so the next Apply tries it again.
    m_uploadNewSecrets = refused;
    if (!refused.isEmpty())
        return tr("Snim could not save the password for %1: %2")
            .arg(refusedNames.join(QStringLiteral(", ")), keychainProblem(why));
    m_uploadSecretEdit->clear();
    m_sftpSecretEdit->clear();
    m_ftpSecretEdit->clear();
    return {};
}

void SettingsDialog::applySettings()
{
    // Two actions sharing a sequence would register once and fire the wrong one.
    // validateHotkeys runs on every edit, so its label already names any clash.
    if (!m_hotkeyConflictLabel->text().isEmpty()) {
        m_tabWidget->setCurrentWidget(m_hotkeysTab);
        QMessageBox::warning(this, tr("Conflicting hotkeys"),
                             tr("Two actions are assigned the same hotkey. Change or clear one "
                                "of them before applying."));
        return;
    }

    // Create the screenshot folder if it doesn't exist (mkpath leaves an existing one be).
    if (const QString folder = m_screenshotFolderEdit->text(); !folder.isEmpty())
        QDir().mkpath(folder);

    const QString secretsError = saveSettings();
    emit settingsApplied();
    if (!secretsError.isEmpty()) {
        // Everything else is saved; stay open so the user can fix the keyring and Apply again.
        m_tabWidget->setCurrentWidget(m_uploadTab);
        QMessageBox::warning(this, tr("Password not saved"), secretsError);
        return;
    }
    accept();
}

void SettingsDialog::resetSettings()
{
    // Reset to default values
    QString defaultFolder = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)
                           + "/Screenshots";
    m_screenshotFolderEdit->setText(defaultFolder);
    m_imageFormatCombo->setCurrentIndex(0); // PNG
    m_foregroundColor = Qt::red;
    m_backgroundColor = Qt::transparent;
    updateColorButtons();

    // Unchecking disables the device combos (toggled), which always hold an entry.
    m_cameraEnabledCheck->setChecked(false);
    m_micEnabledCheck->setChecked(false);
    m_systemAudioCheck->setChecked(true);
    m_cameraCombo->setCurrentIndex(0);
    m_micCombo->setCurrentIndex(0);

    // Form-level only, like the rest of this slot: committed on Apply.
    for (const HotkeyRow &row : m_hotkeyRows)
        row.edit->setKeySequence(Hotkeys::hotkeyActionDefault(row.action));
    validateHotkeys();
}

} // namespace App
