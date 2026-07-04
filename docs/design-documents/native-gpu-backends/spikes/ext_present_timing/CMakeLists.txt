cmake_minimum_required(VERSION 3.21)
project(vulkan_ext_present_timing_spike CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL3 CONFIG REQUIRED)
find_package(VulkanHeaders CONFIG REQUIRED)
find_package(VulkanLoader CONFIG REQUIRED)

add_executable(vulkan_ext_present_timing_spike
	vulkan_ext_present_timing_spike.cpp)

target_link_libraries(vulkan_ext_present_timing_spike PRIVATE
	SDL3::SDL3
	Vulkan::Headers
	Vulkan::Loader)
