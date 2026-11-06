#include "ScreenCapture.h"
#include "internal/ThreadManager.h"
#include "X11FrameProcessor.h"
#include "X11MouseProcessor.h"

namespace SL{
    namespace Screen_Capture{	
        void RunCaptureMouse(std::shared_ptr<Thread_Data> data) {
            TryCaptureMouse<X11MouseProcessor>(data);
        }
        void RunCaptureMonitor(std::shared_ptr<Thread_Data> data, Monitor monitor){
            TryCaptureMonitor<X11FrameProcessor>(data, monitor);
        }
        void RunCaptureWindow(std::shared_ptr<Thread_Data> data, Window window){
            // Window.Position/Size is the visible frame (GetWindows.cpp addWindow), but X11FrameProcessor grabs the
            // client window: capture with the client geometry (visible frame + Frame), as before.
            window.Position.X += window.Frame.LeftFrame;
            window.Position.Y += window.Frame.TopFrame;
            window.Size.Width -= window.Frame.LeftFrame + window.Frame.RightFrame;
            window.Size.Height -= window.Frame.TopFrame + window.Frame.BottomFrame;
            TryCaptureWindow<X11FrameProcessor>(data, window);
        }
    }
}

  
    
