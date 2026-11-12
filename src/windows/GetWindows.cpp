#include "GDIHelpers.h"
#include "ScreenCapture.h"
#include "internal/SCCommon.h"
#include "stringhelper.h"
#include <algorithm>
#include <climits>
#include <cstring>
#include <map>
#include <mutex>

#define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers

#include <Windows.h>
#include <psapi.h>
#include <shellapi.h>
#include <ShlObj.h>
#include <ShlDisp.h>
#include "internal/SCShared.h"
#include <UIAutomation.h>
#include <wrl/client.h>

#define LODEPNG_COMPILE_PNG
#define LODEPNG_COMPILE_DISK

#include "shared/png/lodepng.h"

#include "shared/base64/base64.h"

namespace SL {
    namespace Screen_Capture {

        // COM for the duration of one call on this (libuv worker) thread. CoUninitialize only if this call's
        // CoInitializeEx succeeded (S_OK or S_FALSE both take a reference). RPC_E_CHANGED_MODE means the thread is
        // already in another apartment (e.g. MTA): use it as it is and leave its initialisation alone.
        class ComApartment {
          public:
            ComApartment() : initialized(SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) {}
            ~ComApartment()
            {
                if (initialized) {
                    CoUninitialize();
                }
            }
            ComApartment(const ComApartment &) = delete;
            ComApartment &operator=(const ComApartment &) = delete;

          private:
            bool initialized;
        };

        // What Fullscreen(true) changed, per window, so Fullscreen(false) can put exactly that back. The request's
        // S_RES_WINDOWS_NORMAL_* attributes cannot do it: SMBAct's requests carry no Attributes (ssm-lib's
        // convert() does not copy them), so they never come back here. Lives as long as the process (the renderer
        // or the window engine); after a restart the fallback in Fullscreen(false) applies.
        struct SavedFrame {
            LONG Style;
            LONG ExStyle;
            WINDOWPLACEMENT Placement;
        };
        static std::mutex savedFramesMutex;
        static std::map<HWND, SavedFrame> savedFrames;

        struct SearchWindows {
            std::shared_ptr<std::vector<Window>> Found;
            std::vector<Monitor> *Monitors;
            bool RetrieveIcons;
        };

        std::vector<std::string> splitString(const std::string &str, char delimiter);

        std::shared_ptr<std::pair<HICON, HICON>> retrieveIcon(HANDLE hProcess);

        std::shared_ptr<Window> getWindow(const HWND &hwnd, bool retrieveIcon, const std::vector<Monitor> *monitors);

        RECT getRect(const Rect &rect);

        BOOL CALLBACK enumWindowsProc(HWND hwnd, LPARAM lParam);

        bool filterOutWindow(const HWND &hwnd);

        bool bringToFront(HWND hwnd);

        std::shared_ptr<std::string> retrieveBase64IconData(HICON hIcon);

        void fillIconsIfNeeded(Window &window, HICON hIcon, bool retrieveIcons);


        // show: minimise all windows (the shell's "Minimize all", Win+M); !show: undo that. Through the documented
        // Shell.Application object. The old code sent Progman 0x052C (the message that spawns the wallpaper WorkerW)
        // and WM_SETTINGCHANGE, neither of which shows the desktop, and reported success when SendMessageTimeout
        // failed (it returns 0 on failure).
        std::shared_ptr<ShowingDesktopResult> ShowingDesktop(bool show) {
            auto result = std::make_shared<ShowingDesktopResult>();
            result->success = false;
            ComApartment com;
            Microsoft::WRL::ComPtr<IShellDispatch> shell;
            if (SUCCEEDED(CoCreateInstance(__uuidof(Shell), NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&shell)))) {
                result->success = SUCCEEDED(show ? shell->MinimizeAll() : shell->UndoMinimizeALL());
            }
            return result;
        }

        // will return all windows
        std::shared_ptr<std::vector<Window>> GetWindows(const SearchAllWindowsRequest &request) {
            SearchWindows s;
            s.Found = std::make_shared<std::vector<Window>>();
            auto monitors = GetMonitors();
            s.Monitors = monitors.get();
            s.RetrieveIcons = request.RetrieveIcons;
            ::EnumWindows(enumWindowsProc, reinterpret_cast<LPARAM>(&s));
            return s.Found;
        }


