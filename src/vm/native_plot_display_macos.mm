#include "native_plot_display.hpp"

#import <Cocoa/Cocoa.h>
#import <WebKit/WebKit.h>

#include <cstring>
#include <string>

// A polished native viewer for Dune plots/canvases on macOS.
//
// The chart is a deterministic SVG produced by the stdlib. Rather than dropping
// the raw SVG into a bare WKWebView (which pins it top-left and offers no
// interaction), we wrap it in a small self-contained HTML document that centres
// the chart on a themed backdrop and adds Matplotlib-style tooling — grid,
// pan/zoom, crosshair with a live coordinate readout — implemented in JS so it
// tracks the transformed content correctly. A native unified toolbar drives
// that JS through `evaluateJavaScript:`, and the page reports the cursor back
// through a script-message handler so the status bar can display coordinates.
// Everything is inline; no network or external assets are used.

namespace {

// Wraps deterministic SVG in an interactive, theme-aware HTML shell. The SVG is
// injected verbatim between two static halves so its own `%`/`{}` characters are
// never treated as format specifiers.
std::string build_document(const std::string& svg) {
    static const char* kHead = R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
  :root {
    --bg: #eef1f6;
    --bg2: #dfe4ee;
    --card: #ffffff;
    --card-shadow: rgba(15, 23, 42, 0.18);
    --ink: #0f172a;
    --muted: #64748b;
    --accent: #2563eb;
    --grid: rgba(100, 116, 139, 0.20);
    --hair: rgba(37, 99, 235, 0.85);
  }
  :root[data-theme="dark"] {
    --bg: #17181d;
    --bg2: #101116;
    --card-shadow: rgba(0, 0, 0, 0.55);
    --ink: #e5e7eb;
    --muted: #94a3b8;
    --accent: #60a5fa;
    --grid: rgba(148, 163, 184, 0.16);
    --hair: rgba(96, 165, 250, 0.9);
  }
  @media (prefers-color-scheme: dark) {
    :root:not([data-theme="light"]) {
      --bg: #17181d;
      --bg2: #101116;
      --card-shadow: rgba(0, 0, 0, 0.55);
      --ink: #e5e7eb;
      --muted: #94a3b8;
      --accent: #60a5fa;
      --grid: rgba(148, 163, 184, 0.16);
      --hair: rgba(96, 165, 250, 0.9);
    }
  }
  * { box-sizing: border-box; }
  html, body { height: 100%; margin: 0; }
  body {
    font: 13px -apple-system, "SF Pro Text", system-ui, sans-serif;
    color: var(--ink);
    background: radial-gradient(1200px 800px at 50% -10%, var(--bg), var(--bg2));
    overflow: hidden;
    -webkit-user-select: none;
    user-select: none;
  }
  #stage {
    position: absolute;
    inset: 0;
    display: flex;
    align-items: center;
    justify-content: center;
    cursor: grab;
  }
  #stage.panning { cursor: grabbing; }
  #card {
    position: relative;
    background: var(--card);
    border-radius: 14px;
    padding: 14px;
    box-shadow: 0 18px 50px -12px var(--card-shadow), 0 0 0 1px rgba(148,163,184,0.14);
    transform-origin: center center;
    will-change: transform;
  }
  #card svg { display: block; max-width: none; height: auto; }
  #grid {
    position: absolute;
    inset: 14px;
    pointer-events: none;
    border-radius: 6px;
    opacity: 0;
    transition: opacity .15s ease;
    background-image:
      linear-gradient(to right, var(--grid) 1px, transparent 1px),
      linear-gradient(to bottom, var(--grid) 1px, transparent 1px);
    background-size: 32px 32px;
  }
  #grid.on { opacity: 1; }
  .hair {
    position: absolute;
    background: var(--hair);
    pointer-events: none;
    opacity: 0;
    z-index: 5;
  }
  #hx { height: 1px; left: 0; right: 0; }
  #hy { width: 1px; top: 0; bottom: 0; }
  .hair.on { opacity: 1; }
  #tip {
    position: absolute;
    z-index: 6;
    padding: 3px 7px;
    font: 11px "SF Mono", ui-monospace, monospace;
    color: #fff;
    background: rgba(15, 23, 42, 0.86);
    border-radius: 6px;
    pointer-events: none;
    opacity: 0;
    white-space: nowrap;
  }
  #tip.on { opacity: 1; }
  #empty { color: var(--muted); font-size: 14px; }
