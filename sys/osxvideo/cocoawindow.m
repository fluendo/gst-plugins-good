/* GStreamer
 * Copyright (C) 2004 Zaheer Abbas Merali <zaheerabbas at merali dot org>
 * Copyright (C) 2007 Pioneers of the Inevitable <songbird@songbirdnest.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 * 
 * The development of this code was made possible due to the involvement of Pioneers 
 * of the Inevitable, the creators of the Songbird Music player
 *
 */

/* inspiration gained from looking at source of osx video out of xine and vlc
 * and is reflected in the code
 */


#include <Cocoa/Cocoa.h>
#include <gst/gst.h>
#import "cocoawindow.h"
#import "osxvideosink.h"

#include <OpenGL/OpenGL.h>
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>

/* Debugging category */
#include <gst/gstinfo.h>

@ implementation GstOSXVideoSinkWindow

/* The object has to be released */
- (id) initWithContentNSRect: (NSRect) rect
		 styleMask: (unsigned int) styleMask
		   backing: (NSBackingStoreType) bufferingType 
		     defer: (BOOL) flag
		    screen:(NSScreen *) aScreen
{
  self = [super initWithContentRect: rect
		styleMask: styleMask
		backing: bufferingType 
		defer: flag 
		screen:aScreen];

  GST_DEBUG ("Initializing GstOSXvideoSinkWindow");

  gstview = [[GstGLView alloc] initWithFrame:rect];
  
  if (gstview)
    [self setContentView:gstview];
  [self setTitle:@"GStreamer Video Output"];

  return self;
}

- (void) setContentSize:(NSSize) size {
  width = size.width;
  height = size.height;

  [gstview setVideoSize: (int) width:(int) height];

  [super setContentSize:size];
}

- (GstGLView *) gstView {
  return gstview;
}

- (void) awakeFromNib {
  [self setAcceptsMouseMovedEvents:YES];
}

@end


//
// OpenGL implementation
//

@ implementation GstGLView

- (id) initWithFrame:(NSRect) frame {
  NSOpenGLPixelFormat *fmt;
  NSOpenGLPixelFormatAttribute attribs[] = {
    NSOpenGLPFAAccelerated,
    NSOpenGLPFANoRecovery,
    NSOpenGLPFADoubleBuffer,
    NSOpenGLPFAColorSize, 24,
    NSOpenGLPFAAlphaSize, 8,
    NSOpenGLPFADepthSize, 24,
    NSOpenGLPFAWindow,
    0
  };

  fmt = [[NSOpenGLPixelFormat alloc]
	  initWithAttributes:attribs];

  if (!fmt) {
    GST_WARNING ("Cannot create NSOpenGLPixelFormat");
    return nil;
  }

  self = [super initWithFrame: frame pixelFormat:fmt];
  [fmt release];

   actualContext = [self openGLContext];
   [actualContext makeCurrentContext];
   [actualContext update];

  /* Black background */
  glClearColor (0.0, 0.0, 0.0, 0.0);

  pi_texture = 0;
  data = nil;
  width = frame.size.width;
  height = frame.size.height;
  drawingBounds = NSMakeRect(0, 0, width, height);

  GST_LOG ("Width: %d Height: %d", width, height);

  trackingArea = [[NSTrackingArea alloc] initWithRect:[self bounds]
      options: (NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved | NSTrackingActiveInKeyWindow)
      owner:self
      userInfo:nil];

  [self addTrackingArea:trackingArea];
  mainThread = [NSThread mainThread];

  [self initTextures];
  return self;
}

- (NSRect) getDrawingBounds {
  return drawingBounds;
}

