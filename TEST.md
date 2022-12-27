# Test


````
- (void)test
  {
      CFTypeRef value;
      //    CFArrayRef names;
      NSArray *attrNames;
      AXError result;
      CGRect rect = [self rectOfElement];
      BOOL trusted = AXIsProcessTrusted();
      result = AXUIElementCopyAttributeNames(_underlyingElement, (CFArrayRef)&attrNames);
      NSString *v21 = [self stringValueOfAttribute:kAXTitleAttribute];
      NSString *vAXSubroleAttribute = [self stringValueOfAttribute:kAXSubroleAttribute];
      NSString *vAXRoleAttribute = [self stringValueOfAttribute:kAXRoleAttribute];
      NSLog(@"kAXTitleAttribute %@", v21);
    
      for (id name in attrNames) {
          NSLog(@"%@", name);
          if (AXUIElementCopyAttributeValue(_underlyingElement, name, (CFTypeRef *)&result) == kAXErrorSuccess) {
          NSString *value = [self stringValueOfAttribute:name];
          NSLog(@"success %@ => %@", name, value);
          }
      }
    
      SCAccessibilityElementWindows *res = [self retrieveAllWindows];
    
      for (id elem in [res getWindowsList]) {
          NSLog(@"===========================");
          [elem test];
          break;
      }
  }

````


````
        Rect NormalizeWidth() const
        {
            if (Width >= 0)
                return *this;
            Rect result = Rect(X + Width, Y, -Width, Height);
            return result;
        }

        Rect NormalizeHeight() const
        {
            if (Height >= 0)
                return *this;
            Rect result = Rect(X, Y + Height, Width, -Height);
            return result;
        }

        Rect Normalize() const
        {
            return this->NormalizeWidth().NormalizeHeight();
        }
        std::shared_ptr<Rect> GetIntersection(const Rect &rect)
        {
            const Rect &rectangle1 = this->Normalize();
            const Rect &rectangle2 = rect.Normalize();

            if (rectangle1.IntersectsWith(rectangle2)) {
                int left = std::max(rectangle1.X, rectangle2.X);
                int width = std::min(rectangle1.X + rectangle1.Width, rectangle2.X + rectangle2.Width) - left;
                int top = std::max(rectangle1.Y, rectangle2.Y);
                int height = std::min(rectangle1.Y + rectangle1.Height, rectangle2.Y + rectangle2.Height) - top;
                return std::make_shared<Rect>(Rect{left, top, width, height});
            }

            return NULL;
        }

        bool RectanglesIntersect(const Rect& rectangle1Value, const Rect& rectangle2Value) const
        {
            const auto& rectangle1 = rectangle1Value.Normalize();
            const auto& rectangle2 = rectangle2Value.Normalize();

            if (rectangle2.X >= rectangle1.X + rectangle1.Width)
                return false;
            if (rectangle2.X + rectangle2.Width <= rectangle1.X)
                return false;

            if (rectangle2.Y >= rectangle1.Y + rectangle1.Height)
                return false;
            if (rectangle2.Y + rectangle2.Height <= rectangle1.Y)
                return false;

            return true;
        }

        bool IntersectsWith(const Rect& rectangle) const
        {
            return RectanglesIntersect(*this, rectangle);
        }

````

````
        //        auto *display = XOpenDisplay(NULL);

        //            xcb_connection_t         *xcbConnection;
        //            xcbConnection = xcb_connect (NULL, NULL);
        //            auto setup = xcb_get_setup(xcbConnection);
        xcb_screen_iterator_t it = xcb_setup_roots_iterator(xcbInstance.setup());
        int xcbScreenNumber = 0;
        while (it.rem) {
            xcb_screen_t *xcbScreen = it.data;

            auto screens = XCB_REPLY(xcb_xinerama_query_screens, xcbInstance.xcbConnection());
            //
            //                auto screens = xcb_xinerama_query_screens(xcbConnectionValue);
            if (screens) {
                xcb_xinerama_screen_info_iterator_t it = xcb_xinerama_query_screens_screen_info_iterator(screens.get());
                while (it.rem) {
                    xcb_xinerama_screen_info_t *screen_info = it.data;
                    ////                        QXcbScreen *screen = new QXcbScreen(this, virtualDesktop,
                    ////                                                            XCB_NONE, nullptr,
                    ////                                                            screen_info, it.index);
                    ////                        siblings << screen;
                    ////                        m_screens << screen;
                    xcb_xinerama_screen_info_next(&it);
                }
                ////                    xcb_xinerama_screen_info_iterator_t it = xcb_xinerama_query_screens_screen_info_iterator(screens.get());
                ////                    while (it.rem) {
                ////                        xcb_xinerama_screen_info_t *screen_info = it.data;
                ////                        QXcbScreen *screen = new QXcbScreen(this, virtualDesktop,
                ////                                                            XCB_NONE, nullptr,
                ////                                                            screen_info, it.index);
                ////                        siblings << screen;
                ////                        m_screens << screen;
                ////                        xcb_xinerama_screen_info_next(&it);
                ////                    }
            }

            xcb_screen_next(&it);
            ++xcbScreenNumber;
        }
        Screen *screen = XDefaultScreenOfDisplay(display);
