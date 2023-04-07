#pragma once
#include <assert.h>
#include <chrono>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#if defined(WINDOWS) || defined(WIN32)
#if defined(SC_LITE_DLL)
#define SC_LITE_EXTERN __declspec(dllexport)
#else
#define SC_LITE_EXTERN
#endif
#else
#define SC_LITE_EXTERN
#endif

namespace SL {
namespace Screen_Capture {
    struct SC_LITE_EXTERN FrameValues {
        int LeftFrame = 0;
        int RightFrame = 0;
        int TopFrame = 0;
        int BottomFrame = 0;

      public:
        FrameValues() {}
        FrameValues(int LeftFrame, int RightFrame, int TopFrame, int BottomFrame)
            : LeftFrame(LeftFrame), RightFrame(RightFrame), TopFrame(TopFrame), BottomFrame(BottomFrame)
        {
        }
    };
    struct SC_LITE_EXTERN SizeValues {
        int Width = 0;
        int Height = 0;

      public:
        SizeValues() {}
        SizeValues(int Width, int Height) : Width(Width), Height(Height) {}
    };

    struct SC_LITE_EXTERN Point {
        int X = 0;
        int Y = 0;

      public:
        Point() {}
        Point(int X, int Y) : X(X), Y(Y) {}
    };

    struct SC_LITE_EXTERN MousePoint {
        Point Position;
        Point HotSpot;
    };

    struct SC_LITE_EXTERN WindowAttribute {
        std::string Code;
        std::string Value;
    };

    struct SC_LITE_EXTERN Window {
        size_t Handle;
        size_t Handle1;
        size_t OwnerHandle;
        Point Position;
        FrameValues Frame;
        SizeValues Size;
        std::string Name;
        std::string ProcessName;
        size_t WorkSpace;
        std::vector<WindowAttribute> Attributes;
    };

    struct SC_LITE_EXTERN NativeWindow {
        size_t Handle;
        size_t OwnerHandle;
    };

    struct SC_LITE_EXTERN SingleWindowRequest {
        size_t Handle;
        size_t Handle1;
        size_t OwnerHandle;
        Point Position;
        SizeValues Size;
        bool Minimized;
        bool Fullscreen;
        bool RetrieveIcons;
        bool ActiveFlag;
        std::vector<WindowAttribute> Attributes;
    };

    struct SC_LITE_EXTERN SearchAllWindowsRequest {
        bool RetrieveIcons;
    };

    struct SC_LITE_EXTERN SharedWindowsRequest {
        bool RetrieveIcons;
    };

    struct SC_LITE_EXTERN Rect {
        int X = 0;
        int Y = 0;
        int Height = 0;
        int Width = 0;

      public:
        Rect() {}
        Rect(int X, int Y, int Width, int Height) : X(X), Y(Y), Width(Width), Height(Height) {}
        std::shared_ptr<Rect> operator&(const Rect &r) const noexcept
        {
            int l1 = X;
            int r1 = X;
            if (Width < 0)
                l1 += Width;
            else
                r1 += Width;
            if (l1 == r1) // null rect
                return NULL;
            int l2 = r.X;
            int r2 = r.X;
            if (r.Width < 0)
                l2 += r.Width;
            else
                r2 += r.Width;
            if (l2 == r2) // null rect
                return NULL;
            if (l1 >= r2 || l2 >= r1)
                return NULL;
            int t1 = Y;
            int b1 = Y;
            if (Height < 0)
                t1 += Height;
            else
                b1 += Height;
            if (t1 == b1) // null rect
                return NULL;
            int t2 = r.Y;
            int b2 = r.Y;
            if (r.Height < 0)
                t2 += r.Height;
            else
                b2 += r.Height;
            if (t2 == b2) // null rect
                return NULL;
            if (t1 >= b2 || t2 >= b1)
                return NULL;
            Rect tmp;
            tmp.X = std::max(l1, l2);
            tmp.Y = std::max(t1, t2);
            tmp.Width = std::min(r1, r2) - tmp.X;
            tmp.Height = std::min(b1, b2) - tmp.Y;
            return std::make_shared<Rect>(tmp);
        }
    };

    struct SC_LITE_EXTERN ShowingDesktopResult {
        bool success;
    };

