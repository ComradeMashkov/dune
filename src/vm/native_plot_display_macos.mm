#include "native_plot_display.hpp"

#import <Cocoa/Cocoa.h>
#import <WebKit/WebKit.h>

@interface DunePlotWindowDelegate : NSObject <NSWindowDelegate>
@end

@interface DunePlotOverlayView : NSView
@property(nonatomic) BOOL gridVisible;
@property(nonatomic) BOOL rulerVisible;
@property(nonatomic) NSPoint cursorPoint;
@property(nonatomic) BOOL hasCursor;
@end

@interface DunePlotController : NSObject
@property(nonatomic, assign) WKWebView* webView;
@property(nonatomic, assign) DunePlotOverlayView* overlay;
@property(nonatomic, copy) NSString* svg;
@property(nonatomic) CGFloat zoom;
- (instancetype)initWithWebView:(WKWebView*)webView overlay:(DunePlotOverlayView*)overlay svg:(NSString*)svg;
- (void)toggleGrid:(id)sender;
- (void)zoomIn:(id)sender;
- (void)zoomOut:(id)sender;
- (void)resetZoom:(id)sender;
- (void)toggleRuler:(id)sender;
- (void)saveSVG:(id)sender;
@end

@implementation DunePlotWindowDelegate
- (void)windowWillClose:(NSNotification*)notification {
    (void)notification;
    [NSApp stop:nil];
    NSEvent* event = [NSEvent otherEventWithType:NSEventTypeApplicationDefined
                                        location:NSZeroPoint
                                   modifierFlags:0
                                       timestamp:0
                                    windowNumber:0
                                         context:nil
                                         subtype:0
                                           data1:0
                                           data2:0];
    [NSApp postEvent:event atStart:NO];
}
@end

@implementation DunePlotOverlayView
- (instancetype)initWithFrame:(NSRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        _gridVisible = NO;
        _rulerVisible = NO;
        _hasCursor = NO;
        [self setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    }
    return self;
}

- (BOOL)isOpaque {
    return NO;
}

- (void)updateTrackingAreas {
    [super updateTrackingAreas];
    for (NSTrackingArea* area in [self trackingAreas]) {
        [self removeTrackingArea:area];
    }
    NSTrackingAreaOptions options =
        NSTrackingMouseMoved | NSTrackingMouseEnteredAndExited | NSTrackingActiveAlways | NSTrackingInVisibleRect;
    [self addTrackingArea:[[NSTrackingArea alloc] initWithRect:NSZeroRect options:options owner:self userInfo:nil]];
}

- (void)mouseMoved:(NSEvent*)event {
    _cursorPoint = [self convertPoint:[event locationInWindow] fromView:nil];
    _hasCursor = YES;
    [self setNeedsDisplay:YES];
}

- (void)mouseExited:(NSEvent*)event {
    (void)event;
    _hasCursor = NO;
    [self setNeedsDisplay:YES];
}

- (void)drawRect:(NSRect)dirtyRect {
    (void)dirtyRect;
    if (_gridVisible) {
        [[NSColor colorWithCalibratedWhite:0.55 alpha:0.22] setStroke];
        NSBezierPath* grid = [NSBezierPath bezierPath];
        [grid setLineWidth:1.0];
        const CGFloat step = 50.0;
        for (CGFloat x = 0.5; x < NSWidth([self bounds]); x += step) {
            [grid moveToPoint:NSMakePoint(x, 0.0)];
            [grid lineToPoint:NSMakePoint(x, NSHeight([self bounds]))];
        }
        for (CGFloat y = 0.5; y < NSHeight([self bounds]); y += step) {
            [grid moveToPoint:NSMakePoint(0.0, y)];
            [grid lineToPoint:NSMakePoint(NSWidth([self bounds]), y)];
        }
        [grid stroke];
    }

    if (_rulerVisible && _hasCursor) {
        [[NSColor colorWithCalibratedRed:0.10 green:0.24 blue:0.85 alpha:0.80] setStroke];
        NSBezierPath* crosshair = [NSBezierPath bezierPath];
        [crosshair setLineWidth:1.0];
        [crosshair moveToPoint:NSMakePoint(_cursorPoint.x + 0.5, 0.0)];
        [crosshair lineToPoint:NSMakePoint(_cursorPoint.x + 0.5, NSHeight([self bounds]))];
        [crosshair moveToPoint:NSMakePoint(0.0, _cursorPoint.y + 0.5)];
        [crosshair lineToPoint:NSMakePoint(NSWidth([self bounds]), _cursorPoint.y + 0.5)];
        [crosshair stroke];

        NSString* label = [NSString stringWithFormat:@"x %.0f  y %.0f", _cursorPoint.x, NSHeight([self bounds]) - _cursorPoint.y];
        NSDictionary* attributes = @{
            NSFontAttributeName : [NSFont systemFontOfSize:11.0],
            NSForegroundColorAttributeName : [NSColor whiteColor],
        };
        NSSize size = [label sizeWithAttributes:attributes];
        NSRect box = NSMakeRect(_cursorPoint.x + 8.0, _cursorPoint.y + 8.0, size.width + 12.0, size.height + 6.0);
        [[NSColor colorWithCalibratedWhite:0.05 alpha:0.78] setFill];
        [[NSBezierPath bezierPathWithRoundedRect:box xRadius:4.0 yRadius:4.0] fill];
        [label drawAtPoint:NSMakePoint(NSMinX(box) + 6.0, NSMinY(box) + 3.0) withAttributes:attributes];
    }
}
@end