//                    xcb_atom a1 = XInternAtom(display, "_NET_WORKAREA", true);
        auto workArea1 = XCB_REPLY_UNCHECKED(xcb_get_property, xcbInstance.xcbConnection(), false, screen->root,
                                            xcbInstance.atom(XcbAtom::_NET_WM_STRUT_PARTIAL), XCB_ATOM_CARDINAL, 0, 1024);

        auto workArea = XCB_REPLY_UNCHECKED(xcb_get_property, xcbInstance.xcbConnection(), false, screen->root,
                                            xcbInstance.atom(XcbAtom::_NET_WORKAREA), XCB_ATOM_CARDINAL, 0, 1024);
        if (workArea && workArea->type == XCB_ATOM_CARDINAL && workArea->format == 32 && workArea->value_len >= 4) {
            // If workArea->value_len > 4, the remaining ones seem to be for WM's virtual desktops
            // (don't mess with QXcbVirtualDesktop which represents an X screen).
            // But QScreen doesn't know about that concept.  In reality there could be a
            // "docked" panel (with _NET_WM_STRUT_PARTIAL atom set) on just one desktop.
            // But for now just assume the first 4 values give us the geometry of the
            // "work area", AKA "available geometry"
            uint32_t *geom = (uint32_t *)xcb_get_property_value(workArea.get());
            uint64_t *geom1 = (uint64_t *)xcb_get_property_value(workArea.get());
            int tx = geom[0];
            int ty = geom[1];
            int width = geom[2];
            int height = geom[3];
            //                r = QRect(geom[0], geom[1], geom[2], geom[3]);
            //                Atom actualType;
            int h = 0;
        }
        else {
            //                r = QRect(QPoint(), size());
        }
        //
        //            int format;
        //            unsigned long numItems, bytesAfter;
        //            unsigned char *data1 = NULL;

        //            XGetWindowProperty(display, screen->root, a1, 0, 1, False, XA_CARDINAL, &actualType, &format, &numItems, &bytesAfter, &data1);
        int h = 0;


                            auto wms = XCB_REPLY_UNCHECKED(xcb_get_property, xcbInstance.xcbConnection(), false, window,
                                                         xcbInstance.atom(XcbAtom::_NET_WM_STRUT), XCB_ATOM_CARDINAL, 0, 1024);

                    auto wmsp = XCB_REPLY_UNCHECKED(xcb_get_property, xcbInstance.xcbConnection(), false, window,
                                                        xcbInstance.atom(XcbAtom::_NET_WM_STRUT_PARTIAL), XCB_ATOM_CARDINAL, 0, 1024);
                    auto workArea = XCB_REPLY_UNCHECKED(xcb_get_property, xcbInstance.xcbConnection(), false, window,
                                                        xcbInstance.atom(XcbAtom::_NET_WM_STATE_FULLSCREEN), XCB_ATOM_CARDINAL, 0, 1024);
                    if (workArea && /*workArea->type == XCB_ATOM_CARDINAL && workArea->format == 32 && */workArea->value_len ) {
                        int j = 0;
                    }
                    if (wmsp && /*wmsp->type == XCB_ATOM_CARDINAL && wmsp->format == 32 &&*/ wmsp->value_len ) {
                        int j = 0;
                    }
                    if (wms && /*wms->type == XCB_ATOM_CARDINAL && wms->format == 32 && */wms->value_len ) {
                        int j = 0;
                    }


````