    struct SC_LITE_EXTERN EmptyResult {};

    struct SC_LITE_EXTERN Monitor {
        int Id = INT32_MAX;
        int Index = INT32_MAX;
        int Adapter = INT32_MAX;
        int Height = 0;
        int Width = 0;
        int OriginalHeight = 0;
        int OriginalWidth = 0;
        // Offsets are the number of pixels that a monitor can be from the origin. For example, users can shuffle their
        // monitors around so this affects their offset.
        int OffsetX = 0;
        int OffsetY = 0;
        int OriginalOffsetX = 0;
        int OriginalOffsetY = 0;
        Rect Frame;
        Rect VisibleFrame;
        std::string Name;
        std::string DisplayName;
        float Scaling = 1.0f;
        bool Main;
    };

    struct Image;
    struct ImageBGRA {
        unsigned char B, G, R, A;
    };

    struct ImageBGR {
        unsigned char B, G, R;
    };

    // index to self in the GetMonitors() function
    SC_LITE_EXTERN int Index(const Monitor &mointor);
    // unique identifier
    SC_LITE_EXTERN int Id(const Monitor &mointor);
    SC_LITE_EXTERN int Adapter(const Monitor &mointor);
    SC_LITE_EXTERN int OffsetX(const Monitor &mointor);
    SC_LITE_EXTERN int OffsetY(const Monitor &mointor);
    SC_LITE_EXTERN void OffsetX(Monitor &mointor, int x);
    SC_LITE_EXTERN void OffsetY(Monitor &mointor, int y);
    SC_LITE_EXTERN int OffsetX(const Window &mointor);
    SC_LITE_EXTERN int OffsetY(const Window &mointor);
    SC_LITE_EXTERN void OffsetX(Window &mointor, int x);
    SC_LITE_EXTERN void OffsetY(Window &mointor, int y);
    SC_LITE_EXTERN const char *Name(const Monitor &mointor);
    SC_LITE_EXTERN const char *Name(const Window &mointor);
    SC_LITE_EXTERN int Height(const Monitor &mointor);
    SC_LITE_EXTERN int Width(const Monitor &mointor);
    SC_LITE_EXTERN void Height(Monitor &mointor, int h);
    SC_LITE_EXTERN void Width(Monitor &mointor, int w);
    SC_LITE_EXTERN int Height(const Window &mointor);
    SC_LITE_EXTERN int Width(const Window &mointor);
    SC_LITE_EXTERN void Height(Window &mointor, int h);
    SC_LITE_EXTERN void Width(Window &mointor, int w);
    SC_LITE_EXTERN int Height(const Image &img);
    SC_LITE_EXTERN int Width(const Image &img);
    SC_LITE_EXTERN int X(const Point &p);
    SC_LITE_EXTERN int Y(const Point &p);

    // the start of the image data, this is not guarenteed to be contiguous.
    SC_LITE_EXTERN const ImageBGRA *StartSrc(const Image &img);
    SC_LITE_EXTERN const ImageBGRA *GotoNextRow(const Image &img, const ImageBGRA *current);
    SC_LITE_EXTERN bool isDataContiguous(const Image &img);
    /*
        this is the ONLY funcion for pulling data out of the Image object and is layed out here in the header so that
        users can see how to extra data and convert it to their own needed format. Initially, I included custom extract functions
        but this is beyond the scope of this library. You must copy the image data if you want to use it as the library owns the Image Data.
    */
    inline void Extract(const Image &img, unsigned char *dst, size_t dst_size)
    {
        assert(dst_size >= static_cast<size_t>(Width(img) * Height(img) * sizeof(ImageBGRA)));
        auto startdst = dst;
        auto startsrc = StartSrc(img);
        if (isDataContiguous(img)) { // no padding, the entire copy can be a single memcpy call
            memcpy(startdst, startsrc, Width(img) * Height(img) * sizeof(ImageBGRA));
        }
        else {
            for (auto i = 0; i < Height(img); i++) {
                memcpy(startdst, startsrc, sizeof(ImageBGRA) * Width(img));
                startdst += sizeof(ImageBGRA) * Width(img); // advance to the next row
                startsrc = GotoNextRow(img, startsrc);      // advance to the next row
            }
        }
    }

