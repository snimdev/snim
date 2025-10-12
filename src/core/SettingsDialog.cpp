#include "SettingsDialog.h"
#include <QSettings>
#include <QGroupBox>
#include <QStandardPaths>
#include <QDir>

namespace Core {

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
    , m_tabWidget(nullptr)
    , m_generalTab(nullptr)
    , m_screenshotFolderEdit(nullptr)
    , m_browseButton(nullptr)
    , m_imageFormatCombo(nullptr)
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
    layout->addStretch();

    m_tabWidget->addTab(m_generalTab, "General");
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

void SettingsDialog::loadSettings()
{
    QSettings settings;

    // Load screenshot folder
    QString defaultFolder = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)
                           + "/Screenshots";
    QString screenshotFolder = settings.value("General/ScreenshotFolder", defaultFolder).toString();
    m_screenshotFolderEdit->setText(screenshotFolder);

    // Load image format
    QString imageFormat = settings.value("General/ImageFormat", "png").toString();
    int formatIndex = m_imageFormatCombo->findData(imageFormat);
    if (formatIndex != -1) {
        m_imageFormatCombo->setCurrentIndex(formatIndex);
    }
}

void SettingsDialog::saveSettings()
{
    QSettings settings;

    // Save screenshot folder
    settings.setValue("General/ScreenshotFolder", m_screenshotFolderEdit->text());

    // Save image format
    QString selectedFormat = m_imageFormatCombo->currentData().toString();
    settings.setValue("General/ImageFormat", selectedFormat);

    settings.sync();
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
}

} // namespace Core
