#include "SettingsDialog.h"
#include "core/Settings.h"
#include <QGroupBox>
#include <QStandardPaths>
#include <QDir>
#include <QMediaDevices>
#include <QCameraDevice>
#include <QAudioDevice>
#include <QApplication>
#include <QPermissions>

namespace Core {

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
    // dialog has focus — at recording time the full-screen selection overlays sit
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

void SettingsDialog::setupUploadTab()
{
    m_uploadTab = new QWidget();

    auto *layout = new QVBoxLayout(m_uploadTab);

    auto *label = new QLabel("Upload settings will be implemented here.", m_uploadTab);
    label->setAlignment(Qt::AlignCenter);
    label->setStyleSheet("color: gray; font-style: italic;");

    layout->addWidget(label);
    layout->addStretch();

    m_tabWidget->addTab(m_uploadTab, "Upload");
}

void SettingsDialog::setupHotkeysTab()
{
    m_hotkeysTab = new QWidget();

    auto *layout = new QVBoxLayout(m_hotkeysTab);

    auto *label = new QLabel("Hotkey settings will be implemented here.", m_hotkeysTab);
    label->setAlignment(Qt::AlignCenter);
    label->setStyleSheet("color: gray; font-style: italic;");

    layout->addWidget(label);
    layout->addStretch();

    m_tabWidget->addTab(m_hotkeysTab, "Hotkeys");
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
    m_screenshotFolderEdit->setText(Settings::screenshotFolder());

    int formatIndex = m_imageFormatCombo->findData(Settings::imageFormat());
    if (formatIndex != -1) {
        m_imageFormatCombo->setCurrentIndex(formatIndex);
    }

    m_foregroundColor = Settings::editorForeground();
    m_backgroundColor = Settings::editorBackground();
    updateForegroundButtonStyle();
    updateBackgroundButtonStyle();

    m_cameraEnabledCheck->setChecked(Settings::cameraEnabled());
    int camIdx = m_cameraCombo->findData(Settings::cameraDeviceId());
    if (camIdx != -1)
        m_cameraCombo->setCurrentIndex(camIdx);
    m_cameraCombo->setEnabled(m_cameraEnabledCheck->isChecked());

    m_micEnabledCheck->setChecked(Settings::micEnabled());
    int micIdx = m_micCombo->findData(Settings::micDeviceId());
    if (micIdx != -1)
        m_micCombo->setCurrentIndex(micIdx);
    m_micCombo->setEnabled(m_micEnabledCheck->isChecked());

    m_systemAudioCheck->setChecked(Settings::systemAudioEnabled());
    m_frameCheck->setChecked(Settings::recordingFrameEnabled());
}

void SettingsDialog::saveSettings()
{
    Settings::setScreenshotFolder(m_screenshotFolderEdit->text());
    Settings::setImageFormat(m_imageFormatCombo->currentData().toString());
    Settings::setEditorForeground(m_foregroundColor);
    Settings::setEditorBackground(m_backgroundColor);

    Settings::setCameraEnabled(m_cameraEnabledCheck->isChecked());
    Settings::setCameraDeviceId(m_cameraCombo->currentData().toByteArray());
    Settings::setMicEnabled(m_micEnabledCheck->isChecked());
    Settings::setMicDeviceId(m_micCombo->currentData().toByteArray());
    Settings::setSystemAudioEnabled(m_systemAudioCheck->isChecked());
    Settings::setRecordingFrameEnabled(m_frameCheck->isChecked());
}

void SettingsDialog::applySettings()
{
    // Create screenshot folder if it doesn't exist
    QString folderPath = m_screenshotFolderEdit->text();
    if (!folderPath.isEmpty()) {
        QDir dir;
        if (!dir.exists(folderPath)) {
            dir.mkpath(folderPath);
        }
    }

    saveSettings();
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
}

} // namespace Core
