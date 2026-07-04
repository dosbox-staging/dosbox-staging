// MoltenVK timed-present spike: present clear-colour frames at a
// 70.086 Hz cadence through Vulkan + VK_GOOGLE_display_timing on
// MoltenVK, collect actual glass times from past-presentation
// feedback, report scheduling accuracy. Direct comparison target:
// metal_spike.mm (Spike 4).
//
// Build: clang++ -fobjc-arc -std=c++17 vulkan_timing_spike.mm -o vulkan_timing_spike \
//          -I/opt/homebrew/include -L/opt/homebrew/lib -lMoltenVK \
//          -framework Cocoa -framework Metal -framework QuartzCore
#import <Cocoa/Cocoa.h>
#import <QuartzCore/CAMetalLayer.h>

#define VK_USE_PLATFORM_METAL_EXT
#include <vulkan/vulkan.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <map>
#include <vector>

static constexpr double   DosRateHz = 70.086;
static constexpr int      NumFrames = 300;
static constexpr uint32_t MaxInFlight = 2;

#define CHECK(x)                                                        \
	do {                                                            \
		VkResult r_ = (x);                                      \
		if (r_ != VK_SUCCESS) {                                 \
			printf("FAIL %s -> %d (line %d)\n", #x, r_,     \
			       __LINE__);                               \
			_exit(1);                                       \
		}                                                       \
	} while (0)

static uint64_t NowNs()
{
	// mach_absolute_time in nanoseconds; same domain as
	// CACurrentMediaTime() * 1e9
	return clock_gettime_nsec_np(CLOCK_UPTIME_RAW);
}

struct Feedback {
	uint64_t desired = 0;
	uint64_t actual  = 0;
};

