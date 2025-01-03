#if defined(Hiro_Application)

@implementation CocoaDelegate : NSObject

-(NSApplicationTerminateReply) applicationShouldTerminate:(NSApplication*)sender {
  using hiro::Application;
  if(Application::state().cocoa.onQuit) Application::Cocoa::doQuit();
  else Application::quit();
  return NSTerminateCancel;
}

-(BOOL) applicationShouldHandleReopen:(NSApplication*)application hasVisibleWindows:(BOOL)flag {
  using hiro::Application;
  if(Application::state().cocoa.onActivate) Application::Cocoa::doActivate();
  return NO;
}

-(void) run/*:(NSTimer*)timer*/ {
  using hiro::Application;
  if(Application::state().onMain) Application::doMain();
}

-(BOOL)application:(NSApplication*)sender openFile:(NSString*)filename {
  BOOL isDirectory = NO;
  [[NSFileManager defaultManager] fileExistsAtPath:filename isDirectory:&isDirectory];
  if(isDirectory) filename = [filename stringByAppendingString:@"/"];
  hiro::Application::doOpenFile(filename.UTF8String);
  return YES;
}

@end

CocoaDelegate* cocoaDelegate = nullptr;
NSTimer* applicationTimer = nullptr;
dispatch_queue_t aresQueue = nullptr;
dispatch_source_t aresTimerSource = nullptr;

namespace hiro {

auto pApplication::exit() -> void {
  quit();
  ::exit(EXIT_SUCCESS);
}

auto pApplication::modal() -> bool {
  return Application::state().modal > 0;
}

auto pApplication::run() -> void {
  if(Application::state().onMain) {
    //applicationTimer = [NSTimer scheduledTimerWithTimeInterval:0.0 target:cocoaDelegate selector:@selector(run:) userInfo:nil repeats:YES];
    
    dispatch_source_set_timer(aresTimerSource, DISPATCH_TIME_NOW, 0.001 * NSEC_PER_SEC, 0.001 * NSEC_PER_SEC);
    dispatch_source_set_event_handler(aresTimerSource, ^{
      [cocoaDelegate run];
    });
    dispatch_activate(aresTimerSource);

    //below line is needed to run application during window resize; however it has a large performance penalty on the resize smoothness
    //[[NSRunLoop currentRunLoop] addTimer:applicationTimer forMode:NSEventTrackingRunLoopMode];
  }
  [[NSUserDefaults standardUserDefaults] registerDefaults:@{
    //@"NO" is not a mistake; the value really needs to be a string
    @"NSTreatUnknownArgumentsAsOpen": @"NO"
  }];

  @autoreleasepool {
    [NSApp run];
  }
}

auto pApplication::pendingEvents() -> bool {
  bool result = false;
  @autoreleasepool {
    NSEvent* event = [NSApp nextEventMatchingMask:NSEventMaskAny untilDate:[NSDate distantPast] inMode:NSDefaultRunLoopMode dequeue:NO];
    if(event != nil) result = true;
  }
  return result;
}

auto pApplication::processEvents() -> void {
  @autoreleasepool {
    while(!Application::state().quit) {
      NSEvent* event = [NSApp nextEventMatchingMask:NSEventMaskAny untilDate:[NSDate distantPast] inMode:NSDefaultRunLoopMode dequeue:YES];
      if(event == nil) break;
      [NSApp sendEvent:event];
    }
  }
}

auto pApplication::quit() -> void {
  @autoreleasepool {
    [applicationTimer invalidate];
    dispatch_source_cancel(aresTimerSource);
    aresTimerSource = nullptr;
    [NSApp stop:nil];
    NSEvent* event = [NSEvent otherEventWithType:NSEventTypeApplicationDefined location:NSMakePoint(0, 0) modifierFlags:0 timestamp:0.0 windowNumber:0 context:nil subtype:0 data1:0 data2:0];
    [NSApp postEvent:event atStart:true];
  }
}

auto pApplication::getQueue() -> dispatch_queue_t {
  return aresQueue;
}

auto pApplication::setScreenSaver(bool screenSaver) -> void {
  static IOPMAssertionID powerAssertion = kIOPMNullAssertionID;  //default is enabled

  //do nothing if state has not been changed
  if(screenSaver == (powerAssertion == kIOPMNullAssertionID)) return;

  if(screenSaver) {
    IOPMAssertionRelease(powerAssertion);
    powerAssertion = kIOPMNullAssertionID;
  } else {
    string reason = {Application::state().name, " screensaver suppression"};
    NSString* assertionName = [NSString stringWithUTF8String:reason.data()];
    if(IOPMAssertionCreateWithName(kIOPMAssertionTypePreventUserIdleDisplaySleep,
                                   kIOPMAssertionLevelOn, (__bridge CFStringRef)assertionName, &powerAssertion
    ) != kIOReturnSuccess) {
      powerAssertion = kIOPMNullAssertionID;
    }
  }
}

auto pApplication::initialize() -> void {
  @autoreleasepool {
    [NSApplication sharedApplication];
    cocoaDelegate = [[CocoaDelegate alloc] init];
    aresQueue = dispatch_queue_create("com.ares.worker", DISPATCH_QUEUE_SERIAL);
    aresTimerSource = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, aresQueue);
    [NSApp setDelegate:cocoaDelegate];
  }
}

}

#endif
