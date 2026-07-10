#include "app/SettingsDialog.h"
#include "core/Settings.h"
#include "core/KeychainStore.h"
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

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
    , m_tabWidget(nullptr)
    , m_generalTab(nullptr)
    , m_screenshotFolderEdit(nullptr)
    , m_browseButton(nullptr)
    , m_imageFormatCombo(nullptr)
    , m_foregroundColorButton(nullptr)
    , m_backgroundColorButton(nullptr)
    , m_foregroundColor(Qt::red)
    , m_backgroundColor(Qt::transparent)
    , m_recordingTab(nullptr)
    , m_cameraEnabledCheck(nullptr)
    , m_cameraCombo(nullptr)
    , m_micEnabledCheck(nullptr)
    , m_micCombo(nullptr)
    , m_systemAudioCheck(nullptr)
    , m_frameCheck(nullptr)
    , m_uploadTab(nullptr)
    , m_hotkeysTab(nullptr)
    , m_applyButton(nullptr)
    , m_cancelButton(nullptr)
    , m_resetButton(nullptr)
{
    setWindowTitle("Settings");
    setModal(true);
    resize(500, 400);

    setupUI();
    loadSettings();
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
    connect(m_browseButton, &QPushButton::clicked, this, &SettingsDialog::browseScreenshotFolder);
    connect(m_foregroundColorButton, &QPushButton::clicked, this, &SettingsDialog::chooseForegroundColor);
    connect(m_backgroundColorButton, &QPushButton::clicked, this, &SettingsDialog::chooseBackgroundColor);
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

    // Foreground color (stroke/text color)
    m_foregroundColorButton = new QPushButton(editorGroup);
    m_foregroundColorButton->setMaximumWidth(100);
    m_foregroundColorButton->setMinimumHeight(30);
    updateForegroundButtonStyle();

    editorLayout->addRow("Foreground Color:", m_foregroundColorButton);

    // Background color (fill color)
    m_backgroundColorButton = new QPushButton(editorGroup);
    m_backgroundColorButton->setMaximumWidth(100);
    m_backgroundColorButton->setMinimumHeight(30);
    updateBackgroundButtonStyle();

    editorLayout->addRow("Background Color:", m_backgroundColorButton);

    layout->addWidget(editorGroup);
    layout->addStretch();

    m_tabWidget->addTab(m_generalTab, "General");
}

void SettingsDialog::setupRecordingTab()
{
    m_recordingTab = new QWidget();
    auto *layout = new QVBoxLayout(m_recordingTab);

    // Webcam group
    auto *camGroup = new QGroupBox("Webcam", m_recordingTab);
    auto *camLayout = new QFormLayout(camGroup);
    m_cameraEnabledCheck = new QCheckBox("Show webcam circle in recordings", camGroup);
    m_cameraCombo = new QComboBox(camGroup);
    for (const QCameraDevice &cam : QMediaDevices::videoInputs())
        m_cameraCombo->addItem(cam.description(), cam.id());
    if (m_cameraCombo->count() == 0)
        m_cameraCombo->addItem("No camera found", QByteArray());
    camLayout->addRow(m_cameraEnabledCheck);
    camLayout->addRow("Camera:", m_cameraCombo);
    layout->addWidget(camGroup);

    // Audio group
    auto *audioGroup = new QGroupBox("Audio", m_recordingTab);
    auto *audioLayout = new QFormLayout(audioGroup);
    m_micEnabledCheck = new QCheckBox("Record microphone", audioGroup);
    m_micCombo = new QComboBox(audioGroup);
    for (const QAudioDevice &mic : QMediaDevices::audioInputs())
        m_micCombo->addItem(mic.description(), mic.id());
    if (m_micCombo->count() == 0)
        m_micCombo->addItem("No microphone found", QByteArray());
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

    // Device dropdowns are only relevant when their input is enabled.
    connect(m_cameraEnabledCheck, &QCheckBox::toggled, m_cameraCombo, &QWidget::setEnabled);
    connect(m_micEnabledCheck, &QCheckBox::toggled, m_micCombo, &QWidget::setEnabled);

    // Ask for the TCC permission the moment an input is enabled, while a normal
    // dialog has focus - at recording time the full-screen selection overlays sit
    // above system dialogs and would hide the prompt.
    connect(m_cameraEnabledCheck, &QCheckBox::toggled, this, [this](bool on) {
        if (!on)
            return;
        QCameraPermission permission;
        if (qApp->checkPermission(permission) == Qt::PermissionStatus::Undetermined)
            qApp->requestPermission(permission, this, [](const QPermission &) {});
    });
    connect(m_micEnabledCheck, &QCheckBox::toggled, this, [this](bool on) {
        if (!on)
            return;
        QMicrophonePermission permission;
        if (qApp->checkPermission(permission) == Qt::PermissionStatus::Undetermined)
            qApp->requestPermission(permission, this, [](const QPermission &) {});
    });
}

