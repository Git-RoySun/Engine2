#include "SwapChain.h"
#include "Device.h"

#include <array>

#include "Utils.hpp"

namespace gm {
  SwapChain::SwapChain(Device& deviceRef, VkExtent2D windowExtent, VkSurfaceKHR surface, std::shared_ptr<SwapChain> previous) : device{ deviceRef }, oldSwapChain{ previous } {
    createSwapChain(surface, windowExtent);
    createImageViews();
    createRenderPass();
    createDepthResources();
    createMsaaResources();
    createFramebuffers();
    createSyncObjects();
  }

  SwapChain::~SwapChain() {
    for (auto imageView : swapChainImageViews) {
      vkDestroyImageView(device.getVkDevice(), imageView, nullptr);
    }
    swapChainImageViews.clear();

    if (swapChain != nullptr) {
      vkDestroySwapchainKHR(device.getVkDevice(), swapChain, nullptr);
      swapChain = nullptr;
    }

    for (int i = 0; i < depthImages.size(); i++) {
      vkDestroyImageView(device.getVkDevice(), depthImageViews[i], nullptr);
      vkDestroyImage(device.getVkDevice(), depthImages[i], nullptr);
      vkFreeMemory(device.getVkDevice(), depthImageMemorys[i], nullptr);
    }

    for (int i = 0; i < colorImages.size(); i++) {
      vkDestroyImageView(device.getVkDevice(), colorImageViews[i], nullptr);
      vkDestroyImage(device.getVkDevice(), colorImages[i], nullptr);
      vkFreeMemory(device.getVkDevice(), colorImageMemorys[i], nullptr);
    }

    for (auto framebuffer : swapChainFramebuffers) {
      vkDestroyFramebuffer(device.getVkDevice(), framebuffer, nullptr);
    }

    vkDestroyRenderPass(device.getVkDevice(), renderPass, nullptr);

    // cleanup synchronization objects
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
      vkDestroySemaphore(device.getVkDevice(), renderFinishedSemaphores[i], nullptr);
      vkDestroySemaphore(device.getVkDevice(), imageAvailableSemaphores[i], nullptr);
      vkDestroyFence(device.getVkDevice(), inFlightFences[i], nullptr);
    }
  }

  VkResult SwapChain::acquireNextImage(uint32_t* imageIndex) {
    vkWaitForFences(
      device.getVkDevice(),
      1,
      &inFlightFences[currentFrame],
      VK_TRUE,
      std::numeric_limits<uint64_t>::max());

    VkResult result = vkAcquireNextImageKHR(
      device.getVkDevice(),
      swapChain,
      std::numeric_limits<uint64_t>::max(),
      imageAvailableSemaphores[currentFrame],  // must be a not signaled semaphore
      VK_NULL_HANDLE,
      imageIndex);

    return result;
  }

  VkResult SwapChain::submitCommandBuffers(
    const VkCommandBuffer* buffers, uint32_t* imageIndex) {
    if (imagesInFlight[*imageIndex] != VK_NULL_HANDLE) {
      vkWaitForFences(device.getVkDevice(), 1, &imagesInFlight[*imageIndex], VK_TRUE, UINT64_MAX);
    }
    imagesInFlight[*imageIndex] = inFlightFences[currentFrame];

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[currentFrame] };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = buffers;

    VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[currentFrame] };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    vkResetFences(device.getVkDevice(), 1, &inFlightFences[currentFrame]);
    VK_CHECK_RESULT(vkQueueSubmit(device.getQueueFamilies().graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]), "Failed to submit draw command buffer!")
    

    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = { swapChain };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;

    presentInfo.pImageIndices = imageIndex;

    auto result = vkQueuePresentKHR(device.getQueueFamilies().presentQueue, &presentInfo);

    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;

    return result;
  }

  void SwapChain::createSwapChain(VkSurfaceKHR surface, VkExtent2D windowExtent) {
    vkDeviceWaitIdle(device.getVkDevice());
    Device::SwapChainSupportDetails swapChainSupport = Device::querySwapChainSupport(device.getPhysicalDevice(), surface);

    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities, windowExtent);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 &&
      imageCount > swapChainSupport.capabilities.maxImageCount) {
      imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;
    createInfo.minImageCount = 3;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    Device::QueueFamily indices = device.getQueueFamilies();
    uint32_t queueFamilyIndices[] = { indices.graphicsFamily, indices.presentFamily };

    if (indices.graphicsFamily != indices.presentFamily) {
      createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
      createInfo.queueFamilyIndexCount = 2;
      createInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    else {
      createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
      createInfo.queueFamilyIndexCount = 0;      // Optional
      createInfo.pQueueFamilyIndices = nullptr;  // Optional
    }

    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;

    createInfo.oldSwapchain = (oldSwapChain == nullptr) ? VK_NULL_HANDLE : oldSwapChain->swapChain;
    VkResult err = vkCreateSwapchainKHR(device.getVkDevice(), &createInfo, nullptr, &swapChain);
    if (err != VK_SUCCESS) {
      throw std::runtime_error("failed to create swap chain!");
    }

    // we only specified a minimum number of images in the swap chain, so the implementation is
    // allowed to create a swap chain with more. That's why we'll first query the final number of
    // images with vkGetSwapchainImagesKHR, then resize the container and finally call it again to
    // retrieve the handles.
    vkGetSwapchainImagesKHR(device.getVkDevice(), swapChain, &imageCount, nullptr);
    swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(device.getVkDevice(), swapChain, &imageCount, swapChainImages.data());

    swapChainImageFormat = surfaceFormat.format;
    swapChainExtent = extent;
  }

  void SwapChain::createImageViews() {
    swapChainImageViews.resize(swapChainImages.size());
    for (size_t i = 0; i < swapChainImages.size(); i++) {
      VkImageViewCreateInfo viewInfo{};
      viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      viewInfo.image = swapChainImages[i];
      viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
      viewInfo.format = swapChainImageFormat;
      viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      viewInfo.subresourceRange.baseMipLevel = 0;
      viewInfo.subresourceRange.levelCount = 1;
      viewInfo.subresourceRange.baseArrayLayer = 0;
      viewInfo.subresourceRange.layerCount = 1;

      if (vkCreateImageView(device.getVkDevice(), &viewInfo, nullptr, &swapChainImageViews[i]) !=
        VK_SUCCESS) {
        throw std::runtime_error("failed to create texture image view!");
      }
    }
  }

  void SwapChain::createRenderPass() {
    VkAttachmentDescription colorAttachment = {};
    colorAttachment.format = getSwapChainImageFormat();
    colorAttachment.samples = device.getMsaaSample();
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentRef = {};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = findDepthFormat();
    depthAttachment.samples = device.getMsaaSample();
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription colorAttachmentResolve{};
    colorAttachmentResolve.format = getSwapChainImageFormat();
    colorAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentResolveRef{};
    colorAttachmentResolveRef.attachment = 2;
    colorAttachmentResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;
    subpass.pResolveAttachments = &colorAttachmentResolveRef;

    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.srcAccessMask = 0;
    dependency.srcStageMask =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstSubpass = 0;
    dependency.dstStageMask =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask =
      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 3> attachments = { colorAttachment, depthAttachment, colorAttachmentResolve };
    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(device.getVkDevice(), &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
      throw std::runtime_error("failed to create render pass!");
    }
  }

  void SwapChain::createFramebuffers() {
    swapChainFramebuffers.resize(imageCount());
    for (size_t i = 0; i < imageCount(); i++) {
      std::array<VkImageView, 3> attachments = {
        colorImageViews[i],
        depthImageViews[i],
        swapChainImageViews[i]
      };
      VkExtent2D swapChainExtent = getSwapChainExtent();
      VkFramebufferCreateInfo framebufferInfo = {};
      framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
      framebufferInfo.renderPass = renderPass;
      framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
      framebufferInfo.pAttachments = attachments.data();
      framebufferInfo.width = swapChainExtent.width;
      framebufferInfo.height = swapChainExtent.height;
      framebufferInfo.layers = 1;

      if (vkCreateFramebuffer(
        device.getVkDevice(),
        &framebufferInfo,
        nullptr,
        &swapChainFramebuffers[i]) != VK_SUCCESS) {
        throw std::runtime_error("failed to create framebuffer!");
      }
    }
  }


  void SwapChain::createMsaaResources() {
    VkFormat colorFormat = swapChainImageFormat;
    VkExtent2D swapChainExtent = getSwapChainExtent();

    colorImages.resize(imageCount());
    colorImageMemorys.resize(imageCount());
    colorImageViews.resize(imageCount());

    for (int i = 0; i < colorImages.size(); i++) {
      VkImageCreateInfo imageInfo{};
      imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
      imageInfo.imageType = VK_IMAGE_TYPE_2D;
      imageInfo.extent.width = swapChainExtent.width;
      imageInfo.extent.height = swapChainExtent.height;
      imageInfo.extent.depth = 1;
      imageInfo.mipLevels = 1;
      imageInfo.arrayLayers = 1;
      imageInfo.format = colorFormat;
      imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
      imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      imageInfo.usage = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
      imageInfo.samples = device.getMsaaSample();
      imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
      imageInfo.flags = 0;

      {
        VK_CHECK_RESULT(vkCreateImage(device.getVkDevice(), &imageInfo, nullptr, &colorImages[i]), "Failed to create image!")
          VkMemoryRequirements memRequirements; vkGetImageMemoryRequirements(device.getVkDevice(), colorImages[i], &memRequirements);
        VkPhysicalDeviceMemoryProperties memProperties; vkGetPhysicalDeviceMemoryProperties(device.getPhysicalDevice(), &memProperties);

        uint32_t memType = 0;
        for (uint32_t x = 0; x < memProperties.memoryTypeCount; x++) {
          if ((memRequirements.memoryTypeBits & (1 << x)) &&
            (memProperties.memoryTypes[x].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
            memType = x;
            break;
          }
        }

        VkMemoryAllocateInfo allocInfo{
          .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
          .allocationSize = memRequirements.size,
          .memoryTypeIndex = memType,
        };

        VK_CHECK_RESULT(vkAllocateMemory(device.getVkDevice(), &allocInfo, nullptr, &colorImageMemorys[i]), "Failed to allocate image memory!")
          VK_CHECK_RESULT(vkBindImageMemory(device.getVkDevice(), colorImages[i], colorImageMemorys[i], 0), "Failed to bind image memory!")
      }

      VkImageViewCreateInfo viewInfo{};
      viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      viewInfo.image = colorImages[i];
      viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
      viewInfo.format = colorFormat;
      viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      viewInfo.subresourceRange.baseMipLevel = 0;
      viewInfo.subresourceRange.levelCount = 1;
      viewInfo.subresourceRange.baseArrayLayer = 0;
      viewInfo.subresourceRange.layerCount = 1;

      if (vkCreateImageView(device.getVkDevice(), &viewInfo, nullptr, &colorImageViews[i]) != VK_SUCCESS) {
        throw std::runtime_error("failed to create texture image view!");
      }
    }

  }
  void SwapChain::createDepthResources() {
    VkFormat depthFormat = findDepthFormat();
    swapChainDepthFormat = depthFormat;
    VkExtent2D swapChainExtent = getSwapChainExtent();

    depthImages.resize(imageCount());
    depthImageMemorys.resize(imageCount());
    depthImageViews.resize(imageCount());

    for (int i = 0; i < depthImages.size(); i++) {
      VkImageCreateInfo imageInfo{};
      imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
      imageInfo.imageType = VK_IMAGE_TYPE_2D;
      imageInfo.extent.width = swapChainExtent.width;
      imageInfo.extent.height = swapChainExtent.height;
      imageInfo.extent.depth = 1;
      imageInfo.mipLevels = 1;
      imageInfo.arrayLayers = 1;
      imageInfo.format = depthFormat;
      imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
      imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
      imageInfo.samples = device.getMsaaSample();
      imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
      imageInfo.flags = 0;

      {
        VK_CHECK_RESULT(vkCreateImage(device.getVkDevice(), &imageInfo, nullptr, &depthImages[i]), "Failed to create image!")
          VkMemoryRequirements memRequirements; vkGetImageMemoryRequirements(device.getVkDevice(), depthImages[i], &memRequirements);
        VkPhysicalDeviceMemoryProperties memProperties; vkGetPhysicalDeviceMemoryProperties(device.getPhysicalDevice(), &memProperties);

        uint32_t memType = 0;
        for (uint32_t x = 0; x < memProperties.memoryTypeCount; x++) {
          if ((memRequirements.memoryTypeBits & (1 << x)) &&
            (memProperties.memoryTypes[x].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
            memType = x;
            break;
          }
        }

        VkMemoryAllocateInfo allocInfo{
          .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
          .allocationSize = memRequirements.size,
          .memoryTypeIndex = memType,
        };

        VK_CHECK_RESULT(vkAllocateMemory(device.getVkDevice(), &allocInfo, nullptr, &depthImageMemorys[i]), "Failed to allocate image memory!")
          VK_CHECK_RESULT(vkBindImageMemory(device.getVkDevice(), depthImages[i], depthImageMemorys[i], 0), "Failed to bind image memory!")
      }

      VkImageViewCreateInfo viewInfo{};
      viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      viewInfo.image = depthImages[i];
      viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
      viewInfo.format = depthFormat;
      viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
      viewInfo.subresourceRange.baseMipLevel = 0;
      viewInfo.subresourceRange.levelCount = 1;
      viewInfo.subresourceRange.baseArrayLayer = 0;
      viewInfo.subresourceRange.layerCount = 1;

      if (vkCreateImageView(device.getVkDevice(), &viewInfo, nullptr, &depthImageViews[i]) != VK_SUCCESS) {
        throw std::runtime_error("failed to create texture image view!");
      }
    }
  }

  void SwapChain::createSyncObjects() {
    imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    imagesInFlight.resize(imageCount(), VK_NULL_HANDLE);

    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
      if (vkCreateSemaphore(device.getVkDevice(), &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) !=
        VK_SUCCESS ||
        vkCreateSemaphore(device.getVkDevice(), &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) !=
        VK_SUCCESS ||
        vkCreateFence(device.getVkDevice(), &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
        throw std::runtime_error("failed to create synchronization objects for a frame!");
      }
    }
  }

  VkSurfaceFormatKHR SwapChain::chooseSwapSurfaceFormat(
    const std::vector<VkSurfaceFormatKHR>& availableFormats) {
    for (const auto& availableFormat : availableFormats) {
      if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
        availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
        return availableFormat;
      }
    }

    return availableFormats[0];
  }

  VkPresentModeKHR SwapChain::chooseSwapPresentMode(
    const std::vector<VkPresentModeKHR>& availablePresentModes) {
    for (const auto& availablePresentMode : availablePresentModes) {
      if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
        std::cout << "Present mode: Mailbox" << std::endl;
        return availablePresentMode;
      }
    }

    std::cout << "Present mode: V-Sync" << std::endl;
    return VK_PRESENT_MODE_FIFO_KHR;
  }

  VkExtent2D SwapChain::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, VkExtent2D windowExtent) {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
      return capabilities.currentExtent;
    }
    else {
      VkExtent2D actualExtent = windowExtent;
      actualExtent.width = std::max(
      capabilities.minImageExtent.width,
        std::min(capabilities.maxImageExtent.width, actualExtent.width));
      actualExtent.height = std::max(
        capabilities.minImageExtent.height,
        std::min(capabilities.maxImageExtent.height, actualExtent.height));

      return actualExtent;
    }
  }

  VkFormat SwapChain::findDepthFormat() {
    for (VkFormat format : { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT }) {
      VkFormatProperties props; vkGetPhysicalDeviceFormatProperties(device.getPhysicalDevice(), format, &props);
      if ((props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) == VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
        return format;
      }
    }
    throw std::runtime_error("failed to find supported format!");
  }

}  // namespace lve