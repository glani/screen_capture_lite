#include "ScreenCapture.h"
#include "internal/SCShared.h"
#include "xcb/XcbAtom.h"
#include "xcb/XcbInitializer.h"
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <algorithm>
#include <csignal>
#include <map>
#include <string>
#include <vector>
#include <xcb/xcb.h>

#define LODEPNG_COMPILE_PNG
#define LODEPNG_COMPILE_DISK
#include "png/lodepng.h"

#include "base64/base64.h"

using namespace std::string_literals;

#define _REQUEST_ICON_SIZE 256

namespace {
class UniqueTextProperty {
  public:
    UniqueTextProperty() { p.value = nullptr; }

    UniqueTextProperty(const UniqueTextProperty &) = delete;
    UniqueTextProperty &operator=(const UniqueTextProperty &) = delete;

    UniqueTextProperty(UniqueTextProperty &&other) : p{other.p} { other.p = XTextProperty{}; }

    UniqueTextProperty &operator=(UniqueTextProperty &&other)
    {
        swap(*this, other);
        return *this;
    }

    friend void swap(UniqueTextProperty &lhs, UniqueTextProperty &rhs)
    {
        using std::swap;
        swap(lhs.p, rhs.p);
    }

    ~UniqueTextProperty()
    {
        if (p.value) {
            XFree(p.value);
        }
    }

    auto &get() { return p; }

  private:
    XTextProperty p;
};

auto GetWMName(Display *display, Window window)
{
    auto x = UniqueTextProperty{};
    XGetWMName(display, window, &x.get());
    return x;
}

auto GetWMIconName(Display *display, Window window)
{
    auto x = UniqueTextProperty{};
    XGetWMIconName(display, window, &x.get());
    return x;
}

auto TextPropertyToStrings(Display *dpy, const XTextProperty &prop)
{
    auto result = std::vector<std::string>{};

    auto datalen = (int)prop.nitems;

    int elemStart = 0;
    char *cp = (char *)prop.value;
    for (int i = datalen; i >= 0; cp++, i--) {
        if (*cp == '\0') {
            int len = datalen - i - elemStart;
            std::string s1((char *)(prop.value + elemStart), len);
            result.emplace_back(s1);
            elemStart = datalen - i + 1;
        }
    }

    return result;
}
} // namespace

namespace SL {
namespace Screen_Capture {
    typedef std::function<bool(const XID &window)> RetrieveWindowsConstraint;
    typedef std::function<void(const Window &wnd)> AddWindowsCallback;
    struct _WindowRequest {
        bool retrieveIcon;
        std::map<std::string, std::string> icons;
    };
    std::shared_ptr<std::vector<Window>> getWindows(const std::string &netRequestedAtom, bool retrieveIcon,
                                                    const RetrieveWindowsConstraint &retrieve);
    void addWindow(Display *display, XID &window, struct _WindowRequest wr, const AddWindowsCallback &callback);
    pid_t determinePid(Display *display, XID &window);
    int getExeForPid(pid_t pid, char *buf, size_t sz);

    bool getAtomList(Screen *screen, XID &window, Atom atom, Atom **atoms, int *len);
    bool getCardinalList(Display *display, XID &window, Atom atom, unsigned long **cardinals, int *len);
    bool getWindowPosition(Screen *screen, XID &window, int *xp, int *yp);
    bool getFrameExtents(Display *display, XID &window, int *left_frame, int *right_frame, int *top_frame, int *bottom_frame);
    void argbdataToPixdata(unsigned long *argb_data, int len, unsigned char **pixdata);
    bool findBestSize(unsigned long *data, unsigned long nitems, int ideal_size, int *width, int *height, unsigned long **start);
    bool readRgbIcon(Display *display, XID &window, int *width, int *height, std::string &icon);

    std::shared_ptr<Window> GetActiveWindow(const SharedWindowsRequest &request)
    {
        auto res = getWindows(std::string("_NET_ACTIVE_WINDOW"), request.RetrieveIcons, [&](const XID &id) { return true; });
        if (!res->empty()) {
            return std::make_shared<Window>(res->front());
        }
        else {
            return std::shared_ptr<Window>();
        }
    }

    std::shared_ptr<Window> GetWindow(const SingleWindowRequest &request)
    {
        auto res = getWindows(std::string("_NET_CLIENT_LIST"), request.RetrieveIcons,
                              [&](const XID &id) { return reinterpret_cast<XID>(request.Handle) == id; });
        if (!res->empty()) {
            return std::make_shared<Window>(res->front());
        }
        else {
            return std::shared_ptr<Window>();
        }
    }

