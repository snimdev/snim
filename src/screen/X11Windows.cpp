#include "screen/X11Windows.h"

#include "screen/X11WindowFilter.h"

#include <QGuiApplication>
#include <QSet>
#include <QWindow>

#include <xcb/xcb.h>

#include <cstdlib>
#include <cstring>
#include <iterator>
#include <memory>

namespace Screen::X11Windows {

namespace {

struct FreeReply {
    void operator()(void *reply) const { std::free(reply); }
};
template <typename T>
using Reply = std::unique_ptr<T, FreeReply>;

// Takes a reply, keeping any error away from Qt's event queue, where it would be logged.
template <typename T>
Reply<T> take(T *reply, xcb_generic_error_t *error)
{
    std::free(error);
    return Reply<T>(reply);
}

xcb_connection_t *connection()
{
#if QT_CONFIG(xcb)
    if (qGuiApp) {
        if (auto *x11 = qGuiApp->nativeInterface<QNativeInterface::QX11Application>())
            return x11->connection();
    }
#endif
    return nullptr;
}

xcb_window_t rootWindow(xcb_connection_t *c)
{
    return xcb_setup_roots_iterator(xcb_get_setup(c)).data->root;
}

struct Atoms {
    xcb_atom_t clientListStacking = XCB_ATOM_NONE;
    xcb_atom_t clientList = XCB_ATOM_NONE;
    xcb_atom_t frameExtents = XCB_ATOM_NONE;
    xcb_atom_t gtkFrameExtents = XCB_ATOM_NONE;
    xcb_atom_t wmState = XCB_ATOM_NONE;
    xcb_atom_t wmStateHidden = XCB_ATOM_NONE;
    xcb_atom_t wmWindowType = XCB_ATOM_NONE;
    xcb_atom_t typeDesktop = XCB_ATOM_NONE;
    xcb_atom_t typeDock = XCB_ATOM_NONE;
};

Atoms internAtoms(xcb_connection_t *c)
{
    Atoms atoms;
    const struct {
        const char *name;
        xcb_atom_t *atom;
    } wanted[] = {
        {"_NET_CLIENT_LIST_STACKING", &atoms.clientListStacking},
        {"_NET_CLIENT_LIST", &atoms.clientList},
        {"_NET_FRAME_EXTENTS", &atoms.frameExtents},
        {"_GTK_FRAME_EXTENTS", &atoms.gtkFrameExtents},
        {"_NET_WM_STATE", &atoms.wmState},
        {"_NET_WM_STATE_HIDDEN", &atoms.wmStateHidden},
        {"_NET_WM_WINDOW_TYPE", &atoms.wmWindowType},
        {"_NET_WM_WINDOW_TYPE_DESKTOP", &atoms.typeDesktop},
        {"_NET_WM_WINDOW_TYPE_DOCK", &atoms.typeDock},
    };
    xcb_intern_atom_cookie_t cookies[std::size(wanted)];
    for (std::size_t i = 0; i < std::size(wanted); ++i)
        cookies[i] = xcb_intern_atom(c, 1, std::strlen(wanted[i].name), wanted[i].name);
    for (std::size_t i = 0; i < std::size(wanted); ++i) {
        xcb_generic_error_t *error = nullptr;
        const auto reply = take(xcb_intern_atom_reply(c, cookies[i], &error), error);
        if (reply)
            *wanted[i].atom = reply->atom;
    }
    return atoms;
}

xcb_get_property_cookie_t requestProperty(xcb_connection_t *c, xcb_window_t window,
                                          xcb_atom_t property, xcb_atom_t type, quint32 length)
{
    return xcb_get_property(c, 0, window, property, type, 0, length);
}

// The 32-bit values of a property; empty when it is missing or of another format.
QVector<quint32> values(xcb_connection_t *c, xcb_get_property_cookie_t cookie)
{
    xcb_generic_error_t *error = nullptr;
    const auto reply = take(xcb_get_property_reply(c, cookie, &error), error);
    if (!reply || reply->format != 32)
        return {};
    const auto *data = static_cast<const quint32 *>(xcb_get_property_value(reply.get()));
    return QVector<quint32>(data, data + xcb_get_property_value_length(reply.get()) / 4);
}

// _NET_FRAME_EXTENTS and _GTK_FRAME_EXTENTS both list left, right, top, bottom.
QMargins extents(const QVector<quint32> &values)
{
    if (values.size() != 4)
        return {};
    return {int(values[0]), int(values[2]), int(values[1]), int(values[3])};
}

QVector<quint32> windowList(xcb_connection_t *c, xcb_window_t root, xcb_atom_t property)
{
    if (property == XCB_ATOM_NONE)
        return {};
    return values(c, requestProperty(c, root, property, XCB_ATOM_WINDOW, 16384));
}

// Snim's own windows, which the picker skips. winId() would create a missing handle.
QSet<quint64> ownWindows()
{
    QSet<quint64> ids;
    const QList<QWindow *> windows = QGuiApplication::allWindows();
    for (QWindow *window : windows) {
        if (window->handle())
            ids.insert(window->winId());
    }
    return ids;
}

// Every request for one window goes out before any reply is read: one round trip in all.
struct Pending {
    xcb_window_t window = XCB_WINDOW_NONE;
    xcb_get_window_attributes_cookie_t attributes{};
    xcb_get_geometry_cookie_t geometry{};
    xcb_translate_coordinates_cookie_t origin{};
    xcb_get_property_cookie_t frameExtents{};
    xcb_get_property_cookie_t gtkFrameExtents{};
    xcb_get_property_cookie_t state{};
    xcb_get_property_cookie_t type{};
};

Pending request(xcb_connection_t *c, xcb_window_t window, xcb_window_t root, const Atoms &atoms)
{
    Pending p;
    p.window = window;
    p.attributes = xcb_get_window_attributes(c, window);
    p.geometry = xcb_get_geometry(c, window);
    p.origin = xcb_translate_coordinates(c, window, root, 0, 0);
    p.frameExtents = requestProperty(c, window, atoms.frameExtents, XCB_ATOM_CARDINAL, 4);
    p.gtkFrameExtents = requestProperty(c, window, atoms.gtkFrameExtents, XCB_ATOM_CARDINAL, 4);
    p.state = requestProperty(c, window, atoms.wmState, XCB_ATOM_ATOM, 64);
    p.type = requestProperty(c, window, atoms.wmWindowType, XCB_ATOM_ATOM, 64);
    return p;
}

X11WindowFilter::Candidate collect(xcb_connection_t *c, const Pending &p, const Atoms &atoms)
{
    X11WindowFilter::Candidate w;
    xcb_generic_error_t *error = nullptr;
    const auto attributes = take(xcb_get_window_attributes_reply(c, p.attributes, &error), error);
    error = nullptr;
    const auto geometry = take(xcb_get_geometry_reply(c, p.geometry, &error), error);
    error = nullptr;
    const auto origin = take(xcb_translate_coordinates_reply(c, p.origin, &error), error);
    const QMargins frame = extents(values(c, p.frameExtents));
    const QMargins gtkFrame = extents(values(c, p.gtkFrameExtents));
    const QVector<quint32> state = values(c, p.state);
    const QVector<quint32> type = values(c, p.type);

    w.viewable = attributes && attributes->map_state == XCB_MAP_STATE_VIEWABLE;
    w.hidden = atoms.wmStateHidden != XCB_ATOM_NONE && state.contains(atoms.wmStateHidden);
    w.desktopOrDock = (atoms.typeDesktop != XCB_ATOM_NONE && type.contains(atoms.typeDesktop))
                      || (atoms.typeDock != XCB_ATOM_NONE && type.contains(atoms.typeDock));
    if (geometry && origin) {
        const QRect client(origin->dst_x, origin->dst_y, geometry->width, geometry->height);
        w.bounds = X11WindowFilter::visibleRect(client, frame, gtkFrame);
    }
    return w;
}

} // namespace

QVector<Window> pickableWindows()
{
    xcb_connection_t *c = connection();
    if (!c || xcb_connection_has_error(c))
        return {};

    const xcb_window_t root = rootWindow(c);
    const Atoms atoms = internAtoms(c);
    // Bottom to top; a window manager without the stacking order still lists its clients.
    QVector<quint32> clients = windowList(c, root, atoms.clientListStacking);
    if (clients.isEmpty())
        clients = windowList(c, root, atoms.clientList);

    QVector<Pending> pending;
    pending.reserve(clients.size());
    for (auto it = clients.crbegin(); it != clients.crend(); ++it)
        pending.append(request(c, *it, root, atoms));

    const QSet<quint64> own = ownWindows();
    QVector<Window> pickable;
    for (const Pending &p : std::as_const(pending)) {
        X11WindowFilter::Candidate w = collect(c, p, atoms);
        w.own = own.contains(p.window);
        if (X11WindowFilter::isPickable(w))
            pickable.append(Window{p.window, w.bounds});
    }
    return pickable;
}

} // namespace Screen::X11Windows
