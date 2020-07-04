#include <cstdio>
#include "NSWorkspaceWrapper.h"

namespace SL {
    namespace Screen_Capture {
        size_t NSWorkspaceWrapper::determineFrontmostApplicationPID() {
            return determineFrontmostApplicationPIDWrap(this->NSWorkspaceWrapperImpl_);
        }
    }
}