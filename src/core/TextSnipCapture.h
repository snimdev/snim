#ifndef CORE_TEXTSNIPCAPTURE_H
#define CORE_TEXTSNIPCAPTURE_H

#include <QObject>
#include <QPixmap>
#include <QRect>
#include <memory>

namespace OCR {
class OCRService;
}

namespace Capture {
class AreaSelector;
class CaptureStrategy;
}

namespace Core {

/**
 * @brief Handles text snipping functionality using OCR
 *
 * This class manages the area selection and OCR processing for extracting
 * text from screen regions. It uses AreaSelector for user interaction and
 * OCRService for text extraction.
 */
class TextSnipCapture : public QObject
{
    Q_OBJECT

public:
    explicit TextSnipCapture(QObject *parent = nullptr);
    ~TextSnipCapture() override;

    /**
     * @brief Start the text snipping process
     * Captures the screen and shows area selector
     */
    void startTextSnip();

    /**
     * @brief Check if OCR is available
     * @return true if Tesseract is available, false otherwise
     */
    [[nodiscard]] static bool isOCRAvailable();

signals:
    /**
     * @brief Emitted when text extraction is complete
     * @param text The extracted text
     * @param success Whether OCR was successful
     */
    void textExtracted(const QString &text, bool success);

    /**
     * @brief Emitted when an error occurs
     * @param errorMessage Description of the error
     */
    void errorOccurred(const QString &errorMessage);

private slots:
    void onAreaSelected(const QRect &area);
    void onScreenCaptured(const QPixmap &screenshot);

private:
    void performOCR(const QPixmap &selectedRegion);
    std::unique_ptr<OCR::OCRService> m_ocrService;
    std::unique_ptr<Capture::CaptureStrategy> m_captureStrategy;
    QList<Capture::AreaSelector*> m_areaSelectors;
};

} // namespace Core

#endif // CORE_TEXTSNIPCAPTURE_H
