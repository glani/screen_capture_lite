#include "XcbInitializer.h"
#include "XcbAtom.h"
#include <memory>
#include <stdio.h>

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

static int nullErrorHandler(Display *dpy, XErrorEvent *err)
{
    const int buflen = 1024;
    char buf[buflen];

    XGetErrorText(dpy, err->error_code, buf, buflen);
    fprintf(stderr, "X Error: serial %lu error %d %s\n", err->serial, (int)err->error_code, buf);
    return 0;
}

static int ioErrorHandler(Display *dpy)
{
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

XcbInitializer::XcbInitializer() {}

XcbInitializer::~XcbInitializer()
{
    if (isConnected()) {
        xcb_disconnect(xcbConnectionValue);
    }
}

XcbInitializer &XcbInitializer::getInstance()
{
    static XcbInitializer instance;
    return instance;
}

void XcbInitializer::init()
{
    Display *dpy = XOpenDisplay(NULL);
    if (dpy) {
        primaryScreenNumberValue = DefaultScreen(dpy);
        XSetEventQueueOwner(dpy, XCBOwnsEventQueue);
        XSetErrorHandler(nullErrorHandler);
        XSetIOErrorHandler(ioErrorHandler);
        xlibDisplayValue = dpy;
    }
    xcbConnectionValue = xcb_connect(NULL, NULL);
    setupValues = xcb_get_setup(xcbConnectionValue);
    xcbAtomValue.initialize(xcbConnectionValue);
}

xcb_atom_t XcbInitializer::internAtom(const char *name)
{
    if (!name || *name == 0)
        return XCB_NONE;

    return XCB_REPLY(xcb_intern_atom, xcbConnection(), false, strlen(name), name)->atom;
}

std::string XcbInitializer::atomName(xcb_atom_t atom)
{
    std::string res;
    if (!atom)
        return res;

    auto reply = XCB_REPLY(xcb_get_atom_name, xcbConnection(), atom);
    if (reply) {
        res.assign(xcb_get_atom_name_name(reply.get()), xcb_get_atom_name_name_length(reply.get()));
    }

    return res;
}