        // Make window full screen
        std::shared_ptr<Window> Fullscreen(const SingleWindowRequest &windowRequest, int display, bool fullscreen) {
            HWND hwnd = reinterpret_cast<HWND>(windowRequest.Handle);
            if (IsWindow(hwnd)) {
                auto monitors = GetMonitors();
                if (fullscreen) {
                    if (::IsIconic(hwnd)) {
                        ShowWindow(hwnd, SW_RESTORE);
                        ShowWindow(hwnd, SW_SHOW);
                    }
                    auto wnd = getWindow(hwnd, false, monitors.get());
                    if (wnd) {
                        auto fullscreenIterator = std::find_if(wnd->Attributes.begin(), wnd->Attributes.end(),
                                                               [&](const WindowAttribute &attr) {
                                                                   return attr.Code == S_RES_FULLSCREEN &&
                                                                          attr.Value == S_TRUE;
                                                               });
                        if (fullscreenIterator != wnd->Attributes.end()) {
                            // window already in fullscreen
                            // make it foreground and return
                            // SetForegroundWindow(hwnd);
                            return wnd;
                        } else {
                            // determine display
                            RECT windowRect;
                            windowRect.top = wnd->Position.Y;
                            windowRect.bottom = wnd->Position.Y + wnd->Size.Height;
                            windowRect.left = wnd->Position.X;
                            windowRect.right = wnd->Position.X + wnd->Size.Width;

                            int i = 0;
                            int monitorIndex = -1;
                            int square = -1;
                            for (const auto &monitor: *monitors) {
                                const Rect &rect = monitor.Frame;
                                RECT monitorRect = getRect(rect);
                                RECT rectIntersection;
                                BOOL intersect = ::IntersectRect(&rectIntersection, &monitorRect, &windowRect);
                                if (intersect) {
                                    int area = (rectIntersection.right - rectIntersection.left) *
                                               (rectIntersection.bottom - rectIntersection.top);
                                    if (area > square) {
                                        square = area;
                                        monitorIndex = i;
                                    }
                                }
                                i++;
                            }

                            if (monitorIndex != -1) {
                                auto monitor = monitors->at(monitorIndex);

                                SavedFrame saved = {};
                                saved.Style = GetWindowLong(hwnd, GWL_STYLE);
                                saved.ExStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
                                saved.Placement.length = sizeof(WINDOWPLACEMENT);
                                if (GetWindowPlacement(hwnd, &saved.Placement)) {
                                    std::lock_guard<std::mutex> lock(savedFramesMutex);
                                    auto existing = savedFrames.find(hwnd);
                                    // keep an existing entry if the window still looks like ours (no caption, no
                                    // sizing border): saving that state would make it permanent
                                    if (existing == savedFrames.end() || (saved.Style & (WS_CAPTION | WS_THICKFRAME))) {
                                        savedFrames[hwnd] = saved;
                                    }
                                }

                                auto maximized = !!::IsZoomed(hwnd);
                                if (maximized) {
                                    SendMessage(hwnd, WM_SYSCOMMAND, SC_RESTORE, 0);
                                }

                                auto style = GetWindowLong(hwnd, GWL_STYLE);
                                auto exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);

                                // Set new window style and size.
                                SetWindowLong(hwnd, GWL_STYLE,
                                              style & ~(WS_CAPTION | WS_THICKFRAME));
                                SetWindowLong(hwnd, GWL_EXSTYLE,
                                              exStyle & ~(WS_EX_DLGMODALFRAME |
                                                          WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE));

                                // Set the window's position to cover the entire monitor
                                SetWindowPos(hwnd, HWND_TOP, monitor.Frame.X, monitor.Frame.Y,
                                             monitor.Frame.Width, monitor.Frame.Height,
                                             SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
                                // SetForegroundWindow(hwnd);
                            }

                            auto wndFullscreen = getWindow(hwnd, windowRequest.RetrieveIcons, monitors.get());
                            if (!wndFullscreen) {
                                return nullptr;
                            }
                            auto removeAttributes = std::remove_if(wndFullscreen->Attributes.begin(),
                                                                   wndFullscreen->Attributes.end(),
                                                                   [&](const WindowAttribute &attr) {
                                                                       return attr.Code ==
                                                                              S_RES_WINDOWS_NORMAL_MAXIMIZED ||
                                                                              attr.Code ==
                                                                              S_RES_WINDOWS_NORMAL_EXSTYLE ||
                                                                              attr.Code == S_RES_WINDOWS_NORMAL_STYLE ||
                                                                              attr.Code == S_RES_NORMAL_SIZE;
                                                                   });
                            wndFullscreen->Attributes.erase(removeAttributes, wndFullscreen->Attributes.end());

                            std::copy_if(wnd->Attributes.begin(), wnd->Attributes.end(),
                                         std::back_inserter(wndFullscreen->Attributes),
                                         [&](const WindowAttribute &attr) {
                                             return attr.Code == S_RES_WINDOWS_NORMAL_MAXIMIZED ||
                                                    attr.Code == S_RES_WINDOWS_NORMAL_EXSTYLE ||
                                                    attr.Code == S_RES_WINDOWS_NORMAL_STYLE ||
                                                    attr.Code == S_RES_NORMAL_SIZE;
                                         });

                            return wndFullscreen;
                        }
                    }
                } else {
                    if (::IsIconic(hwnd)) {
                        ShowWindow(hwnd, SW_RESTORE);
                        ShowWindow(hwnd, SW_SHOW);
                    }
                    SavedFrame saved = {};
                    bool haveSaved = false;
                    {
                        std::lock_guard<std::mutex> lock(savedFramesMutex);
                        auto found = savedFrames.find(hwnd);
                        if (found != savedFrames.end()) {
                            saved = found->second;
                            savedFrames.erase(found);
                            haveSaved = true;
                        }
                    }
                    if (haveSaved) {
                        // exactly what Fullscreen(true) found: styles, then the normal rectangle and show state
                        // (maximized included)
                        SetWindowLong(hwnd, GWL_STYLE, saved.Style);
                        SetWindowLong(hwnd, GWL_EXSTYLE, saved.ExStyle);
                        SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
                                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
                        SetWindowPlacement(hwnd, &saved.Placement);
                        return getWindow(hwnd, windowRequest.RetrieveIcons, monitors.get());
                    }

                    LONG style = GetWindowLong(hwnd, GWL_STYLE);
                    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);

                    // Nothing saved (fullscreen entered elsewhere, or this process restarted): give back the caption
                    // and sizing border Fullscreen(true) takes away, and the window edge that goes with a caption.
                    // Not DLGMODALFRAME / CLIENTEDGE / STATICEDGE: OR-ing those in drew a sunken 3D border around
                    // windows that never had one.
                    SetWindowLong(hwnd, GWL_STYLE,
                                  style | (WS_CAPTION | WS_THICKFRAME));
                    SetWindowLong(hwnd, GWL_EXSTYLE, exStyle | WS_EX_WINDOWEDGE);
                    SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
                                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);

                    auto normalSize = std::find_if(windowRequest.Attributes.begin(), windowRequest.Attributes.end(),
                                                   [&](const WindowAttribute &attr) {
                                                       return attr.Code == S_RES_NORMAL_SIZE;
                                                   });
                    auto normalMaximized = std::find_if(windowRequest.Attributes.begin(),
                                                        windowRequest.Attributes.end(),
                                                        [&](const WindowAttribute &attr) {
                                                            return attr.Code == S_RES_WINDOWS_NORMAL_MAXIMIZED &&
                                                                   attr.Value == S_TRUE;
                                                        });
                    if (normalSize != windowRequest.Attributes.end()) {
                        char delimiter = ';';
                        std::vector<std::string> tokens = splitString(normalSize->Value, delimiter);
                        int x = std::atoi(tokens[0].c_str());
                        int y = std::atoi(tokens[1].c_str());
                        int width = std::atoi(tokens[2].c_str());
                        int height = std::atoi(tokens[3].c_str());
                        int offsetDisplay0X = std::atoi(tokens[4].c_str());
                        int offsetDisplay0Y = std::atoi(tokens[5].c_str());
                        POINT p;
                        p.x = x;
                        p.y = y;
                        auto monitorFound = false;
                        for (const auto &monitor: *monitors) {
                            RECT monitorRect = getRect(monitor.Frame);
                            if (::PtInRect(&monitorRect, p)) {
                                monitorFound = true;
                            }
                        }
                        SetWindowPos(hwnd, HWND_TOP, monitorFound ? x : offsetDisplay0X,
                                     monitorFound ? y : offsetDisplay0Y,
                                     width, height,
                                     SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);

                    }

                    if (normalMaximized != windowRequest.Attributes.end()) {
                        ::SendMessage(hwnd, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
                    }

                    // SetForegroundWindow(hwnd);


                    return getWindow(hwnd, windowRequest.RetrieveIcons, monitors.get());
                }
            }
            return nullptr;
        }

        // Make window minimize
        std::shared_ptr<Window> Minimize(const SingleWindowRequest &windowRequest, bool minimize) {
            HWND hwnd = reinterpret_cast<HWND>(windowRequest.Handle);
            if (IsWindow(hwnd)) {
                if (!minimize) {
                    ShowWindow(hwnd, SW_RESTORE);
                    ShowWindow(hwnd, SW_SHOW);

                } else {
                    ShowWindow(hwnd, SW_MINIMIZE);
                }
                auto monitors = GetMonitors();
                return getWindow(hwnd, windowRequest.RetrieveIcons, monitors.get());
            }
            return nullptr;
        }

        // Make window maximize
        std::shared_ptr<Window> Maximize(const SingleWindowRequest &windowRequest, bool maximize) {
            HWND hwnd = reinterpret_cast<HWND>(windowRequest.Handle);
            if (IsWindow(hwnd)) {
                if (IsIconic(hwnd)) {
                    ShowWindow(hwnd, SW_SHOW);
                }
                if (IsZoomed(hwnd) != maximize) {
                    ShowWindow(hwnd, maximize ? SW_MAXIMIZE : SW_RESTORE);
                }
                auto monitors = GetMonitors();
                return getWindow(hwnd, windowRequest.RetrieveIcons, monitors.get());
            }
            return nullptr;
        }

        // MaximizeHorz / MaximizeVert. Windows has no per-axis maximized state (Linux asks the window manager for
        // EWMH's _NET_WM_STATE_MAXIMIZED_HORZ / _VERT; macOS does not support it). Emulated with the same meaning: the
        // window's visible frame (DWM's extended frame bounds; the invisible resize borders stay outside) spans the work
        // area of its monitor on that axis, the other axis is left alone. The extent it had before is kept per HWND in
        // this process (like savedFrames), so maximize=false puts it back, as the window manager does on Linux. If the
        // window was moved or resized on that axis since, maximize=false leaves it there. A maximized window (both
        // axes) that loses one axis keeps the other, as with EWMH. getWindow does not report the emulated state.
        struct SavedExtent {
            LONG Start, Length;               // the outer rect on that axis before maximize=true
            LONG AppliedStart, AppliedLength; // and right after it (the window may have adjusted the requested rect)
        };
        static std::mutex savedExtentsMutex;
        static std::map<HWND, SavedExtent> savedHorz;
        static std::map<HWND, SavedExtent> savedVert;

        static LONG axisStart(const RECT &r, bool horz) { return horz ? r.left : r.top; }
        static LONG axisEnd(const RECT &r, bool horz) { return horz ? r.right : r.bottom; }

        static bool maximizeAxis(HWND hwnd, bool horz, bool maximize) {
            auto &saved = horz ? savedHorz : savedVert;
            if (IsIconic(hwnd)) {
                ShowWindow(hwnd, SW_RESTORE);
            }
            if (IsZoomed(hwnd)) {
                if (maximize) {
                    return true; // spans both axes already
                }
                // loses this axis only: back to the normal rectangle, then the other axis spans the work area
                ShowWindow(hwnd, SW_RESTORE);
                {
                    std::lock_guard<std::mutex> lock(savedExtentsMutex);
                    savedHorz.erase(hwnd);
                    savedVert.erase(hwnd);
                }
                return maximizeAxis(hwnd, !horz, true);
            }
            RECT outer = {};
            if (!::GetWindowRect(hwnd, &outer)) {
                return false;
            }
            RECT visible = outer; // without DWM composition there are no invisible borders
            if (FAILED(DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &visible, sizeof(visible)))) {
                visible = outer;
            }
            const LONG start = axisStart(outer, horz);
            const LONG length = axisEnd(outer, horz) - start;
            LONG newStart, newLength;
            if (maximize) {
                MONITORINFO mi = {};
                mi.cbSize = sizeof(mi);
                HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
                if (!monitor || !GetMonitorInfoW(monitor, &mi)) {
                    return false;
                }
                std::lock_guard<std::mutex> lock(savedExtentsMutex);
                auto found = saved.find(hwnd);
                if (found != saved.end() && found->second.AppliedStart == start && found->second.AppliedLength == length) {
                    return true; // still where maximize=true put it: keep the extent from before
                }
                saved[hwnd] = SavedExtent{start, length, 0, 0};
                newStart = axisStart(mi.rcWork, horz) - (axisStart(visible, horz) - start);
                newLength = axisEnd(mi.rcWork, horz) + (axisEnd(outer, horz) - axisEnd(visible, horz)) - newStart;
            } else {
                std::lock_guard<std::mutex> lock(savedExtentsMutex);
                auto found = saved.find(hwnd);
                if (found == saved.end()) {
                    return true; // not maximized on this axis here: nothing to put back
                }
                const SavedExtent s = found->second;
                saved.erase(found);
                if (s.AppliedStart != start || s.AppliedLength != length) {
                    return true; // moved or resized since: leave it
                }
                newStart = s.Start;
                newLength = s.Length;
            }
            // SetWindowPos outside the lock: it waits for the window's thread (WM_WINDOWPOSCHANGING)
            const BOOL moved = horz ? SetWindowPos(hwnd, nullptr, newStart, outer.top, newLength, outer.bottom - outer.top,
                                                   SWP_NOZORDER | SWP_NOACTIVATE)
                                    : SetWindowPos(hwnd, nullptr, outer.left, newStart, outer.right - outer.left, newLength,
                                                   SWP_NOZORDER | SWP_NOACTIVATE);
            if (maximize) {
                RECT after = {};
                const bool haveAfter = moved && ::GetWindowRect(hwnd, &after);
                std::lock_guard<std::mutex> lock(savedExtentsMutex);
                auto found = saved.find(hwnd);
                if (found != saved.end()) {
                    if (haveAfter) {
                        found->second.AppliedStart = axisStart(after, horz);
                        found->second.AppliedLength = axisEnd(after, horz) - axisStart(after, horz);
                    } else {
                        saved.erase(found);
                    }
                }
            }
            return moved != FALSE;
        }

        std::shared_ptr<Window> MaximizeHorz(const SingleWindowRequest &windowRequest, bool maximize) {
            HWND hwnd = reinterpret_cast<HWND>(windowRequest.Handle);
            if (IsWindow(hwnd)) {
                maximizeAxis(hwnd, true, maximize);
                auto monitors = GetMonitors();
                return getWindow(hwnd, windowRequest.RetrieveIcons, monitors.get());
            }
            return nullptr;
        }

        std::shared_ptr<Window> MaximizeVert(const SingleWindowRequest &windowRequest, bool maximize) {
            HWND hwnd = reinterpret_cast<HWND>(windowRequest.Handle);
            if (IsWindow(hwnd)) {
                maximizeAxis(hwnd, false, maximize);
                auto monitors = GetMonitors();
                return getWindow(hwnd, windowRequest.RetrieveIcons, monitors.get());
            }
            return nullptr;
        }

        // Make window minimize
        std::shared_ptr<Window> Visible(const SingleWindowRequest &windowRequest, bool visible) {
            HWND hwnd = reinterpret_cast<HWND>(windowRequest.Handle);
            if (IsWindow(hwnd)) {
                if (visible) {
                    if (IsIconic(hwnd)) {
                        ShowWindow(hwnd, SW_RESTORE);
                    }
                    ShowWindow(hwnd, SW_SHOW);
                } else {
                    ShowWindow(hwnd, SW_MINIMIZE);
                }
                auto monitors = GetMonitors();
                return getWindow(hwnd, windowRequest.RetrieveIcons, monitors.get());
            }
            return nullptr;
        }

        // Close all windows / terminate the application of the window. It used to do nothing and report success, so
        // MCP close_window with scope "application" closed nothing. macOS asks the application to quit
        // (NSRunningApplication terminate / forceTerminate). Here:
        //  - force: the window's process is ended (TerminateProcess, as CloseWindow(force) does);
        //  - otherwise WM_CLOSE is posted to every window of that process that GetWindows would list (cloaked ones,
        //    e.g. on another virtual desktop, included), and to the requested window itself: what closing each of them
        //    by hand does, so an application with unsaved work asks first. Posted, not sent (see CloseWindow).
        //  - a UWP frame (ApplicationFrameWindow) belongs to ApplicationFrameHost.exe, which hosts the frames of all UWP
        //    apps: only that frame gets WM_CLOSE, and force ends the app process behind it (the process of the frame's
        //    CoreWindow), never the host; without a CoreWindow (a suspended app) force does nothing.
        // Result: nullptr if the handle is not a window or nothing could be posted / terminated.
        struct CloseAllContext {
            DWORD Pid;
            HWND Requested;
            int Posted;
            bool RequestedPosted;
        };

        std::shared_ptr<EmptyResult> CloseAllWindows(const SingleWindowRequest &windowRequest, bool force) {
            HWND hwnd = reinterpret_cast<HWND>(windowRequest.Handle);
            if (!IsWindow(hwnd)) {
                return nullptr;
            }
            DWORD pid = 0;
            GetWindowThreadProcessId(hwnd, &pid);
            wchar_t className[64] = {0};
            const bool uwpFrame = GetClassNameW(hwnd, className, 64) && lstrcmpW(className, L"ApplicationFrameWindow") == 0;
            if (uwpFrame) {
                if (!force) {
                    return PostMessage(hwnd, WM_CLOSE, 0, 0) ? std::make_shared<EmptyResult>() : nullptr;
                }
                HWND core = FindWindowExW(hwnd, nullptr, L"Windows.UI.Core.CoreWindow", nullptr);
                DWORD corePid = 0;
                if (!core || !GetWindowThreadProcessId(core, &corePid) || corePid == 0 || corePid == pid) {
                    return nullptr;
                }
                pid = corePid;
            }
            if (force) {
                HANDLE hProcess = pid ? OpenProcess(PROCESS_TERMINATE, FALSE, pid) : NULL;
                if (!hProcess) {
                    return nullptr;
                }
                const BOOL terminated = TerminateProcess(hProcess, 0);
                CloseHandle(hProcess);
                return terminated ? std::make_shared<EmptyResult>() : nullptr;
            }
            CloseAllContext context = {pid, hwnd, 0, false};
            ::EnumWindows(
                [](HWND h, LPARAM lParam) -> BOOL {
                    auto *c = reinterpret_cast<CloseAllContext *>(lParam);
                    DWORD windowPid = 0;
                    GetWindowThreadProcessId(h, &windowPid);
                    if (windowPid == c->Pid && !filterOutWindow(h) && PostMessage(h, WM_CLOSE, 0, 0)) {
                        c->Posted++;
                        c->RequestedPosted = c->RequestedPosted || h == c->Requested;
                    }
                    return TRUE;
                },
                reinterpret_cast<LPARAM>(&context));
            if (!context.RequestedPosted && PostMessage(hwnd, WM_CLOSE, 0, 0)) {
                context.Posted++;
            }
            return context.Posted ? std::make_shared<EmptyResult>() : nullptr;
        }

        // Close window
        std::shared_ptr<EmptyResult> CloseWindow(const SingleWindowRequest &windowRequest, bool force) {
            HWND hwnd = reinterpret_cast<HWND>(windowRequest.Handle);
            if (IsWindow(hwnd)) {
                if (!force) {
                    // SetForegroundWindow(hwnd);
                    // Posted, not sent: WM_CLOSE runs the app's close logic (a "Save changes?" dialog waits for the
                    // user), and SendMessage waited for all of it, or for ever on a hung window, on this thread.
                    PostMessage(hwnd, WM_CLOSE, 0, 0);
                } else {
                    DWORD dwProcessId;
                    GetWindowThreadProcessId(hwnd, &dwProcessId);
                    // Open a handle to the process
                    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, dwProcessId);
                    if (hProcess) {
                        // Terminate the process
                        TerminateProcess(hProcess, 0);
                        // Close the process handle
                        CloseHandle(hProcess);
                    }
                }
                return std::make_shared<EmptyResult>();
            }
            return nullptr;
        }

        // Set rectangle
        std::shared_ptr<Window>
        Rectangle(const SingleWindowRequest &windowRequest, const Point &position, const SizeValues &size) {
            HWND hwnd = reinterpret_cast<HWND>(windowRequest.Handle);
            if (IsWindow(hwnd)) {
                if (IsIconic(hwnd)) {
                    ShowWindow(hwnd, SW_RESTORE);
                    ShowWindow(hwnd, SW_SHOW);
                }
                int x = position.X, y = position.Y, w = size.Width, h = size.Height;
                if (IsZoomed(hwnd)) {
                    // The caller computed the outer rect from the frame insets of the maximized window (8 px on
                    // every side at 100 %), but SetWindowPos applies to the restored window, whose insets differ
                    // (no top inset): shift by the difference, or a layout on a maximized window lands a border
                    // off (seen 2026-11-12: frame 1279,512 642x529 for the bottom right sixth 1280,520 640x520).
                    RECT r0{}, f0{};
                    const bool before = GetWindowRect(hwnd, &r0) &&
                                        SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &f0, sizeof(f0)));
                    ShowWindow(hwnd, SW_RESTORE);
                    RECT r1{}, f1{};
                    if (before && GetWindowRect(hwnd, &r1) &&
                        SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &f1, sizeof(f1)))) {
                        const int l0 = f0.left - r0.left, t0 = f0.top - r0.top, rr0 = r0.right - f0.right, b0 = r0.bottom - f0.bottom;
                        const int l1 = f1.left - r1.left, t1 = f1.top - r1.top, rr1 = r1.right - f1.right, b1 = r1.bottom - f1.bottom;
                        x += l0 - l1;
                        y += t0 - t1;
                        w += (l1 + rr1) - (l0 + rr0);
                        h += (t1 + b1) - (t0 + b0);
                    }
                }

                SetWindowPos(hwnd, nullptr, x, y, w, h, SWP_NOZORDER);
                auto monitors = GetMonitors();
                return getWindow(hwnd, windowRequest.RetrieveIcons, monitors.get());
            }
            return nullptr;
        }

        // Make window top most
        std::shared_ptr<Window> Topmost(const SingleWindowRequest &windowRequest) {
            HWND hwnd = reinterpret_cast<HWND>(windowRequest.Handle);
            if (IsWindow(hwnd)) {
                if (IsIconic(hwnd)) {
                    ShowWindow(hwnd, SW_RESTORE);
                    ShowWindow(hwnd, SW_SHOW);
                }
                // SetForegroundWindow(hwnd);
                // Bring the window on top of all other windows

                SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
                bringToFront(hwnd);
                SetForegroundWindow(hwnd);
                bringToFront(hwnd);
                auto monitors = GetMonitors();
                return getWindow(hwnd, windowRequest.RetrieveIcons, monitors.get());
            }
            return nullptr;
        }

        // Activate window
        std::shared_ptr<Window> Activate(const SingleWindowRequest &windowRequest) {
            HWND hwnd = reinterpret_cast<HWND>(windowRequest.Handle);
            if (IsWindow(hwnd)) {
                if (IsIconic(hwnd)) {
                    ShowWindow(hwnd, SW_RESTORE);
                    ShowWindow(hwnd, SW_SHOW);
                }
                // Bring the window on top of all other windows
                SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
                bringToFront(hwnd);
                SetForegroundWindow(hwnd);
                bringToFront(hwnd);
                auto monitors = GetMonitors();
                return getWindow(hwnd, windowRequest.RetrieveIcons, monitors.get());
            }
            return nullptr;
        }

        // macOS only: activation here goes through Activate/Topmost.
        bool ActivateApplication(int) { return false; }

        // Native window info: data is what Electron's BrowserWindow.getNativeWindowHandle() returns, the HWND
        // (sizeof(HWND) bytes). Linux reads the X window id the same way; macOS turns an NSView* into the window number.
        // Handle = the HWND, OwnerHandle = its process id (as getWindow reports them). It returned nullptr before.

        std::shared_ptr<NativeWindow> NativeWindowInfo(char *data, const size_t sz) {
            if (data == nullptr || sz < sizeof(HWND)) {
                return nullptr;
            }
            HWND hwnd = nullptr;
            memcpy(&hwnd, data, sizeof(hwnd));
            if (!IsWindow(hwnd)) {
                return nullptr;
            }
            DWORD pid = 0;
            GetWindowThreadProcessId(hwnd, &pid);
            auto window = std::make_shared<NativeWindow>();
            window->Handle = reinterpret_cast<size_t>(hwnd);
            window->OwnerHandle = pid;
            return window;
        }

        // will return active window
        std::shared_ptr<Window> GetActiveWindow(const SharedWindowsRequest &request) {
            HWND hwndForeground = GetForegroundWindow();
            if (hwndForeground) {
                auto monitors = GetMonitors();
                return getWindow(hwndForeground, request.RetrieveIcons, monitors.get());
            }

            return nullptr;
        }

        // will return requested window
        std::shared_ptr<Window> GetWindow(const SingleWindowRequest &request) {
            HWND hwnd = reinterpret_cast<HWND>(request.Handle);
            if (IsWindow(hwnd)) {
                auto monitors = GetMonitors();
                return getWindow(hwnd, request.RetrieveIcons, monitors.get());
            }
            return nullptr;
        }

        // request accessibility permissions
        bool RequestAccessibilityPermissions() {
            return true;
        }

        // InitWindows
        void InitWindows() {
        }

        BOOL CALLBACK enumWindowsProc(HWND hwnd, LPARAM lParam) {
            DWORD pid;
            GetWindowThreadProcessId(hwnd, &pid);
            if (pid == GetCurrentProcessId()) {
                return TRUE;
            }

            // Cloaked windows are not on screen although they are WS_VISIBLE: windows on another virtual desktop,
            // UWP frames of suspended or closed apps (Settings, Calculator, ...). Left out of the list, like
            // Alt+Tab does. Only here: GetWindow / GetActiveWindow on a known handle still answer for them.
            DWORD cloaked = 0;
            if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) && cloaked != 0) {
                return TRUE;
            }

            auto *s = reinterpret_cast<SearchWindows *>(lParam);
            auto w = getWindow(hwnd, s->RetrieveIcons, s->Monitors);
            if (w) {
                s->Found->emplace_back(*w);
            }
            return TRUE;
        }

        bool bringToFront(HWND hwnd) {
            ComApartment com;  // declared first: released after the COM pointers below

            // First restore if window is minimized

            WINDOWPLACEMENT placement{};
            placement.length = sizeof(placement);

            if (!GetWindowPlacement(hwnd, &placement))
                return false;

            bool minimized = placement.showCmd == SW_SHOWMINIMIZED;
            if (minimized) {
                ShowWindow(hwnd, SW_RESTORE);
            }

            // Then bring it to front using UI automation

            Microsoft::WRL::ComPtr<IUIAutomation> uiAutomation;
            if (FAILED(CoCreateInstance(__uuidof(CUIAutomation), NULL, CLSCTX_INPROC_SERVER,
                                        IID_PPV_ARGS(&uiAutomation)))) {
                return false;
            }
            Microsoft::WRL::ComPtr<IUIAutomationElement> window;
            if (FAILED(uiAutomation->ElementFromHandle(hwnd, &window)) || !window) {
                return false;
            }
            return SUCCEEDED(window->SetFocus());
        }

        std::shared_ptr<Window> getWindow(const HWND &hwnd, bool retrieveIcons, const std::vector<Monitor> *monitors) {

            if (filterOutWindow(hwnd)) {
                return nullptr;
            }

            Window w = {};
            w.Name = std::string();

            auto length = 4096;
            std::unique_ptr<wchar_t[]> buffer(new wchar_t[length]);  // new[] needs delete[] (was unique_ptr<wchar_t>)
            auto textlen = GetWindowText(hwnd, buffer.get(), length);
            if (textlen >= 0) {
                w.Name = StringHelper::ws2s(std::wstring(buffer.get(), textlen));
            }

            w.Handle = reinterpret_cast<size_t>(hwnd);
            auto windowRect = GetWindowRect(hwnd);

            w.Position.X = windowRect.ClientRect.left;
            w.Position.Y = windowRect.ClientRect.top;
            w.Size.Width = windowRect.ClientRect.right - windowRect.ClientRect.left;
            w.Size.Height = windowRect.ClientRect.bottom - windowRect.ClientRect.top;

            // the same idea like in Linux
            w.Frame.LeftFrame = -windowRect.ClientBorder.left;
            w.Frame.RightFrame = -windowRect.ClientBorder.right;
            w.Frame.TopFrame = -windowRect.ClientBorder.top;
            w.Frame.BottomFrame = -windowRect.ClientBorder.bottom;

            DWORD dwProcessId;
            if (GetWindowThreadProcessId(hwnd, &dwProcessId))
            {
                w.OwnerHandle = dwProcessId;
            }

            wchar_t filenameBuffer[4000];
            DWORD processId;
            GetWindowThreadProcessId(hwnd, &processId);
            // PROCESS_QUERY_LIMITED_INFORMATION is all GetProcessImageFileName / QueryFullProcessImageName need, and
            // unlike QUERY_INFORMATION | VM_READ it is granted for elevated and protected processes too.
            auto hProcess = OpenProcess(
                    PROCESS_QUERY_LIMITED_INFORMATION,
                    FALSE,
                    processId);
            if (hProcess != NULL) {
                auto lenImageFileName = GetProcessImageFileName(
                        hProcess,
                        filenameBuffer,
                        4000
                );
                if (lenImageFileName > 0) {
                    WindowAttribute attributeName;
                    attributeName.Code = std::string(S_RES_NAME_ALT);
                    attributeName.Value = StringHelper::ws2s(std::wstring(filenameBuffer, lenImageFileName));
                    w.Attributes.emplace_back(attributeName);
                }
                TCHAR processFileName[MAX_PATH];
                DWORD charsCarried = MAX_PATH;
                // on failure charsCarried stays MAX_PATH: do not read the uninitialised buffer then
                if (QueryFullProcessImageName(hProcess, 0, processFileName, &charsCarried) && charsCarried) {
                    w.ProcessName = StringHelper::ws2s(std::wstring(processFileName, charsCarried));
                    WindowAttribute attributeName;
                    attributeName.Code = std::string(S_RES_START_ABSOLUTE_URL);
                    attributeName.Value = w.ProcessName;
                    w.Attributes.emplace_back(attributeName);
                }

                std::string path = w.ProcessName;
                size_t pos = path.find_last_of("\\/");
                if (pos != std::string::npos) {
                    w.ProcessName = path.substr(pos + 1);
                }

                WindowAttribute attributeName;
                attributeName.Code = std::string(S_RES_NAME);
                attributeName.Value = w.ProcessName;
                w.Attributes.emplace_back(attributeName);


                // WM_GETICON is answered by the window's own thread: bounded wait, or one hung window blocks this
                // call (and the worker thread) for ever. The handle is borrowed (the window owns it), so it is not
                // destroyed here; DestroyIcon on it destroyed the icon of SMBAct's own windows.
                DWORD_PTR iconResult = 0;
                HICON hIcon = NULL;
                if (SendMessageTimeout(hwnd, WM_GETICON, ICON_BIG, 0, SMTO_ABORTIFHUNG, 100, &iconResult)) {
                    hIcon = reinterpret_cast<HICON>(iconResult);
                }
                if (hIcon != NULL) {
                    fillIconsIfNeeded(w, hIcon, retrieveIcons);
                } else {
                    auto icons = retrieveIcon(hProcess);
                    if (icons) {
                        HICON hIconProcess;
                        if (icons->second) {
                            hIconProcess = icons->second;
                        } else {
                            hIconProcess = icons->first;
                        }
                        if (hIconProcess) {
                            fillIconsIfNeeded(w, hIconProcess, retrieveIcons);
                        }
                        if (icons->second) {
                            DestroyIcon(icons->second);
                        }
                        if (icons->first) {
                            DestroyIcon(icons->first);
                        }
                    }
                }

                CloseHandle(hProcess);
            }

            wchar_t classNameBuffer[4000];
            // Not `if (UINT len = GetClassName(...) > 0)`: that assigns the comparison, so len was 1 and res_class
            // carried only the first letter of the class ("N" for "Notepad").
            int classNameLength = GetClassName(hwnd, classNameBuffer, 4000);
            if (classNameLength > 0) {
                WindowAttribute attributeName;
                attributeName.Code = S_RES_CLASS;
                attributeName.Value = StringHelper::ws2s(std::wstring(classNameBuffer, classNameLength));
                w.Attributes.emplace_back(attributeName);
            }

            BOOL visible = !IsIconic(hwnd);
            WindowAttribute attributeVisible;
            attributeVisible.Code = S_RES_VISIBLE;
            attributeVisible.Value = visible ? S_TRUE : S_FALSE;
            w.Attributes.emplace_back(attributeVisible);

            if (visible) {
                // check fullscreen
                const Monitor *targetMonitor = nullptr;
                for (const auto &monitor: *monitors) {
                    if (monitor.Frame.X == w.Position.X
                        && monitor.Frame.Y == w.Position.Y) {
                        targetMonitor = &monitor;
                        break;
                    }
                }
                bool fullscreen = false;
                if (targetMonitor
                    && targetMonitor->Frame.Width == w.Size.Width
                    && targetMonitor->Frame.Height == w.Size.Height) {
                    WindowAttribute attributeFullscreen;
                    attributeFullscreen.Code = S_RES_FULLSCREEN;
                    attributeFullscreen.Value = S_TRUE;
                    w.Attributes.emplace_back(attributeFullscreen);
                    fullscreen = true;
                }

                if (!targetMonitor) {
                    POINT p;
                    p.x = w.Position.X;
                    p.y = w.Position.Y;
                    for (const auto &monitor: *monitors) {
                        RECT monitorRect = getRect(monitor.Frame);
                        if (::PtInRect(&monitorRect, p)) {
                            targetMonitor = &monitor;
                            break;
                        }
                    }
                }

                if (!fullscreen) {
                    auto style = GetWindowLong(hwnd, GWL_STYLE);
                    auto exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
                    WindowAttribute attributeStyle;
                    char s[256];
                    sprintf(s, "%ld", style);
                    attributeStyle.Value = s;
                    attributeStyle.Code = S_RES_WINDOWS_NORMAL_STYLE;
                    w.Attributes.emplace_back(attributeStyle);

                    WindowAttribute attributeExStyle;
                    sprintf(s, "%ld", exStyle);
                    attributeExStyle.Value = s;
                    attributeExStyle.Code = S_RES_WINDOWS_NORMAL_EXSTYLE;
                    w.Attributes.emplace_back(attributeExStyle);

                    if (targetMonitor) {
                        WindowAttribute attributeNormalSize;
                        sprintf(s, "%d;%d;%d;%d;%d;%d", w.Position.X, w.Position.Y, w.Size.Width, w.Size.Height,
                                w.Position.X - targetMonitor->Frame.X, w.Position.Y - targetMonitor->Frame.Y);
                        attributeNormalSize.Value = s;
                        attributeNormalSize.Code = S_RES_NORMAL_SIZE;
                        w.Attributes.emplace_back(attributeNormalSize);
                    }


                    WINDOWPLACEMENT wp;
                    wp.length = sizeof(WINDOWPLACEMENT);
                    GetWindowPlacement(hwnd, &wp);
                    if (wp.showCmd == SW_SHOWMAXIMIZED) {
                        WindowAttribute attributeNormalMaximized;
                        attributeNormalMaximized.Code = S_RES_WINDOWS_NORMAL_MAXIMIZED;
                        attributeNormalMaximized.Value = S_TRUE;
                        w.Attributes.emplace_back(attributeNormalMaximized);

                        // the window is maximized, and it may be in fullscreen mode
                        WindowAttribute attributeMaximized;
                        attributeMaximized.Code = S_RES_MAXIMIZED;
                        attributeMaximized.Value = S_TRUE;
                        w.Attributes.emplace_back(attributeMaximized);

                        WindowAttribute attributeFullscreen;
                        attributeFullscreen.Code = S_RES_FULLSCREEN;
                        attributeFullscreen.Value = S_FALSE;
                        w.Attributes.emplace_back(attributeFullscreen);


                    } else {
                        // the window is not maximized, and it is not in fullscreen mode
                        WindowAttribute attributeMaximized;
                        attributeMaximized.Code = S_RES_MAXIMIZED;
                        attributeMaximized.Value = S_FALSE;
                        w.Attributes.emplace_back(attributeMaximized);
                    }
                }
            }

            return std::make_shared<Window>(w);
        }

        void fillIconsIfNeeded(Window &window, HICON hIcon, bool retrieveIcons) {
            if (retrieveIcons) {
                auto image = retrieveBase64IconData(hIcon);
                if (image) {
                    WindowAttribute attributeIcon;
                    attributeIcon.Code = std::string(S_RES_ICON_URL);
                    attributeIcon.Value = *image;
                    window.Attributes.emplace_back(attributeIcon);
                }
            }
            WindowAttribute attributeIconExist;
            attributeIconExist.Code = std::string(S_RES_ICON_EXIST);
            attributeIconExist.Value = S_TRUE;
            window.Attributes.emplace_back(attributeIconExist);
        }

        std::shared_ptr<std::string> retrieveBase64IconData(HICON hIcon) {
            // GetIconInfo hands out copies of the icon's bitmaps, which are ours to delete (once). If it fails,
            // nothing was written: the old code went on and DeleteObject'ed whatever was on the stack.
            ICONINFO infoIconProcess = {};
            if (!GetIconInfo(hIcon, &infoIconProcess)) {
                return nullptr;
            }
            std::shared_ptr<std::string> result;

            // Create bitmap from icon data
            BITMAP bmp = {};
            if (infoIconProcess.hbmColor != NULL) {  // no colour bitmap: a monochrome icon, not encoded
                GetObject(infoIconProcess.hbmColor, sizeof(BITMAP), &bmp);
            }
            HBITMAP hBitmap = NULL;
            HDC hMemDC = NULL;
            HGDIOBJ hOldBitmap = NULL;
            BYTE *pBits = NULL;
            if (bmp.bmWidth > 0 && bmp.bmHeight > 0) {
                BITMAPINFO bmi = {};
                bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                bmi.bmiHeader.biWidth = bmp.bmWidth;
                bmi.bmiHeader.biHeight = -bmp.bmHeight;
                bmi.bmiHeader.biPlanes = 1;
                // Always 32 bpp: the PNG below is encoded as RGBA, i.e. reads width * height * 4 bytes. With the
                // icon's own depth (16 or 24 bpp, e.g. a DDB on a 16-bit display) that read ran past the DIB.
                bmi.bmiHeader.biBitCount = 32;
                bmi.bmiHeader.biCompression = BI_RGB;
                hBitmap = CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, (void **) &pBits, NULL, 0);
                hMemDC = hBitmap != NULL ? CreateCompatibleDC(NULL) : NULL;
                hOldBitmap = hMemDC != NULL ? SelectObject(hMemDC, hBitmap) : NULL;
            }
            if (hOldBitmap != NULL && pBits != NULL &&
                DrawIconEx(hMemDC, 0, 0, hIcon, bmp.bmWidth, bmp.bmHeight, 0, NULL, DI_NORMAL)) {
                GdiFlush();  // the DIB is read directly below
                const size_t bytes = 4;
                const size_t biSizeImage = static_cast<size_t>(bmp.bmWidth) * bmp.bmHeight * bytes;
                // Iterate through the bitmap data and swap the red and blue components
                for (size_t i = 0; i < biSizeImage; i += bytes)
                {
                    BYTE temp = pBits[i];
                    pBits[i] = pBits[i + 2];
                    pBits[i + 2] = temp;
                }
                // Create image from bitmap data
                unsigned char *pngData = nullptr;
                size_t outsize = 0;
                unsigned int pngEncResult = lodepng_encode_memory(&pngData, &outsize, pBits, bmp.bmWidth, bmp.bmHeight,
                                                                  LCT_RGBA, 8);

                if (0 != pngEncResult) {
                    result = nullptr;
                } else {
                    auto iconStrValue = base64_encode(pngData, outsize, std::string("data:image/png;base64,"), false);
                    result = std::make_shared<std::string>(iconStrValue);
                }
                free(pngData);  // lodepng allocates with malloc (lodepng_malloc), not new[]
            } else {
                result = nullptr;
            }

            // Clean up: deselect the DIB before deleting it (DeleteObject's contract: not while selected into a DC)
            if (hMemDC != NULL) {
                if (hOldBitmap != NULL) {
                    SelectObject(hMemDC, hOldBitmap);
                }
                DeleteDC(hMemDC);
            }
            if (hBitmap != NULL) {
                DeleteObject(hBitmap);
            }
            if (infoIconProcess.hbmColor != NULL) {
                DeleteObject(infoIconProcess.hbmColor);
            }
            if (infoIconProcess.hbmMask != NULL) {
                DeleteObject(infoIconProcess.hbmMask);
            }

            return result;
        }

        std::shared_ptr<std::pair<HICON, HICON>> retrieveIcon(HANDLE hProcess) {
            // Get the path of the executable file (QueryFullProcessImageName: GetModuleFileNameEx needs
            // PROCESS_VM_READ, which the handle no longer has)
            wchar_t path[MAX_PATH];
            DWORD pathSize = MAX_PATH;
            if (!QueryFullProcessImageNameW(hProcess, 0, &path[0], &pathSize) || pathSize == 0) {
                return nullptr;
            }

            // Extract the icon. ExtractIconEx writes nothing when the file has no icon (returns 0) or on error
            // (UINT_MAX); the handles were uninitialised then and went on to fillIconsIfNeeded and DestroyIcon.
            HICON hIcon = NULL;
            HICON hIconLarge = NULL;
            UINT extracted = ExtractIconEx(&path[0], 0, &hIconLarge, &hIcon, 1);
            if (extracted == 0 || extracted == UINT_MAX || (hIcon == NULL && hIconLarge == NULL)) {
                return nullptr;
            }
            return std::make_shared<std::pair<HICON, HICON>>(std::make_pair(hIcon, hIconLarge));
        }


        bool filterOutWindow(const HWND &hwnd) {
            if (!::IsWindowVisible(hwnd)) {
                return true;
            }
            wchar_t class_name[MAX_PATH] = {0};
            const bool has_class = ::GetClassNameW(hwnd, class_name, MAX_PATH) > 0;
            // A classic console window (conhost: cmd, PowerShell, any console program) is WS_EX_LAYERED, for its
            // opacity setting, yet an ordinary app window that Alt+Tab lists: keep it. Without this SMBAct could
            // neither list nor place a console (ex style 0x000C0110 on Windows 10 22H2, 2026-11-12).
            const bool console = has_class && lstrcmpiW(class_name, L"ConsoleWindowClass") == 0;
            const DWORD kExcludedStyles = WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW |
                                          WS_EX_COMPOSITED | (console ? 0 : WS_EX_LAYERED);
            const DWORD kAllowedStyles =
                    WS_CHILD | WS_POPUP | WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU |
                    WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_THICKFRAME;
            DWORD ex_style = ::GetWindowLong(hwnd, GWL_EXSTYLE);
            DWORD style = ::GetWindowLong(hwnd, GWL_STYLE);
            // Filter out windows that have excluded styles or do not have allowed styles.
            if ((ex_style & kExcludedStyles) || !(style & kAllowedStyles)) {
                return true;
            }
            // Filter out windows that have certain class names.
            if (has_class &&
                (lstrcmpiW(class_name, L"WorkerW") == 0 ||
                 lstrcmpiW(class_name, L"Progman") == 0)) {
                return true;
            }
            // Filter out windows that have certain titles.
            wchar_t window_title[MAX_PATH] = {0};
            if (::GetWindowTextW(hwnd, window_title, MAX_PATH) &&
                lstrcmpiW(window_title, L"Program Manager") == 0) {
                return true;
            }
            return false;
        }

        std::vector<std::string> splitString(const std::string &str, char delimiter) {
            std::vector<std::string> result;
            size_t pos = 0;
            while (pos != std::string::npos) {
                size_t endPos = str.find(delimiter, pos);
                if (endPos == std::string::npos) {
                    result.push_back(str.substr(pos));
                    break;
                }
                result.push_back(str.substr(pos, endPos - pos));
                pos = endPos + 1;
            }
            return result;
        }

        RECT getRect(const Rect &rect) {
            RECT monitorRect;
            monitorRect.left = rect.X;
            monitorRect.right = rect.X + rect.Width;
            monitorRect.top = rect.Y;
            monitorRect.bottom = rect.Y + rect.Height;
            return monitorRect;
        }

    } // namespace Screen_Capture
} // namespace SL
