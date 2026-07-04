// Present-mode sweep spike (Spike 6): the question is which present mode gives
// smooth 70.086 Hz on a VRR display when we simply pace submission ourselves
// (app-timed) and let the display flip - i.e. the "app-timed + VRR" path, NOT
// VK_EXT_present_timing (which was measured to give 60 Hz + blocking on NVIDIA
// under FIFO relative-time; see the git history of this file).
//
// For each of IMMEDIATE / MAILBOX / FIFO it:
//   - paces submission to exactly 70.086 Hz with SDL_DelayPrecise (which avoids
//     Windows' ~15 ms default timer resolution wrecking the pacing),
//   - measures the ACTUAL on-screen flip interval via VK_KHR_present_wait
//     (vkWaitForPresentKHR returns when a given presentId has been presented;
//     the CPU timestamp when it returns gives clean, monotonic flip times - the
//     same primitive DXVK/Dolphin use to measure real present timing),
//   - measures how long vkQueuePresentKHR itself blocks the calling thread.
//
// A mode whose flip interval sits at ~14.268 ms is doing 70 Hz (VRR engaged).
// ~16.68 ms (two vblanks on a 120 Hz panel) means it fell back to 60 Hz.
//
// Build (from this directory, with VCPKG_ROOT set):
//   cmake --preset windows && cmake --build --preset windows
//   cmake --preset linux   && cmake --build --preset linux
//   cmake --preset macos   && cmake --build --preset macos   (compiles only)
//
// This is a spike: minimal error recovery, deliberate leaks before exit.

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
#include <vector>

static constexpr double   DosRateHz    = 70.086;
static constexpr int      Frames       = 240;  // per mode
static constexpr int      WarmupFrames = 40;   // skipped in stats (VRR settle)
static constexpr uint32_t MaxInFlight  = 2;

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

static const char* ModeName(VkPresentModeKHR m)
{
	switch (m) {
	case VK_PRESENT_MODE_IMMEDIATE_KHR: return "IMMEDIATE";
	case VK_PRESENT_MODE_MAILBOX_KHR: return "MAILBOX";
	case VK_PRESENT_MODE_FIFO_KHR: return "FIFO";
	case VK_PRESENT_MODE_FIFO_RELAXED_KHR: return "FIFO_RELAXED";
	default: return "?";
	}
}

