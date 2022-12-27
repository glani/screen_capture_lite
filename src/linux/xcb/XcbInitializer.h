#pragma once
#include <xcb/xcb.h>
#include <string>
#include <memory>

#include "XcbAtom.h"

#include <X11/Xlib-xcb.h>
#include <X11/Xlib.h>
#include <X11/Xlibint.h>
#include <X11/Xutil.h>

#define XCB_REPLY_CONNECTION_ARG(connection, ...) connection

struct StdFreeDeleter {
    void operator()(void *p) const noexcept { return std::free(p); }
};

#define XCB_REPLY(call, ...)                                                                                                                         \
    std::unique_ptr<call##_reply_t, StdFreeDeleter>(call##_reply(XCB_REPLY_CONNECTION_ARG(__VA_ARGS__), call(__VA_ARGS__), nullptr))

#define XCB_REPLY_UNCHECKED(call, ...)                                                                                                               \
    std::unique_ptr<call##_reply_t, StdFreeDeleter>(call##_reply(XCB_REPLY_CONNECTION_ARG(__VA_ARGS__), call##_unchecked(__VA_ARGS__), nullptr))

//class XcbAtom;
//class XcbAtom::Atom;

class XcbInitializer {
  private:
    XcbInitializer();

    virtual ~XcbInitializer();

  public:
    void init();

    const xcb_setup_t *setup() const { return setupValues; }

    bool isConnected() const { return xcbConnectionValue && !xcb_connection_has_error(xcbConnectionValue); }
    xcb_connection_t *xcbConnection() const { return xcbConnectionValue; }

    static XcbInitializer &getInstance();
    XcbInitializer(XcbInitializer const &) = delete;
    void operator=(XcbInitializer const &) = delete;

    Display *xlibDisplay() const { return xlibDisplayValue; }

    inline xcb_atom_t atom(XcbAtom::Atom atom) const { return xcbAtomValue.atom(atom); }
    XcbAtom::Atom xcbAtom(xcb_atom_t atom) const { return xcbAtomValue.xcbAtom(atom); }
    xcb_atom_t internAtom(const char *name);
    std::string  atomName(xcb_atom_t atom);


  private:
    Display *xlibDisplayValue = nullptr;
    xcb_connection_t *xcbConnectionValue = nullptr;
    XcbAtom xcbAtomValue;
    const xcb_setup_t *setupValues;
    int primaryScreenNumberValue = -1;
};