    std::shared_ptr<std::vector<Window>> GetWindows(const SearchAllWindowsRequest &request)
    {
        return getWindows(std::string("_NET_CLIENT_LIST_STACKING"), request.RetrieveIcons, [&](const XID &id) { return true; });
    }

    // Make window full screen
    std::shared_ptr<Window> Fullscreen(const SingleWindowRequest &windowRequest, int display, bool fullscreen)
    {
        std::shared_ptr<Window> res;
        auto &xcbInstance = XcbInitializer::getInstance();
        if (xcbInstance.windowExists(windowRequest.Handle)) {
            xcbInstance.fullscreen(windowRequest.Handle, fullscreen);
            xcbInstance.flush();
            struct _WindowRequest wr;
            wr.retrieveIcon = windowRequest.Handle;
            XID window = static_cast<XID>(windowRequest.Handle);
            addWindow(xcbInstance.xlibDisplay(), window, wr, [&](const Window &wnd) { res = std::make_shared<Window>(wnd); });
        }
        return res;
    }

    // Make window minimize
    std::shared_ptr<Window> Minimize(const SingleWindowRequest &windowRequest, bool minimize)
    {
        std::shared_ptr<Window> res;
        auto &xcbInstance = XcbInitializer::getInstance();
        if (xcbInstance.windowExists(windowRequest.Handle)) {
            xcbInstance.minimize(windowRequest.Handle, minimize);
            xcbInstance.flush();
            struct _WindowRequest wr;
            wr.retrieveIcon = windowRequest.Handle;
            XID window = static_cast<XID>(windowRequest.Handle);
            addWindow(xcbInstance.xlibDisplay(), window, wr, [&](const Window &wnd) { res = std::make_shared<Window>(wnd); });
        }

        return res;
    }

    // Make window maximize
    std::shared_ptr<Window> Maximize(const SingleWindowRequest &windowRequest, bool maximize)
    {
        std::shared_ptr<Window> res;
        auto &xcbInstance = XcbInitializer::getInstance();
        if (xcbInstance.windowExists(windowRequest.Handle)) {
            xcbInstance.maximize(windowRequest.Handle, maximize);
            xcbInstance.flush();
            struct _WindowRequest wr;
            wr.retrieveIcon = windowRequest.Handle;
            XID window = static_cast<XID>(windowRequest.Handle);
            addWindow(xcbInstance.xlibDisplay(), window, wr, [&](const Window &wnd) { res = std::make_shared<Window>(wnd); });
        }

        return res;
    }

    // Make window visible
    std::shared_ptr<Window> Visible(const SingleWindowRequest &windowRequest, bool visible)
    {
        std::shared_ptr<Window> res;
        auto &xcbInstance = XcbInitializer::getInstance();
        if (xcbInstance.windowExists(windowRequest.Handle)) {
            xcbInstance.visible(windowRequest.Handle, visible);
            xcbInstance.flush();
            struct _WindowRequest wr;
            wr.retrieveIcon = windowRequest.Handle;
            XID window = static_cast<XID>(windowRequest.Handle);
            addWindow(xcbInstance.xlibDisplay(), window, wr, [&](const Window &wnd) { res = std::make_shared<Window>(wnd); });
        }
        return res;
    }

    // Set rectangle
    std::shared_ptr<Window> Rectangle(const SingleWindowRequest &windowRequest, const Point &position, const SizeValues &size)
    {
        std::shared_ptr<Window> res;
        auto &xcbInstance = XcbInitializer::getInstance();
        if (xcbInstance.windowExists(windowRequest.Handle)) {
            xcbInstance.rectangle(windowRequest.Handle, position, size);
            xcbInstance.flush();

            struct _WindowRequest wr;
            wr.retrieveIcon = windowRequest.Handle;

            XID window = static_cast<XID>(windowRequest.Handle);
            addWindow(xcbInstance.xlibDisplay(), window, wr, [&](const Window &wnd) { res = std::make_shared<Window>(wnd); });
        }

        return res;
    }

