#include "xcb/XcbInitializer.h"
// init

namespace SL {
namespace Screen_Capture {
    void Init()
    {
        auto &xcbInstance = XcbInitializer::getInstance();
        xcbInstance.init();
    }
}
}