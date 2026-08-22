#ifndef OCRRESULT_H
#define OCRRESULT_H

#include <QString>

namespace OCR {

// What one OCR pass found: the text, or why there is none.
struct OCRResult {
    bool success = false;
    QString text;
    QString errorMessage;
    float overallConfidence = 0.0f;   // 0..1
};

} // namespace OCR

#endif // OCRRESULT_H