    // Make window top most
    std::shared_ptr<Window> Topmost(const SingleWindowRequest &windowRequest)
    {
        std::shared_ptr<Window> res;
        auto &xcbInstance = XcbInitializer::getInstance();
        if (xcbInstance.windowExists(windowRequest.Handle)) {
            xcbInstance.visible(windowRequest.Handle, true);
            xcbInstance.activate(windowRequest.Handle);
            xcbInstance.flush();

            struct _WindowRequest wr;
            wr.retrieveIcon = windowRequest.Handle;

            XID window = static_cast<XID>(windowRequest.Handle);
            addWindow(xcbInstance.xlibDisplay(), window, wr, [&](const Window &wnd) { res = std::make_shared<Window>(wnd); });
        }

        return res;
    }

    std::shared_ptr<ShowingDesktopResult> ShowingDesktop(bool show)
    {
        std::shared_ptr<ShowingDesktopResult> res = std::make_shared<ShowingDesktopResult>();
        auto &xcbInstance = XcbInitializer::getInstance();
        if (xcbInstance.showingDesktop(show)) {
            // TODO think
        }
        xcbInstance.flush();

        return res;
    }

    bool RequestAccessibilityPermissions() { return true; }

    // helpers

    std::shared_ptr<std::vector<Window>> getWindows(const std::string &netRequestedAtom, bool retrieveIcon, const RetrieveWindowsConstraint &retrieve)
    {
        std::shared_ptr<std::vector<Window>> result = std::make_shared<std::vector<Window>>();

        auto &xcbInstance = XcbInitializer::getInstance();
        auto display = xcbInstance.xlibDisplay();

        struct _WindowRequest wr;
        wr.retrieveIcon = retrieveIcon;

        Atom atomWindowsRequested = xcbInstance.internAtom(netRequestedAtom.c_str());
        Atom actualType;
        int format;
        unsigned long numItems, bytesAfter;
        unsigned char *data = 0;
        int status = XGetWindowProperty(display, XDefaultRootWindow(display), atomWindowsRequested, 0L, (~0L), false, AnyPropertyType, &actualType,
                                        &format, &numItems, &bytesAfter, &data);
        if (status >= Success && numItems) {
            auto array = (XID *)data;
            for (decltype(numItems) k = 0; k < numItems; k++) {
                auto window = array[k];
                if (retrieve(window)) {
                    addWindow(display, window, wr, [&](const Window &wnd) { result->emplace_back(wnd); });
                }
            }
            XFree(data);
        }
        return result;
    }

