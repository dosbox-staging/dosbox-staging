// Metal scheduled-present spike: present clear-colour frames at a
// 70.086 Hz cadence via presentDrawable:atTime:, record actual
// presented times, report scheduling accuracy.
//
// Build: clang++ -fobjc-arc -std=c++17 metal_spike.mm -o metal_spike \
//          -framework Cocoa -framework Metal -framework QuartzCore
#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>
#include <atomic>

static constexpr double DosRateHz  = 70.086;
static constexpr int    NumFrames  = 300;

struct Sample {
	double target    = 0.0;
	double presented = 0.0;
};

static std::vector<Sample> samples;
static NSCondition* done_cond;
static bool done = false;
static std::atomic<int> gpu_completed{0};
static std::atomic<int> handler_fired{0};

static void RunPresentLoop(CAMetalLayer* layer, id<MTLDevice> device)
{
	id<MTLCommandQueue> queue = [device newCommandQueue];

	const double period = 1.0 / DosRateHz;
	// Start half a second out so the pipeline is warm and targets are
	// comfortably in the future.
	const double t0 = CACurrentMediaTime() + 0.5;

	samples.resize(NumFrames);

	for (int i = 0; i < NumFrames; ++i) {
		const double target = t0 + i * period;

		// Sleep until ~3 ms before the target, then acquire+submit
		// (models "submit early with a timestamp, display engine
		// holds the flip").
		const double wake = target - 0.003;
		double now        = CACurrentMediaTime();
		if (wake > now) {
			usleep((useconds_t)((wake - now) * 1e6));
		}

		@autoreleasepool {
			id<CAMetalDrawable> drawable = [layer nextDrawable];
			if (!drawable) {
				if (i < 3) {
					printf("frame %d: nextDrawable nil\n", i);
				}
				samples[i].target = target; // presented stays 0
				continue;
			}
			if (i < 3) {
				printf("frame %d: drawable ok, now=%.4f target=%.4f\n",
				       i, CACurrentMediaTime(), target);
			}

			MTLRenderPassDescriptor* rp =
			        [MTLRenderPassDescriptor renderPassDescriptor];
			rp.colorAttachments[0].texture     = drawable.texture;
			rp.colorAttachments[0].loadAction  = MTLLoadActionClear;
			rp.colorAttachments[0].storeAction = MTLStoreActionStore;
			const double phase = (i % 70) / 70.0;
			rp.colorAttachments[0].clearColor =
			        MTLClearColorMake(phase, 0.3, 1.0 - phase, 1.0);

			id<MTLCommandBuffer> cmd = [queue commandBuffer];
			id<MTLRenderCommandEncoder> enc =
			        [cmd renderCommandEncoderWithDescriptor:rp];
			[enc endEncoding];

			const int idx = i;
			[drawable addPresentedHandler:^(id<MTLDrawable> d) {
				samples[idx].presented = d.presentedTime;
				++handler_fired;
			}];
			[cmd addCompletedHandler:^(id<MTLCommandBuffer> cb) {
				++gpu_completed;
			}];
			samples[idx].target = target;

			[cmd presentDrawable:drawable atTime:target];
			[cmd commit];
		}
	}

	// Let the last presented-handlers fire.
	usleep(300000);

	[done_cond lock];
	done = true;
	[done_cond signal];
	[done_cond unlock];
}

