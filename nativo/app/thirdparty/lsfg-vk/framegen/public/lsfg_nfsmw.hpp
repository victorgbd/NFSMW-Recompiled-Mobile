#pragma once
// NFSMW: framegen's interface for the native renderer (nativo/app/src/nfsmw_lsfg.cpp). Unlike lsfg_3_1.hpp of
// lsfg-vk (removed here), it runs on the renderer's own Vulkan device: no second device, no shared memory through
// file descriptors. The renderer draws each frame into nextInput(), calls present() and reads outputs().
// Every function may throw std::exception (LSFG::vulkan_error, std::runtime_error).

#include <vulkan/vulkan_core.h>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace LSFG_NFSMW {

struct Device {
    VkInstance instance = VK_NULL_HANDLE;
    PFN_vkGetInstanceProcAddr getInstanceProcAddr = nullptr;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;  // Vulkan 1.3 with synchronization2, vulkanMemoryModel and nullDescriptor
    uint32_t queueFamily = 0;
    VkQueue queue = VK_NULL_HANDLE;
    // Every submission goes through this (the queue is shared with other threads).
    VkResult (*submit)(VkQueue queue, uint32_t count, const VkSubmitInfo* submits, VkFence fence) = nullptr;
};

struct Image {
    VkImage image = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
};

/// @param performance LSFG 3.1 "performance" (lighter) instead of 3.1.
/// @param flowScale Motion estimation resolution, 0.25 to 1 (1 = the frame's resolution).
/// @param generationCount Frames generated between two real ones (1 = x2).
/// @param loader SPIR-V of each shader by name (translated from Lossless.dll).
void initialize(const Device& device, bool performance, float flowScale, uint32_t generationCount,
    const std::function<std::vector<uint8_t>(const std::string&)>& loader);

/// Creates the context and its images (inputs and outputs) for frames of this size and format. Replaces any
/// previous one (waiting for its work first).
void createContext(VkExtent2D extent, VkFormat format);

/// Both inputs, to put them in GENERAL once; after that, call inputsAreGeneral().
Image input(size_t index);
void inputsAreGeneral();

/// Where the next frame goes (GENERAL layout). The renderer writes it (with a barrier to compute shaders after
/// the write) and then calls present().
Image nextInput();

/// Generates generationCount frames between the previous input and the new one. Waits for the generation of 8
/// presents ago. Only one thread.
void present();

/// The generated frames of the last present(), in display order (GENERAL layout once that work has run).
std::vector<Image> outputs();

/// Waits for every submitted generation and destroys the context.
void deleteContext();
void finalize();

}  // namespace LSFG_NFSMW