    void addWindow(Display *display, XID &window, struct _WindowRequest wr, const AddWindowsCallback &callback)
    {
        auto &xcbInstance = XcbInitializer::getInstance();
        Screen *screen = XDefaultScreenOfDisplay(display);
        pid_t ownerPid = 0;
        std::string exec;

        const int size = 2048;
        ownerPid = determinePid(display, window);
        auto buf = std::unique_ptr<char[]>(new char[size]);
        int len;
        if (-1 != (len = getExeForPid(ownerPid, buf.get(), size))) {
            exec.assign(buf.get(), len);
        }

        auto wm_name = GetWMName(display, window);

        auto candidates = TextPropertyToStrings(display, wm_name.get());
        auto name = candidates.empty() ? ""s : std::move(candidates.front());
        if (!name.empty()) {
            Window w = {};
            w.Handle = reinterpret_cast<size_t>(window);
            w.OwnerHandle = ownerPid;

            XWindowAttributes wndattr;
            XGetWindowAttributes(display, window, &wndattr);

            int xp = 0;
            int yp = 0;
            getWindowPosition(wndattr.screen, window, &xp, &yp);
            int leftFrame = 0;
            int rightFrame = 0;
            int topFrame = 0;
            int bottomFrame = 0;
            getFrameExtents(display, window, &leftFrame, &rightFrame, &topFrame, &bottomFrame);
            w.Frame = FrameValues(leftFrame, rightFrame, topFrame, bottomFrame);

            w.Position = Point(xp, yp);
            w.Size = SizeValues(wndattr.width, wndattr.height);
            w.Name = name;

            XClassHint hint;
            XGetClassHint(display, window, &hint);
            WindowAttribute attributeClass;
            attributeClass.Code = std::string(S_RES_CLASS);
            attributeClass.Value = hint.res_class != NULL ? std::string(hint.res_class) : ""s;
            w.Attributes.emplace_back(attributeClass);

            WindowAttribute attributeName;
            attributeName.Code = std::string(S_RES_NAME);
            attributeName.Value = hint.res_name != NULL ? std::string(hint.res_name) : ""s;
            w.Attributes.emplace_back(attributeName);

            w.ProcessName = attributeName.Value;

            XFree(hint.res_class);
            XFree(hint.res_name);

            if (!exec.empty()) {
                WindowAttribute attributeExec;
                attributeExec.Code = std::string(S_RES_START_ABSOLUTE_URL);
                attributeExec.Value = exec;
                w.Attributes.emplace_back(attributeExec);
            }

            auto desktop = XCB_REPLY_UNCHECKED(xcb_get_property, xcbInstance.xcbConnection(), false, window,
                                               xcbInstance.atom(XcbAtom::_NET_WM_DESKTOP), XCB_ATOM_CARDINAL, 0, 1024);
            if (desktop && desktop->type == XCB_ATOM_CARDINAL && desktop->format == 32 && desktop->value_len) {
                auto desktopValue = (uint32_t *)xcb_get_property_value(desktop.get());
                w.WorkSpace = static_cast<size_t>(*desktopValue);
            }

            Atom atomWmState = XInternAtom(display, "_NET_WM_STATE", true);
            Atom *atoms = NULL;
            int n_atoms = 0;
            if (getAtomList(wndattr.screen, window, atomWmState, &atoms, &n_atoms)) {
                Atom atomWmStateHidden = XInternAtom(display, "_NET_WM_STATE_HIDDEN", true);
                Atom atomWmStateFullscreen = XInternAtom(display, "_NET_WM_STATE_FULLSCREEN", true);
                Atom atomWmStateMaxVert = XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_VERT", true);
                Atom atomWmStateMaxHorz = XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_HORZ", true);
                bool visible = true;
                bool fullscreen = false;
                bool maxVert = false;
                bool maxHorz = false;
                for (int i = 0; i < n_atoms; i++) {
                    if (atoms[i] == atomWmStateHidden) {
                        visible = false;
                    }
                    else if (atoms[i] == atomWmStateFullscreen) {
                        fullscreen = true;
                    }
                    else if (atoms[i] == atomWmStateMaxVert) {
                        maxVert = true;
                    }
                    else if (atoms[i] == atomWmStateMaxHorz) {
                        maxHorz = true;
                    }
                }

                WindowAttribute attributeVisible;
                attributeVisible.Code = std::string(S_RES_VISIBLE);
                attributeVisible.Value = visible ? std::string(S_TRUE) : std::string(S_FALSE);
                w.Attributes.emplace_back(attributeVisible);

                WindowAttribute attributeFullscreen;
                attributeFullscreen.Code = std::string(S_RES_FULLSCREEN);
                attributeFullscreen.Value = fullscreen ? std::string(S_TRUE) : std::string(S_FALSE);
                w.Attributes.emplace_back(attributeFullscreen);

                WindowAttribute attributeMaximized;
                attributeMaximized.Code = std::string(S_RES_MAXIMIZED);
                attributeMaximized.Value = (maxVert && maxHorz) ? std::string(S_TRUE) : std::string(S_FALSE);
                w.Attributes.emplace_back(attributeMaximized);
            }
            if (atoms) {
                XFree(atoms);
            }

            if (wr.retrieveIcon) {
                int width;
                int height;
                std::string iconData;
                auto it = wr.icons.find(attributeClass.Value);
                if (it != wr.icons.end()) {
                    WindowAttribute attributeIcon;
                    attributeIcon.Code = std::string(S_RES_ICON_URL);
                    attributeIcon.Value = it->second;
                    w.Attributes.emplace_back(attributeIcon);
                    WindowAttribute attributeIconExist;
                    attributeIconExist.Code = std::string(S_RES_ICON_EXIST);
                    attributeIconExist.Value = S_TRUE;
                    w.Attributes.emplace_back(attributeIconExist);
                }
                else {
                    bool iconExists = readRgbIcon(display, window, &width, &height, iconData);
                    if (iconExists) {
                        WindowAttribute attributeIcon;
                        attributeIcon.Code = std::string(S_RES_ICON_URL);
                        attributeIcon.Value = iconData;
                        w.Attributes.emplace_back(attributeIcon);
                        WindowAttribute attributeIconExist;
                        attributeIconExist.Code = std::string(S_RES_ICON_EXIST);
                        attributeIconExist.Value = S_TRUE;
                        w.Attributes.emplace_back(attributeIconExist);

                        wr.icons.emplace(std::make_pair(std::string(S_RES_ICON_URL), iconData));
                    }
                    else {
                        WindowAttribute attributeIconExist;
                        attributeIconExist.Code = std::string(S_RES_ICON_EXIST);
                        attributeIconExist.Value = S_FALSE;
                        w.Attributes.emplace_back(attributeIconExist);
                    }
                }
            }
            callback(w);
        }
    }

