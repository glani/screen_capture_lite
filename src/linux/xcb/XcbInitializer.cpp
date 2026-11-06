#include "XcbInitializer.h"
#include "XcbAtom.h"
#include <memory>
#include <xcb/xcb.h>

#include <stdio.h>
#include <xcb/xcb_icccm.h>

static const char *const xcbConnectionErrors[] = {
        "No error",                                  /* Error 0 */
        "I/O error",                                 /* XCB_CONN_ERROR */
        "Unsupported extension used",                /* XCB_CONN_CLOSED_EXT_NOTSUPPORTED */
        "Out of memory",                             /* XCB_CONN_CLOSED_MEM_INSUFFICIENT */
        "Maximum allowed requested length exceeded", /* XCB_CONN_CLOSED_REQ_LEN_EXCEED */
        "Failed to parse display string",            /* XCB_CONN_CLOSED_PARSE_ERR */
        "No such screen on display",                 /* XCB_CONN_CLOSED_INVALID_SCREEN */
        "Error during FD passing"                    /* XCB_CONN_CLOSED_FDPASSING_FAILED */
};

static int nullErrorHandler(Display *dpy, XErrorEvent *err) {
    const int buflen = 1024;
    char buf[buflen];

    XGetErrorText(dpy, err->error_code, buf, buflen);
    fprintf(stderr, "X Error: serial %lu error %d %s\n", err->serial, (int) err->error_code, buf);
    return 0;
}

static int ioErrorHandler(Display *dpy) {
    xcb_connection_t *conn = XGetXCBConnection(dpy);
    if (conn != nullptr) {
        /* Print a message with a textual description of the error */
        int code = xcb_connection_has_error(conn);
        const char *str = "Unknown error";
        int arrayLength = sizeof(xcbConnectionErrors) / sizeof(xcbConnectionErrors[0]);
        if (code >= 0 && code < arrayLength)
            str = xcbConnectionErrors[code];

        fprintf(stderr, "The X11 connection broke: %s (code %d)", str, code);
    }
    return _XDefaultIOError(dpy);
}

typedef enum {
    WINDOW_GRAVITY_CURRENT = 0,
    WINDOW_GRAVITY_NORTHWEST = 1,
    WINDOW_GRAVITY_NORTH = 2,
    WINDOW_GRAVITY_NORTHEAST = 3,
    WINDOW_GRAVITY_WEST = 4,
    WINDOW_GRAVITY_CENTER = 5,
    WINDOW_GRAVITY_EAST = 6,
    WINDOW_GRAVITY_SOUTHWEST = 7,
    WINDOW_GRAVITY_SOUTH = 8,
    WINDOW_GRAVITY_SOUTHEAST = 9,
    WINDOW_GRAVITY_STATIC = 10
} WindowGravity;

typedef enum {
    WINDOW_CHANGE_X = 1 << 0,
    WINDOW_CHANGE_Y = 1 << 1,
    WINDOW_CHANGE_WIDTH = 1 << 2,
    WINDOW_CHANGE_HEIGHT = 1 << 3
} WindowMoveResizeMask;

XcbInitializer::XcbInitializer() {
    this->init();
}

XcbInitializer::~XcbInitializer() {
    if (xcbConnectionValue) {
        // Also for a connection in an error state: xcb_disconnect frees it (and is a no-op for xcb's static error objects).
        xcb_disconnect(xcbConnectionValue);
    }
    if (xlibDisplayValue) {
        XCloseDisplay(xlibDisplayValue);
    }
}

void XcbInitializer::init() {
    Display *dpy = XOpenDisplay(NULL);
    if (dpy) {
        XSetEventQueueOwner(dpy, XCBOwnsEventQueue);
        XSetErrorHandler(nullErrorHandler);
        XSetIOErrorHandler(ioErrorHandler);
        xlibDisplayValue = dpy;
        rootWindowValue = XDefaultRootWindow(xlibDisplayValue);
        primaryScreenNumberValue = DefaultScreen(dpy);
    }
    xcbConnectionValue = xcb_connect(NULL, NULL);
    if (!isValid()) {
        // No reachable X server (DISPLAY unset or wrong, the server gone). Every public function checks isValid()
        // and returns an empty result instead of dereferencing a NULL display or a failed reply.
        return;
    }
    setupValues = xcb_get_setup(xcbConnectionValue);
    xcbAtomValue.initialize(xcbConnectionValue);
}

xcb_atom_t XcbInitializer::internAtom(const char *name) {
    if (!name || *name == 0)
        return XCB_NONE;

    auto reply = XCB_REPLY(xcb_intern_atom, xcbConnection(), false, strlen(name), name);
    return reply ? reply->atom : XCB_NONE;
}

std::string XcbInitializer::atomName(xcb_atom_t atom) {
    std::string res;
    if (!atom)
        return res;

    auto reply = XCB_REPLY(xcb_get_atom_name, xcbConnection(), atom);
    if (reply) {
        res.assign(xcb_get_atom_name_name(reply.get()), xcb_get_atom_name_name_length(reply.get()));
    }

    return res;
}

