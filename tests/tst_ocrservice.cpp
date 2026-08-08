#include <QtTest>
#include <QCoreApplication>
#include <QPixmap>
#include <QPainter>
#include <QFont>
#include <QImage>
#include <QThread>

#include "ocr/OCRService.h"
#include "ocr/OCRResult.h"

using namespace OCR;

// Availability-gated so the suite stays green whether or not Tesseract (and its
// language data) is present.
class tst_OCRService : public QObject
{
    Q_OBJECT

private slots:
    void availabilityContract()
    {
        if (OCRService::isAvailable())
            QSKIP("Tesseract compiled in; covered by recognizesRenderedText()");
        // No Tesseract: performOCR must fail cleanly with a message.
        OCRService svc;
        const OCRResult r = svc.performOCR(QImage(10, 10, QImage::Format_RGB32));
        QVERIFY(!r.isSuccess());
        QVERIFY(!r.getErrorMessage().isEmpty());
    }

    void recognizesRenderedText()
    {
        if (!OCRService::isAvailable())
            QSKIP("Tesseract not compiled in");

        QPixmap pm(360, 110);
        pm.fill(Qt::white);
        {
            QPainter p(&pm);
            QFont f("Helvetica", 56);
            f.setBold(true);
            p.setFont(f);
            p.setPen(Qt::black);
            p.drawText(pm.rect(), Qt::AlignCenter, "HELLO");
        }

        OCRService svc;
        const OCRResult r = svc.performOCR(pm.toImage(), "eng");
        if (!r.isSuccess())
            QSKIP("Tesseract present but init/recognition unavailable on this host (e.g. no tessdata)");
        QVERIFY(r.getText().toUpper().contains("HELLO"));
    }

    void asyncDeliversOnCallerThread()
    {
        QPixmap pm(360, 110);
        pm.fill(Qt::white);
        {
            QPainter p(&pm);
            QFont f("Helvetica", 56);
            f.setBold(true);
            p.setFont(f);
            p.setPen(Qt::black);
            p.drawText(pm.rect(), Qt::AlignCenter, "HELLO");
        }

        bool done = false;
        OCRResult res;
        QThread *seenThread = nullptr;
        OCRService::performOCRAsync(pm.toImage(), "eng", this,
                                    [&](const OCRResult &r) {
                                        res = r;
                                        seenThread = QThread::currentThread();
                                        done = true;
                                    });

        QTRY_VERIFY_WITH_TIMEOUT(done, 15000);
        QCOMPARE(seenThread, QCoreApplication::instance()->thread());

        if (!OCRService::isAvailable()) {
            // No Tesseract: the async path must still report a clean failure.
            QVERIFY(!res.isSuccess());
            QVERIFY(!res.getErrorMessage().isEmpty());
            return;
        }
        if (!res.isSuccess())
            QSKIP("Tesseract present but init/recognition unavailable on this host (e.g. no tessdata)");
        QVERIFY(res.getText().toUpper().contains("HELLO"));
    }

    void asyncDroppedWhenContextDies()
    {
        // Smoke test that a dead context drops delivery without crashing.
        auto *ctx = new QObject;
        bool fired = false;

        QImage img(20, 20, QImage::Format_RGB32);
        img.fill(Qt::white);
        OCRService::performOCRAsync(img, "eng", ctx,
                                    [&](const OCRResult &) { fired = true; });
        delete ctx;

        QTest::qWait(3000);
        QVERIFY(!fired);
    }
};

QTEST_MAIN(tst_OCRService)
#include "tst_ocrservice.moc"
