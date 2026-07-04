// VK_EXT_present_timing spike (Spike 6): present clear-colour frames at a
// 70.086 Hz cadence through native desktop Vulkan + VK_EXT_present_timing, and
// read back per-present feedback to prove the display engine actually paces to
// the target. Desktop analogue of Spike 5 (MoltenVK + VK_GOOGLE_display_timing).
//
// It ALSO times vkAcquireNextImageKHR and vkQueuePresentKHR to answer whether
// present blocks the calling thread on NVIDIA-Windows FIFO (the "emulation
// never stalls on the display" invariant).
//
// SCHEDULING MODE (learned from a first run on an RTX 3060 / R595 driver):
//   NVIDIA reports presentAtAbsoluteTimeSupported=0 and does NOT offer the
//   FIRST_PIXEL_VISIBLE stage or a host-clock time domain. The spec only
//   guarantees relative-time scheduling and the QUEUE_OPERATIONS_END stage. So
//   this spike schedules with RELATIVE time (targetTime = the frame period, a
//   plain nanosecond duration needing no calibration) and measures the
//   best stage the surface offers (FIRST_PIXEL_VISIBLE if present, else
//   QUEUE_OPERATIONS_END). The pacing question is: do consecutive feedback
//   timestamps sit ~14.268 ms apart?
//
// Build (from this directory, with VCPKG_ROOT set):
//   cmake --preset linux   && cmake --build --preset linux
//   cmake --preset windows && cmake --build --preset windows
//   cmake --preset macos   && cmake --build --preset macos   (compiles only;
//     MoltenVK does not expose VK_EXT_present_timing, so it exits at step [3])
//
// Written against Vulkan-Headers 1.4.350 (spec v3). This is a spike: minimal
// error recovery, deliberate leaks before exit.

#ifdef _WIN32
// SDL pulls in <windows.h>, which defines min/max macros that break std::min/
// std::max. Define this before any header to suppress them.
#define NOMINMAX
#endif

#include <vulkan/vulkan.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <thread>
#include <vector>

static constexpr double   DosRateHz   = 70.086;
static constexpr int      NumFrames   = 300;
static constexpr uint32_t MaxInFlight = 2;