int main()
{
	setvbuf(stdout, nullptr, _IONBF, 0);

	// ------- Cocoa window + CAMetalLayer (lessons from Spike 4) -------
	[NSApplication sharedApplication];
	[NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
	[NSApp finishLaunching];
	[NSApp activateIgnoringOtherApps:YES];

	NSRect rect      = NSMakeRect(200, 200, 640, 400);
	NSWindow* window = [[NSWindow alloc]
	        initWithContentRect:rect
	                  styleMask:(NSWindowStyleMaskTitled |
	                             NSWindowStyleMaskClosable)
	                    backing:NSBackingStoreBuffered
	                      defer:NO];
	window.title = @"vulkan_timing_spike (auto-closes)";

	CAMetalLayer* layer = [CAMetalLayer layer];
	layer.drawableSize  = CGSizeMake(640, 400);
	window.contentView.wantsLayer = YES;
	layer.frame = window.contentView.bounds;
	[window.contentView.layer addSublayer:layer];
	[window makeKeyAndOrderFront:nil];
	printf("[1] window + CAMetalLayer up (screen max fps: %ld)\n",
	       (long)[NSScreen mainScreen].maximumFramesPerSecond);

	// ------------------------- instance -------------------------
	const char* inst_exts[] = {
	        VK_KHR_SURFACE_EXTENSION_NAME,
	        VK_EXT_METAL_SURFACE_EXTENSION_NAME,
	};
	VkApplicationInfo app_info = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
	app_info.pApplicationName  = "vulkan_timing_spike";
	app_info.apiVersion        = VK_API_VERSION_1_2;

	VkInstanceCreateInfo ici = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
	ici.pApplicationInfo        = &app_info;
	ici.enabledExtensionCount   = 2;
	ici.ppEnabledExtensionNames = inst_exts;

	VkInstance instance = VK_NULL_HANDLE;
	CHECK(vkCreateInstance(&ici, nullptr, &instance));
	printf("[2] instance created\n");

	// ------------------------- surface -------------------------
	VkMetalSurfaceCreateInfoEXT sci = {
	        VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT};
	sci.pLayer = layer;
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	CHECK(vkCreateMetalSurfaceEXT(instance, &sci, nullptr, &surface));

	// --------------------- physical device ---------------------
	uint32_t pd_count = 0;
	CHECK(vkEnumeratePhysicalDevices(instance, &pd_count, nullptr));
	std::vector<VkPhysicalDevice> pds(pd_count);
	CHECK(vkEnumeratePhysicalDevices(instance, &pd_count, pds.data()));
	VkPhysicalDevice pd = pds[0];

	VkPhysicalDeviceProperties props = {};
	vkGetPhysicalDeviceProperties(pd, &props);
	printf("[3] device: %s (Vulkan %d.%d)\n", props.deviceName,
	       VK_API_VERSION_MAJOR(props.apiVersion),
	       VK_API_VERSION_MINOR(props.apiVersion));

	// check VK_GOOGLE_display_timing availability
	uint32_t ext_count = 0;
	vkEnumerateDeviceExtensionProperties(pd, nullptr, &ext_count, nullptr);
	std::vector<VkExtensionProperties> exts(ext_count);
	vkEnumerateDeviceExtensionProperties(pd, nullptr, &ext_count,
	                                     exts.data());
	bool has_timing      = false;
	bool has_portability = false;
	for (const auto& e : exts) {
		if (!strcmp(e.extensionName,
		            VK_GOOGLE_DISPLAY_TIMING_EXTENSION_NAME)) {
			has_timing = true;
		}
		if (!strcmp(e.extensionName, "VK_KHR_portability_subset")) {
			has_portability = true;
		}
	}
	printf("[4] VK_GOOGLE_display_timing: %s\n",
	       has_timing ? "SUPPORTED" : "NOT SUPPORTED");
	if (!has_timing) {
		_exit(2);
	}

	// ------------------------- device -------------------------
	uint32_t qf_count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(pd, &qf_count, nullptr);
	std::vector<VkQueueFamilyProperties> qfs(qf_count);
	vkGetPhysicalDeviceQueueFamilyProperties(pd, &qf_count, qfs.data());
	uint32_t qf = UINT32_MAX;
	for (uint32_t i = 0; i < qf_count; ++i) {
		VkBool32 present_ok = VK_FALSE;
		vkGetPhysicalDeviceSurfaceSupportKHR(pd, i, surface, &present_ok);
		if ((qfs[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && present_ok) {
			qf = i;
			break;
		}
	}

	float prio                 = 1.0f;
	VkDeviceQueueCreateInfo qi = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
	qi.queueFamilyIndex        = qf;
	qi.queueCount              = 1;
	qi.pQueuePriorities        = &prio;

	std::vector<const char*> dev_exts = {
	        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
	        VK_GOOGLE_DISPLAY_TIMING_EXTENSION_NAME,
	};
	if (has_portability) {
		dev_exts.push_back("VK_KHR_portability_subset");
	}

	VkDeviceCreateInfo dci      = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
	dci.queueCreateInfoCount    = 1;
	dci.pQueueCreateInfos       = &qi;
	dci.enabledExtensionCount   = (uint32_t)dev_exts.size();
	dci.ppEnabledExtensionNames = dev_exts.data();

	VkDevice device = VK_NULL_HANDLE;
	CHECK(vkCreateDevice(pd, &dci, nullptr, &device));
	VkQueue queue = VK_NULL_HANDLE;
	vkGetDeviceQueue(device, qf, 0, &queue);

	auto pfnGetRefresh = (PFN_vkGetRefreshCycleDurationGOOGLE)
	        vkGetDeviceProcAddr(device, "vkGetRefreshCycleDurationGOOGLE");
	auto pfnGetPast = (PFN_vkGetPastPresentationTimingGOOGLE)
	        vkGetDeviceProcAddr(device, "vkGetPastPresentationTimingGOOGLE");
	if (!pfnGetRefresh || !pfnGetPast) {
		printf("timing entry points missing\n");
		_exit(3);
	}
	printf("[5] device + timing entry points ready\n");

	// ------------------------ swapchain ------------------------
	VkSurfaceCapabilitiesKHR caps = {};
	CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pd, surface, &caps));

	VkSwapchainCreateInfoKHR sc = {
	        VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
	sc.surface          = surface;
	sc.minImageCount    = std::max(caps.minImageCount, 3u);
	sc.imageFormat      = VK_FORMAT_B8G8R8A8_UNORM;
	sc.imageColorSpace  = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	sc.imageExtent      = caps.currentExtent;
	sc.imageArrayLayers = 1;
	sc.imageUsage       = VK_IMAGE_USAGE_TRANSFER_DST_BIT |
	               VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	sc.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	sc.preTransform     = caps.currentTransform;
	sc.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	sc.presentMode      = VK_PRESENT_MODE_FIFO_KHR;
	sc.clipped          = VK_TRUE;

	VkSwapchainKHR swapchain = VK_NULL_HANDLE;
	CHECK(vkCreateSwapchainKHR(device, &sc, nullptr, &swapchain));

	uint32_t img_count = 0;
	CHECK(vkGetSwapchainImagesKHR(device, swapchain, &img_count, nullptr));
	std::vector<VkImage> images(img_count);
	CHECK(vkGetSwapchainImagesKHR(device, swapchain, &img_count,
	                              images.data()));
	printf("[6] swapchain: %u images, %ux%u\n", img_count,
	       sc.imageExtent.width, sc.imageExtent.height);

	VkRefreshCycleDurationGOOGLE refresh = {};
	CHECK(pfnGetRefresh(device, swapchain, &refresh));
	printf("[7] refreshDuration: %.3f ms (%.1f Hz)\n",
	       refresh.refreshDuration / 1e6, 1e9 / refresh.refreshDuration);

	// ------- per-image command buffers (clear + transitions) -------
	VkCommandPoolCreateInfo cpci = {
	        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
	cpci.queueFamilyIndex = qf;
	VkCommandPool pool    = VK_NULL_HANDLE;
	CHECK(vkCreateCommandPool(device, &cpci, nullptr, &pool));

	VkCommandBufferAllocateInfo cbai = {
	        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
	cbai.commandPool        = pool;
	cbai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cbai.commandBufferCount = img_count;
	std::vector<VkCommandBuffer> cmds(img_count);
	CHECK(vkAllocateCommandBuffers(device, &cbai, cmds.data()));

	for (uint32_t i = 0; i < img_count; ++i) {
		VkCommandBufferBeginInfo bi = {
		        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
		CHECK(vkBeginCommandBuffer(cmds[i], &bi));

		VkImageSubresourceRange range = {VK_IMAGE_ASPECT_COLOR_BIT, 0,
		                                 1, 0, 1};

		VkImageMemoryBarrier to_dst = {
		        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
		to_dst.srcAccessMask    = 0;
		to_dst.dstAccessMask    = VK_ACCESS_TRANSFER_WRITE_BIT;
		to_dst.oldLayout        = VK_IMAGE_LAYOUT_UNDEFINED;
		to_dst.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		to_dst.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		to_dst.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		to_dst.image               = images[i];
		to_dst.subresourceRange    = range;
		vkCmdPipelineBarrier(cmds[i],
		                     VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		                     VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,
		                     nullptr, 0, nullptr, 1, &to_dst);

		const float phase       = (float)i / (float)img_count;
		VkClearColorValue color = {
		        {phase, 0.3f, 1.0f - phase, 1.0f}};
		vkCmdClearColorImage(cmds[i], images[i],
		                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		                     &color, 1, &range);

		VkImageMemoryBarrier to_present = to_dst;
		to_present.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		to_present.dstAccessMask = 0;
		to_present.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		to_present.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		vkCmdPipelineBarrier(cmds[i], VK_PIPELINE_STAGE_TRANSFER_BIT,
		                     VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0,
		                     0, nullptr, 0, nullptr, 1, &to_present);
		CHECK(vkEndCommandBuffer(cmds[i]));
	}

	// --------------------- sync primitives ---------------------
	VkSemaphoreCreateInfo sem_ci = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
	VkFenceCreateInfo fence_ci   = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
	fence_ci.flags               = VK_FENCE_CREATE_SIGNALED_BIT;

	VkSemaphore acquire_sems[MaxInFlight] = {};
	VkFence     frame_fences[MaxInFlight] = {};
	std::vector<VkSemaphore> render_sems(img_count);
	for (uint32_t i = 0; i < MaxInFlight; ++i) {
		CHECK(vkCreateSemaphore(device, &sem_ci, nullptr,
		                        &acquire_sems[i]));
		CHECK(vkCreateFence(device, &fence_ci, nullptr,
		                    &frame_fences[i]));
	}
	for (uint32_t i = 0; i < img_count; ++i) {
		CHECK(vkCreateSemaphore(device, &sem_ci, nullptr,
		                        &render_sems[i]));
	}
	printf("[8] beginning %d timed presents @ %.3f Hz\n", NumFrames,
	       DosRateHz);

	// ------------------------ present loop ------------------------
	const uint64_t period_ns = (uint64_t)(1e9 / DosRateHz);
	// Schedule well ahead (Spike 4 lesson: >= 2-3 refresh cycles)
	const uint64_t t0 = NowNs() + 500'000'000ull;

	std::map<uint32_t, Feedback> feedback; // presentID -> times
	int presented = 0;

	for (int i = 0; i < NumFrames; ++i) {
		const uint64_t target = t0 + (uint64_t)i * period_ns;

		// submit ~10 ms ahead of target (display engine holds the
		// flip; scheduling ahead of pipeline depth)
		const uint64_t wake = target - 10'000'000ull;
		uint64_t now        = NowNs();
		if (wake > now) {
			usleep((useconds_t)((wake - now) / 1000));
		}

		const uint32_t slot = i % MaxInFlight;
		CHECK(vkWaitForFences(device, 1, &frame_fences[slot], VK_TRUE,
		                      UINT64_MAX));
		CHECK(vkResetFences(device, 1, &frame_fences[slot]));

		uint32_t img = 0;
		VkResult ar = vkAcquireNextImageKHR(device, swapchain,
		                                    UINT64_MAX,
		                                    acquire_sems[slot],
		                                    VK_NULL_HANDLE, &img);
		if (ar != VK_SUCCESS && ar != VK_SUBOPTIMAL_KHR) {
			printf("acquire failed: %d\n", ar);
			break;
		}

		VkPipelineStageFlags wait_stage =
		        VK_PIPELINE_STAGE_TRANSFER_BIT;
		VkSubmitInfo si         = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
		si.waitSemaphoreCount   = 1;
		si.pWaitSemaphores      = &acquire_sems[slot];
		si.pWaitDstStageMask    = &wait_stage;
		si.commandBufferCount   = 1;
		si.pCommandBuffers      = &cmds[img];
		si.signalSemaphoreCount = 1;
		si.pSignalSemaphores    = &render_sems[img];
		CHECK(vkQueueSubmit(queue, 1, &si, frame_fences[slot]));

		VkPresentTimeGOOGLE ptime = {};
		ptime.presentID           = (uint32_t)(i + 1);
		ptime.desiredPresentTime  = target;

		VkPresentTimesInfoGOOGLE ptimes = {
		        VK_STRUCTURE_TYPE_PRESENT_TIMES_INFO_GOOGLE};
		ptimes.swapchainCount = 1;
		ptimes.pTimes         = &ptime;

		VkPresentInfoKHR pi    = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
		pi.pNext               = &ptimes;
		pi.waitSemaphoreCount  = 1;
		pi.pWaitSemaphores     = &render_sems[img];
		pi.swapchainCount      = 1;
		pi.pSwapchains         = &swapchain;
		pi.pImageIndices       = &img;
		VkResult pr = vkQueuePresentKHR(queue, &pi);
		if (pr != VK_SUCCESS && pr != VK_SUBOPTIMAL_KHR) {
			printf("present failed: %d\n", pr);
			break;
		}
		++presented;

		// poll past-presentation feedback
		uint32_t n = 0;
		pfnGetPast(device, swapchain, &n, nullptr);
		if (n > 0) {
			std::vector<VkPastPresentationTimingGOOGLE> past(n);
			pfnGetPast(device, swapchain, &n, past.data());
			for (uint32_t k = 0; k < n; ++k) {
				feedback[past[k].presentID] = {
				        past[k].desiredPresentTime,
				        past[k].actualPresentTime};
			}
		}

		// keep the Cocoa runloop serviced
		@autoreleasepool {
			NSEvent* ev = [NSApp
			        nextEventMatchingMask:NSEventMaskAny
			                    untilDate:[NSDate date]
			                       inMode:NSDefaultRunLoopMode
			                      dequeue:YES];
			if (ev) {
				[NSApp sendEvent:ev];
			}
		}
	}

	// drain the last feedback entries
	vkDeviceWaitIdle(device);
	usleep(300000);
	uint32_t n = 0;
	pfnGetPast(device, swapchain, &n, nullptr);
	if (n > 0) {
		std::vector<VkPastPresentationTimingGOOGLE> past(n);
		pfnGetPast(device, swapchain, &n, past.data());
		for (uint32_t k = 0; k < n; ++k) {
			feedback[past[k].presentID] = {
			        past[k].desiredPresentTime,
			        past[k].actualPresentTime};
		}
	}

	// -------------------------- report --------------------------
	std::vector<double> err_ms, interval_ms;
	uint64_t prev_actual = 0;
	for (const auto& [id, fb] : feedback) {
		if (fb.actual == 0) {
			continue;
		}
		err_ms.push_back(((double)fb.actual - (double)fb.desired) / 1e6);
		if (prev_actual) {
			interval_ms.push_back(
			        ((double)fb.actual - (double)prev_actual) / 1e6);
		}
		prev_actual = fb.actual;
	}

	auto stats = [](std::vector<double> v, const char* name) {
		if (v.empty()) {
			printf("%s: no data\n", name);
			return;
		}
		std::sort(v.begin(), v.end());
		double sum = 0;
		for (double x : v) {
			sum += x;
		}
		printf("%s: mean=%.3f p50=%.3f p95=%.3f min=%.3f max=%.3f (n=%zu)\n",
		       name, sum / v.size(), v[v.size() / 2],
		       v[(size_t)(v.size() * 0.95)], v.front(), v.back(),
		       v.size());
	};

	printf("\n=== MoltenVK VK_GOOGLE_display_timing @ %.3f Hz, %d frames ===\n",
	       DosRateHz, NumFrames);
	printf("target period: %.3f ms | presents submitted: %d | feedback entries: %zu\n",
	       1000.0 / DosRateHz, presented, feedback.size());
	stats(err_ms, "actual vs desired [ms]");
	stats(interval_ms, "actual interval  [ms]");

	if (!interval_ms.empty()) {
		printf("interval histogram:\n");
		for (double lo = 4.0; lo < 26.0; lo += 1.0) {
			int cnt = 0;
			for (double x : interval_ms) {
				if (x >= lo && x < lo + 1.0) {
					++cnt;
				}
			}
			if (cnt > 0) {
				printf("  %5.1f-%5.1f ms: %4d %s\n", lo, lo + 1,
				       cnt,
				       std::string(std::min(cnt, 60), '#').c_str());
			}
		}
	}
	fflush(stdout);
	_exit(0);
}
