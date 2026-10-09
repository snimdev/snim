#ifndef HOTKEYS_WINDOWSSCREENSHOTKEY_H
#define HOTKEYS_WINDOWSSCREENSHOTKEY_H

#include "hotkeys/ScreenshotKey.h"

namespace Hotkeys {

/**
 * Windows: Print Screen belongs to Snipping Tool while PrintScreenKeyForSnippingEnabled
 * is on (the default from Windows 11 22H2). release() turns it off and broadcasts the
 * change; the memento is the previous value (encodeSnippingSetting). No Windows types
 * here; the .cpp body is Q_OS_WIN-gated and only listed in the WIN32 build.
 */
class WindowsScreenshotKey : public ScreenshotKey
{
public:
    [[nodiscard]] Support support() const override { return Support::Automatic; }
    [[nodiscard]] QString keyName() const override;
    [[nodiscard]] QString ownerName() const override;
    [[nodiscard]] QString holderOf(const QKeySequence &seq) const override;
    [[nodiscard]] QList<HotkeyBinding> preset() const override;

    Result release() override;
    Result restore(const QString &memento) override;
};

} // namespace Hotkeys

#endif // HOTKEYS_WINDOWSSCREENSHOTKEY_H
