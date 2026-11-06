#include "X11MouseProcessor.h"

#include <assert.h>
#include <cstring>

namespace SL {
    namespace Screen_Capture {
        
        X11MouseProcessor::X11MouseProcessor()
        {
            
          
        }

        X11MouseProcessor::~X11MouseProcessor()
        {
            if (SelectedDisplay) {
                XCloseDisplay(SelectedDisplay);
            }
        }
        DUPL_RETURN X11MouseProcessor::Init(std::shared_ptr<Thread_Data> data) {
            auto ret = DUPL_RETURN::DUPL_RETURN_SUCCESS;
            Data = data;
            SelectedDisplay = XOpenDisplay(NULL);
            if (!SelectedDisplay) {
                return DUPL_RETURN::DUPL_RETURN_ERROR_EXPECTED;
            }
            RootWindow = DefaultRootWindow(SelectedDisplay);
            if (!RootWindow) {
                return DUPL_RETURN::DUPL_RETURN_ERROR_EXPECTED;
            } 
            return ret;
        }
        //
        // Process a given frame and its metadata
        //
        DUPL_RETURN X11MouseProcessor::ProcessFrame()
        {
            auto Ret = DUPL_RETURN_SUCCESS;
    
            auto img = XFixesGetCursorImage(SelectedDisplay);
            if (!img) {
                // no XFixes on this server, or the request failed (was dereferenced as is); as on Windows (GetCursorInfo)
                return DUPL_RETURN_ERROR_EXPECTED;
            }

            if (sizeof(img->pixels[0]) == 8) {//if the pixelstride is 64 bits.. scale down to 32bits
                auto pixels = (int *)img->pixels;
                for (auto i = 0; i < img->width * img->height; ++i) {
                    pixels[i] = pixels[i * 2];
                }
            }
            ImageRect imgrect;
            imgrect.left = imgrect.top = 0;
            imgrect.right = img->width;
            imgrect.bottom = img->height;
            const int newsize = static_cast<int>(sizeof(ImageBGRA)) * imgrect.right * imgrect.bottom;
            // Both buffers hold ImageBufferSize bytes, so it has to follow a reallocation. It did not: once the first
            // frame had sized them for a 16x16 cursor (1024 bytes), a 24x24 one (2304 bytes, within the initial 32x32
            // capacity) was copied into them (heap overflow), and a cursor above 32x32 reallocated both on every frame.
            if (newsize > ImageBufferSize || !OldImageBuffer) {
                ImageBuffer = std::make_unique<unsigned char[]>(newsize);
                OldImageBuffer = std::make_unique<unsigned char[]>(newsize);
                ImageBufferSize = newsize;
                LastWidth = LastHeight = -1; // nothing sent from these buffers yet
            }

            memcpy(ImageBuffer.get(), img->pixels, newsize);

                // Get the mouse cursor position
            int x, y, root_x, root_y = 0;
            unsigned int mask = 0;
            XID child_win, root_win;
            XQueryPointer(SelectedDisplay, RootWindow, &child_win, &root_win, &root_x, &root_y, &x, &y, &mask);

            // the hotspot is read before XFree(img): it was read from the freed image (use after free)
            const Point hotSpot{static_cast<int>(img->xhot), static_cast<int>(img->yhot)};
            XFree(img);

            MousePoint mousepoint = {};
            mousepoint.Position = Point{x, y};
            mousepoint.HotSpot = hotSpot;

            if (Data->ScreenCaptureData.OnMouseChanged) {

                // the image just read, as on Windows (it was OldImageBuffer: the cursor sent last time, all zeros on
                // the first frame and after a reallocation). Rows are packed: CreateImage's second argument is the
                // distance to the next row (GotoNextRow), so 0 made Extract() copy the first row into every row.
                auto wholeimg = CreateImage(imgrect, imgrect.right * static_cast<int>(sizeof(ImageBGRA)),
                                            reinterpret_cast<const ImageBGRA*>(ImageBuffer.get()));
                wholeimg.isContiguous = true;

                //if the mouse image is different (or its size), send the new image and swap the data
                if (imgrect.right != LastWidth || imgrect.bottom != LastHeight || memcmp(ImageBuffer.get(), OldImageBuffer.get(), newsize) != 0) {
                    Data->ScreenCaptureData.OnMouseChanged(&wholeimg, mousepoint);
                    std::swap(ImageBuffer, OldImageBuffer);
                    LastWidth = imgrect.right;
                    LastHeight = imgrect.bottom;
                }
                else if(Last_x != x || Last_y != y){
                    Data->ScreenCaptureData.OnMouseChanged(nullptr, mousepoint);
                }
                Last_x = x;
                Last_y = y;
            }
            return Ret;
        }

    }
}