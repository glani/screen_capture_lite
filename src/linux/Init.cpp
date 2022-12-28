#include "xcb/XcbInitializer.h"

namespace SL {
namespace Screen_Capture {
    void InitWindows()
    {
        auto &xcbInstance = XcbInitializer::getInstance();
        xcbInstance.init();
    }
}
}