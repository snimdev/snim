#include "OCRService.h"
#include "core/Perf.h"
#include <QClipboard>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QBuffer>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QMetaObject>
#include <QPointer>
#include <QtConcurrentRun>
#include <utility>

#ifdef HAVE_TESSERACT
#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>
#endif

namespace OCR {

namespace {

// Every worker -> GUI hop goes through here; qApp is gone during shutdown, so dropping the post is correct.
template <typename F>
void postToGui(F &&fn)
{
    if (QCoreApplication *app = QCoreApplication::instance())
        QMetaObject::invokeMethod(app, std::forward<F>(fn), Qt::QueuedConnection);
}

#ifdef HAVE_TESSERACT
// The macOS bundle ships its own language packs, which Tesseract's built-in search
// path never finds. Returns nullptr elsewhere, which means "use the default path".
const char *bundledTessdataPath()
{
#ifdef Q_OS_MACOS
    static const QByteArray path = [] {
        const QString dir = QDir::cleanPath(QCoreApplication::applicationDirPath()
                                            + QStringLiteral("/../Resources/tessdata"));
        return QDir(dir).exists() ? QFile::encodeName(dir) : QByteArray();
    }();
    if (!path.isEmpty())
        return path.constData();
#endif
    return nullptr;
}
#endif

} // namespace

#ifdef HAVE_TESSERACT
/**
 * @brief Private implementation class for Tesseract integration
 */
class OCRService::Impl {
public:
    Impl() : m_api(new tesseract::TessBaseAPI()) {
        // Initialize Tesseract with English language by default
        if (m_api->Init(bundledTessdataPath(), "eng") != 0) {
            qWarning() << "Failed to initialize Tesseract API";
            delete m_api;
            m_api = nullptr;
        } else {
            m_language = "eng";
        }
    }

    ~Impl() {
        if (m_api) {
            m_api->End();
            delete m_api;
        }
    }

    tesseract::TessBaseAPI* api() { return m_api; }

    // Re-Inits only when the requested language differs from the loaded one.
    bool ensureLanguage(const QString& lang) {
        if (!m_api) {
            return false;
        }
        if (lang == m_language) {
            return true;
        }
        if (m_api->Init(bundledTessdataPath(), lang.toStdString().c_str()) != 0) {
            return false;
        }
        m_language = lang;
        return true;
    }

private:
    tesseract::TessBaseAPI* m_api;
    QString m_language;
};
#endif

OCRService::OCRService(QObject* parent)
    : QObject(parent)
#ifdef HAVE_TESSERACT
    , m_impl(new Impl())
#endif
{
}

OCRService::~OCRService() {
#ifdef HAVE_TESSERACT
    delete m_impl;
#endif
}

bool OCRService::isAvailable() {
#ifdef HAVE_TESSERACT
    return true;
#else
    return false;
#endif
}

QStringList OCRService::getAvailableLanguages() {
#ifdef HAVE_TESSERACT
    // Common languages, actual availability depends on installed language packs
    return {"eng", "deu", "fra", "spa", "ita", "por", "rus", "jpn", "chi_sim", "chi_tra"};
#else
    return {};
#endif
}

OCRResult OCRService::performOCR(const QPixmap& pixmap, const QString& language) {
    return performOCR(pixmap.toImage(), language);
}

OCRResult OCRService::performOCR(const QImage& image, const QString& language) {
    OCRResult result;

#ifdef HAVE_TESSERACT
    if (!m_impl || !m_impl->api()) {
        result.setSuccess(false);
        result.setErrorMessage("Tesseract API not initialized");
        return result;
    }

    // Times the whole Tesseract path; local timer only, so it is safe on the pool thread.
    QElapsedTimer perfTimer;
    perfTimer.start();

    // Preprocess the image
    QImage processedImage = preprocessImage(image);
    processedImage = convertToRGB888(processedImage);

    // Set language if different from current
    if (!m_impl->ensureLanguage(language)) {
        result.setSuccess(false);
        result.setErrorMessage("Failed to initialize Tesseract for language: " + language);
        emit ocrCompleted(result);
        return result;
    }

    // Cleared per call, after any re-Init (Init resets Tesseract variables)
    m_impl->api()->SetVariable("tesseract_char_whitelist", "");

    // Convert QImage to Tesseract format
    const int width = processedImage.width();
    const int height = processedImage.height();
    const int bytesPerLine = processedImage.bytesPerLine();
    const uchar* imageData = processedImage.constBits();

    // Set image for Tesseract
    m_impl->api()->SetImage(imageData, width, height, 3, bytesPerLine);

    // Perform OCR
    char* outText = m_impl->api()->GetUTF8Text();
    if (outText) {
        QString text = QString::fromUtf8(outText);
        result.setText(text);
        result.setSuccess(true);

        // Get confidence
        int confidence = m_impl->api()->MeanTextConf();
        result.setOverallConfidence(confidence / 100.0f);

        delete[] outText;

        // Optionally get word-level results for bounding boxes
        tesseract::ResultIterator* ri = m_impl->api()->GetIterator();
        QList<TextRegion> regions;

        if (ri) {
            tesseract::PageIteratorLevel level = tesseract::RIL_WORD;
            do {
                const char* word = ri->GetUTF8Text(level);
                if (word) {
                    float wordConfidence = ri->Confidence(level);
                    int x1, y1, x2, y2;
                    ri->BoundingBox(level, &x1, &y1, &x2, &y2);

                    TextRegion region;
                    region.text = QString::fromUtf8(word);
                    region.boundingBox = QRect(x1, y1, x2 - x1, y2 - y1);
                    region.confidence = wordConfidence / 100.0f;
                    regions.append(region);

                    delete[] word;
                }
            } while (ri->Next(level));

            delete ri;
        }

        result.setTextRegions(regions);
    } else {
        result.setSuccess(false);
        result.setErrorMessage("Failed to extract text from image");
    }

    Core::Perf::reportElapsed("ocr", perfTimer.elapsed(), Core::Perf::kBudgetOcrMs,
                              QStringLiteral("%1x%2 px").arg(image.width()).arg(image.height()));

#else
    result.setSuccess(false);
    result.setErrorMessage("Tesseract OCR is not available. Please install tesseract-ocr and rebuild the application.");
#endif

    emit ocrCompleted(result);
    return result;
}

void OCRService::performOCRAsync(const QImage& image, const QString& language,
                                 QObject* context, std::function<void(const OCRResult&)> onDone) {
    // Worker-local service: the Tesseract handle never leaves the pool thread that made it.
    (void) QtConcurrent::run([image, language, self = QPointer<QObject>(context),
                             onDone = std::move(onDone)] {
        OCRService svc;
        const OCRResult r = svc.performOCR(image, language);
        postToGui([self, r, onDone] {
            if (self)
                onDone(r);
        });
    });
}

QImage OCRService::preprocessImage(const QImage& image) const {
    QImage processed = image;

    // Tesseract works best when text x-height is at least 20 pixels
    // For typical text, we can estimate this as roughly 1/3 of the line height
    // If the image is too small, scale it up
    const int minHeight = 100; // Minimum height for decent OCR

    if (processed.height() < minHeight) {
        float scaleFactor = static_cast<float>(minHeight) / processed.height();
        processed = processed.scaled(
            processed.width() * scaleFactor,
            processed.height() * scaleFactor,
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation
        );
    }

    return processed;
}

QImage OCRService::convertToRGB888(const QImage& image) const {
    if (image.format() == QImage::Format_RGB888) {
        return image;
    }
    return image.convertToFormat(QImage::Format_RGB888);
}

} // namespace OCR