</style>
</head>
<body>
<div id="stage">
  <div id="card">
    <div id="chart">)HTML";

    static const char* kTail = R"HTML(</div>
    <div id="grid"></div>
    <div id="hx" class="hair"></div>
    <div id="hy" class="hair"></div>
    <div id="tip"></div>
  </div>
</div>
<script>
(function () {
  var stage = document.getElementById('stage');
  var card = document.getElementById('card');
  var grid = document.getElementById('grid');
  var hx = document.getElementById('hx');
  var hy = document.getElementById('hy');
  var tip = document.getElementById('tip');
  var svg = card.querySelector('svg');

  var state = { zoom: 1, minZoom: 0.2, maxZoom: 8, x: 0, y: 0, grid: false, hair: false, base: 1 };

  function report(kind, payload) {
    try {
      if (window.webkit && window.webkit.messageHandlers && window.webkit.messageHandlers.dune) {
        window.webkit.messageHandlers.dune.postMessage(Object.assign({ kind: kind }, payload || {}));
      }
    } catch (e) {}
  }

  function apply() {
    card.style.transform =
      'translate(' + state.x + 'px,' + state.y + 'px) scale(' + (state.base * state.zoom) + ')';
    report('zoom', { zoom: Math.round(state.base * state.zoom * 100) });
  }

  // Scale the card so the chart fits the viewport with a comfortable margin.
  function fit() {
    if (!svg) return;
    var vw = window.innerWidth, vh = window.innerHeight;
    var cw = card.offsetWidth, ch = card.offsetHeight;
    if (!cw || !ch) return;
    state.base = Math.min((vw - 80) / cw, (vh - 96) / ch, 1.6);
    if (!isFinite(state.base) || state.base <= 0) state.base = 1;
    state.zoom = 1; state.x = 0; state.y = 0;
    apply();
  }
  window.duneFit = fit;
  window.duneReset = fit;

  window.duneZoomIn = function () { zoomBy(1.25); };
  window.duneZoomOut = function () { zoomBy(1 / 1.25); };
  function zoomBy(f) {
    state.zoom = Math.min(state.maxZoom, Math.max(state.minZoom, state.zoom * f));
    apply();
  }

  window.duneToggleGrid = function () {
    state.grid = !state.grid;
    grid.classList.toggle('on', state.grid);
    return state.grid;
  };
  window.duneToggleCrosshair = function () {
    state.hair = !state.hair;
    if (!state.hair) { hx.classList.remove('on'); hy.classList.remove('on'); tip.classList.remove('on'); }
    return state.hair;
  };
  window.duneSetTheme = function (name) {
    if (name === 'toggle') {
      var cur = document.documentElement.getAttribute('data-theme');
      var dark = cur ? cur === 'dark'
                     : window.matchMedia('(prefers-color-scheme: dark)').matches;
      name = dark ? 'light' : 'dark';
    }
    document.documentElement.setAttribute('data-theme', name);
    return name;
  };

  // Wheel: zoom toward the pointer.
  stage.addEventListener('wheel', function (e) {
    e.preventDefault();
    zoomBy(e.deltaY < 0 ? 1.1 : 1 / 1.1);
  }, { passive: false });

  // Drag to pan.
  var drag = null;
  stage.addEventListener('mousedown', function (e) {
    drag = { x: e.clientX - state.x, y: e.clientY - state.y };
    stage.classList.add('panning');
  });
  window.addEventListener('mouseup', function () { drag = null; stage.classList.remove('panning'); });
  window.addEventListener('mousemove', function (e) {
    if (drag) { state.x = e.clientX - drag.x; state.y = e.clientY - drag.y; apply(); }
  });

  // Crosshair + live coordinate readout in the chart's own SVG space.
  card.addEventListener('mousemove', function (e) {
    if (!state.hair || !svg || typeof svg.getScreenCTM !== 'function') return;
    var ctm = svg.getScreenCTM();
    if (!ctm) return;
    var cr = card.getBoundingClientRect();
    hx.style.top = (e.clientY - cr.top) + 'px';
    hy.style.left = (e.clientX - cr.left) + 'px';
    hx.classList.add('on'); hy.classList.add('on');
    var pt = (svg.createSVGPoint ? svg.createSVGPoint() : new DOMPoint());
    pt.x = e.clientX; pt.y = e.clientY;
    var loc = pt.matrixTransform(ctm.inverse());
    var sx = Math.round(loc.x), sy = Math.round(loc.y);
    tip.textContent = 'x ' + sx + '   y ' + sy;
    tip.style.left = (e.clientX - cr.left + 12) + 'px';
    tip.style.top = (e.clientY - cr.top + 12) + 'px';
    tip.classList.add('on');
    report('cursor', { x: sx, y: sy });
  });
  card.addEventListener('mouseleave', function () {
    hx.classList.remove('on'); hy.classList.remove('on'); tip.classList.remove('on');
    report('cursor', { x: null, y: null });
  });

  window.addEventListener('resize', fit);
  if (svg) { requestAnimationFrame(fit); } else {
    document.getElementById('chart').innerHTML = '<div id="empty">No chart to display.</div>';
  }
  report('ready', { series: svg ? svg.querySelectorAll('.series').length : 0 });
})();
</script>
</body>
</html>)HTML";

    std::string html;
    html.reserve(std::strlen(kHead) + svg.size() + std::strlen(kTail));
    html.append(kHead);
    html.append(svg);
    html.append(kTail);
    return html;
}

} // namespace

