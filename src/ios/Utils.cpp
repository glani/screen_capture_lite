#include "Utils.h"

namespace SL {
namespace Screen_Capture {

        std::shared_ptr<std::string> getString(const CFStringRef &value)
        {
            CFIndex bufferSize = 2 * (CFStringGetLength(value) + 1);
            char *buffer = new char[bufferSize];

            std::string result;
            if (CFStringGetCString(value, buffer, bufferSize, kCFStringEncodingUTF8)) {
                auto result = std::make_shared<std::string>(buffer);
                delete[] buffer;
                return result;
            }
            delete[] buffer;
            return std::shared_ptr<std::string>();
        }
}
}