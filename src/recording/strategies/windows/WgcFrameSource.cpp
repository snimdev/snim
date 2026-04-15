#include "recording/strategies/windows/WgcFrameSource.h"

#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>

#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Metadata.h>
#include <winrt/Windows.Graphics.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>

#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>

#include <mutex>

namespace Recording {

namespace {

namespace wgc = winrt::Windows::Graphics::Capture;
namespace wgd = winrt::Windows::Graphics::DirectX;
namespace d3d = winrt::Windows::Graphics::DirectX::Direct3D11;

constexpr auto kPixelFormat = wgd::DirectXPixelFormat::B8G8R8A8UIntNormalized;
constexpr int kPoolBuffers = 2;
// Refresh-rate jitter must not halve the frame rate, so frames may come this early.
constexpr qint64 kEarlyFrameUs = 3000;

QString errorText(const winrt::hresult_error &error)
{
    return QString::fromWCharArray(error.message().c_str());
}

winrt::com_ptr<ID3D11Device> createDevice()
{
    winrt::com_ptr<ID3D11Device> device;
    const UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    // WARP keeps capture working on machines without a GPU driver (CI, some VMs).
    for (D3D_DRIVER_TYPE type : {D3D_DRIVER_TYPE_HARDWARE, D3D_DRIVER_TYPE_WARP}) {
        if (SUCCEEDED(D3D11CreateDevice(nullptr, type, nullptr, flags, nullptr, 0,
                                        D3D11_SDK_VERSION, device.put(), nullptr, nullptr)))
            return device;
    }
    return nullptr;
}

wgc::GraphicsCaptureItem createItem(HMONITOR monitor, HWND window)
{
    auto interop = winrt::get_activation_factory<wgc::GraphicsCaptureItem,
                                                 IGraphicsCaptureItemInterop>();
    wgc::GraphicsCaptureItem item{nullptr};
    const winrt::guid iid = winrt::guid_of<wgc::GraphicsCaptureItem>();
    if (window)
        winrt::check_hresult(interop->CreateForWindow(window, iid, winrt::put_abi(item)));
    else
        winrt::check_hresult(interop->CreateForMonitor(monitor, iid, winrt::put_abi(item)));
    return item;
}

} // namespace

struct WgcFrameSource::Impl {
    winrt::com_ptr<ID3D11Device> device;
    winrt::com_ptr<ID3D11DeviceContext> context;
    winrt::com_ptr<ID3D11Texture2D> staging;
    d3d::IDirect3DDevice winrtDevice{nullptr};
    wgc::GraphicsCaptureItem item{nullptr};
    wgc::Direct3D11CaptureFramePool pool{nullptr};
    wgc::GraphicsCaptureSession session{nullptr};
    winrt::event_token frameToken{};
    winrt::event_token closedToken{};

    std::mutex mutex;   // one frame at a time; stop() waits for the one in flight
    bool running = false;
    FrameHandler onFrame;
    ClosedHandler onClosed;
    QRect cropPx;
    qint64 minIntervalUs = 0;
    qint64 lastFrameUs = -1;
    winrt::Windows::Graphics::SizeInt32 poolSize{};
    QSize itemSize;

    void handleFrame();
    bool ensureStaging(int width, int height);
    void release();
};

bool WgcFrameSource::Impl::ensureStaging(int width, int height)
{
    if (staging) {
        D3D11_TEXTURE2D_DESC desc{};
        staging->GetDesc(&desc);
        if (int(desc.Width) == width && int(desc.Height) == height)
            return true;
        staging = nullptr;
    }
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = UINT(width);
    desc.Height = UINT(height);
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    return SUCCEEDED(device->CreateTexture2D(&desc, nullptr, staging.put()));
}

void WgcFrameSource::Impl::handleFrame()
{
    std::lock_guard lock(mutex);
    if (!running || !pool)
        return;
    wgc::Direct3D11CaptureFrame frame = pool.TryGetNextFrame();
    if (!frame)
        return;

    const auto contentSize = frame.ContentSize();
    if (contentSize.Width != poolSize.Width || contentSize.Height != poolSize.Height) {
        // A resized window: later frames come at the new size.
        poolSize = contentSize;
        pool.Recreate(winrtDevice, kPixelFormat, kPoolBuffers, poolSize);
    }

    const qint64 timestampUs = frame.SystemRelativeTime().count() / 10;
    if (lastFrameUs >= 0 && timestampUs - lastFrameUs < minIntervalUs)
        return;

    winrt::com_ptr<ID3D11Texture2D> texture;
    auto access = frame.Surface().as<::Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();
    if (FAILED(access->GetInterface(IID_PPV_ARGS(texture.put()))))
        return;
    D3D11_TEXTURE2D_DESC desc{};
    texture->GetDesc(&desc);

    const QRect available(0, 0, qMin(int(desc.Width), contentSize.Width),
                          qMin(int(desc.Height), contentSize.Height));
    const QRect source = cropPx.isEmpty() ? available : cropPx.intersected(available);
    if (source.width() < 2 || source.height() < 2 || !ensureStaging(source.width(), source.height()))
        return;

    D3D11_BOX box{};
    box.left = UINT(source.left());
    box.top = UINT(source.top());
    box.right = UINT(source.left() + source.width());
    box.bottom = UINT(source.top() + source.height());
    box.front = 0;
    box.back = 1;
    context->CopySubresourceRegion(staging.get(), 0, 0, 0, 0, texture.get(), 0, &box);

    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(context->Map(staging.get(), 0, D3D11_MAP_READ, 0, &mapped)))
        return;
    Frame out;
    out.bgra = static_cast<const uchar *>(mapped.pData);
    out.stride = int(mapped.RowPitch);
    out.width = source.width();
    out.height = source.height();
    out.timestampUs = timestampUs;
    lastFrameUs = timestampUs;
    if (onFrame)
        onFrame(out);
    context->Unmap(staging.get(), 0);
}

