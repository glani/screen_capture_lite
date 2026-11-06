#include "GDIHelpers.h"
#include "ScreenCapture.h"
#include "internal/SCCommon.h"
#include "stringhelper.h"
#include <algorithm>

#define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers

#include <Windows.h>
#include <psapi.h>
#include <shellapi.h>
#include <ShlObj.h>
#include "internal/SCShared.h"
#include <UIAutomation.h>

#define LODEPNG_COMPILE_PNG
#define LODEPNG_COMPILE_DISK

#include "shared/png/lodepng.h"

#include "shared/base64/base64.h"

namespace SL {
    namespace Screen_Capture {

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


        std::shared_ptr<ShowingDesktopResult> ShowingDesktop(bool show) {
            auto result = std::make_shared<ShowingDesktopResult>();
            result->success = false;
            HWND progman = FindWindow(L"Progman", L"Program Manager");
            if (progman != NULL) {
                // Send the message to show / hide the desktop
                auto res = SendMessageTimeout(progman, show ? 0x052C : 0x001A, 0, 0, SMTO_NORMAL, 1000, NULL);
                result->success = res == 0;
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
                    LONG style = GetWindowLong(hwnd, GWL_STYLE);
                    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);

                    // Set new window style and size.
                    SetWindowLong(hwnd, GWL_STYLE,
                                  style | (WS_CAPTION | WS_THICKFRAME));
                    SetWindowLong(hwnd, GWL_EXSTYLE,
                                  exStyle | (WS_EX_DLGMODALFRAME |
                                             WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE));

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

        std::shared_ptr<Window> MaximizeHorz(const SingleWindowRequest &windowRequest, bool maximize) {
            // not supported
            return nullptr;
        }

        std::shared_ptr<Window> MaximizeVert(const SingleWindowRequest &windowRequest, bool maximize) {
            // not supported
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

        // Close all windows / terminate application
        std::shared_ptr<EmptyResult> CloseAllWindows(const SingleWindowRequest &windowRequest, bool force) {
            // not supported
            return std::make_shared<EmptyResult>();
        }

        // Close window
        std::shared_ptr<EmptyResult> CloseWindow(const SingleWindowRequest &windowRequest, bool force) {
            HWND hwnd = reinterpret_cast<HWND>(windowRequest.Handle);
            if (IsWindow(hwnd)) {
                if (!force) {
                    // SetForegroundWindow(hwnd);
                    SendMessage(hwnd, WM_CLOSE, 0, 0);
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
                if (IsZoomed(hwnd)) {
                    ShowWindow(hwnd, SW_RESTORE);
                }

                SetWindowPos(hwnd, nullptr, position.X, position.Y, size.Width, size.Height, SWP_NOZORDER);
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

        // Native window info
        std::shared_ptr<NativeWindow> NativeWindowInfo(char *data, const size_t sz) {
            return nullptr;
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

            auto *s = reinterpret_cast<SearchWindows *>(lParam);
            auto w = getWindow(hwnd, s->RetrieveIcons, s->Monitors);
            if (w) {
                s->Found->emplace_back(*w);
            }
            return TRUE;
        }

        bool bringToFront(HWND hwnd) {
            if (!SUCCEEDED(CoInitialize(nullptr))) {
            }

            IUIAutomation* uiAutomation;
            bool result = false;
            if (!SUCCEEDED(CoCreateInstance(__uuidof(CUIAutomation), NULL, CLSCTX_INPROC_SERVER, __uuidof(IUIAutomation), (void**)&uiAutomation)))
            {
                return result;
            }

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

            IUIAutomationElement* window = nullptr;
            if (SUCCEEDED(uiAutomation->ElementFromHandle(hwnd, &window)))
            {
                if (SUCCEEDED(window->SetFocus()))
                {
                    result = true;
                }

                window->Release();
            }

            uiAutomation->Release();
            CoUninitialize();
            return result;
        }

        std::shared_ptr<Window> getWindow(const HWND &hwnd, bool retrieveIcons, const std::vector<Monitor> *monitors) {

            if (filterOutWindow(hwnd)) {
                return nullptr;
            }

            Window w = {};
            w.Name = std::string();

            auto length = 4096;
            auto buffer = new wchar_t[length];
            std::unique_ptr<wchar_t> _(buffer);
            auto textlen = GetWindowText(hwnd, buffer, length);
            if (textlen >= 0) {
                w.Name = StringHelper::ws2s(std::wstring(buffer, textlen));
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
            ICONINFO infoIconProcess;
            GetIconInfo(hIcon, &infoIconProcess);
            std::shared_ptr<std::string> result;

            // Create bitmap from icon data
            BITMAP bmp = {};
            GetObject(infoIconProcess.hbmColor, sizeof(BITMAP), &bmp);
            BITMAPINFO bmi = {};
            bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bmi.bmiHeader.biWidth = bmp.bmWidth;
            bmi.bmiHeader.biHeight = -bmp.bmHeight;
            bmi.bmiHeader.biPlanes = 1;
            bmi.bmiHeader.biBitCount = bmp.bmBitsPixel;
            bmi.bmiHeader.biCompression = BI_RGB;
            BYTE *pBits = NULL;
            HBITMAP hBitmap = CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, (void **) &pBits, NULL, 0);
            HDC hMemDC = CreateCompatibleDC(NULL);
            HBITMAP hOldBitmap = (HBITMAP) SelectObject(hMemDC, hBitmap);
            if (DrawIconEx(hMemDC, 0, 0, hIcon, bmp.bmWidth, bmp.bmHeight, 0, NULL, DI_NORMAL)) {
                auto bytes = bmi.bmiHeader.biBitCount / 8;
                auto biSizeImage = bmp.bmWidth * bmp.bmHeight * bytes;
                // Iterate through the bitmap data and swap the red and blue components
                for (int i = 0; i < biSizeImage; i +=  bytes)
                {
                    BYTE temp = pBits[i];
                    pBits[i] = pBits[i + 2];
                    pBits[i + 2] = temp;
                }
                // Create image from bitmap data
                unsigned char *pngData;
                size_t outsize;
                unsigned int pngEncResult = lodepng_encode_memory(&pngData, &outsize, pBits, bmp.bmWidth, bmp.bmHeight,
                                                                  LCT_RGBA, 8);

                if (0 != pngEncResult) {
                    result = nullptr;
                } else {
                    auto iconStrValue = base64_encode(pngData, outsize, std::string("data:image/png;base64,"), false);
                    result = std::make_shared<std::string>(iconStrValue);
                }
                delete[] pngData;
            } else {
                result = nullptr;
            }

            // Clean up
            DeleteObject(infoIconProcess.hbmColor);
            DeleteObject(infoIconProcess.hbmMask);
            DeleteObject(hBitmap);
            SelectObject(hMemDC, hOldBitmap);
            DeleteDC(hMemDC);

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

            // Extract the icon
            HICON hIcon;
            HICON hIconLarge;
            ExtractIconEx(&path[0], 0, &hIconLarge, &hIcon, 1);
            return std::make_shared<std::pair<HICON, HICON>>(std::make_pair(hIcon, hIconLarge));
        }


        bool filterOutWindow(const HWND &hwnd) {
            if (!::IsWindowVisible(hwnd)) {
                return true;
            }
            const DWORD kExcludedStyles = WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW |
                                          WS_EX_COMPOSITED | WS_EX_LAYERED;
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
            wchar_t class_name[MAX_PATH] = {0};
            if (::GetClassNameW(hwnd, class_name, MAX_PATH) &&
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
