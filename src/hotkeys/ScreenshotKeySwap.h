#ifndef HOTKEYS_SCREENSHOTKEYSWAP_H
#define HOTKEYS_SCREENSHOTKEYSWAP_H

#include "hotkeys/HotkeyAction.h"
#include "hotkeys/ScreenshotKey.h"

#include <QCoreApplication>
#include <QList>
#include <QString>

#include <functional>
#include <memory>
#include <optional>

namespace Hotkeys {

// Memento: the OS state and Snim's bindings from before the swap.
struct ScreenshotKeyMemento {
    QString system;                  // what ScreenshotKey::release() returned
    QList<HotkeyBinding> bindings;   // effective pre-swap bindings of the preset's actions

    [[nodiscard]] QString toJson() const;
    // nullopt for anything that is not a memento this class wrote.
    [[nodiscard]] static std::optional<ScreenshotKeyMemento> fromJson(const QString &json);
};

/**
 * Command with undo, and caretaker of the memento: swap() frees the screenshot key and
 * puts the strategy's preset on Snim's actions, undo() gives the key back and restores
 * the bindings the user has not changed since. The memento lives in Core::Settings, so
 * the swap outlasts the run. reapply re-registers the hotkeys after either step.
 */
class ScreenshotKeySwap
{
    Q_DECLARE_TR_FUNCTIONS(ScreenshotKeySwap)

public:
    // A null key falls back to UnsupportedScreenshotKey.
    ScreenshotKeySwap(std::unique_ptr<ScreenshotKey> key, std::function<void()> reapply);

    [[nodiscard]] ScreenshotKey &key() const;
    // A memento is stored: the key is Snim's until undo().
    [[nodiscard]] bool isSwapped() const;

    // Each returns the strategy's result; a refusal changes nothing.
    ScreenshotKey::Result swap();
    ScreenshotKey::Result undo();

private:
    void reapply() const;

    std::unique_ptr<ScreenshotKey> m_key;
    std::function<void()> m_reapply;
};

} // namespace Hotkeys

#endif // HOTKEYS_SCREENSHOTKEYSWAP_H
