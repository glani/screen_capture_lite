#include "NSWorkspaceWrapper.h"
#include <CoreFoundation/CoreFoundation.h>
#include <map>

namespace SL {
    namespace Screen_Capture {

        CFDictionaryRef NSWorkspaceWrapper::determineFrontmostApplication() {
            return determineFrontmostApplicationWrap(this->NSWorkspaceWrapperImpl_);
        }

        void NSWorkspaceWrapper::releaseFrontmostApplication(CFDictionaryRef query) {
            releaseFrontmostApplicationWrap(this->NSWorkspaceWrapperImpl_, query);
        }

        std::shared_ptr<std::map<int, struct ApplicationData>> NSWorkspaceWrapper::determineApplicationValues() {
            return determineApplicationValuesWrap(this->NSWorkspaceWrapperImpl_);
        }

    } // namespace Screen_Capture
} // namespace SL