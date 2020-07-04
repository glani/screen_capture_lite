//
// Created by Denis Esakov on 04.07.20.
//

#ifndef SIMPLE_SCREENSHOT_MAKER_NSWORKSPACEWRAPPER_H
#define SIMPLE_SCREENSHOT_MAKER_NSWORKSPACEWRAPPER_H

namespace SL {
    namespace Screen_Capture {
        struct NSWorkspaceWrapperImpl;

        NSWorkspaceWrapperImpl *CreateNSWorkspaceWrapperImpl();

        void DestroyNSWorkspaceWrapperImpl(NSWorkspaceWrapperImpl *p);

        size_t determineFrontmostApplicationPIDWrap(NSWorkspaceWrapperImpl *p);

        class NSWorkspaceWrapper {
            NSWorkspaceWrapperImpl *NSWorkspaceWrapperImpl_ = nullptr;
        public:
            NSWorkspaceWrapper() {
                NSWorkspaceWrapperImpl_ = CreateNSWorkspaceWrapperImpl();
            }

            ~NSWorkspaceWrapper() {
                DestroyNSWorkspaceWrapperImpl(NSWorkspaceWrapperImpl_);
            }

            size_t determineFrontmostApplicationPID();
        };
    }
}

#endif //SIMPLE_SCREENSHOT_MAKER_NSWORKSPACEWRAPPER_H