void XcbInitializer::flush() { xcb_flush(xcbConnectionValue); }

void XcbInitializer::rectangle(const xcb_window_t &win, const SL::Screen_Capture::Point &position,
                               const SL::Screen_Capture::SizeValues &size) {
    xcb_client_message_event_t event;

    int gravityAndFlags = WINDOW_GRAVITY_STATIC;

    auto flags = WINDOW_CHANGE_X | WINDOW_CHANGE_Y | WINDOW_CHANGE_WIDTH | WINDOW_CHANGE_HEIGHT;

    gravityAndFlags |= flags << 8;
    gravityAndFlags |= 1 << 12;

    event.response_type = XCB_CLIENT_MESSAGE;
    event.window = win;
    event.type = atom(XcbAtom::Atom::_NET_MOVERESIZE_WINDOW);
    event.format = 32;
    event.data.data32[0] = gravityAndFlags;
    event.data.data32[1] = static_cast<uint32_t>(position.X);
    event.data.data32[2] = static_cast<uint32_t>(position.Y);
    event.data.data32[3] = static_cast<uint32_t>(size.Width);
    event.data.data32[4] = static_cast<uint32_t>(size.Height);

    xcb_send_event(xcbConnectionValue, false, rootWindowValue,
                   XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT,
                   (char *) &event);

    uint32_t values[] = {static_cast<uint32_t>(position.X), static_cast<uint32_t>(position.Y), static_cast<uint32_t>(size.Width),
                         static_cast<uint32_t>(size.Height)};
    xcb_configure_window(xcbConnectionValue, win, XCB_CONFIG_WINDOW_X_Y_WIDTH_HEIGHT, values);
}

bool XcbInitializer::windowExists(const xcb_window_t &win) const {
    xcb_generic_error_t *err;
    free(xcb_query_tree_reply(xcbConnectionValue, xcb_query_tree(xcbConnectionValue, win), &err));

    if (err != NULL) {
        free(err);
        return false;
    }

    return true;
}

void XcbInitializer::visible(const xcb_window_t &win, bool visible) {
    uint32_t values_off[] = {ROOT_EVENT_MASK & ~XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY};
    uint32_t values_on[] = {ROOT_EVENT_MASK};
    xcb_change_window_attributes(xcbConnectionValue, rootWindow(), XCB_CW_EVENT_MASK, values_off);
    if (visible) {
        setWindowState(win, XCB_ICCCM_WM_STATE_ICONIC);
        xcb_map_window(xcbConnectionValue, win);
    } else {
        xcb_unmap_window(xcbConnectionValue, win);
        setWindowState(win, XCB_ICCCM_WM_STATE_ICONIC);
    }
    xcb_change_window_attributes(xcbConnectionValue, rootWindow(), XCB_CW_EVENT_MASK, values_on);
}

void XcbInitializer::activate(const xcb_window_t &win) {
    const static uint32_t values[] = {XCB_STACK_MODE_ABOVE};

    /* Move the window on the top of the stack */
    xcb_configure_window(xcbConnectionValue, win, XCB_CONFIG_WINDOW_STACK_MODE, values);
    xcb_client_message_event_t event;

    event.response_type = XCB_CLIENT_MESSAGE;
    event.window = win;
    event.type = atom(XcbAtom::Atom::_NET_ACTIVE_WINDOW);
    event.format = 32;
    event.data.data32[0] = 1;
    event.data.data32[1] = XCB_CURRENT_TIME;
    event.data.data32[2] = XCB_NONE;
    event.data.data32[3] = 0;
    event.data.data32[4] = 0;

    xcb_send_event(xcbConnectionValue, false, rootWindowValue,
                   XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT,
                   (char *) &event);
}

void XcbInitializer::setWindowState(const xcb_window_t &win, uint32_t state) {
    long data[] = {state, XCB_NONE};
    const xcb_atom_t &wmState = atom(XcbAtom::WM_STATE);
    xcb_change_property(xcbConnectionValue, XCB_PROP_MODE_REPLACE, win, wmState, wmState, 32, 2, data);
}

void XcbInitializer::maximize(const xcb_window_t &win, bool maximize) {
    xcb_atom_t max_h = atom(XcbAtom::Atom::_NET_WM_STATE_MAXIMIZED_HORZ);
    xcb_atom_t max_v = atom(XcbAtom::Atom::_NET_WM_STATE_MAXIMIZED_VERT);
    xcb_atom_t state = atom(XcbAtom::Atom::_NET_WM_STATE);

    xcb_client_message_event_t event;

    event.response_type = XCB_CLIENT_MESSAGE;
    event.window = win;
    event.type = state;
    event.format = 32;
    event.data.data32[0] = maximize ? 1 : 0;
    event.data.data32[1] = max_v;
    event.data.data32[2] = max_h;
    event.data.data32[3] = 1;
    event.data.data32[4] = 0;
    xcb_send_event(xcbConnectionValue, false, rootWindowValue,
                   XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT,
                   (char *) &event);
}