@interface DunePlotWindowDelegate : NSObject <NSWindowDelegate>
@end

@implementation DunePlotWindowDelegate
- (void)windowWillClose:(NSNotification*)notification {
    (void)notification;
    [NSApp stop:nil];
    // Nudge the run loop so -[NSApplication run] returns promptly after stop.
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

// Owns the web view + status bar and bridges the native toolbar to the page's
// JavaScript, and the page's cursor messages back to the status bar.
@interface DunePlotController : NSObject <NSToolbarDelegate, WKScriptMessageHandler>
@property(nonatomic, strong) WKWebView* webView;
@property(nonatomic, strong) NSTextField* coordLabel;
@property(nonatomic, strong) NSTextField* zoomLabel;
@property(nonatomic, copy) NSString* svg;
@end

@implementation DunePlotController

- (void)runJS:(NSString*)script {
    [self.webView evaluateJavaScript:script completionHandler:nil];
}

- (void)toggleGrid:(id)sender { (void)sender; [self runJS:@"window.duneToggleGrid && duneToggleGrid();"]; }
- (void)zoomIn:(id)sender { (void)sender; [self runJS:@"window.duneZoomIn && duneZoomIn();"]; }
- (void)zoomOut:(id)sender { (void)sender; [self runJS:@"window.duneZoomOut && duneZoomOut();"]; }
- (void)resetView:(id)sender { (void)sender; [self runJS:@"window.duneReset && duneReset();"]; }
- (void)toggleCrosshair:(id)sender {
    (void)sender;
    [self runJS:@"window.duneToggleCrosshair && duneToggleCrosshair();"];
}
- (void)toggleTheme:(id)sender { (void)sender; [self runJS:@"window.duneSetTheme && duneSetTheme('toggle');"]; }

- (void)saveSVG:(id)sender {
    (void)sender;
    NSSavePanel* panel = [NSSavePanel savePanel];
    [panel setNameFieldStringValue:@"dune-plot.svg"];
    if ([panel runModal] != NSModalResponseOK || [panel URL] == nil) {
        return;
    }

    NSError* error = nil;
    if (![self.svg writeToURL:[panel URL] atomically:YES encoding:NSUTF8StringEncoding error:&error]) {
        NSAlert* alert = [[NSAlert alloc] init];
        [alert setMessageText:@"Could not save SVG"];
        [alert setInformativeText:error == nil ? @"Unknown write error." : [error localizedDescription]];
        [alert runModal];
    }
}

// Receives {kind:'cursor'|'zoom'|'ready', ...} messages posted by the page.
- (void)userContentController:(WKUserContentController*)controller didReceiveScriptMessage:(WKScriptMessage*)message {
    (void)controller;
    NSDictionary* body = [message body];
    if (![body isKindOfClass:[NSDictionary class]]) {
        return;
    }

    NSString* kind = body[@"kind"];
    if ([kind isEqualToString:@"cursor"]) {
        id x = body[@"x"];
        id y = body[@"y"];
        if (x == nil || x == [NSNull null]) {
            [self.coordLabel setStringValue:@"—"];
        } else {
            [self.coordLabel setStringValue:[NSString stringWithFormat:@"x %@    y %@", x, y]];
        }
    } else if ([kind isEqualToString:@"zoom"]) {
        [self.zoomLabel setStringValue:[NSString stringWithFormat:@"%@%%", body[@"zoom"] ?: @"100"]];
    }
}

// --- NSToolbar ---------------------------------------------------------------

- (NSToolbarItem*)item:(NSString*)identifier symbol:(NSString*)symbol label:(NSString*)label action:(SEL)action {
    NSToolbarItem* item = [[NSToolbarItem alloc] initWithItemIdentifier:identifier];
    [item setLabel:label];
    [item setToolTip:label];
    [item setTarget:self];
    [item setAction:action];
    NSImage* image = nil;
    if (@available(macOS 11.0, *)) {
        image = [NSImage imageWithSystemSymbolName:symbol accessibilityDescription:label];
    }
    if (image != nil) {
        [item setImage:image];
    }
    return item;
}

- (NSArray<NSToolbarItemIdentifier>*)toolbarDefaultItemIdentifiers:(NSToolbar*)toolbar {
    (void)toolbar;
    return @[
        @"grid", @"crosshair", NSToolbarSpaceItemIdentifier, @"zoomOut", @"reset", @"zoomIn",
        NSToolbarFlexibleSpaceItemIdentifier, @"theme", @"save"
    ];
}

- (NSArray<NSToolbarItemIdentifier>*)toolbarAllowedItemIdentifiers:(NSToolbar*)toolbar {
    return [self toolbarDefaultItemIdentifiers:toolbar];
}

- (NSToolbarItem*)toolbar:(NSToolbar*)toolbar
        itemForItemIdentifier:(NSToolbarItemIdentifier)identifier
    willBeInsertedIntoToolbar:(BOOL)flag {
    (void)toolbar;
    (void)flag;
    if ([identifier isEqualToString:@"grid"]) {
        return [self item:identifier symbol:@"grid" label:@"Grid" action:@selector(toggleGrid:)];
    }
    if ([identifier isEqualToString:@"crosshair"]) {
        return [self item:identifier symbol:@"plus.viewfinder" label:@"Crosshair" action:@selector(toggleCrosshair:)];
    }
    if ([identifier isEqualToString:@"zoomIn"]) {
        return [self item:identifier symbol:@"plus.magnifyingglass" label:@"Zoom In" action:@selector(zoomIn:)];
    }
    if ([identifier isEqualToString:@"zoomOut"]) {
        return [self item:identifier symbol:@"minus.magnifyingglass" label:@"Zoom Out" action:@selector(zoomOut:)];
    }
    if ([identifier isEqualToString:@"reset"]) {
        return [self item:identifier symbol:@"arrow.counterclockwise" label:@"Reset" action:@selector(resetView:)];
    }
    if ([identifier isEqualToString:@"theme"]) {
        return [self item:identifier symbol:@"circle.lefthalf.filled" label:@"Theme" action:@selector(toggleTheme:)];
    }
    if ([identifier isEqualToString:@"save"]) {
        return [self item:identifier symbol:@"square.and.arrow.down" label:@"Save SVG" action:@selector(saveSVG:)];
    }
    return nil;
}

@end

namespace dune {

NativePlotDisplayResult show_native_plot_svg(const std::string& svg) {
    if (svg.empty()) {
        return {false, "native plot display backend received an empty SVG"};
    }

    @autoreleasepool {
        NSApplication* app = [NSApplication sharedApplication];
        [app setActivationPolicy:NSApplicationActivationPolicyRegular];

        const NSRect frame = NSMakeRect(0, 0, 960, 640);
        NSWindow* window =
            [[NSWindow alloc] initWithContentRect:frame
                                        styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                                                  NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable
                                          backing:NSBackingStoreBuffered
                                            defer:NO];
        [window setTitle:@"Dune Plot"];
        [window setReleasedWhenClosed:NO];
        if (@available(macOS 11.0, *)) {
            [window setToolbarStyle:NSWindowToolbarStyleUnified];
        }

        DunePlotController* controller = [[DunePlotController alloc] init];
        controller.svg = [[NSString alloc] initWithBytes:svg.data() length:svg.size() encoding:NSUTF8StringEncoding];

        // Web view with a message handler so the page can push cursor/zoom state.
        WKWebViewConfiguration* config = [[WKWebViewConfiguration alloc] init];
        WKUserContentController* content = [[WKUserContentController alloc] init];
        [content addScriptMessageHandler:controller name:@"dune"];
        [config setUserContentController:content];

        NSView* root = [[NSView alloc] initWithFrame:frame];
        [root setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];

        const CGFloat statusHeight = 28.0;
        WKWebView* web = [[WKWebView alloc] initWithFrame:NSMakeRect(0, statusHeight, NSWidth(frame),
                                                                     NSHeight(frame) - statusHeight)
                                            configuration:config];
        [web setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
        controller.webView = web;

        const std::string document = build_document(svg);
        NSString* html = [[NSString alloc] initWithBytes:document.data()
                                                  length:document.size()
                                                encoding:NSUTF8StringEncoding];
        [web loadHTMLString:html baseURL:nil];

        // Bottom status bar: live coordinate + zoom readout.
        NSView* status = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, NSWidth(frame), statusHeight)];
        [status setAutoresizingMask:NSViewWidthSizable | NSViewMaxYMargin];
        [status setWantsLayer:YES];
        [[status layer] setBackgroundColor:[[NSColor windowBackgroundColor] CGColor]];

        NSTextField* coord = [NSTextField labelWithString:@"—"];
        [coord setFont:[NSFont monospacedSystemFontOfSize:11.0 weight:NSFontWeightRegular]];
        [coord setTextColor:[NSColor secondaryLabelColor]];
        [coord setFrame:NSMakeRect(12.0, 5.0, 260.0, 18.0)];
        [coord setAutoresizingMask:NSViewMaxXMargin];
        controller.coordLabel = coord;

        NSTextField* hint = [NSTextField labelWithString:@"scroll to zoom · drag to pan"];
        [hint setFont:[NSFont systemFontOfSize:11.0]];
        [hint setTextColor:[NSColor tertiaryLabelColor]];
        [hint setAlignment:NSTextAlignmentCenter];
        [hint setFrame:NSMakeRect(NSWidth(frame) / 2.0 - 130.0, 5.0, 260.0, 18.0)];
        [hint setAutoresizingMask:NSViewMinXMargin | NSViewMaxXMargin];

        NSTextField* zoom = [NSTextField labelWithString:@"100%"];
        [zoom setFont:[NSFont monospacedSystemFontOfSize:11.0 weight:NSFontWeightRegular]];
        [zoom setTextColor:[NSColor secondaryLabelColor]];
        [zoom setAlignment:NSTextAlignmentRight];
        [zoom setFrame:NSMakeRect(NSWidth(frame) - 84.0, 5.0, 72.0, 18.0)];
        [zoom setAutoresizingMask:NSViewMinXMargin];
        controller.zoomLabel = zoom;

        [status addSubview:coord];
        [status addSubview:hint];
        [status addSubview:zoom];

        [root addSubview:web];
        [root addSubview:status];

        NSToolbar* toolbar = [[NSToolbar alloc] initWithIdentifier:@"DunePlotToolbar"];
        [toolbar setDelegate:controller];
        [toolbar setDisplayMode:NSToolbarDisplayModeIconOnly];
        [toolbar setAllowsUserCustomization:NO];
        [window setToolbar:toolbar];

        DunePlotWindowDelegate* delegate = [[DunePlotWindowDelegate alloc] init];
        [window setDelegate:delegate];
        [window setContentView:root];
        [window center];
        [window makeKeyAndOrderFront:nil];
        [app activateIgnoringOtherApps:YES];
        [app run];

        return {true, "native plot window closed"};
    }
}

} // namespace dune
