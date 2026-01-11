#include "core/Perf.h"

#include <QElapsedTimer>

Q_LOGGING_CATEGORY(lcPerf, "Niceshot.perf")

namespace Core::Perf {

namespace {
    // Capture latency state, GUI thread only
    QElapsedTimer g_captureTimer;
    const char *g_captureLabel = "capture";
    bool g_capturePending = false;
}

void markCaptureStart(const char *what)
{
    g_captureLabel = what;
    g_captureTimer.start();
    g_capturePending = true;
}

void reportCaptureShown(const char *stage)
{
    if (!g_capturePending)
        return;
    g_capturePending = false;
    reportElapsed(g_captureLabel, g_captureTimer.elapsed(), kBudgetOverlayMs, QString::fromUtf8(stage));
}

void reportElapsed(const char *what, qint64 elapsedMs, qint64 budgetMs, const QString &detail)
{
    const QString head = detail.isEmpty() ? QString::fromUtf8(what)
                                          : QStringLiteral("%1 %2").arg(QString::fromUtf8(what), detail);
    const QString line = QStringLiteral("%1 %2 ms (budget %3 ms)").arg(head).arg(elapsedMs).arg(budgetMs);
    if (elapsedMs > budgetMs)
        qCWarning(lcPerf).noquote() << line + QStringLiteral(" OVER BUDGET");
    else
        qCDebug(lcPerf).noquote() << line;
}

} // namespace Core::Perf