    pid_t determinePid(Display *display, XID &window)
    {
        auto &xcbInstance = XcbInitializer::getInstance();
        const auto atomPID = xcbInstance.atom(XcbAtom::_NET_WM_PID);
        struct FreeWrapRAII {
            void *data;
            FreeWrapRAII(void *data) : data(data) {}
            ~FreeWrapRAII() { XFree(data); }
        };
        // Get the PID for the given Window
        Atom type;
        int format;
        unsigned long nItems;
        unsigned long bytesAfter;
        unsigned char *propPID = 0;
        if (Success == XGetWindowProperty(display, window, atomPID, 0, 1, False, XA_CARDINAL, &type, &format, &nItems, &bytesAfter, &propPID)) {
            if (propPID != 0) {
                FreeWrapRAII tmp(propPID);
                return *reinterpret_cast<pid_t *>(propPID);
            }
        }
        return 0;
    }

    int getExeForPid(pid_t pid, char *buf, size_t sz)
    {
        char path[32];
        sprintf(path, "/proc/%d/exe", pid);
        return readlink(path, buf, sz);
    }

    bool findBestSize(unsigned long *data, unsigned long nitems, int ideal_size, int *width, int *height, unsigned long **start)
    {
        int best_w;
        int best_h;
        unsigned long *best_start;

        *width = 0;
        *height = 0;
        *start = NULL;

        best_w = 0;
        best_h = 0;
        best_start = NULL;

        while (nitems > 0) {
            int w, h;
            bool replace;

            replace = false;

            if (nitems < 3)
                return false; /* no space for w, h */

            w = data[0];
            h = data[1];

            if (nitems < ((unsigned long)(w * h) + 2))
                break; /* not enough data */

            if (best_start == NULL) {
                replace = true;
            }
            else {
                /* work with averages */
                int best_size = (best_w + best_h) / 2;
                int this_size = (w + h) / 2;

                /* larger than desired is always better than smaller */
                if (best_size < ideal_size && this_size >= ideal_size)
                    replace = true;
                /* if we have too small, pick anything bigger */
                else if (best_size < ideal_size && this_size > best_size)
                    replace = true;
                /* if we have too large, pick anything smaller
                 * but still >= the ideal
                 */
                else if (best_size > ideal_size && this_size >= ideal_size && this_size < best_size)
                    replace = true;
            }

            if (replace) {
                best_start = data + 2;
                best_w = w;
                best_h = h;
            }

            data += (w * h) + 2;
            nitems -= (w * h) + 2;
        }

        if (best_start) {
            *start = best_start;
            *width = best_w;
            *height = best_h;
            return true;
        }
        else
            return true;
    }

    bool readRgbIcon(Display *display, XID &window, int *width, int *height, std::string &icon)
    {
        int result;
        int w, h;
        Atom type;
        int format;
        unsigned long nitems;
        unsigned long bytes_after;
        unsigned long *best;

        unsigned long *data = 0;
        Atom atomWMIcon = XInternAtom(display, "_NET_WM_ICON", true);
        result = XGetWindowProperty(display, window, atomWMIcon, 0, (~0L), False, XA_CARDINAL, &type, &format, &nitems, &bytes_after,
                                    reinterpret_cast<unsigned char **>(&data));
        if (result != Success) {
            return false;
        }

        if (type != XA_CARDINAL) {
            XFree(data);
            return false;
        }

        if (!findBestSize(data, nitems, _REQUEST_ICON_SIZE, &w, &h, &best)) {
            XFree(data);
            return false;
        }

        *width = w;
        *height = h;

        unsigned char *pixdata = NULL;
        argbdataToPixdata(best, w * h, &pixdata);
        XFree(data);

        unsigned char *pngData;
        size_t outsize;
        unsigned int pngEncResult = lodepng_encode_memory(&pngData, &outsize, pixdata, w, h, LCT_RGBA, 8);
        delete[] pixdata;
        if (0 != pngEncResult) {
            return false;
        }
        icon = base64_encode(pngData, outsize, std::string("data:image/png;base64,"), false);
        delete[] pngData;

        return true;
    }

