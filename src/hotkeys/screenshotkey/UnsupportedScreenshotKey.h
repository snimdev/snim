#ifndef HOTKEYS_UNSUPPORTEDSCREENSHOTKEY_H
#define HOTKEYS_UNSUPPORTEDSCREENSHOTKEY_H

#include "hotkeys/ScreenshotKey.h"

namespace Hotkeys {

/**
 * Null Object for a system whose screenshot key Snim cannot take: other desktops, a
 * Linux session without the GlobalShortcuts portal. Holds no key, frees nothing, so
 * callers never test for a missing strategy.
 */
class UnsupportedScreenshotKey : public ScreenshotKey
{
public:
    [[nodiscard]] Support support() const override { return Support::Unsupported; }
    [[nodiscard]] QString keyName() const override { return tr("the screenshot key"); }
    [[nodiscard]] QString ownerName() const override { return tr("your system"); }
    [[nodiscard]] QString holderOf(const QKeySequence &) const override { return {}; }
    [[nodiscard]] QList<HotkeyBinding> preset() const override { return {}; }

    Result release() override { return {false, {}, unsupported()}; }
    Result restore(const QString &) override { return {false, {}, unsupported()}; }

private:
    [[nodiscard]] static QString unsupported()
    {
        return tr("Your system does not let Snim take its screenshot key.");
    }
};

} // namespace Hotkeys

#endif // HOTKEYS_UNSUPPORTEDSCREENSHOTKEY_H
