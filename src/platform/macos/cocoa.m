#import <Cocoa/Cocoa.h>

void setDisplayP3ColorSpace(void* nswindow)
{
	NSWindow* window = nswindow;

	// This is not set by default in Cocoa OpenGL apps, which means macOS
	// won't colour manage the app. SDL2 hardcodes this to [NSColorSpace
	// sRGBColorSpace] as arguably that's most useful general-purpose setting.
	// But we're doing our own colour management in OpenGL mode, so we'll
	// reset this.
	//
	[window setColorSpace: [NSColorSpace displayP3ColorSpace]];
}