@implementation DunePlotController
- (instancetype)initWithWebView:(WKWebView*)webView overlay:(DunePlotOverlayView*)overlay svg:(NSString*)svg {
    self = [super init];
    if (self) {
        _webView = webView;
        _overlay = overlay;
        _svg = [svg copy];
        _zoom = 1.0;
    }
    return self;
}

- (void)applyZoom {
    [_webView setMagnification:_zoom centeredAtPoint:NSMakePoint(0.0, 0.0)];
}

- (void)toggleGrid:(id)sender {
    (void)sender;
    [_overlay setGridVisible:![_overlay gridVisible]];
    [_overlay setNeedsDisplay:YES];
}

- (void)zoomIn:(id)sender {
    (void)sender;
    _zoom = _zoom * 1.25;
    if (_zoom > 8.0) {
        _zoom = 8.0;
    }
    [self applyZoom];
}

- (void)zoomOut:(id)sender {
    (void)sender;
    _zoom = _zoom / 1.25;
    if (_zoom < 0.125) {
        _zoom = 0.125;
    }
    [self applyZoom];
}

- (void)resetZoom:(id)sender {
    (void)sender;
    _zoom = 1.0;
    [self applyZoom];
}

- (void)toggleRuler:(id)sender {
    (void)sender;
    [_overlay setRulerVisible:![_overlay rulerVisible]];
    [_overlay setNeedsDisplay:YES];
}

- (void)saveSVG:(id)sender {
    (void)sender;
    NSSavePanel* panel = [NSSavePanel savePanel];
    [panel setNameFieldStringValue:@"dune-plot.svg"];
    if ([panel runModal] != NSModalResponseOK) {
        return;
    }

    NSError* error = nil;
    if (![_svg writeToURL:[panel URL] atomically:YES encoding:NSUTF8StringEncoding error:&error]) {
        NSAlert* alert = [[NSAlert alloc] init];
        [alert setMessageText:@"Could not save SVG"];
        [alert setInformativeText:error == nil ? @"Unknown write error." : [error localizedDescription]];
        [alert runModal];
    }
}
@end

NSButton* makePlotButton(NSString* title, CGFloat x, CGFloat width, id target, SEL action) {
    NSButton* button = [NSButton buttonWithTitle:title target:target action:action];
    [button setFrame:NSMakeRect(x, 8.0, width, 28.0)];
    [button setBezelStyle:NSBezelStyleRounded];
    return button;
}

namespace dune {

NativePlotDisplayResult show_native_plot_svg(const std::string& svg) {
    if (svg.empty()) {
        return {false, "native plot display backend received an empty SVG"};
    }

    @autoreleasepool {
        NSApplication* app = [NSApplication sharedApplication];
        [app setActivationPolicy:NSApplicationActivationPolicyRegular];

        const NSRect frame = NSMakeRect(0, 0, 860, 540);
        NSWindow* window =
            [[NSWindow alloc] initWithContentRect:frame
                                        styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                                                  NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable
                                          backing:NSBackingStoreBuffered
                                            defer:NO];
        [window setTitle:@"Dune Plot"];
        [window setAcceptsMouseMovedEvents:YES];

        NSView* content = [[NSView alloc] initWithFrame:frame];
        [content setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];

        const CGFloat toolbarHeight = 44.0;
        NSView* toolbar = [[NSView alloc] initWithFrame:NSMakeRect(0, NSHeight(frame) - toolbarHeight, NSWidth(frame), toolbarHeight)];
        [toolbar setAutoresizingMask:NSViewWidthSizable | NSViewMinYMargin];

        NSView* plotArea = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, NSWidth(frame), NSHeight(frame) - toolbarHeight)];
        [plotArea setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];

        WKWebView* view = [[WKWebView alloc] initWithFrame:[plotArea bounds]];
        [view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];

        NSData* data = [NSData dataWithBytes:svg.data() length:svg.size()];
        [view loadData:data
              MIMEType:@"image/svg+xml"
 characterEncodingName:@"UTF-8"
               baseURL:[NSURL URLWithString:@"about:blank"]];

        DunePlotOverlayView* overlay = [[DunePlotOverlayView alloc] initWithFrame:[plotArea bounds]];
        [plotArea addSubview:view];
        [plotArea addSubview:overlay];

        NSString* svgString = [[NSString alloc] initWithBytes:svg.data() length:svg.size() encoding:NSUTF8StringEncoding];
        DunePlotController* controller = [[DunePlotController alloc] initWithWebView:view overlay:overlay svg:svgString];
        [toolbar addSubview:makePlotButton(@"Grid", 12.0, 72.0, controller, @selector(toggleGrid:))];
        [toolbar addSubview:makePlotButton(@"Zoom +", 92.0, 82.0, controller, @selector(zoomIn:))];
        [toolbar addSubview:makePlotButton(@"Zoom -", 182.0, 82.0, controller, @selector(zoomOut:))];
        [toolbar addSubview:makePlotButton(@"Reset", 272.0, 74.0, controller, @selector(resetZoom:))];
        [toolbar addSubview:makePlotButton(@"Ruler", 354.0, 76.0, controller, @selector(toggleRuler:))];
        [toolbar addSubview:makePlotButton(@"Save SVG", 438.0, 96.0, controller, @selector(saveSVG:))];

        DunePlotWindowDelegate* delegate = [[DunePlotWindowDelegate alloc] init];
        [window setDelegate:delegate];
        [content addSubview:plotArea];
        [content addSubview:toolbar];
        [window setContentView:content];
        [window center];
        [window makeKeyAndOrderFront:nil];
        [app activateIgnoringOtherApps:YES];
        [app run];

        return {true, "native plot window closed"};
    }
}

} // namespace dune