namespace {

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

// Which factory backend a profile type needs, for the compile-time availability check.
Upload::UploaderFactory::StrategyType strategyForType(Upload::ProviderType t)
{
    switch (t) {
    case Upload::ProviderType::Sftp: return Upload::UploaderFactory::StrategyType::Sftp;
    case Upload::ProviderType::Ftp:  return Upload::UploaderFactory::StrategyType::Ftp;
    case Upload::ProviderType::S3:   break;
    }
    return Upload::UploaderFactory::StrategyType::S3;
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

// The one place that formats a list row ("★ Name - SFTP"), so the full rebuild and the
// in-place refresh after a name edit cannot drift apart.
QString displayRowText(const Upload::UploadProfile &p, bool isDefault)
{
    const QString name = p.name.isEmpty() ? SettingsDialog::tr("(unnamed)") : p.name;
    const QString label = name + QStringLiteral(" - ") + Upload::providerDisplayName(p.type);
    return isDefault ? QStringLiteral("★ ") + label : label;
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
        const bool available =
            Upload::UploaderFactory::isStrategyAvailable(strategyForType(entry.type));
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
    connect(m_sftpKeyBrowseButton, &QPushButton::clicked, this, &SettingsDialog::browseSftpKeyFile);
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
    const bool keyAuth =
        sftpAuthForIndex(m_sftpAuthCombo->currentIndex()) == Upload::SftpAuthMode::PrivateKey;
    m_sftpSecretLabel->setText(keyAuth ? tr("Key passphrase (optional):") : tr("Password:"));
    m_sftpForm->setRowVisible(m_sftpKeyPathRow, keyAuth);
    // Keep the hint in step with the mode - but never clobber the "stored" marker.
    if (m_sftpSecretEdit->placeholderText() != tr("•••••••• (stored)"))
        m_sftpSecretEdit->setPlaceholderText(keyAuth ? tr("Key passphrase") : tr("Password"));
}

void SettingsDialog::browseSftpKeyFile()
{
    QString start = m_sftpKeyPathEdit->text().trimmed();
    if (start.isEmpty())
        start = QDir::homePath() + QStringLiteral("/.ssh");
    const QString path = QFileDialog::getOpenFileName(this, tr("Select private key file"), start);
    if (!path.isEmpty())
        m_sftpKeyPathEdit->setText(path);
}

void SettingsDialog::refreshUploadList()
{
    const QSignalBlocker block(m_uploadList);   // don't fire selection changes while rebuilding
    m_uploadList->clear();
    for (const Upload::UploadProfile &p : m_uploadWorking)
        m_uploadList->addItem(displayRowText(p, p.id == m_uploadDefaultId));
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
    m_uploadTopForm->setRowVisible(
        m_uploadWarningRow,
        valid && !Upload::UploaderFactory::isStrategyAvailable(strategyForType(p.type)));

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
    QString hint;
    switch (p.type) {
    case Upload::ProviderType::S3:
        hint = tr("Secret access key");
        break;
    case Upload::ProviderType::Sftp:
        hint = p.sftpAuth == Upload::SftpAuthMode::PrivateKey ? tr("Key passphrase") : tr("Password");
        break;
    case Upload::ProviderType::Ftp:
        hint = tr("Password");
        break;
    }
    secret->setPlaceholderText(hasSecret ? tr("•••••••• (stored)") : hint);
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
            it->setText(displayRowText(p, p.id == m_uploadDefaultId));
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
    if (!Upload::UploaderFactory::isStrategyAvailable(strategyForType(p.type))) {
        setStatus(tr("%1 support is not included in this build.")
                      .arg(Upload::providerDisplayName(p.type)), "#c62828");
        return;
    }

    // Build the config straight from the working copy: the point is to test values the
    // user has typed but not saved. The secret comes from this session's staged entry if
    // there is one, else from the keychain - and is never written back here (only Apply
    // does that). `enabled` is forced on: the global upload switch says whether the app
    // uploads, not whether a destination may be checked while it is being set up.
    Upload::UploadConfig cfg;
    cfg.enabled = true;
    cfg.type = p.type;
    cfg.endpoint = p.endpoint;
    cfg.region = p.region;
    cfg.bucket = p.bucket;
    cfg.accessKeyId = p.accessKeyId;
    cfg.keyPrefix = p.keyPrefix;
    cfg.forcePathStyle = p.forcePathStyle;
    cfg.host = p.host;
    cfg.port = p.port;
    cfg.username = p.username;
    cfg.remoteDir = p.remoteDir;
    cfg.sftpAuth = p.sftpAuth;
    cfg.privateKeyPath = p.privateKeyPath;
    cfg.ftpEncryption = p.ftpEncryption;
    cfg.publicBaseUrl = p.publicBaseUrl;
    const QString staged = m_uploadNewSecrets.value(p.id);
    if (!staged.isEmpty()) {
        cfg.secretKey = staged;
    } else if (const auto stored =
                   Core::KeychainStore::retrieve(Upload::keychainServiceFor(p.type), p.id)) {
        cfg.secretKey = *stored;
    }

    if (!cfg.isComplete()) {
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
            edit->setKeySequence(Hotkeys::HotkeyBindings::defaultSequence(action));
        });

        auto *row = new QHBoxLayout();
        row->addWidget(edit);
        row->addWidget(defaultButton);
        form->addRow(Hotkeys::hotkeyActionDescription(action) + ":", row);

        connect(edit, &QKeySequenceEdit::keySequenceChanged, this, &SettingsDialog::validateHotkeys);
        m_hotkeyRows.append(HotkeyRow{action, edit});
    }

