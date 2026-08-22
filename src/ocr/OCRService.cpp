#include "OCRService.h"
#include "core/BundledPaths.h"
#include "core/Perf.h"
#include "core/PostToGui.h"
#include <QElapsedTimer>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QPointer>
#include <QtConcurrentRun>
#include <utility>

#ifdef HAVE_TESSERACT
#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>
#endif

namespace OCR {

namespace {

#ifdef HAVE_TESSERACT
// Tesseract works best when the text x-height is at least 20 px, roughly a third of a
// line, so a short image is scaled up to at least 100 px tall.
QImage preprocessImage(const QImage& image) {
    const int minHeight = 100;
    if (image.height() >= minHeight)
        return image;
    const float scaleFactor = static_cast<float>(minHeight) / image.height();
    return image.scaled(image.width() * scaleFactor, image.height() * scaleFactor,
                        Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

// A bundle ships its own language packs, which Tesseract's built-in search path never
// finds: macOS keeps them in the .app, a relocatable Linux install and Windows beside
// the binary.
// Returns nullptr when there are none, and whenever TESSDATA_PREFIX already names a
// directory, which both mean "use the default path".
const char *bundledTessdataPath()
{
#if defined(Q_OS_MACOS) || defined(Q_OS_LINUX) || defined(Q_OS_WIN)
    if (qEnvironmentVariableIsSet("TESSDATA_PREFIX"))
        return nullptr;
    static const QByteArray path = [] {
        const QString dir = Core::BundledPaths::tessdataDirForThisApp();
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

OCRService::OCRService()
#ifdef HAVE_TESSERACT
    : m_impl(std::make_unique<Impl>())
#endif
{
}

OCRService::~OCRService() = default;

bool OCRService::isAvailable() {
#ifdef HAVE_TESSERACT
    return true;
#else
    return false;
#endif
}

OCRResult OCRService::performOCR(const QImage& image, const QString& language) {
    OCRResult result;

#ifdef HAVE_TESSERACT
    if (!m_impl || !m_impl->api()) {
        result.errorMessage = "Tesseract API not initialized";
        return result;
    }

    // Times the whole Tesseract path; local timer only, so it is safe on the pool thread.
    QElapsedTimer perfTimer;
    perfTimer.start();

    const QImage processedImage = preprocessImage(image).convertToFormat(QImage::Format_RGB888);

    // Set language if different from current
    if (!m_impl->ensureLanguage(language)) {
        result.errorMessage = "Failed to initialize Tesseract for language: " + language;
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
        result.text = QString::fromUtf8(outText);
        result.success = true;
        result.overallConfidence = m_impl->api()->MeanTextConf() / 100.0f;
        delete[] outText;
    } else {
        result.errorMessage = "Failed to extract text from image";
    }

    Core::Perf::reportElapsed("ocr", perfTimer.elapsed(), Core::Perf::kBudgetOcrMs,
                              QStringLiteral("%1x%2 px").arg(image.width()).arg(image.height()));

#else
    result.errorMessage = "Tesseract OCR is not available. Please install tesseract-ocr and rebuild the application.";
#endif

    return result;
}

void OCRService::performOCRAsync(const QImage& image, const QString& language,
                                 QObject* context, std::function<void(const OCRResult&)> onDone) {
    // Worker-local service: the Tesseract handle never leaves the pool thread that made it.
    (void) QtConcurrent::run([image, language, self = QPointer<QObject>(context),
                             onDone = std::move(onDone)] {
        OCRService svc;
        const OCRResult r = svc.performOCR(image, language);
        Core::postToGui([self, r, onDone] {
            if (self)
                onDone(r);
        });
    });
}

} // namespace OCR