static void Stats(std::vector<double> v, const char* name)
{
	if (v.empty()) {
		printf("    %s: no data\n", name);
		return;
	}
	std::sort(v.begin(), v.end());
	double sum = 0;
	for (double x : v) {
		sum += x;
	}
	printf("    %s: mean=%.3f p50=%.3f p95=%.3f min=%.3f max=%.3f (n=%zu)\n",
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
	SDL_Window* window = SDL_CreateWindow("present-mode sweep", 1280, 720, flags);
	if (!window) {
		printf("SDL_CreateWindow failed: %s\n", SDL_GetError());
		return 1;
	}
	printf("[1] window up (%s)\n", windowed ? "windowed" : "fullscreen");

	// ------------------------- instance -------------------------
	uint32_t           sdl_ext_count = 0;
	char const* const* sdl_exts = SDL_Vulkan_GetInstanceExtensions(&sdl_ext_count);
	std::vector<const char*> inst_exts(sdl_exts, sdl_exts + sdl_ext_count);

	VkApplicationInfo app_info = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
	app_info.pApplicationName  = "present_mode_sweep";
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
	VkPhysicalDevice pd = pds.empty() ? VK_NULL_HANDLE : pds[0];
	if (!pd) {
		printf("no physical device\n");
		return 1;
	}
	VkPhysicalDeviceProperties props = {};
	vkGetPhysicalDeviceProperties(pd, &props);
	printf("[3] device: %s (Vulkan %d.%d)\n", props.deviceName,
	       VK_API_VERSION_MAJOR(props.apiVersion),
	       VK_API_VERSION_MINOR(props.apiVersion));

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

	// present_id + present_wait (v1) are how we measure the real flip time.
	uint32_t ext_count = 0;
	vkEnumerateDeviceExtensionProperties(pd, nullptr, &ext_count, nullptr);
	std::vector<VkExtensionProperties> exts(ext_count);
	vkEnumerateDeviceExtensionProperties(pd, nullptr, &ext_count, exts.data());
	bool has_id_ext = false, has_wait_ext = false;
	for (const auto& e : exts) {
		if (!strcmp(e.extensionName, VK_KHR_PRESENT_ID_EXTENSION_NAME))
			has_id_ext = true;
		if (!strcmp(e.extensionName, VK_KHR_PRESENT_WAIT_EXTENSION_NAME))
			has_wait_ext = true;
	}
	VkPhysicalDevicePresentWaitFeaturesKHR feat_pw = {
	        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_WAIT_FEATURES_KHR};
	VkPhysicalDevicePresentIdFeaturesKHR feat_pid = {
	        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_ID_FEATURES_KHR};
	feat_pid.pNext                   = &feat_pw;
	VkPhysicalDeviceFeatures2 feats2 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
	feats2.pNext                     = &feat_pid;
	vkGetPhysicalDeviceFeatures2(pd, &feats2);
	const bool have_pw = has_id_ext && has_wait_ext && feat_pid.presentId &&
	                     feat_pw.presentWait;
	printf("[4] present_wait flip measurement: %s (id_ext=%d wait_ext=%d "
	       "presentId=%d presentWait=%d)\n",
	       have_pw ? "AVAILABLE" : "UNAVAILABLE - falling back to block-time "
	                               "+ your eyes",
	       has_id_ext, has_wait_ext, feat_pid.presentId, feat_pw.presentWait);

	// ------------------------- device -------------------------
	float                   prio = 1.0f;
	VkDeviceQueueCreateInfo qi   = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
	qi.queueFamilyIndex          = qf;
	qi.queueCount                = 1;
	qi.pQueuePriorities          = &prio;

	std::vector<const char*> dev_exts = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
	if (have_pw) {
		dev_exts.push_back(VK_KHR_PRESENT_ID_EXTENSION_NAME);
		dev_exts.push_back(VK_KHR_PRESENT_WAIT_EXTENSION_NAME);
	}
	VkPhysicalDevicePresentWaitFeaturesKHR en_pw = {
	        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_WAIT_FEATURES_KHR};
	en_pw.presentWait = VK_TRUE;
	VkPhysicalDevicePresentIdFeaturesKHR en_pid = {
	        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_ID_FEATURES_KHR};
	en_pid.presentId = VK_TRUE;
	en_pid.pNext     = &en_pw;

	VkDeviceCreateInfo dci      = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
	dci.pNext                   = have_pw ? (void*)&en_pid : nullptr;
	dci.queueCreateInfoCount    = 1;
	dci.pQueueCreateInfos       = &qi;
	dci.enabledExtensionCount   = (uint32_t)dev_exts.size();
	dci.ppEnabledExtensionNames = dev_exts.data();

	VkDevice device = VK_NULL_HANDLE;
	CHECK(vkCreateDevice(pd, &dci, nullptr, &device));
	VkQueue queue = VK_NULL_HANDLE;
	vkGetDeviceQueue(device, qf, 0, &queue);

	auto pfnWaitForPresent = have_pw
	        ? (PFN_vkWaitForPresentKHR)vkGetDeviceProcAddr(device,
	                                                       "vkWaitForPresentKHR")
	        : nullptr;

	// supported present modes
	uint32_t mode_count = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(pd, surface, &mode_count, nullptr);
	std::vector<VkPresentModeKHR> avail(mode_count);
	vkGetPhysicalDeviceSurfacePresentModesKHR(pd, surface, &mode_count,
	                                           avail.data());
	auto supported = [&](VkPresentModeKHR m) {
		for (auto a : avail) {
			if (a == m) {
				return true;
			}
		}
		return false;
	};
	printf("[5] supported present modes:");
	for (auto a : avail) {
		printf(" %s", ModeName(a));
	}
	printf("\n");

	// persistent per-frame sync (reused across modes)
	VkCommandPoolCreateInfo cpci = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
	cpci.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	cpci.queueFamilyIndex = qf;
	VkCommandPool pool    = VK_NULL_HANDLE;
	CHECK(vkCreateCommandPool(device, &cpci, nullptr, &pool));

	VkSemaphoreCreateInfo sem_ci   = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
	VkFenceCreateInfo     fence_ci = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
	fence_ci.flags                 = VK_FENCE_CREATE_SIGNALED_BIT;
	VkSemaphore acquire_sems[MaxInFlight] = {};
	VkFence     frame_fences[MaxInFlight] = {};
	for (uint32_t i = 0; i < MaxInFlight; ++i) {
		CHECK(vkCreateSemaphore(device, &sem_ci, nullptr, &acquire_sems[i]));
		CHECK(vkCreateFence(device, &fence_ci, nullptr, &frame_fences[i]));
	}

	const uint64_t period_ns = (uint64_t)(1e9 / DosRateHz);
	printf("[6] sweeping @ %.3f Hz (period %.3f ms), %d frames/mode "
	       "(%d warmup)\n\n",
	       DosRateHz, 1000.0 / DosRateHz, Frames, WarmupFrames);

	// -------------------- one mode --------------------
	auto run_mode = [&](VkPresentModeKHR mode) {
		VkSurfaceCapabilitiesKHR caps = {};
		CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pd, surface, &caps));
		uint32_t want_imgs = (mode == VK_PRESENT_MODE_MAILBOX_KHR)
		        ? std::max(caps.minImageCount + 1, 3u)
		        : std::max(caps.minImageCount, 3u);
		if (caps.maxImageCount) {
			want_imgs = std::min(want_imgs, caps.maxImageCount);
		}

		VkSwapchainCreateInfoKHR sc = {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
		sc.surface          = surface;
		sc.minImageCount    = want_imgs;
		sc.imageFormat      = VK_FORMAT_B8G8R8A8_UNORM;
		sc.imageColorSpace  = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
		sc.imageExtent      = caps.currentExtent;
		sc.imageArrayLayers = 1;
		sc.imageUsage       = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		sc.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		sc.preTransform     = caps.currentTransform;
		sc.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		sc.presentMode      = mode;
		sc.clipped          = VK_TRUE;

		VkSwapchainKHR swapchain = VK_NULL_HANDLE;
		CHECK(vkCreateSwapchainKHR(device, &sc, nullptr, &swapchain));
		uint32_t img_count = 0;
		CHECK(vkGetSwapchainImagesKHR(device, swapchain, &img_count, nullptr));
		std::vector<VkImage> images(img_count);
		CHECK(vkGetSwapchainImagesKHR(device, swapchain, &img_count,
		                              images.data()));

		std::vector<VkSemaphore> render_sems(img_count);
		for (uint32_t i = 0; i < img_count; ++i) {
			CHECK(vkCreateSemaphore(device, &sem_ci, nullptr,
			                        &render_sems[i]));
		}
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
			VkImageSubresourceRange range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1,
			                                 0, 1};
			VkImageMemoryBarrier to_dst = {
			        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
			to_dst.dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
			to_dst.oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
			to_dst.newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			to_dst.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			to_dst.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			to_dst.image               = images[i];
			to_dst.subresourceRange    = range;
			vkCmdPipelineBarrier(cmds[i], VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			                     VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,
			                     nullptr, 0, nullptr, 1, &to_dst);
			const float       phase = (float)i / (float)img_count;
			VkClearColorValue color = {{phase, 0.3f, 1.0f - phase, 1.0f}};
			vkCmdClearColorImage(cmds[i], images[i],
			                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &color,
			                     1, &range);
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

		std::vector<uint64_t> flip_ns;
		std::vector<double>    present_call_ms;
		int                    wait_misses = 0;

		const uint64_t t0 = SteadyNs() + 200'000'000ull;  // 200 ms settle
		for (int i = 0; i < Frames; ++i) {
			const uint64_t due = t0 + (uint64_t)i * period_ns;
			const uint64_t now = SteadyNs();
			if (due > now) {
				SDL_DelayPrecise(due - now);
			}

			const uint32_t slot = (uint32_t)i % MaxInFlight;
			CHECK(vkWaitForFences(device, 1, &frame_fences[slot], VK_TRUE,
			                      UINT64_MAX));
			CHECK(vkResetFences(device, 1, &frame_fences[slot]));

			uint32_t img = 0;
			VkResult ar  = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX,
			                                    acquire_sems[slot],
			                                    VK_NULL_HANDLE, &img);
			if (ar != VK_SUCCESS && ar != VK_SUBOPTIMAL_KHR) {
				printf("    acquire failed: %d\n", ar);
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
			VkPresentIdKHR pid        = {VK_STRUCTURE_TYPE_PRESENT_ID_KHR};
			pid.swapchainCount        = 1;
			pid.pPresentIds           = &present_id;

			VkPresentInfoKHR pi   = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
			pi.pNext              = have_pw ? &pid : nullptr;
			pi.waitSemaphoreCount = 1;
			pi.pWaitSemaphores    = &render_sems[img];
			pi.swapchainCount     = 1;
			pi.pSwapchains        = &swapchain;
			pi.pImageIndices      = &img;

			const uint64_t p0 = SteadyNs();
			VkResult       pr = vkQueuePresentKHR(queue, &pi);
			present_call_ms.push_back((double)(SteadyNs() - p0) / 1e6);
			if (pr != VK_SUCCESS && pr != VK_SUBOPTIMAL_KHR) {
				printf("    present failed: %d\n", pr);
				break;
			}

			if (pfnWaitForPresent) {
				VkResult wr = pfnWaitForPresent(device, swapchain,
				                                present_id, 1'000'000'000ull);
				if (wr == VK_SUCCESS) {
					flip_ns.push_back(SteadyNs());
				} else {
					++wait_misses;
				}
			}

			SDL_Event ev;
			while (SDL_PollEvent(&ev)) {
			}
		}
		vkDeviceWaitIdle(device);

		// intervals from flip times, skipping warmup
		std::vector<double> interval_ms;
		for (size_t k = (size_t)WarmupFrames + 1; k < flip_ns.size(); ++k) {
			interval_ms.push_back(
			        (double)(flip_ns[k] - flip_ns[k - 1]) / 1e6);
		}

		printf("=== MODE: %s ===\n", ModeName(mode));
		if (pfnWaitForPresent) {
			Stats(interval_ms, "flip interval [ms]  (want ~14.268)");
			if (!interval_ms.empty()) {
				std::vector<double> s = interval_ms;
				std::sort(s.begin(), s.end());
				const double p50 = s[s.size() / 2];
				printf("    -> p50 flip = %.3f ms = %.1f Hz  => %s\n", p50,
				       1000.0 / p50,
				       (p50 > 13.3 && p50 < 15.3)
				               ? "HITS DOS RATE (VRR working)"
				               : "NOT 70 Hz");
			}
			if (wait_misses) {
				printf("    present_wait misses/timeouts: %d\n", wait_misses);
			}
		}
		Stats(present_call_ms, "vkQueuePresentKHR call [ms]  (blocking check)");
		if (!pfnWaitForPresent) {
			printf("    (no present_wait: judge smoothness by eye; a blocking "
			       "present call ~= the display is pacing us)\n");
		}
		printf("\n");

		vkFreeCommandBuffers(device, pool, img_count, cmds.data());
		for (auto s : render_sems) {
			vkDestroySemaphore(device, s, nullptr);
		}
		vkDestroySwapchainKHR(device, swapchain, nullptr);
	};

	const VkPresentModeKHR order[] = {VK_PRESENT_MODE_IMMEDIATE_KHR,
	                                  VK_PRESENT_MODE_MAILBOX_KHR,
	                                  VK_PRESENT_MODE_FIFO_KHR};
	for (auto m : order) {
		if (supported(m)) {
			run_mode(m);
		} else {
			printf("=== MODE: %s === not supported, skipped\n\n", ModeName(m));
		}
	}

	printf("Done. A mode at ~14.268 ms flip interval is doing 70 Hz on VRR; "
	       "~16.68 ms is 60 Hz.\n");
	fflush(stdout);
	std::exit(0);
}
