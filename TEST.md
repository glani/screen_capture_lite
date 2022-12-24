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