#define CHECK(x)                                                               \
	do {                                                                   \
		VkResult r_ = (x);                                             \
		if (r_ != VK_SUCCESS) {                                        \
			printf("FAIL %s -> %d (line %d)\n", #x, r_, __LINE__); \
			std::exit(1);                                         \
		}                                                             \
	} while (0)

static uint64_t SteadyNs()
{
	return (uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(
	        std::chrono::steady_clock::now().time_since_epoch())
	        .count();
}

static void PrintStages(const char* label, VkPresentStageFlagsEXT s)
{
	printf("%s0x%x [", label, s);
	if (s & VK_PRESENT_STAGE_QUEUE_OPERATIONS_END_BIT_EXT)
		printf(" queue_ops_end");
	if (s & VK_PRESENT_STAGE_REQUEST_DEQUEUED_BIT_EXT)
		printf(" request_dequeued");
	if (s & VK_PRESENT_STAGE_IMAGE_FIRST_PIXEL_OUT_BIT_EXT)
		printf(" first_pixel_out");
	if (s & VK_PRESENT_STAGE_IMAGE_FIRST_PIXEL_VISIBLE_BIT_EXT)
		printf(" first_pixel_visible");
	printf(" ]\n");
}

static void Stats(std::vector<double> v, const char* name)
{
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
	       v[(size_t)(v.size() * 0.95)], v.front(), v.back(), v.size());
}

int main(int argc, char** argv)
{
	setvbuf(stdout, nullptr, _IONBF, 0);
	bool windowed = false;
	for (int i = 1; i < argc; ++i) {
		if (!strcmp(argv[i], "--windowed")) {
			windowed = true;
		}
	}

	// --------------------------- window ---------------------------
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		printf("SDL_Init failed: %s\n", SDL_GetError());
		return 1;
	}
	if (!SDL_Vulkan_LoadLibrary(nullptr)) {
		printf("SDL_Vulkan_LoadLibrary failed: %s\n", SDL_GetError());
		return 1;
	}
	SDL_WindowFlags flags = SDL_WINDOW_VULKAN;
	if (!windowed) {
		flags |= SDL_WINDOW_FULLSCREEN;
	}
	SDL_Window* window = SDL_CreateWindow("vulkan_ext_present_timing_spike",
	                                      1280, 720, flags);
	if (!window) {
		printf("SDL_CreateWindow failed: %s\n", SDL_GetError());
		return 1;
	}
	printf("[1] window up (%s). Fullscreen is required for real pacing "
	       "numbers; pass --windowed to compare.\n",
	       windowed ? "windowed" : "fullscreen");

	// ------------------------- instance -------------------------
	uint32_t           sdl_ext_count = 0;
	char const* const* sdl_exts = SDL_Vulkan_GetInstanceExtensions(&sdl_ext_count);
	std::vector<const char*> inst_exts(sdl_exts, sdl_exts + sdl_ext_count);
	inst_exts.push_back(VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME);

	VkApplicationInfo app_info = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
	app_info.pApplicationName  = "vulkan_ext_present_timing_spike";
	app_info.apiVersion        = VK_API_VERSION_1_3;

	VkInstanceCreateInfo ici    = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
	ici.pApplicationInfo        = &app_info;
	ici.enabledExtensionCount   = (uint32_t)inst_exts.size();
	ici.ppEnabledExtensionNames = inst_exts.data();

	VkInstance instance = VK_NULL_HANDLE;
	CHECK(vkCreateInstance(&ici, nullptr, &instance));

	VkSurfaceKHR surface = VK_NULL_HANDLE;
	if (!SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface)) {
		printf("SDL_Vulkan_CreateSurface failed: %s\n", SDL_GetError());
		return 1;
	}
	printf("[2] instance + surface created\n");

	// --------------------- physical device ---------------------
	uint32_t pd_count = 0;
	CHECK(vkEnumeratePhysicalDevices(instance, &pd_count, nullptr));
	std::vector<VkPhysicalDevice> pds(pd_count);
	CHECK(vkEnumeratePhysicalDevices(instance, &pd_count, pds.data()));

	VkPhysicalDevice pd = VK_NULL_HANDLE;
	for (auto cand : pds) {
		uint32_t n = 0;
		vkEnumerateDeviceExtensionProperties(cand, nullptr, &n, nullptr);
		std::vector<VkExtensionProperties> exts(n);
		vkEnumerateDeviceExtensionProperties(cand, nullptr, &n, exts.data());
		for (const auto& e : exts) {
			if (!strcmp(e.extensionName,
			            VK_EXT_PRESENT_TIMING_EXTENSION_NAME)) {
				pd = cand;
				break;
			}
		}
		if (pd) {
			break;
		}
	}
	if (!pd) {
		printf("[3] no device exposes VK_EXT_present_timing - this GPU/"
		       "driver is a tier-3 (app-timed) target. Exiting.\n");
		std::exit(2);
	}
	VkPhysicalDeviceProperties props = {};
	vkGetPhysicalDeviceProperties(pd, &props);
	printf("[3] device: %s (Vulkan %d.%d) exposes VK_EXT_present_timing\n",
	       props.deviceName, VK_API_VERSION_MAJOR(props.apiVersion),
	       VK_API_VERSION_MINOR(props.apiVersion));

	// ------------------- device features -------------------
	VkPhysicalDevicePresentId2FeaturesKHR feat_pid = {
	        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_ID_2_FEATURES_KHR};
	VkPhysicalDevicePresentTimingFeaturesEXT feat_pt = {
	        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_TIMING_FEATURES_EXT};
	feat_pt.pNext                    = &feat_pid;
	VkPhysicalDeviceFeatures2 feats2 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
	feats2.pNext                     = &feat_pt;
	vkGetPhysicalDeviceFeatures2(pd, &feats2);
	printf("[4] device features: presentTiming=%u absoluteTime=%u "
	       "relativeTime=%u presentId2=%u\n",
	       feat_pt.presentTiming, feat_pt.presentAtAbsoluteTime,
	       feat_pt.presentAtRelativeTime, feat_pid.presentId2);
	if (!feat_pt.presentTiming) {
		printf("presentTiming device feature missing\n");
		std::exit(2);
	}

	// ------------------- surface caps -------------------
	auto pfnSurfCaps2 = (PFN_vkGetPhysicalDeviceSurfaceCapabilities2KHR)
	        vkGetInstanceProcAddr(instance,
	                              "vkGetPhysicalDeviceSurfaceCapabilities2KHR");
	VkPresentTimingSurfaceCapabilitiesEXT ts_caps = {
	        VK_STRUCTURE_TYPE_PRESENT_TIMING_SURFACE_CAPABILITIES_EXT};
	VkSurfaceCapabilities2KHR caps2 = {VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR};
	caps2.pNext                     = &ts_caps;
	VkPhysicalDeviceSurfaceInfo2KHR surf_info = {
	        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR};
	surf_info.surface = surface;
	CHECK(pfnSurfCaps2(pd, &surf_info, &caps2));
	printf("[5] surface caps: timingSupported=%u absoluteTime=%u "
	       "relativeTime=%u\n",
	       ts_caps.presentTimingSupported,
	       ts_caps.presentAtAbsoluteTimeSupported,
	       ts_caps.presentAtRelativeTimeSupported);
	PrintStages("    presentStageQueries: ", ts_caps.presentStageQueries);
	if (!ts_caps.presentTimingSupported) {
		printf("present timing not supported on this surface\n");
		std::exit(2);
	}

	// Choose scheduling mode: prefer relative (needs no host-clock
	// calibration and is what NVIDIA offers); absolute is the alternative.
	const bool use_relative = feat_pt.presentAtRelativeTime &&
	                          ts_caps.presentAtRelativeTimeSupported;
	if (!use_relative) {
		printf("relative-time scheduling unavailable (device relative=%u, "
		       "surface relative=%u). Absolute-time needs calibration, not "
		       "implemented in this spike. Exiting.\n",
		       feat_pt.presentAtRelativeTime,
		       ts_caps.presentAtRelativeTimeSupported);
		std::exit(2);
	}

	// Choose the feedback stage: photons-on-glass if offered, else the
	// guaranteed queue-operations-end.
	VkPresentStageFlagBitsEXT stage;
	if (ts_caps.presentStageQueries &
	    VK_PRESENT_STAGE_IMAGE_FIRST_PIXEL_VISIBLE_BIT_EXT) {
		stage = VK_PRESENT_STAGE_IMAGE_FIRST_PIXEL_VISIBLE_BIT_EXT;
	} else if (ts_caps.presentStageQueries &
	           VK_PRESENT_STAGE_QUEUE_OPERATIONS_END_BIT_EXT) {
		stage = VK_PRESENT_STAGE_QUEUE_OPERATIONS_END_BIT_EXT;
	} else {
		printf("no usable feedback stage advertised\n");
		std::exit(2);
	}
	printf("[5] scheduling=relative, feedback stage=0x%x %s\n", stage,
	       stage == VK_PRESENT_STAGE_IMAGE_FIRST_PIXEL_VISIBLE_BIT_EXT
	               ? "(first_pixel_visible = true glass time)"
	               : "(queue_operations_end = GPU-done, not scanout)");

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

	float                   prio = 1.0f;
	VkDeviceQueueCreateInfo qi   = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
	qi.queueFamilyIndex          = qf;
	qi.queueCount                = 1;
	qi.pQueuePriorities          = &prio;

	const char* dev_exts[] = {
	        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
	        VK_EXT_PRESENT_TIMING_EXTENSION_NAME,
	        VK_KHR_PRESENT_ID_2_EXTENSION_NAME,
	        VK_KHR_CALIBRATED_TIMESTAMPS_EXTENSION_NAME,
	};

	VkPhysicalDevicePresentId2FeaturesKHR en_pid = {
	        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_ID_2_FEATURES_KHR};
	en_pid.presentId2 = feat_pid.presentId2;
	VkPhysicalDevicePresentTimingFeaturesEXT en_pt = {
	        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_TIMING_FEATURES_EXT};
	en_pt.presentTiming        = VK_TRUE;
	en_pt.presentAtRelativeTime = VK_TRUE;
	en_pt.pNext                = &en_pid;

	VkDeviceCreateInfo dci      = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
	dci.pNext                   = &en_pt;
	dci.queueCreateInfoCount    = 1;
	dci.pQueueCreateInfos       = &qi;
	dci.enabledExtensionCount   = (uint32_t)(sizeof(dev_exts) / sizeof(dev_exts[0]));
	dci.ppEnabledExtensionNames = dev_exts;

	VkDevice device = VK_NULL_HANDLE;
	CHECK(vkCreateDevice(pd, &dci, nullptr, &device));
	VkQueue queue = VK_NULL_HANDLE;
	vkGetDeviceQueue(device, qf, 0, &queue);

	auto pfnSetQueueSize = (PFN_vkSetSwapchainPresentTimingQueueSizeEXT)
	        vkGetDeviceProcAddr(device, "vkSetSwapchainPresentTimingQueueSizeEXT");
	auto pfnTimingProps = (PFN_vkGetSwapchainTimingPropertiesEXT)
	        vkGetDeviceProcAddr(device, "vkGetSwapchainTimingPropertiesEXT");
	auto pfnTimeDomains = (PFN_vkGetSwapchainTimeDomainPropertiesEXT)
	        vkGetDeviceProcAddr(device, "vkGetSwapchainTimeDomainPropertiesEXT");
	auto pfnGetPast = (PFN_vkGetPastPresentationTimingEXT)
	        vkGetDeviceProcAddr(device, "vkGetPastPresentationTimingEXT");
	if (!pfnSetQueueSize || !pfnTimingProps || !pfnTimeDomains || !pfnGetPast) {
		printf("timing entry points missing\n");
		std::exit(3);
	}
	printf("[6] device + present-timing entry points ready\n");

	// ------------------------ swapchain ------------------------
	VkSurfaceCapabilitiesKHR caps = {};
	CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pd, surface, &caps));

	VkSwapchainCreateInfoKHR sc = {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
	sc.flags            = VK_SWAPCHAIN_CREATE_PRESENT_TIMING_BIT_EXT;
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
	CHECK(vkGetSwapchainImagesKHR(device, swapchain, &img_count, images.data()));

	CHECK(pfnSetQueueSize(device, swapchain, MaxInFlight + 4));

	VkSwapchainTimingPropertiesEXT tprops = {
	        VK_STRUCTURE_TYPE_SWAPCHAIN_TIMING_PROPERTIES_EXT};
	uint64_t tprops_counter = 0;
	CHECK(pfnTimingProps(device, swapchain, &tprops, &tprops_counter));
	printf("[7] swapchain: %u images, %ux%u | refreshDuration=%.3f ms "
	       "refreshInterval=%.3f ms\n",
	       img_count, sc.imageExtent.width, sc.imageExtent.height,
	       tprops.refreshDuration / 1e6, tprops.refreshInterval / 1e6);

	// Time domain id (relative-time still needs one for the feedback results).
	VkSwapchainTimeDomainPropertiesEXT tdp = {
	        VK_STRUCTURE_TYPE_SWAPCHAIN_TIME_DOMAIN_PROPERTIES_EXT};
	uint64_t td_counter = 0;
	CHECK(pfnTimeDomains(device, swapchain, &tdp, &td_counter));
	std::vector<VkTimeDomainKHR> domains(tdp.timeDomainCount);
	std::vector<uint64_t>        domain_ids(tdp.timeDomainCount);
	tdp.pTimeDomains   = domains.data();
	tdp.pTimeDomainIds = domain_ids.data();
	CHECK(pfnTimeDomains(device, swapchain, &tdp, &td_counter));
	if (tdp.timeDomainCount == 0) {
		printf("no swapchain time domains\n");
		std::exit(4);
	}
	const uint64_t time_domain_id = domain_ids[0];
	printf("[8] time domains: ");
	for (uint32_t i = 0; i < tdp.timeDomainCount; ++i) {
		printf("%d(id=%llu) ", (int)domains[i],
		       (unsigned long long)domain_ids[i]);
	}
	printf("| using id=%llu (feedback times reported in this domain; "
	       "intervals below assume it counts nanoseconds)\n",
	       (unsigned long long)time_domain_id);

	// ---------------- per-image clear command buffers ----------------
	VkCommandPoolCreateInfo cpci = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
	cpci.queueFamilyIndex        = qf;
	VkCommandPool pool           = VK_NULL_HANDLE;
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
		VkImageSubresourceRange range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

		VkImageMemoryBarrier to_dst = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
		to_dst.dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
		to_dst.oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
		to_dst.newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		to_dst.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		to_dst.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		to_dst.image               = images[i];
		to_dst.subresourceRange    = range;
		vkCmdPipelineBarrier(cmds[i], VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		                     VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr,
		                     0, nullptr, 1, &to_dst);

		const float       phase = (float)i / (float)img_count;
		VkClearColorValue color = {{phase, 0.3f, 1.0f - phase, 1.0f}};
		vkCmdClearColorImage(cmds[i], images[i],
		                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &color, 1,
		                     &range);

		VkImageMemoryBarrier to_present = to_dst;
		to_present.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		to_present.dstAccessMask = 0;
		to_present.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		to_present.newLayout     = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		vkCmdPipelineBarrier(cmds[i], VK_PIPELINE_STAGE_TRANSFER_BIT,
		                     VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0,
		                     nullptr, 0, nullptr, 1, &to_present);
		CHECK(vkEndCommandBuffer(cmds[i]));
	}

	// --------------------- sync primitives ---------------------
	VkSemaphoreCreateInfo sem_ci   = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
	VkFenceCreateInfo     fence_ci = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
	fence_ci.flags                 = VK_FENCE_CREATE_SIGNALED_BIT;
	VkSemaphore acquire_sems[MaxInFlight] = {};
	VkFence     frame_fences[MaxInFlight] = {};
	std::vector<VkSemaphore> render_sems(img_count);
	for (uint32_t i = 0; i < MaxInFlight; ++i) {
		CHECK(vkCreateSemaphore(device, &sem_ci, nullptr, &acquire_sems[i]));
		CHECK(vkCreateFence(device, &fence_ci, nullptr, &frame_fences[i]));
	}
	for (uint32_t i = 0; i < img_count; ++i) {
		CHECK(vkCreateSemaphore(device, &sem_ci, nullptr, &render_sems[i]));
	}
	printf("[9] beginning %d relative-timed presents @ %.3f Hz\n", NumFrames,
	       DosRateHz);

	// ------------------- feedback poll (reused buffers) -------------------
	std::map<uint64_t, uint64_t> feedback;  // presentId -> chosen-stage time
	static constexpr uint32_t    kMaxTimings = 64;
	static constexpr uint32_t    kMaxStages  = 8;
	std::vector<VkPastPresentationTimingEXT> timings(kMaxTimings);
	std::vector<VkPresentStageTimeEXT>       stage_store(kMaxTimings * kMaxStages);

	auto poll_feedback = [&]() {
		for (uint32_t i = 0; i < kMaxTimings; ++i) {
			timings[i] = {VK_STRUCTURE_TYPE_PAST_PRESENTATION_TIMING_EXT};
			timings[i].presentStageCount = kMaxStages;
			timings[i].pPresentStages    = &stage_store[i * kMaxStages];
		}
		VkPastPresentationTimingInfoEXT info = {
		        VK_STRUCTURE_TYPE_PAST_PRESENTATION_TIMING_INFO_EXT};
		info.swapchain = swapchain;
		VkPastPresentationTimingPropertiesEXT pp = {
		        VK_STRUCTURE_TYPE_PAST_PRESENTATION_TIMING_PROPERTIES_EXT};
		pp.presentationTimingCount = kMaxTimings;
		pp.pPresentationTimings    = timings.data();
		VkResult r = pfnGetPast(device, &info, &pp);
		if (r != VK_SUCCESS && r != VK_INCOMPLETE) {
			return;
		}
		for (uint32_t k = 0; k < pp.presentationTimingCount; ++k) {
			const auto& t = timings[k];
			for (uint32_t s = 0; s < t.presentStageCount; ++s) {
				if (t.pPresentStages[s].stage & stage) {
					feedback[t.presentId] = t.pPresentStages[s].time;
				}
			}
		}
	};

	// ------------------------ present loop ------------------------
	// Relative-time targetTime is the minimum visible duration of the PREVIOUS
	// image, in nanoseconds. Setting it to the frame period makes the display
	// engine pace the stream; no CPU-side sleep needed (the 2-frame fence ring
	// throttles submission), so any pacing we measure is the driver's.
	const uint64_t period_ns = (uint64_t)(1e9 / DosRateHz);

	std::vector<double> present_call_ms, acquire_call_ms;
	int                 presented = 0;

	for (int i = 0; i < NumFrames; ++i) {
		const uint32_t slot = (uint32_t)i % MaxInFlight;
		CHECK(vkWaitForFences(device, 1, &frame_fences[slot], VK_TRUE,
		                      UINT64_MAX));
		CHECK(vkResetFences(device, 1, &frame_fences[slot]));

		uint32_t       img       = 0;
		const uint64_t acq_start = SteadyNs();
		VkResult       ar = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX,
		                                          acquire_sems[slot],
		                                          VK_NULL_HANDLE, &img);
		acquire_call_ms.push_back((double)(SteadyNs() - acq_start) / 1e6);
		if (ar != VK_SUCCESS && ar != VK_SUBOPTIMAL_KHR) {
			printf("acquire failed: %d\n", ar);
			break;
		}

		VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		VkSubmitInfo         si = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
		si.waitSemaphoreCount   = 1;
		si.pWaitSemaphores      = &acquire_sems[slot];
		si.pWaitDstStageMask    = &wait_stage;
		si.commandBufferCount   = 1;
		si.pCommandBuffers      = &cmds[img];
		si.signalSemaphoreCount = 1;
		si.pSignalSemaphores    = &render_sems[img];
		CHECK(vkQueueSubmit(queue, 1, &si, frame_fences[slot]));

		const uint64_t present_id = (uint64_t)(i + 1);

		VkPresentTimingInfoEXT ti = {VK_STRUCTURE_TYPE_PRESENT_TIMING_INFO_EXT};
		ti.flags                        = VK_PRESENT_TIMING_INFO_PRESENT_AT_RELATIVE_TIME_BIT_EXT;
		ti.targetTime                   = period_ns;
		ti.timeDomainId                 = time_domain_id;
		ti.presentStageQueries          = stage;
		ti.targetTimeDomainPresentStage = stage;

		VkPresentTimingsInfoEXT tis = {VK_STRUCTURE_TYPE_PRESENT_TIMINGS_INFO_EXT};
		tis.swapchainCount          = 1;
		tis.pTimingInfos            = &ti;

		VkPresentId2KHR pid = {VK_STRUCTURE_TYPE_PRESENT_ID_2_KHR};
		pid.pNext           = &tis;
		pid.swapchainCount  = 1;
		pid.pPresentIds     = &present_id;

		VkPresentInfoKHR pi   = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
		pi.pNext              = &pid;
		pi.waitSemaphoreCount = 1;
		pi.pWaitSemaphores    = &render_sems[img];
		pi.swapchainCount     = 1;
		pi.pSwapchains        = &swapchain;
		pi.pImageIndices      = &img;

		const uint64_t pres_start = SteadyNs();
		VkResult       pr         = vkQueuePresentKHR(queue, &pi);
		present_call_ms.push_back((double)(SteadyNs() - pres_start) / 1e6);
		if (pr != VK_SUCCESS && pr != VK_SUBOPTIMAL_KHR) {
			printf("present failed: %d\n", pr);
			break;
		}
		++presented;
		poll_feedback();

		SDL_Event ev;
		while (SDL_PollEvent(&ev)) {
		}
	}

	vkDeviceWaitIdle(device);
	std::this_thread::sleep_for(std::chrono::milliseconds(300));
	poll_feedback();

	// -------------------------- report --------------------------
	std::vector<double> interval_ms, interval_err_ms;
	const double        period_ms = 1000.0 / DosRateHz;
	uint64_t            prev      = 0;
	for (const auto& [id, t] : feedback) {  // std::map iterates by presentId
		if (prev) {
			const double ms = (double)((int64_t)(t - prev)) / 1e6;
			interval_ms.push_back(ms);
			interval_err_ms.push_back(ms - period_ms);
		}
		prev = t;
	}

	printf("\n=== VK_EXT_present_timing (relative) @ %.3f Hz, %d frames - %s ===\n",
	       DosRateHz, NumFrames, props.deviceName);
	printf("target period: %.3f ms | submitted: %d | feedback entries: %zu | "
	       "stage: %s\n",
	       period_ms, presented, feedback.size(),
	       stage == VK_PRESENT_STAGE_IMAGE_FIRST_PIXEL_VISIBLE_BIT_EXT
	               ? "first_pixel_visible"
	               : "queue_operations_end");
	Stats(interval_ms, "feedback interval [ms]  (want ~14.268)");
	Stats(interval_err_ms, "interval - period [ms]  (jitter; want ~0)");
	Stats(present_call_ms, "vkQueuePresentKHR call [ms]  (blocking check)");
	Stats(acquire_call_ms, "vkAcquireNextImageKHR call [ms]");

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
				printf("  %5.1f-%5.1f ms: %4d %s\n", lo, lo + 1, cnt,
				       std::string(std::min(cnt, 60), '#').c_str());
			}
		}
	}
	fflush(stdout);
	std::exit(0);
}