void XcbInitializer::maximizeVert(const xcb_window_t &win, bool maximize) {
    xcb_atom_t max_v = atom(XcbAtom::Atom::_NET_WM_STATE_MAXIMIZED_VERT);
    xcb_atom_t state = atom(XcbAtom::Atom::_NET_WM_STATE);

    xcb_client_message_event_t event;

    event.response_type = XCB_CLIENT_MESSAGE;
    event.window = win;
    event.type = state;
    event.format = 32;
    event.data.data32[0] = maximize ? 1 : 0;
    event.data.data32[1] = max_v;
    event.data.data32[2] = 0;
    event.data.data32[3] = 1;
    event.data.data32[4] = 0;
    xcb_send_event(xcbConnectionValue, false, rootWindowValue,
                   XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT,
                   (char *) &event);
}

void XcbInitializer::maximizeHorz(const xcb_window_t &win, bool maximize) {

    xcb_atom_t max_h = atom(XcbAtom::Atom::_NET_WM_STATE_MAXIMIZED_HORZ);
    xcb_atom_t state = atom(XcbAtom::Atom::_NET_WM_STATE);

    xcb_client_message_event_t event;

    event.response_type = XCB_CLIENT_MESSAGE;
    event.window = win;
    event.type = state;
    event.format = 32;
    event.data.data32[0] = maximize ? 1 : 0;
    event.data.data32[1] = max_h;
    event.data.data32[2] = 0;
    event.data.data32[3] = 1;
    event.data.data32[4] = 0;
    xcb_send_event(xcbConnectionValue, false, rootWindowValue,
                   XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT,
                   (char *) &event);
}

void XcbInitializer::minimize(const xcb_window_t &win, bool minimize) {
    if (minimize) {
        XIconifyWindow(xlibDisplay(), win, primaryScreenNumberValue);
    } else {
        visible(win, false);
        visible(win, true);
        XMapWindow(xlibDisplay(), win);
    }
}

void XcbInitializer::fullscreen(const xcb_window_t &win, bool fullscreen) {
    xcb_atom_t fs = atom(XcbAtom::Atom::_NET_WM_STATE_FULLSCREEN);
    xcb_atom_t state = atom(XcbAtom::Atom::_NET_WM_STATE);

    xcb_client_message_event_t event;

    event.response_type = XCB_CLIENT_MESSAGE;
    event.window = win;
    event.type = state;
    event.format = 32;
    event.data.data32[0] = fullscreen ? 1 : 0;
    event.data.data32[1] = fs;
    event.data.data32[2] = 0;
    event.data.data32[3] = 1;
    event.data.data32[4] = 0;
    xcb_send_event(xcbConnectionValue, false, rootWindowValue,
                   XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT,
                   (char *) &event);

}

bool XcbInitializer::showingDesktop(bool show) {
    xcb_atom_t sd = internAtom("_NET_SHOWING_DESKTOP");

    xcb_client_message_event_t event;

    event.response_type = XCB_CLIENT_MESSAGE;
    event.window = DefaultRootWindow(xlibDisplayValue);
    event.type = sd;
    event.format = 32;
    event.data.data32[0] = show ? 1 : 0;
    event.data.data32[1] = 0;
    event.data.data32[2] = 0;
    event.data.data32[3] = 0;
    event.data.data32[4] = 0;
    xcb_send_event(xcbConnectionValue, false, rootWindowValue,
                   XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT,
                   (char *) &event);
    return false;
}

void XcbInitializer::killWindow(const xcb_window_t &win) {
    XKillClient(xlibDisplayValue, win);
}

bool XcbInitializer::closeWindowGracefully(const xcb_window_t &win) {
//    xcb_client_message_event_t event;
//
//    event.response_type = XCB_CLIENT_MESSAGE;
//    event.window = win;
//    event.type = this->atom(XcbAtom::Atom::_NET_CLOSE_WINDOW);
//    event.format = 32;
//    event.data.data32[0] = 1;
//    event.data.data32[1] = XCB_CURRENT_TIME;
//    event.data.data32[2] = XCB_NONE;
//    event.data.data32[3] = 0;
//    event.data.data32[4] = 0;
//
//    xcb_send_event_checked(xcbConnectionValue, false, rootWindowValue,
//                   XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT,
//                   (char *) &event);
    XEvent xev;
    memset(&xev, 0, sizeof(xev));
    xev.type = ClientMessage;
    xev.xclient.serial = 0;
    xev.xclient.send_event = True;
    xev.xclient.display = xlibDisplayValue;
    xev.xclient.window = win;
    xev.xclient.message_type = XInternAtom(xlibDisplayValue, "_NET_CLOSE_WINDOW", False);
    xev.xclient.format = 32;

    int ret = XSendEvent(xlibDisplayValue, rootWindowValue, False,
                     SubstructureNotifyMask | SubstructureRedirectMask,
                     &xev);
    return ret == 0;
}
