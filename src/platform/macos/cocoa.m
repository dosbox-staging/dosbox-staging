#import <Cocoa/Cocoa.h>

void setDisplayP3ColorSpace(void* nswindow)
{
	NSWindow* window = nswindow;

	// This is not set by default in Cocoa OpenGL apps, which means macOS
	// won't colour manage the app. SDL2 hardcodes this to [NSColorSpace
	// sRGBColorSpace] as arguably that's most useful general-purpose setting.
	// But as we're doing our own colour management in OpenGL mode, we set
	// this to Display P3 which effectively becomes our internal colour space,
	// then the OS transforms this to the colour profile set in system
	// settings before displaying the image.
	//
	[window setColorSpace: [NSColorSpace displayP3ColorSpace]];
}
