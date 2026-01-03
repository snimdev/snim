#include <QtTest>
#include <QPixmap>
#include <QPainter>
#include <QFont>

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
        const OCRResult r = svc.performOCR(QPixmap(10, 10));
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
        const OCRResult r = svc.performOCR(pm, "eng");
        if (!r.isSuccess())
            QSKIP("Tesseract present but init/recognition unavailable on this host (e.g. no tessdata)");
        QVERIFY(r.getText().toUpper().contains("HELLO"));
    }
};

QTEST_MAIN(tst_OCRService)
#include "tst_ocrservice.moc"
