#include <QtTest>
#include <QStandardPaths>

#include "editor/ToolRegistry.h"
#include "editor/tools/ITool.h"
#include "editor/tools/ArrowTool.h"
#include "editor/tools/RectangleTool.h"
#include "editor/tools/EllipseTool.h"
#include "editor/tools/FreehandTool.h"
#include "editor/tools/HighlightTool.h"
#include "editor/tools/BlurTool.h"
#include "editor/tools/TextTool.h"
#include "editor/tools/StepTool.h"
#include "editor/interactions/IDrawingInteraction.h"

#include <QGraphicsItem>

using namespace Editor;

// Tool registry: one ToolSpec per tool supplies an interaction builder and a
// property-template builder, plus metadata.
class tst_ToolRegistry : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // makeTemplate() reads the default color via Core::Settings; isolate it.
        QCoreApplication::setOrganizationName("NiceshotTest");
        QCoreApplication::setApplicationName("tst_toolregistry");
        QStandardPaths::setTestModeEnabled(true);
    }

    void allExpectedToolsPresent()
    {
        QVERIFY(ToolRegistry::tools().size() >= 9);
        for (const char *id : {"pointer", "arrow", "text", "rectangle",
                               "ellipse", "freehand", "highlight", "blur", "step"})
            QVERIFY2(ToolRegistry::find(id) != nullptr, id);
        QCOMPARE(ToolRegistry::find("does-not-exist"), nullptr);
    }

    void pointerIsNotADrawingTool()
    {
        const ToolSpec *p = ToolRegistry::find("pointer");
        QVERIFY(p);
        QVERIFY(!p->isDrawingTool);
        QVERIFY(!p->autoRevealPanel);
        QVERIFY(!bool(p->makeTemplate));        // no property template
        QVERIFY(bool(p->makeInteraction));      // but it has an interaction
    }

    void drawingToolsHaveBothFactories()
    {
        for (const ToolSpec &s : ToolRegistry::tools()) {
            if (!s.isDrawingTool)
                continue;
            QVERIFY2(bool(s.makeInteraction), qPrintable(s.id));
            QVERIFY2(bool(s.makeTemplate), qPrintable(s.id));
            QVERIFY2(!s.namePrefix.isEmpty(), qPrintable(s.id));
        }
    }

    void makeTemplate_returnsExpectedConcreteType()
    {
        auto produces = [](const char *id) -> Tools::ITool* {
            const ToolSpec *s = ToolRegistry::find(id);
            return s && s->makeTemplate ? s->makeTemplate() : nullptr;
        };
        Tools::ITool *arrow = produces("arrow");
        QVERIFY(dynamic_cast<Tools::ArrowTool*>(arrow));
        QVERIFY(dynamic_cast<Tools::RectangleTool*>(produces("rectangle")));
        QVERIFY(dynamic_cast<Tools::EllipseTool*>(produces("ellipse")));
        QVERIFY(dynamic_cast<Tools::FreehandTool*>(produces("freehand")));
        QVERIFY(dynamic_cast<Tools::HighlightTool*>(produces("highlight")));
        QVERIFY(dynamic_cast<Tools::BlurTool*>(produces("blur")));
        QVERIFY(dynamic_cast<Tools::TextTool*>(produces("text")));
        QVERIFY(dynamic_cast<Tools::StepTool*>(produces("step")));

        // Templates are heap ITool* with no parent; free them (virtual ITool dtor).
        for (const ToolSpec &s : ToolRegistry::tools())
            if (s.makeTemplate)
                delete s.makeTemplate();
        delete arrow;
    }

    void makeInteraction_producesNonNull()
    {
        const ToolSpec *a = ToolRegistry::find("arrow");
        Interactions::IDrawingInteraction *i = a->makeInteraction(nullptr);
        QVERIFY(i);
        delete dynamic_cast<QObject*>(i);
    }
};

QTEST_MAIN(tst_ToolRegistry)
#include "tst_toolregistry.moc"