    inline SingleWindowRequest WindowToWindowRequest(const Window &window)
    {
        SingleWindowRequest windowRequest;

        windowRequest.Handle = window.Handle;
        windowRequest.OwnerHandle = window.OwnerHandle;
        windowRequest.Position = window.Position;
        windowRequest.Size = window.Size;

        windowRequest.Attributes = window.Attributes;
        //        windowRequest.Minimized = window.Minimized;
        //        windowRequest.Fullscreen = window.Fullscreen;

        return windowRequest;
    }

    class Timer {
        using Clock =
            std::conditional<std::chrono::high_resolution_clock::is_steady, std::chrono::high_resolution_clock, std::chrono::steady_clock>::type;

        std::chrono::microseconds Duration;
        Clock::time_point Deadline;

      public:
        template <typename Rep, typename Period>
        Timer(const std::chrono::duration<Rep, Period> &duration)
            : Duration(std::chrono::duration_cast<std::chrono::microseconds>(duration)), Deadline(Clock::now() + Duration)
        {
        }
        void start() { Deadline = Clock::now() + Duration; }
        void wait()
        {
            const auto now = Clock::now();
            if (now < Deadline) {
                std::this_thread::sleep_for(Deadline - now);
            }
        }
        std::chrono::microseconds duration() const { return Duration; }
    };
    // showing desktop
    SC_LITE_EXTERN std::shared_ptr<ShowingDesktopResult> ShowingDesktop(bool show);
    // will return all attached monitors
    SC_LITE_EXTERN std::shared_ptr<std::vector<Monitor>> GetMonitors();
    // will return all windows
    SC_LITE_EXTERN std::shared_ptr<std::vector<Window>> GetWindows(const SearchAllWindowsRequest &request);

    // Make window full screen
    SC_LITE_EXTERN std::shared_ptr<Window> Fullscreen(const SingleWindowRequest &windowRequest, int display, bool fullscreen);
    // Make window minimize
    SC_LITE_EXTERN std::shared_ptr<Window> Minimize(const SingleWindowRequest &windowRequest, bool minimize);
    // Make window maximize
    SC_LITE_EXTERN std::shared_ptr<Window> Maximize(const SingleWindowRequest &windowRequest, bool maximize);

    SC_LITE_EXTERN std::shared_ptr<Window> MaximizeHorz(const SingleWindowRequest &windowRequest, bool maximize);

    SC_LITE_EXTERN std::shared_ptr<Window> MaximizeVert(const SingleWindowRequest &windowRequest, bool maximize);
    // Make window minimize
    SC_LITE_EXTERN std::shared_ptr<Window> Visible(const SingleWindowRequest &windowRequest, bool visible);

    // Close all windows / terminate application
    SC_LITE_EXTERN std::shared_ptr<EmptyResult> CloseAllWindows(const SingleWindowRequest &windowRequest, bool force);

    // Close window
    SC_LITE_EXTERN std::shared_ptr<EmptyResult> CloseWindow(const SingleWindowRequest &windowRequest, bool force);

    // Set rectangle
    SC_LITE_EXTERN std::shared_ptr<Window> Rectangle(const SingleWindowRequest &windowRequest, const Point &position, const SizeValues &size);
    // Make window top most
    SC_LITE_EXTERN std::shared_ptr<Window> Topmost(const SingleWindowRequest &windowRequest);
    // Activate window
    SC_LITE_EXTERN std::shared_ptr<Window> Activate(const SingleWindowRequest &windowRequest);

    // Native window info
    SC_LITE_EXTERN std::shared_ptr<NativeWindow> NativeWindowInfo(char* data, const size_t sz);

    // will return active window
    SC_LITE_EXTERN std::shared_ptr<Window> GetActiveWindow(const SharedWindowsRequest &request);
    // will return requested window
    SC_LITE_EXTERN std::shared_ptr<Window> GetWindow(const SingleWindowRequest &request);
    // request accessibility permissions
    SC_LITE_EXTERN bool RequestAccessibilityPermissions();
    // InitWindows
    SC_LITE_EXTERN void InitWindows();

