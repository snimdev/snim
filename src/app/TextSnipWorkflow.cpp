#include "app/TextSnipWorkflow.h"
#include "capture/CaptureFactory.h"
#include "capture/strategies/CaptureStrategy.h"
#include "ocr/OCRService.h"
#include <QScreen>
#include <QGuiApplication>
#include <QClipboard>
#include <QMessageBox>

namespace App {

TextSnipWorkflow::TextSnipWorkflow(QObject *parent)
    : QObject(parent)
{
    // Initialize capture strategy using factory
    m_captureStrategy = Capture::CaptureFactory::createStrategy(
        Capture::CaptureFactory::StrategyType::Auto,
        this
    );

    // OCR only needs the selected region, so no Edit/Copy/Save action toolbar.
    m_captureStrategy->setQuickActionsEnabled(false);

    // The capture strategy hands over the selected area, which is all OCR needs.
    connect(m_captureStrategy.get(), &Capture::CaptureStrategy::screenshotReady,
            this, &TextSnipWorkflow::performOCR);
    connect(m_captureStrategy.get(), &Capture::CaptureStrategy::screenshotFailed,
            this, [](const QString &error) {
                QMessageBox::warning(nullptr, "Screenshot Failed", error);
            });
    connect(m_captureStrategy.get(), &Capture::CaptureStrategy::screenshotCancelled,
            this, [] { qDebug() << "Text snip cancelled"; });
}

TextSnipWorkflow::~TextSnipWorkflow() {
    // Never leak the wait cursor if we die mid OCR
    if (m_ocrInFlight) {
        QGuiApplication::restoreOverrideCursor();
    }
}

void TextSnipWorkflow::startTextSnip() {
    // One snip at a time now that OCR is async
    if (m_ocrInFlight) {
        return;
    }

    if (!OCR::OCRService::isAvailable()) {
        QMessageBox::warning(nullptr, "OCR Not Available",
                           "Tesseract OCR is not installed or not available.\n\n"
                           "Please install it using:\n"
                           "  Fedora/RHEL: sudo dnf install tesseract-devel\n"
                           "  Ubuntu/Debian: sudo apt install tesseract-ocr libtesseract-dev\n"
                           "  Arch: sudo pacman -S tesseract");
        return;
    }

    qDebug() << "Starting text snip with capture strategy:" << m_captureStrategy->name();

    // Use the capture strategy to capture the screen
    // This will properly handle both Wayland and X11
    m_captureStrategy->captureArea();
}

void TextSnipWorkflow::performOCR(const QPixmap &selectedRegion) {
    if (selectedRegion.isNull())
        return;

    if (m_ocrInFlight) {
        return;
    }
    m_ocrInFlight = true;

    qDebug() << "Performing OCR on screenshot, size:" << selectedRegion.size();

    QGuiApplication::setOverrideCursor(Qt::WaitCursor);

    // A QPixmap must not cross threads, so the worker gets a QImage
    const QImage image = selectedRegion.toImage();

    OCR::OCRService::performOCRAsync(image, "eng", this, [this](const OCR::OCRResult &result) {
        QGuiApplication::restoreOverrideCursor();
        m_ocrInFlight = false;

        if (result.success) {
            QString text = result.text.trimmed();

            if (text.isEmpty()) {
                QMessageBox::information(nullptr, "No Text Found",
                                       "No text was detected in the selected area.\n\n"
                                       "Tips:\n"
                                       "• Ensure the text is clear and readable\n"
                                       "• Select a larger area with more context\n"
                                       "• Check if the text language is supported");
            } else {
                // Copy to clipboard
                QClipboard *clipboard = QGuiApplication::clipboard();
                clipboard->setText(text);

                // Show notification
                QMessageBox msgBox;
                msgBox.setWindowTitle("Text Extracted");
                msgBox.setText("Text has been copied to clipboard!");
                msgBox.setDetailedText(text);
                msgBox.setIcon(QMessageBox::Information);

                // Show confidence if available
                if (result.overallConfidence > 0) {
                    msgBox.setInformativeText(
                        QString("Confidence: %1%").arg(
                            static_cast<int>(result.overallConfidence * 100)
                        )
                    );
                }

                msgBox.exec();
            }
        } else {
            QMessageBox::warning(nullptr, "OCR Failed", result.errorMessage);
        }
    });
}

} // namespace App
