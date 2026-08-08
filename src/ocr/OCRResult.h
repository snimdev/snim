#ifndef OCRRESULT_H
#define OCRRESULT_H

#include <QString>

namespace OCR {

/**
 * @brief Result of OCR processing
 */
class OCRResult {
public:
    OCRResult() : m_success(false), m_overallConfidence(0.0f) {}

    [[nodiscard]] bool isSuccess() const { return m_success; }
    [[nodiscard]] QString getText() const { return m_text; }
    [[nodiscard]] QString getErrorMessage() const { return m_errorMessage; }
    [[nodiscard]] float getOverallConfidence() const { return m_overallConfidence; }

    void setSuccess(bool success) { m_success = success; }
    void setText(const QString& text) { m_text = text; }
    void setErrorMessage(const QString& message) { m_errorMessage = message; }
    void setOverallConfidence(float confidence) { m_overallConfidence = confidence; }

private:
    bool m_success;
    QString m_text;
    QString m_errorMessage;
    float m_overallConfidence;
};

} // namespace OCR

#endif // OCRRESULT_H