static void Report()
{
	std::vector<double> err_ms;
	std::vector<double> interval_ms;
	int dropped = 0;

	double prev_presented = 0.0;
	for (const auto& s : samples) {
		if (s.presented <= 0.0) {
			++dropped;
			continue;
		}
		err_ms.push_back((s.presented - s.target) * 1000.0);
		if (prev_presented > 0.0) {
			interval_ms.push_back((s.presented - prev_presented) * 1000.0);
		}
		prev_presented = s.presented;
	}

	auto stats = [](std::vector<double> v, const char* name) {
		if (v.empty()) {
			printf("%s: no data\n", name);
			return;
		}
		std::sort(v.begin(), v.end());
		double sum = 0.0;
		for (double x : v) {
			sum += x;
		}
		const double mean = sum / v.size();
		const double p50  = v[v.size() / 2];
		const double p95  = v[(size_t)(v.size() * 0.95)];
		printf("%s: mean=%.3f  p50=%.3f  p95=%.3f  min=%.3f  max=%.3f  (n=%zu)\n",
		       name, mean, p50, p95, v.front(), v.back(), v.size());
	};

	printf("\n=== presentDrawable:atTime: @ %.3f Hz, %d frames ===\n",
	       DosRateHz, NumFrames);
	printf("target period: %.3f ms\n", 1000.0 / DosRateHz);
	printf("gpu completed: %d, presented handlers fired: %d\n",
	       gpu_completed.load(), handler_fired.load());
	printf("dropped/unpresented: %d\n", dropped);
	stats(err_ms, "present error vs target [ms]");
	stats(interval_ms, "presented interval       [ms]");

	// Interval histogram (0.5 ms buckets around the expected period)
	if (!interval_ms.empty()) {
		printf("interval histogram:\n");
		for (double lo = 8.0; lo < 22.0; lo += 1.0) {
			int n = 0;
			for (double x : interval_ms) {
				if (x >= lo && x < lo + 1.0) {
					++n;
				}
			}
			if (n > 0) {
				printf("  %5.1f-%5.1f ms: %4d %s\n", lo, lo + 1.0, n,
				       std::string(std::min(n, 60), '#').c_str());
			}
		}
	}
}

int main()
{
	setvbuf(stdout, nullptr, _IONBF, 0);
	@autoreleasepool {
		printf("[1] creating NSApplication\n");
		[NSApplication sharedApplication];
		[NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
		[NSApp finishLaunching];
		[NSApp activateIgnoringOtherApps:YES];
		printf("[2] NSApplication up (launched + active)\n");

		NSRect rect = NSMakeRect(200, 200, 640, 400);
		NSWindow* window = [[NSWindow alloc]
		        initWithContentRect:rect
		                  styleMask:(NSWindowStyleMaskTitled |
		                             NSWindowStyleMaskClosable)
		                    backing:NSBackingStoreBuffered
		                      defer:NO];
		window.title = @"metal_spike (auto-closes)";
		printf("[3] window created\n");

		id<MTLDevice> device = MTLCreateSystemDefaultDevice();
		if (!device) {
			printf("no Metal device\n");
			return 1;
		}
		printf("[4] device: %s\n", device.name.UTF8String);

		CAMetalLayer* layer  = [CAMetalLayer layer];
		layer.device         = device;
		layer.pixelFormat    = MTLPixelFormatBGRA8Unorm;
		layer.framebufferOnly = YES;
		layer.drawableSize   = CGSizeMake(640, 400);
		// displaySyncEnabled defaults to YES (vsync on)
		printf("[5] layer created\n");

		// Layer-backed content view with the Metal layer as a
		// sublayer (the robust hosting pattern)
		window.contentView.wantsLayer = YES;
		layer.frame = window.contentView.bounds;
		[window.contentView.layer addSublayer:layer];
		printf("[6] layer hosted as sublayer\n");
		[window makeKeyAndOrderFront:nil];
		printf("[7] window on screen\n");

		NSScreen* screen = [NSScreen mainScreen];
		if (screen) {
			printf("main screen maximumFramesPerSecond: %ld\n",
			       (long)screen.maximumFramesPerSecond);
		} else {
			printf("mainScreen is nil (no GUI session?)\n");
		}

		done_cond = [[NSCondition alloc] init];

		[NSThread detachNewThreadWithBlock:^{
			RunPresentLoop(layer, device);
		}];

		// Pump the run loop until the present loop finishes.
		while (true) {
			@autoreleasepool {
				NSEvent* ev = [NSApp
				        nextEventMatchingMask:NSEventMaskAny
				                    untilDate:[NSDate dateWithTimeIntervalSinceNow:0.05]
				                       inMode:NSDefaultRunLoopMode
				                      dequeue:YES];
				if (ev) {
					[NSApp sendEvent:ev];
				}
			}
			[done_cond lock];
			const bool finished = done;
			[done_cond unlock];
			if (finished) {
				break;
			}
		}

		Report();
		fflush(stdout);
		_exit(0); // skip AppKit teardown; this is a spike
	}
	return 0;
}