void WgcFrameSource::Impl::release()
{
    if (pool) {
        pool.FrameArrived(frameToken);
        pool.Close();
    }
    if (session)
        session.Close();
    if (item)
        item.Closed(closedToken);
    session = nullptr;
    pool = nullptr;
    item = nullptr;
    winrtDevice = nullptr;
    staging = nullptr;
    context = nullptr;
    device = nullptr;
}

WgcFrameSource::WgcFrameSource() : d(std::make_unique<Impl>()) {}

WgcFrameSource::~WgcFrameSource()
{
    stop();
}

bool WgcFrameSource::isSupported()
{
    static const bool supported = [] {
        try {
            // Contract 10 is Windows 10 2004: cursor control and capture exclusion.
            return winrt::Windows::Foundation::Metadata::ApiInformation::IsApiContractPresent(
                       L"Windows.Foundation.UniversalApiContract", 10)
                   && wgc::GraphicsCaptureSession::IsSupported();
        } catch (const winrt::hresult_error &) {
            return false;
        }
    }();
    return supported;
}

bool WgcFrameSource::start(const Target &target, FrameHandler onFrame, ClosedHandler onClosed,
                           QString *error)
{
    stop();
    auto failWith = [this, error](const QString &message) {
        d->release();
        if (error)
            *error = message;
        return false;
    };
    if (!isSupported())
        return failWith(QStringLiteral("Windows Graphics Capture is not available"));

    try {
        d->device = createDevice();
        if (!d->device)
            return failWith(QStringLiteral("Cannot create a Direct3D device"));
        d->device->GetImmediateContext(d->context.put());
        auto dxgiDevice = d->device.as<IDXGIDevice>();
        winrt::com_ptr<::IInspectable> inspectable;
        winrt::check_hresult(CreateDirect3D11DeviceFromDXGIDevice(dxgiDevice.get(),
                                                                  inspectable.put()));
        d->winrtDevice = inspectable.as<d3d::IDirect3DDevice>();

        d->item = createItem(reinterpret_cast<HMONITOR>(target.monitor),
                             reinterpret_cast<HWND>(target.window));
        d->poolSize = d->item.Size();
        d->itemSize = QSize(d->poolSize.Width, d->poolSize.Height);
        d->cropPx = target.window ? QRect() : target.cropPx;
        d->minIntervalUs = target.maxFps > 0
            ? qMax<qint64>(0, 1000000 / target.maxFps - kEarlyFrameUs) : 0;
        d->lastFrameUs = -1;
        d->onFrame = std::move(onFrame);
        d->onClosed = std::move(onClosed);

        d->pool = wgc::Direct3D11CaptureFramePool::CreateFreeThreaded(
            d->winrtDevice, kPixelFormat, kPoolBuffers, d->poolSize);
        d->session = d->pool.CreateCaptureSession(d->item);
        d->session.IsCursorCaptureEnabled(target.captureCursor);
        try {
            // Windows 11 only; Windows 10 keeps its yellow capture border.
            d->session.IsBorderRequired(false);
        } catch (const winrt::hresult_error &) {
        }

        Impl *impl = d.get();
        d->frameToken = d->pool.FrameArrived([impl](const auto &, const auto &) {
            impl->handleFrame();
        });
        d->closedToken = d->item.Closed([impl](const auto &, const auto &) {
            ClosedHandler handler;
            {
                std::lock_guard lock(impl->mutex);
                if (impl->running)
                    handler = impl->onClosed;
            }
            if (handler)
                handler();
        });

        {
            std::lock_guard lock(d->mutex);
            d->running = true;
        }
        d->session.StartCapture();
    } catch (const winrt::hresult_error &e) {
        {
            std::lock_guard lock(d->mutex);
            d->running = false;
        }
        return failWith(errorText(e));
    }
    return true;
}

void WgcFrameSource::stop()
{
    {
        // Waits out a frame in flight; later ones see running == false and return.
        std::lock_guard lock(d->mutex);
        if (!d->running && !d->pool)
            return;
        d->running = false;
    }
    try {
        d->release();
    } catch (const winrt::hresult_error &) {
    }
    d->onFrame = nullptr;
    d->onClosed = nullptr;
}

QSize WgcFrameSource::itemSize() const
{
    return d->itemSize;
}

} // namespace Recording
