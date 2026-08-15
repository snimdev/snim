#ifndef TESTS_FAKEFRAMESOURCES_H
#define TESTS_FAKEFRAMESOURCES_H

#include <QList>
#include <QMap>
#include <QPixmap>
#include <QTimer>
#include <memory>

#include "screen/sources/DesktopFrameSource.h"
#include "screen/sources/FrameSourceChain.h"

// Scripted frame sources for the chain walk, so no test needs a real desktop.
namespace FakeFrames {

using Type = Screen::SourceType;

// What a fake source answers when grabbed.
struct Script {
    enum Kind { Frame, Fail, Cancel } kind = Frame;
    QSize pixels{200, 100};
    qreal dpr = 2.0;
    QRect geometry{0, 0, 100, 50};
    bool async = false;
    bool picker = false;   // announce a screen picker first, as a ScreenCast source without consent
};

class Source : public Screen::DesktopFrameSource
{
public:
    Source(const Script &script, QObject *parent) : DesktopFrameSource(parent), m_script(script) {}

    void grab() override
    {
        if (m_script.picker)
            emit sourcePickerExpected(true);
        if (m_script.async)
            QTimer::singleShot(0, this, [this] { answer(); });
        else
            answer();
    }

private:
    void answer()
    {
        switch (m_script.kind) {
        case Script::Frame: {
            QPixmap frame(m_script.pixels);
            frame.fill(Qt::red);
            frame.setDevicePixelRatio(m_script.dpr);
            emit frameReady(frame, m_script.geometry);
            return;
        }
        case Script::Fail:
            emit frameFailed(QStringLiteral("broken"), false);
            return;
        case Script::Cancel:
            emit frameFailed(QStringLiteral("dismissed"), true);
            return;
        }
    }

    Script m_script;
};

// The sources one desktop offers, by type, and which of them a walk asked.
struct Desktop {
    QMap<Type, Script> offered;
    QList<Type> asked;
    // Like the ScreenCast source: once it fails (a cancel aside), it stops offering itself.
    QList<Type> withdrawOnFailure;

    Screen::FrameSourceChain::Factory factory()
    {
        return [this](Type type, QObject *parent) -> std::unique_ptr<Screen::DesktopFrameSource> {
            if (!offered.contains(type))
                return nullptr;
            asked.append(type);
            auto source = std::make_unique<Source>(offered.value(type), parent);
            if (withdrawOnFailure.contains(type))
                QObject::connect(source.get(), &Screen::DesktopFrameSource::frameFailed,
                                 [this, type](const QString &, bool cancelled) {
                                     if (!cancelled)
                                         offered.remove(type);
                                 });
            return source;
        };
    }
};

inline Script failing()
{
    return {Script::Fail};
}

inline Script cancelling()
{
    return {Script::Cancel};
}

// A frame of pixels over geometry; by default a 2x frame of a 100x50 desktop.
inline Script delivering(QSize pixels = {200, 100}, qreal dpr = 2.0, QRect geometry = {0, 0, 100, 50})
{
    Script script;
    script.pixels = pixels;
    script.dpr = dpr;
    script.geometry = geometry;
    return script;
}

} // namespace FakeFrames

#endif // TESTS_FAKEFRAMESOURCES_H
