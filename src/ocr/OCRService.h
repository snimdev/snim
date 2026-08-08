#ifndef OCRSERVICE_H
#define OCRSERVICE_H

#include <QObject>
#include <QImage>
#include <functional>
#include "OCRResult.h"

namespace OCR {

/**
 * @brief Service class for performing OCR on images using Tesseract
 *
 * This class provides an interface for extracting text from images.
 * If Tesseract is not available at build time, the methods will return
 * appropriate error messages.
 */
class OCRService : public QObject {
    Q_OBJECT

public:
    explicit OCRService(QObject* parent = nullptr);
    ~OCRService() override;

    /**
     * @brief Check if OCR functionality is available
     * @return true if Tesseract is available, false otherwise
     */
    [[nodiscard]] static bool isAvailable();

    /**
     * @brief Perform OCR on a QImage
     * @param image The image to process
     * @param language Language code (default: "eng" for English)
     * @return OCRResult containing the extracted text and metadata
     */
    [[nodiscard]] OCRResult performOCR(const QImage& image, const QString& language = "eng");

    /**
     * @brief Perform OCR on a QtConcurrent pool thread with a worker-local OCRService
     * @param image The image to process (a QImage, so it can cross threads)
     * @param language Language code (e.g. "eng")
     * @param context Lifetime guard: onDone runs only if it is still alive
     * @param onDone Callback invoked with the result on the GUI thread
     */
    static void performOCRAsync(const QImage& image, const QString& language,
                                QObject* context, std::function<void(const OCRResult&)> onDone);

private:
    /**
     * @brief Preprocess image for better OCR results
     * Scales image if text is too small (target: x-height >= 20px)
     * @param image Input image
     * @return Preprocessed image
     */
    [[nodiscard]] QImage preprocessImage(const QImage& image) const;

    /**
     * @brief Convert QImage to format suitable for Tesseract
     * @param image Input image
     * @return Converted image in RGB888 format
     */
    [[nodiscard]] QImage convertToRGB888(const QImage& image) const;

#ifdef HAVE_TESSERACT
    class Impl;
    Impl* m_impl;
#endif
};

} // namespace OCR

#endif // OCRSERVICE_H
