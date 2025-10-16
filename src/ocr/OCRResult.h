#ifndef OCRRESULT_H
#define OCRRESULT_H

#include <QString>
#include <QRect>
#include <QList>

namespace OCR {

/**
 * @brief Structure representing a detected text region with OCR results
 */
struct TextRegion {
    QString text;           // Recognized text
    QRect boundingBox;      // Bounding box of the text region
    float confidence;       // Confidence score (0.0 - 1.0)
};

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
    [[nodiscard]] const QList<TextRegion>& getTextRegions() const { return m_textRegions; }

    void setSuccess(bool success) { m_success = success; }
    void setText(const QString& text) { m_text = text; }
    void setErrorMessage(const QString& message) { m_errorMessage = message; }
    void setOverallConfidence(float confidence) { m_overallConfidence = confidence; }
    void setTextRegions(const QList<TextRegion>& regions) { m_textRegions = regions; }

private:
    bool m_success;
    QString m_text;
    QString m_errorMessage;
    float m_overallConfidence;
    QList<TextRegion> m_textRegions;
};

} // namespace OCR

#endif // OCRRESULT_H
