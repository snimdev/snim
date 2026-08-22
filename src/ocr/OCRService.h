#ifndef OCRSERVICE_H
#define OCRSERVICE_H

#include <QImage>
#include <functional>
#include <memory>
#include "OCRResult.h"

class QObject;

namespace OCR {

/**
 * Text extraction from images with Tesseract. A build without Tesseract still has the
 * class; every call then fails with a message saying so. One instance holds one
 * Tesseract handle, so it stays on the thread that made it.
 */
class OCRService {
public:
    OCRService();
    ~OCRService();

    // Whether Tesseract was compiled in.
    [[nodiscard]] static bool isAvailable();

    // language is a Tesseract code such as "eng".
    [[nodiscard]] OCRResult performOCR(const QImage& image, const QString& language = "eng");

    // Runs performOCR on a QtConcurrent pool thread with a worker-local service, then
    // hands the result to onDone on the GUI thread, but only while context is alive.
    static void performOCRAsync(const QImage& image, const QString& language,
                                QObject* context, std::function<void(const OCRResult&)> onDone);

private:
#ifdef HAVE_TESSERACT
    class Impl;
    std::unique_ptr<Impl> m_impl;
#endif
};

} // namespace OCR

#endif // OCRSERVICE_H
