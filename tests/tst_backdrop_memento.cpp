#include <QtTest>
#include <QUndoStack>
#include <QColor>

#include "editor/image/BackdropItem.h"
#include "editor/image/BackdropMemento.h"
#include "editor/annotations/commands/EditorCommands.h"

using namespace Editor;
using namespace Editor::Image;

// BackdropItem snapshots/restores its complete state via an opaque BackdropMemento;
// BackdropChangeCommand holds the memento to make backdrop edits undoable. Assertions
// only use BackdropItem's public surface; the memento's contents stay opaque.
class tst_BackdropMemento : public QObject
{
    Q_OBJECT

private slots:
    void memento_roundTrip()
    {
        BackdropItem bd;
        bd.setProperty("padding", 120);
        bd.setProperty("fill", QString("Solid"));
        bd.setProperty("color", QColor("#123456"));
        bd.setActivePreset("Indigo");                 // set AFTER edits (edits clear it)

        const BackdropMemento snap = bd.createMemento();

        // Diverge…
        bd.setProperty("padding", 10);
        bd.setProperty("fill", QString("Gradient"));
        bd.setActivePreset("Other");

        // …then restore the complete state from the memento.
        bd.restore(snap);
        QCOMPARE(bd.padding(), 120);
        QCOMPARE(bd.toConfig().value("fill").toString(), QStringLiteral("Solid"));
        QCOMPARE(bd.toConfig().value("solidColor").toString(), QStringLiteral("#123456"));
        QCOMPARE(bd.activePreset(), QStringLiteral("Indigo"));   // active preset restored too
    }

    void command_undoRedo()
    {
        BackdropItem bd;
        bd.setProperty("padding", 50);

        QUndoStack stack;
        stack.push(new Commands::BackdropChangeCommand(
            &bd, "padding", 200, bd.createMemento(), "Backdrop padding"));
        QCOMPARE(bd.padding(), 200);     // redo applied the edit

        stack.undo();
        QCOMPARE(bd.padding(), 50);      // memento restored
        stack.redo();
        QCOMPARE(bd.padding(), 200);
    }

    void command_mergesSliderDrag()
    {
        BackdropItem bd;
        bd.setProperty("padding", 10);

        QUndoStack stack;
        stack.push(new Commands::BackdropChangeCommand(&bd, "padding", 20, bd.createMemento(), "p"));
        stack.push(new Commands::BackdropChangeCommand(&bd, "padding", 30, bd.createMemento(), "p"));

        QCOMPARE(stack.count(), 1);      // consecutive same-property edits merged
        QCOMPARE(bd.padding(), 30);      // latest value applied
        stack.undo();
        QCOMPARE(bd.padding(), 10);      // single undo restores the pre-drag value
    }
};

QTEST_MAIN(tst_BackdropMemento)
#include "tst_backdrop_memento.moc"