    layout->addWidget(group);

    // The factory answers for the platform without creating a backend, so no Carbon
    // handler is installed and no portal session is opened by opening Settings.
    if (!Hotkeys::HotkeyBackendFactory::isAvailable()) {
        auto *info = new QLabel(tr("Global hotkeys are not supported on this system."), m_hotkeysTab);
        info->setWordWrap(true);
        info->setStyleSheet("color: gray;");
        layout->addWidget(info);
        group->setEnabled(false);
    } else if (!Hotkeys::HotkeyBackendFactory::capabilities()
                    .testFlag(Hotkeys::HotkeyBackend::Capability::UserConfiguresKeys)) {
        auto *info = new QLabel(tr("Your desktop manages global shortcut keys. The combinations "
                                   "below are suggestions; the system's own shortcut dialog "
                                   "decides the final bindings."), m_hotkeysTab);
        info->setWordWrap(true);
        info->setStyleSheet("color: gray;");
        layout->addWidget(info);
    }

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

bool SettingsDialog::hasHotkeyConflicts() const
{
    QSet<QString> seen;
    for (const HotkeyRow &row : m_hotkeyRows) {
        const QKeySequence seq = row.edit->keySequence();
        if (seq.isEmpty())
            continue;
        const QString text = seq.toString(QKeySequence::PortableText);
        if (seen.contains(text))
            return true;
        seen.insert(text);
    }
    return false;
}

void SettingsDialog::browseScreenshotFolder()
{
    QString currentPath = m_screenshotFolderEdit->text();
    if (currentPath.isEmpty()) {
        currentPath = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    }

    QString folderPath = QFileDialog::getExistingDirectory(
        this,
        "Select Screenshot Folder",
        currentPath,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
    );

    if (!folderPath.isEmpty()) {
        m_screenshotFolderEdit->setText(folderPath);
    }
}

void SettingsDialog::chooseForegroundColor()
{
    QColor color = QColorDialog::getColor(m_foregroundColor, this, "Choose Foreground Color");
    if (color.isValid()) {
        m_foregroundColor = color;
        updateForegroundButtonStyle();
    }
}

void SettingsDialog::chooseBackgroundColor()
{
    QColor color = QColorDialog::getColor(
        m_backgroundColor,
        this,
        "Choose Background Color",
        QColorDialog::ShowAlphaChannel  // Allow transparency
    );
    if (color.isValid()) {
        m_backgroundColor = color;
        updateBackgroundButtonStyle();
    }
}

void SettingsDialog::updateForegroundButtonStyle()
{
    QString styleSheet = QString(
        "QPushButton {"
        "  background-color: %1;"
        "  border: 2px solid #555;"
        "  border-radius: 4px;"
        "}"
        "QPushButton:hover {"
        "  border: 2px solid #777;"
        "}"
    ).arg(m_foregroundColor.name());

    m_foregroundColorButton->setStyleSheet(styleSheet);
}

void SettingsDialog::updateBackgroundButtonStyle()
{
    // Create a checkerboard pattern for transparent backgrounds
    QString bgColor = m_backgroundColor.name(QColor::HexArgb);

    QString styleSheet = QString(
        "QPushButton {"
        "  background-color: %1;"
        "  border: 2px solid #555;"
        "  border-radius: 4px;"
        "}"
        "QPushButton:hover {"
        "  border: 2px solid #777;"
        "}"
    ).arg(bgColor);

    m_backgroundColorButton->setStyleSheet(styleSheet);

    // Update button text to show "Transparent" if fully transparent
    if (m_backgroundColor.alpha() == 0) {
        m_backgroundColorButton->setText("Transparent");
    } else {
        m_backgroundColorButton->setText("");
    }
}

void SettingsDialog::loadSettings()
{
    m_screenshotFolderEdit->setText(Core::Settings::screenshotFolder());

    int formatIndex = m_imageFormatCombo->findData(Core::Settings::imageFormat());
    if (formatIndex != -1) {
        m_imageFormatCombo->setCurrentIndex(formatIndex);
    }

    m_foregroundColor = Core::Settings::editorForeground();
    m_backgroundColor = Core::Settings::editorBackground();
    updateForegroundButtonStyle();
    updateBackgroundButtonStyle();

    m_cameraEnabledCheck->setChecked(Core::Settings::cameraEnabled());
    int camIdx = m_cameraCombo->findData(Core::Settings::cameraDeviceId());
    if (camIdx != -1)
        m_cameraCombo->setCurrentIndex(camIdx);
    m_cameraCombo->setEnabled(m_cameraEnabledCheck->isChecked());

    m_micEnabledCheck->setChecked(Core::Settings::micEnabled());
    int micIdx = m_micCombo->findData(Core::Settings::micDeviceId());
    if (micIdx != -1)
        m_micCombo->setCurrentIndex(micIdx);
    m_micCombo->setEnabled(m_micEnabledCheck->isChecked());

    m_systemAudioCheck->setChecked(Core::Settings::systemAudioEnabled());
    m_frameCheck->setChecked(Core::Settings::recordingFrameEnabled());

    for (const HotkeyRow &row : m_hotkeyRows)
        row.edit->setKeySequence(Hotkeys::HotkeyBindings::sequence(row.action));
    validateHotkeys();

    // Upload - load the working copy of all profiles (this also runs the one-time
    // legacy single-config migration the first time). Secrets stay in the keychain.
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

void SettingsDialog::saveSettings()
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
    for (auto it = m_uploadNewSecrets.cbegin(); it != m_uploadNewSecrets.cend(); ++it) {
        if (!workingIds.contains(it.key()) || it.value().isEmpty())
            continue;
        const auto profile = std::find_if(m_uploadWorking.cbegin(), m_uploadWorking.cend(),
                                          [&it](const Upload::UploadProfile &p) {
                                              return p.id == it.key();
                                          });
        if (profile != m_uploadWorking.cend())
            Core::KeychainStore::store(Upload::keychainServiceFor(profile->type), it.key(), it.value());
    }

    Upload::UploadProfiles::setAll(m_uploadWorking, m_uploadDefaultId);
    m_uploadNewSecrets.clear();
    m_uploadSecretEdit->clear();
    m_sftpSecretEdit->clear();
    m_ftpSecretEdit->clear();
}

void SettingsDialog::applySettings()
{
    // Two actions sharing a sequence would register once and fire the wrong one.
    if (hasHotkeyConflicts()) {
        m_tabWidget->setCurrentWidget(m_hotkeysTab);
        QMessageBox::warning(this, tr("Conflicting hotkeys"),
                             tr("Two actions are assigned the same hotkey. Change or clear one "
                                "of them before applying."));
        return;
    }

    // Create screenshot folder if it doesn't exist
    QString folderPath = m_screenshotFolderEdit->text();
    if (!folderPath.isEmpty()) {
        QDir dir;
        if (!dir.exists(folderPath)) {
            dir.mkpath(folderPath);
        }
    }

    saveSettings();
    emit settingsApplied();
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
    updateForegroundButtonStyle();
    updateBackgroundButtonStyle();

    m_cameraEnabledCheck->setChecked(false);
    m_micEnabledCheck->setChecked(false);
    m_systemAudioCheck->setChecked(true);
    if (m_cameraCombo->count() > 0)
        m_cameraCombo->setCurrentIndex(0);
    if (m_micCombo->count() > 0)
        m_micCombo->setCurrentIndex(0);
    m_cameraCombo->setEnabled(false);
    m_micCombo->setEnabled(false);

    // Form-level only, like the rest of this slot: committed on Apply.
    for (const HotkeyRow &row : m_hotkeyRows)
        row.edit->setKeySequence(Hotkeys::HotkeyBindings::defaultSequence(row.action));
    validateHotkeys();
}

} // namespace App
