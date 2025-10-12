#include "ScreenshotDialog.h"
#include "ImageEditor.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QApplication>
#include <QClipboard>
#include <QDebug>

ScreenshotDialog::ScreenshotDialog(const QPixmap &screenshot, QWidget *parent)
    : QDialog(parent)
    , m_screenshot(screenshot)
{
    setWindowTitle("Screenshot Captured");
    setModal(true);

    auto *mainLayout = new QVBoxLayout(this);

    // Preview label
    m_previewLabel = new QLabel();
    QPixmap preview = m_screenshot.scaled(400, 300, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    m_previewLabel->setPixmap(preview);
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setStyleSheet("border: 1px solid gray;");
    mainLayout->addWidget(m_previewLabel);

    // Buttons
    auto *buttonLayout = new QHBoxLayout();

    m_copyButton = new QPushButton("Copy to Clipboard");
    m_copyButton->setDefault(true);
    connect(m_copyButton, &QPushButton::clicked, this, &ScreenshotDialog::copyToClipboard);

    m_editorButton = new QPushButton("Open in Editor");
    connect(m_editorButton, &QPushButton::clicked, this, &ScreenshotDialog::openInEditor);

    m_cancelButton = new QPushButton("Cancel");
    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);

    buttonLayout->addWidget(m_copyButton);
    buttonLayout->addWidget(m_editorButton);
    buttonLayout->addWidget(m_cancelButton);

    mainLayout->addLayout(buttonLayout);

    resize(450, 400);
}

void ScreenshotDialog::copyToClipboard()
{
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setPixmap(m_screenshot);
    accept();
}

void ScreenshotDialog::openInEditor()
{
    qDebug() << "Opening editor with screenshot size:" << m_screenshot.size();
    auto *editor = new ImageEditor(m_screenshot);
    editor->setAttribute(Qt::WA_DeleteOnClose);
    qDebug() << "Editor created, showing...";

    // Ensure the editor window is properly shown and brought to front
    editor->show();
    editor->raise();
    editor->activateWindow();

    qDebug() << "Editor show() called";
    accept(); // Close the dialog
}
