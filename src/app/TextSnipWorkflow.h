#ifndef APP_TEXTSNIPWORKFLOW_H
#define APP_TEXTSNIPWORKFLOW_H

#include <QObject>
#include <QPixmap>
#include <memory>

namespace Capture {
class CaptureStrategy;
}

namespace App {

/**
 * @brief Handles text snipping functionality using OCR
 *
 * This class manages the area selection and OCR processing for extracting
 * text from screen regions. It uses AreaSelector for user interaction and
 * OCRService for text extraction.
 */
class TextSnipWorkflow : public QObject
{
    Q_OBJECT

public:
    explicit TextSnipWorkflow(QObject *parent = nullptr);
    ~TextSnipWorkflow() override;

    /**
     * @brief Start the text snipping process
     * Captures the screen and shows area selector
     */
    void startTextSnip();

private:
    void performOCR(const QPixmap &selectedRegion);
    std::unique_ptr<Capture::CaptureStrategy> m_captureStrategy;
    bool m_ocrInFlight = false;
};

} // namespace App

#endif // APP_TEXTSNIPWORKFLOW_H