    void argbdataToPixdata(unsigned long *argb_data, int len, unsigned char **pixdata)
    {
        unsigned char *p;
        int i;

        *pixdata = new unsigned char[len * 4];
        p = *pixdata;

        /* One could speed this up a lot. */
        i = 0;
        while (i < len) {
            unsigned int argb;
            unsigned int rgba;

            argb = argb_data[i];
            rgba = (argb << 8) | (argb >> 24);

            *p = rgba >> 24;
            ++p;
            *p = (rgba >> 16) & 0xff;
            ++p;
            *p = (rgba >> 8) & 0xff;
            ++p;
            *p = rgba & 0xff;
            ++p;

            ++i;
        }
    }

    bool getFrameExtents(Display *display, XID &window, int *left_frame, int *right_frame, int *top_frame, int *bottom_frame)
    {
        unsigned long *p_size;
        int n_size;
        bool retval = false;
        p_size = NULL;
        n_size = 0;

        Atom atomFrameExtents = XInternAtom(display, "_NET_FRAME_EXTENTS", true);
        getCardinalList(display, window, atomFrameExtents, &p_size, &n_size);

        if (p_size != NULL && n_size == 4) {
            *left_frame = p_size[0];
            *right_frame = p_size[1];
            *top_frame = p_size[2];
            *bottom_frame = p_size[3];

            retval = true;
        }

        if (p_size == NULL) {
            Atom atomGtkFrameExtents = XInternAtom(display, "_GTK_FRAME_EXTENTS", true);
            getCardinalList(display, window, atomGtkFrameExtents, &p_size, &n_size);

            if (p_size != NULL && n_size == 4) {
                *left_frame = -p_size[0];
                *right_frame = -p_size[1];
                *top_frame = -p_size[2];
                *bottom_frame = -p_size[3];

                retval = true;
            }
        }
        if (p_size == NULL) {
            Atom atomGtkFrameExtents = XInternAtom(display, "_KDE_NET_WM_FRAME_STRUT", true);
            getCardinalList(display, window, atomGtkFrameExtents, &p_size, &n_size);

            if (p_size != NULL && n_size == 4) {
                *left_frame = p_size[0];
                *right_frame = p_size[1];
                *top_frame = p_size[2];
                *bottom_frame = p_size[3];

                retval = true;
            }
        }

        if (p_size != NULL) {
            delete p_size;
        }

        return retval;
    }

    bool getWindowPosition(Screen *screen, XID &window, int *xp, int *yp)
    {
        int x, y;
        XID child;

        x = 0;
        y = 0;

        Display *display = DisplayOfScreen(screen);
        XID root = RootWindowOfScreen(screen);

        XTranslateCoordinates(display, window, root, 0, 0, &x, &y, &child);

        if (xp) {
            *xp = x;
        }
        if (yp) {
            *yp = y;
        }
        return true;
    }

    bool getCardinalList(Display *display, XID &window, Atom atom, unsigned long **cardinals, int *len)
    {
        Atom type;
        int format;
        unsigned long nitems;
        unsigned long bytes_after;
        unsigned long *nums;
        int result;

        *cardinals = NULL;
        *len = 0;

        type = None;
        result = XGetWindowProperty(display, window, atom, 0, (~0L), False, XA_CARDINAL, &type, &format, &nitems, &bytes_after,
                                    reinterpret_cast<unsigned char **>(&nums));
        if (result != Success)
            return false;

        if (type != XA_CARDINAL) {
            XFree(nums);
            return false;
        }

        *cardinals = new unsigned long[nitems];
        memcpy(*cardinals, nums, sizeof(unsigned long) * nitems);
        *len = nitems;

        XFree(nums);

        return true;
    }

    bool getAtomList(Screen *screen, XID &window, Atom atom, Atom **atoms, int *len)
    {
        Display *display;
        Atom type;
        int format;
        unsigned long nitems;
        unsigned long bytes_after;
        Atom *data;
        int err, result;

        display = DisplayOfScreen(screen);

        *atoms = NULL;
        *len = 0;

        type = None;
        result = XGetWindowProperty(display, window, atom, 0, (~0L), False, XA_ATOM, &type, &format, &nitems, &bytes_after,
                                    reinterpret_cast<unsigned char **>(&data));
        if (result != Success) {
            return false;
        }

        if (type != XA_ATOM) {
            XFree(data);
            return false;
        }

        *atoms = new Atom[nitems];
        memcpy(*atoms, data, sizeof(Atom) * nitems);
        *len = nitems;
        XFree(data);

        return true;
    }

} // namespace Screen_Capture
} // namespace SL
