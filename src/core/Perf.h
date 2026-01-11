#ifndef CORE_PERFLOG_H
#define CORE_PERFLOG_H

#include <QLoggingCategory>
#include <QString>

Q_DECLARE_LOGGING_CATEGORY(lcPerf)

namespace Core::Perf {

// Bounds hotkey-to-overlay-visible latency; past this the capture feels laggy.
constexpr qint64 kBudgetOverlayMs = 150;
// Bounds a single Tesseract recognition pass on a typical text snip.
constexpr qint64 kBudgetOcrMs = 2000;
// Bounds one full scene render for save / clipboard / upload.
constexpr qint64 kBudgetRenderMs = 100;

// Arms the capture latency timer, GUI thread only
void markCaptureStart(const char *what);

// Logs hotkey-to-visible once per capture, later calls no-op
void reportCaptureShown(const char *stage);

// Thread-safe budget line
void reportElapsed(const char *what, qint64 elapsedMs, qint64 budgetMs, const QString &detail = {});

} // namespace Core::Perf

#endif // CORE_PERFLOG_H
