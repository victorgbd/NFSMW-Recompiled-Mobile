// NFSMW: implementation of public/lsfg_nfsmw.hpp over the LSFG 3.1 and 3.1p contexts of lsfg-vk v1.0.0 (MIT).
// Replaces v3.1_src/lsfg.cpp and v3.1p_src/lsfg.cpp, which created their own Vulkan device.

#include <volk.h>
#include <vulkan/vulkan_core.h>

#include "lsfg_nfsmw.hpp"
#include "v3_1/context.hpp"
#include "v3_1p/context.hpp"
#include "core/commandbuffer.hpp"
#include "core/commandpool.hpp"
#include "core/descriptorpool.hpp"
#include "core/instance.hpp"
#include "pool/resourcepool.hpp"
#include "pool/shaderpool.hpp"
#include "common/exception.hpp"
#include "common/utils.hpp"

#include <optional>

namespace {

std::optional<LSFG::Core::Instance> instance;
std::optional<LSFG::Vulkan> vk;
bool performance = false;
std::optional<LSFG_3_1::Context> context;
std::optional<LSFG_3_1P::Context> contextP;

LSFG_NFSMW::Image Wrap(const LSFG::Core::Image& image) {
    return {image.handle(), image.getView()};
}

void Require() {
    if (!vk.has_value())
        throw LSFG::vulkan_error(VK_ERROR_INITIALIZATION_FAILED, "LSFG not initialized");
    if (!context.has_value() && !contextP.has_value())
        throw LSFG::vulkan_error(VK_ERROR_INITIALIZATION_FAILED, "LSFG context not created");
}

}  // namespace

void LSFG_NFSMW::initialize(const Device& device, bool performanceMode, float flowScale, uint32_t generationCount,
        const std::function<std::vector<uint8_t>(const std::string&)>& loader) {
    if (vk.has_value())
        return;
    volkInitializeCustom(device.getInstanceProcAddr);
    volkLoadInstanceOnly(device.instance);
    volkLoadDevice(device.device);
    LSFG::Core::submitHook = device.submit;

    instance.emplace(device.instance);
    vk.emplace(LSFG::Vulkan{
        .device{device.device, device.physicalDevice, device.queueFamily, device.queue},
        .generationCount = generationCount,
        .flowScale = 1.0F / flowScale,  // as lsfg-vk's layer passes it
        .isHdr = false,
    });
    vk->commandPool = LSFG::Core::CommandPool(vk->device);
    vk->descriptorPool = LSFG::Core::DescriptorPool(vk->device);
    vk->resources = LSFG::Pool::ResourcePool(vk->isHdr, vk->flowScale);
    vk->shaders = LSFG::Pool::ShaderPool(loader);
    performance = performanceMode;
}

void LSFG_NFSMW::createContext(VkExtent2D extent, VkFormat format) {
    if (!vk.has_value())
        throw LSFG::vulkan_error(VK_ERROR_INITIALIZATION_FAILED, "LSFG not initialized");
    deleteContext();
    const std::vector<int> noFds;  // -1: the images are allocated here, not imported
    if (performance)
        contextP.emplace(*vk, -1, -1, noFds, extent, format);
    else
        context.emplace(*vk, -1, -1, noFds, extent, format);
}

LSFG_NFSMW::Image LSFG_NFSMW::input(size_t index) {
    Require();
    return Wrap(performance ? contextP->input(index) : context->input(index));
}

void LSFG_NFSMW::inputsAreGeneral() {
    Require();
    for (size_t i = 0; i < 2; i++)
        (performance ? contextP->input(i) : context->input(i)).setLayout(VK_IMAGE_LAYOUT_GENERAL);
}

LSFG_NFSMW::Image LSFG_NFSMW::nextInput() {
    Require();
    return Wrap(performance ? contextP->nextInput() : context->nextInput());
}

void LSFG_NFSMW::present() {
    Require();
    const std::vector<int> noFds;
    if (performance)
        contextP->present(*vk, -1, noFds);
    else
        context->present(*vk, -1, noFds);
}

std::vector<LSFG_NFSMW::Image> LSFG_NFSMW::outputs() {
    Require();
    std::vector<Image> result;
    for (const auto& image : performance ? contextP->outputs() : context->outputs())
        result.push_back(Wrap(image));
    return result;
}

void LSFG_NFSMW::deleteContext() {
    if (!vk.has_value())
        return;
    if (context.has_value()) {
        context->waitIdle(*vk);
        context.reset();
    }
    if (contextP.has_value()) {
        contextP->waitIdle(*vk);
        contextP.reset();
    }
}

void LSFG_NFSMW::finalize() {
    deleteContext();
    vk.reset();
    instance.reset();
    LSFG::Core::submitHook = nullptr;
}