    typedef std::function<void(const SL::Screen_Capture::Image &img, const Window &window)> WindowCaptureCallback;
    typedef std::function<void(const SL::Screen_Capture::Image &img, const Monitor &monitor)> ScreenCaptureCallback;
    typedef std::function<void(const SL::Screen_Capture::Image *img, const MousePoint &mousepoint)> MouseCallback;
    typedef std::function<std::vector<Monitor>()> MonitorCallback;
    typedef std::function<std::vector<Window>()> WindowCallback;

    class SC_LITE_EXTERN IScreenCaptureManager {
      public:
        virtual ~IScreenCaptureManager() {}

        // Used by the library to determine the callback frequency
        template <class Rep, class Period> void setFrameChangeInterval(const std::chrono::duration<Rep, Period> &rel_time)
        {
            setFrameChangeInterval(std::make_shared<Timer>(rel_time));
        }
        // Used by the library to determine the callback frequency
        template <class Rep, class Period> void setMouseChangeInterval(const std::chrono::duration<Rep, Period> &rel_time)
        {
            setMouseChangeInterval(std::make_shared<Timer>(rel_time));
        }

        virtual void setFrameChangeInterval(const std::shared_ptr<Timer> &timer) = 0;
        virtual void setMouseChangeInterval(const std::shared_ptr<Timer> &timer) = 0;

        // Will pause all capturing
        virtual void pause() = 0;
        // Will return whether the library is paused
        virtual bool isPaused() const = 0;
        // Will resume all capturing if paused, otherwise has no effect
        virtual void resume() = 0;
    };

    template <typename CAPTURECALLBACK> class ICaptureConfiguration {
      public:
        virtual ~ICaptureConfiguration() {}
        // When a new Frame is available the callback is invoked
        virtual std::shared_ptr<ICaptureConfiguration<CAPTURECALLBACK>> onNewFrame(const CAPTURECALLBACK &cb) = 0;
        // When a change in a Frame is detected, the callback is invoked
        virtual std::shared_ptr<ICaptureConfiguration<CAPTURECALLBACK>> onFrameChanged(const CAPTURECALLBACK &cb) = 0;
        // When a mouse image changes or the mouse changes position, the callback is invoked.
        virtual std::shared_ptr<ICaptureConfiguration<CAPTURECALLBACK>> onMouseChanged(const MouseCallback &cb) = 0;
        // start capturing
        virtual std::shared_ptr<IScreenCaptureManager> start_capturing() = 0;
    };

    // the callback of windowstocapture represents the list of monitors which should be captured. Users should return the list of monitors they want
    // to be captured
    SC_LITE_EXTERN std::shared_ptr<ICaptureConfiguration<ScreenCaptureCallback>> CreateCaptureConfiguration(const MonitorCallback &monitorstocapture);
    // the callback of windowstocapture represents the list of windows which should be captured. Users should return the list of windows they want to
    // be captured
    SC_LITE_EXTERN std::shared_ptr<ICaptureConfiguration<WindowCaptureCallback>> CreateCaptureConfiguration(const WindowCallback &windowstocapture);

    class SC_LITE_EXTERN IScreenCaptureManagerSync {
      public:
        virtual ~IScreenCaptureManagerSync() {}
    };

    template <typename CAPTURECALLBACK> class ICaptureConfigurationSync {
      public:
        virtual ~ICaptureConfigurationSync() {}
        // When a new Frame is available the callback is invoked
        virtual std::shared_ptr<ICaptureConfigurationSync<CAPTURECALLBACK>> onNewFrame(const CAPTURECALLBACK &cb) = 0;
        // When a change in a Frame is detected, the callback is invoked
        virtual std::shared_ptr<ICaptureConfigurationSync<CAPTURECALLBACK>> onFrameChanged(const CAPTURECALLBACK &cb) = 0;
        // start capturing
        virtual std::shared_ptr<IScreenCaptureManagerSync> startCapturing() = 0;
    };

    // the callback of windowstocapture represents the list of monitors which should be captured. Users should return the list of monitors they want
    // to be captured
    SC_LITE_EXTERN std::shared_ptr<ICaptureConfigurationSync<ScreenCaptureCallback>>
    CreateCaptureConfigurationSync(const MonitorCallback &monitorstocapture);
    SC_LITE_EXTERN bool RequestPermissionsSync();

} // namespace Screen_Capture
} // namespace SL
