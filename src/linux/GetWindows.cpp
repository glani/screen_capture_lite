#include "ScreenCapture.h"
#include "internal/SCCommon.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <algorithm>
#include <string>
#include <vector>
#include <iostream>


namespace {
    class UniqueTextProperty {
    public:
        UniqueTextProperty()
        {
            p.value = nullptr;
        }

        UniqueTextProperty(const UniqueTextProperty&) = delete;
        UniqueTextProperty& operator=(const UniqueTextProperty&) = delete;

        UniqueTextProperty(UniqueTextProperty&& other):
            p{other.p}
        {
            other.p = XTextProperty{};
        }

        UniqueTextProperty& operator=(UniqueTextProperty&& other)
        {
            swap(*this, other);
            return *this;
        }

        friend void swap(UniqueTextProperty& lhs, UniqueTextProperty& rhs) {
            using std::swap;
            swap(lhs.p, rhs.p);
        }

        ~UniqueTextProperty()
        {
            if (p.value) {
                XFree(p.value);
            }
        }

        auto& get() {
            return p;
        }

    private:
        XTextProperty p;
    };

    auto GetWMName(Display* display, Window window)
    {
        auto x = UniqueTextProperty{};
        XGetWMName(display, window, &x.get());
        return x;
    }

    auto GetWMIconName(Display* display, Window window)
    {
        auto x = UniqueTextProperty{};
        XGetWMIconName(display, window, &x.get());
        return x;
    }

    auto TextPropertyToStrings(
        Display* dpy,
        const XTextProperty& prop
    )
    {
        auto result = std::vector<std::string>{};

        auto datalen = (int) prop.nitems;

        int elemStart = 0;
        char* cp = (char *) prop.value;
        for (int i = datalen; i >= 0; cp++, i--) {
            if (*cp == '\0') {
                int len = datalen - i - elemStart;
                std::string s1((char*)(prop.value + elemStart), len);
                result.emplace_back(s1);
                elemStart = datalen - i + 1;
            }
        }

        return result;
    }
}

namespace SL
{
namespace Screen_Capture
{
    std::shared_ptr<std::vector<Window>> getWindows(const std::string& netRequestedAtom);

    void AddWindow(Display* display, XID& window, std::vector<Window>& wnd)
    {
        using namespace std::string_literals;

        auto wm_name = GetWMName(display, window);


        auto candidates = TextPropertyToStrings(display, wm_name.get());
        auto name = candidates.empty() ? ""s : std::move(candidates.front());
        if (!name.empty()) {
            Window w = {};
            w.Handle = reinterpret_cast<size_t>(window);

            XWindowAttributes wndattr;
            XGetWindowAttributes(display, window, &wndattr);

            w.Position = Point{ wndattr.x, wndattr.y };
            w.Size = Point{ wndattr.width, wndattr.height };
            w.Name = name;

            XClassHint hint;
            XGetClassHint(
                    display,
                    window,
                    &hint);
            WindowAttribute attributeClass;
            attributeClass.Code = std::string("res_class");
            attributeClass.Value = hint.res_class != NULL ? std::string(hint.res_class) : ""s;
            w.Attributes.emplace_back(attributeClass);

            WindowAttribute attributeName;
            attributeName.Code = std::string("res_name");
            attributeName.Value = hint.res_name != NULL ? std::string(hint.res_name) : ""s;
            w.Attributes.emplace_back(attributeName);
            wnd.emplace_back(w);
        }
    }

    std::shared_ptr<Window> GetActiveWindow() {
        auto res = getWindows(std::string("_NET_ACTIVE_WINDOW"));
        if (!res->empty()) {
            return std::make_shared<Window>(res->front());
        } else {
            return std::shared_ptr<Window>();
        }
    }

    std::shared_ptr<std::vector<Window>> GetWindows()
    {
        return getWindows(std::string("_NET_CLIENT_LIST"));
    }

    std::shared_ptr<std::vector<Window>> getWindows(const std::string& netRequestedAtom) {
        std::shared_ptr<std::vector<Window>> result = std::make_shared<std::vector<Window>>();

        auto* display = XOpenDisplay(NULL);

        Atom a = XInternAtom(display, netRequestedAtom.c_str(), true);
        Atom actualType;
        int format;
        unsigned long numItems, bytesAfter;
        unsigned char* data = 0;
        int status = XGetWindowProperty(display,
                                        XDefaultRootWindow(display),
                                        a,
                                        0L,
                                        (~0L),
                                        false,
                                        AnyPropertyType,
                                        &actualType,
                                        &format,
                                        &numItems,
                                        &bytesAfter,
                                        &data);
        if(status >= Success && numItems) {
            auto array = (XID*)data;
            for(decltype(numItems) k = 0; k < numItems; k++) {
                auto w = array[k];
                AddWindow(display, w, *result);
            }
            XFree(data);
        }
        XCloseDisplay(display);
        return result;
    }
}
}
