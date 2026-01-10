#include "TextSnipCapture.h"
#include "../capture/CaptureFactory.h"
#include "../capture/strategies/CaptureStrategy.h"
#include "../ocr/OCRService.h"
#include <QScreen>
#include <QGuiApplication>
#include <QClipboard>
#include <QMessageBox>

namespace Core {

TextSnipCapture::TextSnipCapture(QObject *parent)
    : QObject(parent)
{
    // Initialize capture strategy using factory
    m_captureStrategy = Capture::CaptureFactory::createStrategy(
        Capture::CaptureFactory::StrategyType::Auto,
        this
    );

    // OCR only needs the selected region, so no Edit/Copy/Save action toolbar.
    m_captureStrategy->setQuickActionsEnabled(false);

    connect(m_captureStrategy.get(), &Capture::CaptureStrategy::screenshotReady,
            this, &TextSnipCapture::onScreenCaptured);
    connect(m_captureStrategy.get(), &Capture::CaptureStrategy::screenshotFailed,
            this, [this](const QString &error) {
                emit errorOccurred(error);
                QMessageBox::warning(nullptr, "Screenshot Failed", error);
            });
}

TextSnipCapture::~TextSnipCapture() {
    // Never leak the wait cursor if we die mid OCR
    if (m_ocrInFlight) {
        QGuiApplication::restoreOverrideCursor();
    }
}

bool TextSnipCapture::isOCRAvailable() {
    return OCR::OCRService::isAvailable();
}

void TextSnipCapture::startTextSnip() {
    // One snip at a time now that OCR is async
    if (m_ocrInFlight) {
        return;
    }

    if (!isOCRAvailable()) {
        emit errorOccurred("Tesseract OCR is not available. Please install tesseract-ocr package.");
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

void TextSnipCapture::onScreenCaptured(const QPixmap &screenshot) {
    qDebug() << "Screen captured for text snip, size:" << screenshot.size();

    // The screenshot from capture strategy is already the selected area
    // Perform OCR directly on it
    performOCR(screenshot);
}

void TextSnipCapture::performOCR(const QPixmap &selectedRegion) {
    if (selectedRegion.isNull()) {
        emit errorOccurred("No screenshot available for OCR");
        return;
    }

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

        if (result.isSuccess()) {
            QString text = result.getText().trimmed();

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
                if (result.getOverallConfidence() > 0) {
                    msgBox.setInformativeText(
                        QString("Confidence: %1%").arg(
                            static_cast<int>(result.getOverallConfidence() * 100)
                        )
                    );
                }

                msgBox.exec();
            }

            emit textExtracted(text, true);
        } else {
            emit errorOccurred(result.getErrorMessage());
            QMessageBox::warning(nullptr, "OCR Failed", result.getErrorMessage());
        }
    });
}

} // namespace Core