- (void) reshape {
  NSRect bounds;
  gdouble frame_par, view_par;
  gint view_height, view_width, c_height, c_width, c_x, c_y;


  GST_LOG ("reshaping");

  if (!initDone) {
    return;
  }

  [actualContext makeCurrentContext];

  bounds = [self bounds];
  view_width = bounds.size.width;
  view_height = bounds.size.height;

  frame_par = (gdouble) width / height;
  view_par = (gdouble) view_width / view_height;
  if (!keepAspectRatio)
    view_par = frame_par;

  if (frame_par == view_par) {
    c_height = view_height;
    c_width = view_width;
    c_x = 0;
    c_y = 0;
  } else if (frame_par < view_par) {
    c_height = view_height;
    c_width = c_height * frame_par;
    c_x = (view_width - c_width) / 2;
    c_y = 0;
  } else {
    c_width = view_width;
    c_height = c_width / frame_par;
    c_x = 0;
    c_y = (view_height - c_height) / 2;
  }

  drawingBounds = NSMakeRect(c_x, c_y, c_width, c_height);
  glViewport (c_x, c_y, (GLint) c_width, (GLint) c_height);
}

- (void) initTextures {

  [actualContext makeCurrentContext];

  /* Free previous texture if any */
  if (pi_texture) {
    glDeleteTextures (1, (GLuint *)&pi_texture);
  }

  if (data) {
    data = g_realloc (data, width * height * sizeof(short)); // short or 3byte?
  } else {
    data = g_malloc0(width * height * sizeof(short));
  }
  /* Create textures */
  glGenTextures (1, (GLuint *)&pi_texture);

  glEnable (GL_TEXTURE_RECTANGLE_EXT);
  glEnable (GL_UNPACK_CLIENT_STORAGE_APPLE);

  glPixelStorei (GL_UNPACK_ALIGNMENT, 1);
  glPixelStorei (GL_UNPACK_ROW_LENGTH, width);
  
  glBindTexture (GL_TEXTURE_RECTANGLE_EXT, pi_texture);

  /* Use VRAM texturing */
  glTexParameteri (GL_TEXTURE_RECTANGLE_EXT,
		   GL_TEXTURE_STORAGE_HINT_APPLE, GL_STORAGE_CACHED_APPLE);

  /* Tell the driver not to make a copy of the texture but to use
     our buffer */
  glPixelStorei (GL_UNPACK_CLIENT_STORAGE_APPLE, GL_TRUE);

  /* Linear interpolation */
  glTexParameteri (GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri (GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  /* I have no idea what this exactly does, but it seems to be
     necessary for scaling */
  glTexParameteri (GL_TEXTURE_RECTANGLE_EXT,
		   GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri (GL_TEXTURE_RECTANGLE_EXT,
		   GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  // glPixelStorei (GL_UNPACK_ROW_LENGTH, 0); WHY ??

  glTexImage2D (GL_TEXTURE_RECTANGLE_EXT, 0, GL_RGBA,
		width, height, 0, 
		GL_YCBCR_422_APPLE, GL_UNSIGNED_SHORT_8_8_APPLE, data);


  initDone = 1;
}

- (void) reloadTexture {
  if (!initDone) {
    return;
  }

  GST_LOG ("Reloading Texture");

  [actualContext makeCurrentContext];

  glBindTexture (GL_TEXTURE_RECTANGLE_EXT, pi_texture);
  glPixelStorei (GL_UNPACK_ROW_LENGTH, width);

  /* glTexSubImage2D is faster than glTexImage2D
     http://developer.apple.com/samplecode/Sample_Code/Graphics_3D/
     TextureRange/MainOpenGLView.m.htm */
  glTexSubImage2D (GL_TEXTURE_RECTANGLE_EXT, 0, 0, 0,
		   width, height,
		   GL_YCBCR_422_APPLE, GL_UNSIGNED_SHORT_8_8_APPLE, data);    //FIXME
}

- (void) cleanUp {
  initDone = 0;
}

- (void) drawQuad {
  f_x = 1.0;
  f_y = 1.0;

  glBegin (GL_QUADS);
  /* Top left */
  glTexCoord2f (0.0, 0.0);
  glVertex2f (-f_x, f_y);
  /* Bottom left */
  glTexCoord2f (0.0, (float) height);
  glVertex2f (-f_x, -f_y);
  /* Bottom right */
  glTexCoord2f ((float) width, (float) height);
  glVertex2f (f_x, -f_y);
  /* Top right */
  glTexCoord2f ((float) width, 0.0);
  glVertex2f (f_x, f_y);
  glEnd ();
}

- (void) drawRect:(NSRect) rect {
  GLint params[] = { 1 };

  [actualContext makeCurrentContext];

  CGLSetParameter (CGLGetCurrentContext (), kCGLCPSwapInterval, params);

  /* Black background */
  glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  if (!initDone) {
    [actualContext flushBuffer];
    return;
  }

  /* Draw */
  glBindTexture (GL_TEXTURE_RECTANGLE_EXT, pi_texture); // FIXME
  [self drawQuad];
  /* Draw */
  [actualContext flushBuffer];
}

- (void) displayTexture {
  if ([self lockFocusIfCanDraw]) {

    [self drawRect:[self bounds]];
    [self reloadTexture];

    [self unlockFocus];

  }

}

- (char *) getTextureBuffer {
  return data;
}

- (void) setFullScreen:(BOOL) flag {
  if (!fullscreen && flag) {
    // go to full screen
    /* Create the new pixel format */
    NSOpenGLPixelFormat *fmt;
    NSOpenGLPixelFormatAttribute attribs[] = {
      NSOpenGLPFAAccelerated,
      NSOpenGLPFANoRecovery,
      NSOpenGLPFADoubleBuffer,
      NSOpenGLPFAColorSize, 24,
      NSOpenGLPFAAlphaSize, 8,
      NSOpenGLPFADepthSize, 24,
      NSOpenGLPFAFullScreen,
      NSOpenGLPFAScreenMask,
      CGDisplayIDToOpenGLDisplayMask (kCGDirectMainDisplay),
      0
    };

    fmt = [[NSOpenGLPixelFormat alloc]
	    initWithAttributes:attribs];

    if (!fmt) {
      GST_WARNING ("Cannot create NSOpenGLPixelFormat");
      return;
    }

    /* Create the new OpenGL context */
    fullScreenContext = [[NSOpenGLContext alloc]
			  initWithFormat: fmt shareContext:nil];
    if (!fullScreenContext) {
      GST_WARNING ("Failed to create new NSOpenGLContext");
      return;
    }

    actualContext = fullScreenContext;

    /* Capture display, switch to fullscreen */
    if (CGCaptureAllDisplays () != CGDisplayNoErr) {
      GST_WARNING ("CGCaptureAllDisplays() failed");
      return;
    }
    [fullScreenContext setFullScreen];
    [fullScreenContext makeCurrentContext];

    fullscreen = YES;

    [self initTextures];
    [self setNeedsDisplay:YES];

  } else if (fullscreen && !flag) {
    // fullscreen now and needs to go back to normal
    initDone = NO;
    
    actualContext = [self openGLContext];

    [NSOpenGLContext clearCurrentContext];
    [fullScreenContext clearDrawable];
    [fullScreenContext release];
    fullScreenContext = nil;

    CGReleaseAllDisplays ();

    [self reshape];
    [self initTextures];

    [self setNeedsDisplay:YES];

    fullscreen = NO;
    initDone = YES;
  }
}

- (void) setVideoSize: (int) w:(int) h {
  GST_LOG ("width:%d, height:%d", w, h);

  width = w;
  height = h;

//  if (data) g_free(data);

//  data = g_malloc0 (2 * w * h);
  [self initTextures];
  [self reshape];
}

- (void) setKeepAspectRatio: (BOOL) flag {
  keepAspectRatio = flag;
  [self reshape];
}

- (void) setMainThread: (NSThread *) thread {
  mainThread = thread;
}

- (void) haveSuperviewReal:(NSMutableArray *)closure {
	BOOL haveSuperview = [self superview] != nil;
	[closure addObject:[NSNumber numberWithBool:haveSuperview]];
}

- (BOOL) haveSuperview {
	NSMutableArray *closure = [NSMutableArray arrayWithCapacity:1];
	[self performSelector:@selector(haveSuperviewReal:)
		onThread:mainThread
		withObject:(id)closure waitUntilDone:YES];

	return [[closure objectAtIndex:0] boolValue];
}

- (void) addToSuperviewReal:(NSView *)superview {
	NSRect bounds;
	[superview addSubview:self];
	bounds = [superview bounds];
	[self setFrame:bounds];
	[self setAutoresizingMask:NSViewWidthSizable|NSViewHeightSizable];
}

- (void) addToSuperview: (NSView *)superview {
	[self performSelector:@selector(addToSuperviewReal:)
		onThread:mainThread
		withObject:superview waitUntilDone:YES];
}

- (void) removeFromSuperview: (id)unused
{
	[self removeFromSuperview];
}

- (void) dealloc {
  GST_LOG ("dealloc called");
  if (data) g_free(data);

  if (fullScreenContext) {
    [NSOpenGLContext clearCurrentContext];
    [fullScreenContext clearDrawable];
    [fullScreenContext release];
    if (actualContext == fullScreenContext) actualContext = nil;
    fullScreenContext = nil;
  }

  [super dealloc];
}

- (void)updateTrackingAreas {
  [self removeTrackingArea:trackingArea];
  [trackingArea release];
  trackingArea = [[NSTrackingArea alloc] initWithRect: [self bounds]
      options: (NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved | NSTrackingActiveInKeyWindow)
      owner:self userInfo:nil];
  [self addTrackingArea:trackingArea];
}

- (BOOL)acceptsFirstResponder {
    return YES;
}

- (void) setNavigation:(GstNavigation *)nav
{
  navigation = nav;
}

- (void)sendMouseEvent:(NSEvent *)event: (const char *)event_name
{
  NSPoint location;
  gint button;
  gdouble x, y;

  if (!navigation)
    return;

  switch ([event type]) {
    case NSMouseMoved:
      button = 0;
      break;
    case NSLeftMouseDown:
    case NSLeftMouseUp:
      button = 1;
      break;
    case NSRightMouseDown:
    case NSRightMouseUp:
      button = 2;
      break;
    default:
      button = 3;
      break;
  }

  location = [self convertPoint:[event locationInWindow] fromView:nil];

  x = location.x;
  y = location.y;
  /* invert Y */

  y = (1 - ((gdouble) y) / [self bounds].size.height) * [self bounds].size.height;

  gst_navigation_send_mouse_event (navigation, event_name, button, x, y);
}

- (void)sendKeyEvent:(NSEvent *)event: (const char *)event_name
{
  NSString *keyCharStr = [event charactersIgnoringModifiers];
  gchar * key_str;

  if (!navigation)
    return;

  if ( [keyCharStr length] == 0 )
    return;

  if ( [keyCharStr length] == 1 ) {
    key_str = g_strdup_printf("%c", [keyCharStr characterAtIndex:0]);
    gst_navigation_send_key_event(navigation, event_name, (const gchar *) key_str);
    g_free(key_str);
  }
}

- (void)keyDown:(NSEvent *) event;
{
  [self sendKeyEvent: event: "key-press"];
}

- (void)keyUp:(NSEvent *) event;
{
  [self sendKeyEvent: event: "key-release"];
}

- (void)mouseDown:(NSEvent *) event;
{
  [self sendMouseEvent:event: "mouse-button-press"];
}

- (void)mouseUp:(NSEvent *) event;
{
  [self sendMouseEvent:event: "mouse-button-release"];
}

- (void)mouseMoved:(NSEvent *)event;
{
  [self sendMouseEvent:event: "mouse-move"];
}

- (void)mouseEntered:(NSEvent *)event;
{
}

- (void)mouseExited:(NSEvent *)event;
{
}

@end
