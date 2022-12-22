#include "NSScreenWrapper.h"

namespace SL {
namespace Screen_Capture {
    std::shared_ptr<std::vector<struct ScreenData>> Screen_Capture::NSScreenWrapper::retrieveScreens() {
        return retrieveScreensWrap(this->NSScreenWrapperImpl_);
    }
} // namespace Screen_Capture
} // namespace SL
