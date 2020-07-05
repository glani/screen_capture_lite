#include "GDIHelpers.h"
#include "ScreenCapture.h"
#include "internal/SCCommon.h"
#include "stringhelper.h"
#include <algorithm>

#define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers

#include <Windows.h>
#include <psapi.h>

namespace SL {
    namespace Screen_Capture {

        std::shared_ptr<Window> getWindow(const HWND &hwnd);

        struct srch {
            std::shared_ptr<std::vector<Window>> Found;
        };

        std::shared_ptr<Window> GetActiveWindow() {
            HWND hwndForeground = GetForegroundWindow();
            if (hwndForeground) {
                return getWindow(hwndForeground);
            }

            return std::shared_ptr<Window>();
        }


        std::shared_ptr<Window> getWindow(const HWND &hwnd) {
            Window w = {};
            w.Name = std::string();

            auto length = 4096;
            auto buffer = new wchar_t[length];
            std::unique_ptr<wchar_t> _(buffer);
            auto textlen = GetWindowText(hwnd, buffer, length);
            w.Name = StringHelper::ws2s(std::wstring(buffer, textlen));
            if (w.Name.size() == 0) {
                return std::shared_ptr<Window>();
            }
            w.Handle = reinterpret_cast<size_t>(hwnd);
            auto windowrect = GetWindowRect(hwnd);
            w.Position.x = windowrect.ClientRect.left;
            w.Position.y = windowrect.ClientRect.top;
            w.Size.x = windowrect.ClientRect.right - windowrect.ClientRect.left;
            w.Size.y = windowrect.ClientRect.bottom - windowrect.ClientRect.top;

            wchar_t filenameBuffer[4000];
            DWORD processId;
            GetWindowThreadProcessId(hwnd, &processId);
            auto hProcess = OpenProcess(
                    PROCESS_QUERY_LIMITED_INFORMATION,
                    FALSE,
                    processId);
            if (hProcess != NULL) {
//                DWORD len;
//                auto r = QueryFullProcessImageName(hProcess, 0, filenameBuffer, &len);
//                DWORD dwLastError = GetLastError();
//                if (r) {
//                    WindowAttribute attributeName;
//                    attributeName.Code = std::string("res_name");
//                    attributeName.Value = StringHelper::ws2s(std::wstring(filenameBuffer, len));
//                    w.Attributes.emplace_back(attributeName);
//                }
                auto len = GetProcessImageFileName(
                        hProcess,
                        filenameBuffer,
                        4000
                );
                if (len > 0) {
                    WindowAttribute attributeName;
                    attributeName.Code = std::string("res_name");
                    attributeName.Value = StringHelper::ws2s(std::wstring(filenameBuffer, len));
                    w.Attributes.emplace_back(attributeName);
                }
                CloseHandle(hProcess);
            }

            wchar_t classNameBuffer[4000];
            if (UINT len = GetClassName(hwnd, classNameBuffer, 4000) > 0) {
                WindowAttribute attributeName;
                attributeName.Code = std::string("res_class");
                attributeName.Value = StringHelper::ws2s(std::wstring(classNameBuffer, len));
                w.Attributes.emplace_back(attributeName);
            }

            BOOL visible = IsWindowVisible(hwnd);
            WindowAttribute attributeVisible;
            attributeVisible.Code = std::string("res_visible");
            attributeVisible.Value = visible ? "TRUE" : "FALSE";
            w.Attributes.emplace_back(attributeVisible);

            return std::make_shared<Window>(w);
        }

        int EnumWindowsProc(HWND hwnd, LPARAM lParam) {
            DWORD pid;
            GetWindowThreadProcessId(hwnd, &pid);
//            auto parent = GetParent(hwnd);
            if (/*parent == NULL || */pid == GetCurrentProcessId()) {
                return TRUE;
            }

            auto w = getWindow(hwnd);
            if (w) {
                srch *s = (srch *) lParam;
                s->Found->emplace_back(*w);
            }
            return TRUE;
        }

        std::shared_ptr<std::vector<Window>> GetWindows() {
            srch s;
            s.Found = std::make_shared<std::vector<Window>>();
            EnumWindows(EnumWindowsProc, (LPARAM) &s);
            return s.Found;
        }

    } // namespace Screen_Capture
} // namespace SL
