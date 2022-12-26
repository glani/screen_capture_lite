#pragma once
#include <CoreFoundation/CoreFoundation.h>
#include <string>
#include <memory>
#include <algorithm>


namespace SL {
namespace Screen_Capture {

    std::shared_ptr<std::string> getString(const CFStringRef &value);
}
}