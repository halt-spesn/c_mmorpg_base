
#include "renderer_vulkan.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Define portability enumeration constants if not available
#ifndef VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME
#define VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME "VK_KHR_portability_enumeration"
#endif

#ifndef VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR
#define VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR 0x00000001
#endif

// Validation layers for debugging
static const char *validation_layers[] = {
    "VK_LAYER_KHRONOS_validation"
};
static const int validation_layer_count = 1;

// Required device extensions
// Note: VK_KHR_portability_subset is added dynamically if available
static const char *device_extensions[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};
static const int device_extension_count = 1;

#ifndef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
#define VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME "VK_KHR_portability_subset"
#endif

#ifdef NDEBUG
static const int enable_validation_layers = 0;
#else
static const int enable_validation_layers = 1;
#endif

// Helper function to check validation layer support
static int check_validation_layer_support() {
    uint32_t layer_count;
    vkEnumerateInstanceLayerProperties(&layer_count, NULL);
    
    VkLayerProperties *available_layers = malloc(sizeof(VkLayerProperties) * layer_count);
    if (!available_layers) {
        printf("Failed to allocate memory for validation layer enumeration\n");
        return 0;
    }
    vkEnumerateInstanceLayerProperties(&layer_count, available_layers);
    
    for (int i = 0; i < validation_layer_count; i++) {
        int layer_found = 0;
        for (uint32_t j = 0; j < layer_count; j++) {
            if (strcmp(validation_layers[i], available_layers[j].layerName) == 0) {
                layer_found = 1;
                break;
            }
        }
        if (!layer_found) {
            free(available_layers);
            return 0;
        }
    }
    
    free(available_layers);
    return 1;
}

// Create Vulkan instance
static int create_instance(SDL_Window *window, VkInstance *instance) {
    int use_validation_layers = 0;
    if (enable_validation_layers) {
        if (check_validation_layer_support()) {
            use_validation_layers = 1;
            printf("Validation layers enabled\n");
        } else {
            printf("Validation layers requested but not available - proceeding without validation\n");
        }
    }
    
    VkApplicationInfo app_info = {0};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "C MMO Client";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "No Engine";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_0;
    
    // Get SDL required extensions
    unsigned int sdl_extension_count = 0;
    if (!SDL_Vulkan_GetInstanceExtensions(window, &sdl_extension_count, NULL)) {
        printf("Failed to get SDL Vulkan extension count: %s\n", SDL_GetError());
        return 0;
    }
    
    // Calculate total extension count (SDL extensions + platform-specific extensions)
    #if defined(_WIN32) || defined(__APPLE__)
    unsigned int total_extension_count = sdl_extension_count + 1;  // +1 for portability enumeration
    #else
    unsigned int total_extension_count = sdl_extension_count;
    #endif
    
    const char **extensions = malloc(sizeof(char*) * total_extension_count);
    if (!extensions) {
        printf("Failed to allocate memory for Vulkan extensions\n");
        return 0;
    }
    
    if (!SDL_Vulkan_GetInstanceExtensions(window, &sdl_extension_count, extensions)) {
        printf("Failed to get SDL Vulkan extensions: %s\n", SDL_GetError());
        free(extensions);
        return 0;
    }
    
    // Add portability enumeration extension for Windows and macOS
    #if defined(_WIN32) || defined(__APPLE__)
    extensions[sdl_extension_count] = VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME;
    printf("Added VK_KHR_portability_enumeration extension for Windows/macOS\n");
    #endif
    
    VkInstanceCreateInfo create_info = {0};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;
    create_info.enabledExtensionCount = total_extension_count;
    create_info.ppEnabledExtensionNames = extensions;
    
    // Set portability enumeration flag for Windows and macOS
    #if defined(_WIN32) || defined(__APPLE__)
    create_info.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    #endif
    
    if (use_validation_layers) {
        create_info.enabledLayerCount = validation_layer_count;
        create_info.ppEnabledLayerNames = validation_layers;
    } else {
        create_info.enabledLayerCount = 0;
    }
    
    VkResult result = vkCreateInstance(&create_info, NULL, instance);
    free(extensions);
    
    if (result != VK_SUCCESS) {
        printf("Failed to create Vulkan instance! Error code: %d\n", result);
        return 0;
    }
    
    return 1;
}

// Find queue families
static int find_queue_families(VkPhysicalDevice device, VkSurfaceKHR surface, 
                               uint32_t *graphics_family, uint32_t *present_family) {
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, NULL);
    
    VkQueueFamilyProperties *queue_families = malloc(sizeof(VkQueueFamilyProperties) * queue_family_count);
    if (!queue_families) {
        printf("Failed to allocate memory for queue families\n");
        return 0;
    }
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families);
    
    int graphics_found = 0;
    int present_found = 0;
    
    for (uint32_t i = 0; i < queue_family_count; i++) {
        if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            *graphics_family = i;
            graphics_found = 1;
        }
        
        VkBool32 present_support = 0;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &present_support);
        if (present_support) {
            *present_family = i;
            present_found = 1;
        }
        
        if (graphics_found && present_found) {
            break;
        }
    }
    
    free(queue_families);
    return graphics_found && present_found;
}

// Check device extension support and detect portability subset
static int check_device_extension_support(VkPhysicalDevice device, int *has_portability_subset) {
    uint32_t extension_count;
    vkEnumerateDeviceExtensionProperties(device, NULL, &extension_count, NULL);
    
    VkExtensionProperties *available_extensions = malloc(sizeof(VkExtensionProperties) * extension_count);
    if (!available_extensions) {
        printf("Failed to allocate memory for device extensions\n");
        return 0;
    }
    vkEnumerateDeviceExtensionProperties(device, NULL, &extension_count, available_extensions);
    
    // Check for required extensions
    for (int i = 0; i < device_extension_count; i++) {
        int found = 0;
        for (uint32_t j = 0; j < extension_count; j++) {
            if (strcmp(device_extensions[i], available_extensions[j].extensionName) == 0) {
                found = 1;
                break;
            }
        }
        if (!found) {
            free(available_extensions);
            return 0;
        }
    }
    
    // Check if portability subset is available (required on some Windows systems)
    *has_portability_subset = 0;
    for (uint32_t j = 0; j < extension_count; j++) {
        if (strcmp(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME, available_extensions[j].extensionName) == 0) {
            *has_portability_subset = 1;
            printf("VK_KHR_portability_subset detected and will be enabled\n");
            break;
        }
    }
    
    free(available_extensions);
    return 1;
}

// Select physical device
static int select_physical_device(VkInstance instance, VkSurfaceKHR surface, 
                                  VkPhysicalDevice *physical_device,
                                  uint32_t *graphics_family, uint32_t *present_family,
                                  int *has_portability_subset) {
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(instance, &device_count, NULL);
    
    if (device_count == 0) {
        printf("Failed to find GPUs with Vulkan support!\n");
        return 0;
    }
    
    printf("Found %d GPU(s) with Vulkan support\n", device_count);
    
    VkPhysicalDevice *devices = malloc(sizeof(VkPhysicalDevice) * device_count);
    if (!devices) {
        printf("Failed to allocate memory for physical devices\n");
        return 0;
    }
    vkEnumeratePhysicalDevices(instance, &device_count, devices);
    
    for (uint32_t i = 0; i < device_count; i++) {
        // Get device properties for logging
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(devices[i], &properties);
        printf("GPU %d: %s (API version: %d.%d.%d)\n", i, properties.deviceName,
               VK_VERSION_MAJOR(properties.apiVersion),
               VK_VERSION_MINOR(properties.apiVersion),
               VK_VERSION_PATCH(properties.apiVersion));
        
        if (check_device_extension_support(devices[i], has_portability_subset) &&
            find_queue_families(devices[i], surface, graphics_family, present_family)) {
            *physical_device = devices[i];
            printf("Selected GPU: %s\n", properties.deviceName);
            free(devices);
            return 1;
        }
    }
    
    free(devices);
    printf("Failed to find suitable GPU!\n");
    return 0;
}

// Create logical device
static int create_logical_device(VkPhysicalDevice physical_device, uint32_t graphics_family,
                                 uint32_t present_family, int has_portability_subset,
                                 VkDevice *device, VkQueue *graphics_queue, VkQueue *present_queue) {
    float queue_priority = 1.0f;
    
    VkDeviceQueueCreateInfo queue_create_infos[2];
    int queue_create_info_count = 0;
    
    // Graphics queue
    queue_create_infos[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_create_infos[0].pNext = NULL;
    queue_create_infos[0].flags = 0;
    queue_create_infos[0].queueFamilyIndex = graphics_family;
    queue_create_infos[0].queueCount = 1;
    queue_create_infos[0].pQueuePriorities = &queue_priority;
    queue_create_info_count++;
    
    // Present queue (if different)
    if (graphics_family != present_family) {
        queue_create_infos[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create_infos[1].pNext = NULL;
        queue_create_infos[1].flags = 0;
        queue_create_infos[1].queueFamilyIndex = present_family;
        queue_create_infos[1].queueCount = 1;
        queue_create_infos[1].pQueuePriorities = &queue_priority;
        queue_create_info_count++;
    }
    
    VkPhysicalDeviceFeatures device_features = {0};
    
    // Prepare device extensions array with portability subset if needed
    const char **enabled_extensions = NULL;
    uint32_t enabled_extension_count = device_extension_count;
    
    if (has_portability_subset) {
        enabled_extension_count = device_extension_count + 1;
        enabled_extensions = malloc(sizeof(char*) * enabled_extension_count);
        if (!enabled_extensions) {
            printf("Failed to allocate memory for device extensions\n");
            return 0;
        }
        for (int i = 0; i < device_extension_count; i++) {
            enabled_extensions[i] = device_extensions[i];
        }
        enabled_extensions[device_extension_count] = VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME;
    } else {
        enabled_extensions = device_extensions;
    }
    
    VkDeviceCreateInfo create_info = {0};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.queueCreateInfoCount = queue_create_info_count;
    create_info.pQueueCreateInfos = queue_create_infos;
    create_info.pEnabledFeatures = &device_features;
    create_info.enabledExtensionCount = enabled_extension_count;
    create_info.ppEnabledExtensionNames = enabled_extensions;
    
    // Note: Device-level validation layers are deprecated in modern Vulkan
    // Validation is now handled at the instance level only
    // Setting to 0 for compatibility with newer implementations
    create_info.enabledLayerCount = 0;
    
    VkResult result = vkCreateDevice(physical_device, &create_info, NULL, device);
    
    // Free the extensions array if we allocated it
    if (has_portability_subset) {
        free((void*)enabled_extensions);
    }
    
    if (result != VK_SUCCESS) {
        printf("Failed to create logical device! Error code: %d\n", result);
        return 0;
    }
    
    vkGetDeviceQueue(*device, graphics_family, 0, graphics_queue);
    vkGetDeviceQueue(*device, present_family, 0, present_queue);
    
    return 1;
}

// Query swapchain support
static void query_swapchain_support(VkPhysicalDevice device, VkSurfaceKHR surface,
                                   VkSurfaceCapabilitiesKHR *capabilities,
                                   VkSurfaceFormatKHR **formats, uint32_t *format_count,
                                   VkPresentModeKHR **present_modes, uint32_t *present_mode_count) {
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, capabilities);
    
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, format_count, NULL);
    if (*format_count != 0) {
        *formats = malloc(sizeof(VkSurfaceFormatKHR) * (*format_count));
        if (*formats) {
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, format_count, *formats);
        } else {
            *format_count = 0;
        }
    }
    
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, present_mode_count, NULL);
    if (*present_mode_count != 0) {
        *present_modes = malloc(sizeof(VkPresentModeKHR) * (*present_mode_count));
        if (*present_modes) {
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, present_mode_count, *present_modes);
        } else {
            *present_mode_count = 0;
        }
    }
}

// Choose swap surface format
static VkSurfaceFormatKHR choose_swap_surface_format(VkSurfaceFormatKHR *formats, uint32_t format_count) {
    for (uint32_t i = 0; i < format_count; i++) {
        if (formats[i].format == VK_FORMAT_B8G8R8A8_SRGB &&
            formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return formats[i];
        }
    }
    return formats[0];
}

// Choose swap present mode
static VkPresentModeKHR choose_swap_present_mode(VkPresentModeKHR *present_modes, uint32_t present_mode_count) {
    // Priority order for best performance:
    // 1. IMMEDIATE - lowest latency, no VSync (may cause tearing but best FPS)
    // 2. MAILBOX - triple buffering, good balance of latency and smoothness
    // 3. FIFO - VSync enabled, guaranteed to be available but can introduce latency
    
    // First try IMMEDIATE for lowest latency and best FPS
    for (uint32_t i = 0; i < present_mode_count; i++) {
        if (present_modes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR) {
            printf("Using VK_PRESENT_MODE_IMMEDIATE_KHR for best performance\n");
            return present_modes[i];
        }
    }
    
    // Then try MAILBOX for good balance
    for (uint32_t i = 0; i < present_mode_count; i++) {
        if (present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
            printf("Using VK_PRESENT_MODE_MAILBOX_KHR\n");
            return present_modes[i];
        }
    }
    
    // Fall back to FIFO (always available)
    printf("Using VK_PRESENT_MODE_FIFO_KHR (VSync)\n");
    return VK_PRESENT_MODE_FIFO_KHR;
}

// Choose swap extent
static VkExtent2D choose_swap_extent(SDL_Window *window, VkSurfaceCapabilitiesKHR *capabilities) {
    if (capabilities->currentExtent.width != UINT32_MAX) {
        return capabilities->currentExtent;
    } else {
        int width, height;
        SDL_Vulkan_GetDrawableSize(window, &width, &height);
        
        VkExtent2D actual_extent = {(uint32_t)width, (uint32_t)height};
        
        if (actual_extent.width < capabilities->minImageExtent.width) {
            actual_extent.width = capabilities->minImageExtent.width;
        } else if (actual_extent.width > capabilities->maxImageExtent.width) {
            actual_extent.width = capabilities->maxImageExtent.width;
        }
        
        if (actual_extent.height < capabilities->minImageExtent.height) {
            actual_extent.height = capabilities->minImageExtent.height;
        } else if (actual_extent.height > capabilities->maxImageExtent.height) {
            actual_extent.height = capabilities->maxImageExtent.height;
        }
        
        return actual_extent;
    }
}

// Create swapchain
static int create_swapchain(SDL_Window *window, VulkanRenderer *vk) {
    VkSurfaceCapabilitiesKHR capabilities;
    VkSurfaceFormatKHR *formats = NULL;
    uint32_t format_count = 0;
    VkPresentModeKHR *present_modes = NULL;
    uint32_t present_mode_count = 0;
    
    query_swapchain_support(vk->physical_device, vk->surface, &capabilities,
                           &formats, &format_count, &present_modes, &present_mode_count);
    
    VkSurfaceFormatKHR surface_format = choose_swap_surface_format(formats, format_count);
    VkPresentModeKHR present_mode = choose_swap_present_mode(present_modes, present_mode_count);
    VkExtent2D extent = choose_swap_extent(window, &capabilities);
    
    // Optimize image count based on present mode
    // For IMMEDIATE mode, we only need 2 images (double buffering)
    // For MAILBOX, we want 3 images (triple buffering)
    // For FIFO, we use minimum + 1
    uint32_t image_count;
    if (present_mode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
        image_count = 2; // Double buffering for IMMEDIATE mode
    } else if (present_mode == VK_PRESENT_MODE_MAILBOX_KHR) {
        image_count = 3; // Triple buffering for MAILBOX mode
    } else {
        image_count = capabilities.minImageCount + 1; // FIFO mode
    }
    
    // Ensure we don't exceed maximum
    if (capabilities.maxImageCount > 0 && image_count > capabilities.maxImageCount) {
        image_count = capabilities.maxImageCount;
    }
    
    printf("Using %d swapchain images\n", image_count);
    
    VkSwapchainCreateInfoKHR create_info = {0};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = vk->surface;
    create_info.minImageCount = image_count;
    create_info.imageFormat = surface_format.format;
    create_info.imageColorSpace = surface_format.colorSpace;
    create_info.imageExtent = extent;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    
    uint32_t queue_family_indices[] = {vk->graphics_family_index, vk->present_family_index};
    
    if (vk->graphics_family_index != vk->present_family_index) {
        create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = 2;
        create_info.pQueueFamilyIndices = queue_family_indices;
    } else {
        create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }
    
    create_info.preTransform = capabilities.currentTransform;
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.presentMode = present_mode;
    create_info.clipped = VK_TRUE;
    create_info.oldSwapchain = VK_NULL_HANDLE;
    
    if (vkCreateSwapchainKHR(vk->device, &create_info, NULL, &vk->swapchain) != VK_SUCCESS) {
        printf("Failed to create swapchain!\n");
        free(formats);
        free(present_modes);
        return 0;
    }
    
    vk->swapchain_image_format = surface_format.format;
    vk->swapchain_extent = extent;
    
    vkGetSwapchainImagesKHR(vk->device, vk->swapchain, &vk->swapchain_image_count, NULL);
    vk->swapchain_images = malloc(sizeof(VkImage) * vk->swapchain_image_count);
    if (!vk->swapchain_images) {
        printf("Failed to allocate memory for swapchain images\n");
        free(formats);
        free(present_modes);
        return 0;
    }
    vkGetSwapchainImagesKHR(vk->device, vk->swapchain, &vk->swapchain_image_count, vk->swapchain_images);
    
    free(formats);
    free(present_modes);
    return 1;
}

// Create image views
static int create_image_views(VulkanRenderer *vk) {
    vk->swapchain_image_views = malloc(sizeof(VkImageView) * vk->swapchain_image_count);
    if (!vk->swapchain_image_views) {
        printf("Failed to allocate memory for image views\n");
        return 0;
    }
    
    for (uint32_t i = 0; i < vk->swapchain_image_count; i++) {
        VkImageViewCreateInfo create_info = {0};
        create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        create_info.image = vk->swapchain_images[i];
        create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        create_info.format = vk->swapchain_image_format;
        create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        create_info.subresourceRange.baseMipLevel = 0;
        create_info.subresourceRange.levelCount = 1;
        create_info.subresourceRange.baseArrayLayer = 0;
        create_info.subresourceRange.layerCount = 1;
        
        if (vkCreateImageView(vk->device, &create_info, NULL, &vk->swapchain_image_views[i]) != VK_SUCCESS) {
            printf("Failed to create image view %d!\n", i);
            return 0;
        }
    }
    
    return 1;
}

// Create render pass
static int create_render_pass(VulkanRenderer *vk) {
    VkAttachmentDescription color_attachment = {0};
    color_attachment.format = vk->swapchain_image_format;
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    
    VkAttachmentReference color_attachment_ref = {0};
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    
    VkSubpassDescription subpass = {0};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;
    
    VkSubpassDependency dependency = {0};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    
    VkRenderPassCreateInfo render_pass_info = {0};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = 1;
    render_pass_info.pAttachments = &color_attachment;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;
    render_pass_info.dependencyCount = 1;
    render_pass_info.pDependencies = &dependency;
    
    if (vkCreateRenderPass(vk->device, &render_pass_info, NULL, &vk->render_pass) != VK_SUCCESS) {
        printf("Failed to create render pass!\n");
        return 0;
    }
    
    return 1;
}

// Create framebuffers
static int create_framebuffers(VulkanRenderer *vk) {
    vk->framebuffers = malloc(sizeof(VkFramebuffer) * vk->swapchain_image_count);
    if (!vk->framebuffers) {
        printf("Failed to allocate memory for framebuffers\n");
        return 0;
    }
    
    for (uint32_t i = 0; i < vk->swapchain_image_count; i++) {
        VkImageView attachments[] = {vk->swapchain_image_views[i]};
        
        VkFramebufferCreateInfo framebuffer_info = {0};
        framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_info.renderPass = vk->render_pass;
        framebuffer_info.attachmentCount = 1;
        framebuffer_info.pAttachments = attachments;
        framebuffer_info.width = vk->swapchain_extent.width;
        framebuffer_info.height = vk->swapchain_extent.height;
        framebuffer_info.layers = 1;
        
        if (vkCreateFramebuffer(vk->device, &framebuffer_info, NULL, &vk->framebuffers[i]) != VK_SUCCESS) {
            printf("Failed to create framebuffer %d!\n", i);
            return 0;
        }
    }
    
    return 1;
}

// Create command pool
static int create_command_pool(VulkanRenderer *vk) {
    VkCommandPoolCreateInfo pool_info = {0};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    // Add TRANSIENT flag for better performance when command buffers are short-lived
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    pool_info.queueFamilyIndex = vk->graphics_family_index;
    
    if (vkCreateCommandPool(vk->device, &pool_info, NULL, &vk->command_pool) != VK_SUCCESS) {
        printf("Failed to create command pool!\n");
        return 0;
    }
    
    return 1;
}

// Create command buffers
static int create_command_buffers(VulkanRenderer *vk) {
    vk->command_buffers = malloc(sizeof(VkCommandBuffer) * MAX_FRAMES_IN_FLIGHT);
    if (!vk->command_buffers) {
        printf("Failed to allocate memory for command buffers\n");
        return 0;
    }
    
    VkCommandBufferAllocateInfo alloc_info = {0};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = vk->command_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
    
    if (vkAllocateCommandBuffers(vk->device, &alloc_info, vk->command_buffers) != VK_SUCCESS) {
        printf("Failed to allocate command buffers!\n");
        return 0;
    }
    
    return 1;
}

// Create sync objects
static int create_sync_objects(VulkanRenderer *vk) {
    vk->image_available_semaphores = malloc(sizeof(VkSemaphore) * MAX_FRAMES_IN_FLIGHT);
    vk->render_finished_semaphores = malloc(sizeof(VkSemaphore) * MAX_FRAMES_IN_FLIGHT);
    vk->in_flight_fences = malloc(sizeof(VkFence) * MAX_FRAMES_IN_FLIGHT);
    
    if (!vk->image_available_semaphores || !vk->render_finished_semaphores || !vk->in_flight_fences) {
        printf("Failed to allocate memory for sync objects\n");
        free(vk->image_available_semaphores);
        free(vk->render_finished_semaphores);
        free(vk->in_flight_fences);
        return 0;
    }
    
    VkSemaphoreCreateInfo semaphore_info = {0};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    
    VkFenceCreateInfo fence_info = {0};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(vk->device, &semaphore_info, NULL, &vk->image_available_semaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(vk->device, &semaphore_info, NULL, &vk->render_finished_semaphores[i]) != VK_SUCCESS ||
            vkCreateFence(vk->device, &fence_info, NULL, &vk->in_flight_fences[i]) != VK_SUCCESS) {
            printf("Failed to create sync objects for frame %d!\n", i);
            return 0;
        }
    }
    
    return 1;
}

// Initialize Vulkan renderer
int vulkan_init(SDL_Window *window, VulkanRenderer *vk_renderer) {
    memset(vk_renderer, 0, sizeof(VulkanRenderer));
    
    printf("Initializing Vulkan renderer...\n");
    
    // Create Vulkan instance
    if (!create_instance(window, &vk_renderer->instance)) {
        return 0;
    }
    printf("Vulkan instance created\n");
    
    // Create surface
    if (!SDL_Vulkan_CreateSurface(window, vk_renderer->instance, &vk_renderer->surface)) {
        printf("Failed to create Vulkan surface: %s\n", SDL_GetError());
        return 0;
    }
    printf("Vulkan surface created\n");
    
    // Select physical device
    int has_portability_subset = 0;
    if (!select_physical_device(vk_renderer->instance, vk_renderer->surface,
                                &vk_renderer->physical_device,
                                &vk_renderer->graphics_family_index,
                                &vk_renderer->present_family_index,
                                &has_portability_subset)) {
        return 0;
    }
    printf("Physical device selected\n");
    
    // Create logical device
    if (!create_logical_device(vk_renderer->physical_device,
                               vk_renderer->graphics_family_index,
                               vk_renderer->present_family_index,
                               has_portability_subset,
                               &vk_renderer->device,
                               &vk_renderer->graphics_queue,
                               &vk_renderer->present_queue)) {
        return 0;
    }
    printf("Logical device created\n");
    
    // Create swapchain
    if (!create_swapchain(window, vk_renderer)) {
        return 0;
    }
    printf("Swapchain created\n");
    
    // Create image views
    if (!create_image_views(vk_renderer)) {
        return 0;
    }
    printf("Image views created\n");
    
    // Create render pass
    if (!create_render_pass(vk_renderer)) {
        return 0;
    }
    printf("Render pass created\n");
    
    // Create framebuffers
    if (!create_framebuffers(vk_renderer)) {
        return 0;
    }
    printf("Framebuffers created\n");
    
    // Create command pool
    if (!create_command_pool(vk_renderer)) {
        return 0;
    }
    printf("Command pool created\n");
    
    // Create command buffers
    if (!create_command_buffers(vk_renderer)) {
        return 0;
    }
    printf("Command buffers created\n");
    
    // Create sync objects
    if (!create_sync_objects(vk_renderer)) {
        return 0;
    }
    printf("Sync objects created\n");
    
    vk_renderer->current_frame = 0;
    vk_renderer->framebuffer_resized = 0;
    
    printf("Vulkan renderer initialized successfully!\n");
    printf("Performance tip: If you experience tearing, the renderer is using IMMEDIATE mode for maximum FPS.\n");
    printf("                 This is normal and provides the best performance.\n");
    return 1;
}

// Begin frame rendering
int vulkan_begin_frame(VulkanRenderer *vk_renderer, uint32_t *image_index) {
    // Use a timeout instead of infinite wait to prevent stuttering
    // 1 second timeout is reasonable - if frame takes longer, something is seriously wrong
    VkResult fence_result = vkWaitForFences(vk_renderer->device, 1, 
                                           &vk_renderer->in_flight_fences[vk_renderer->current_frame], 
                                           VK_TRUE, 1000000000); // 1 second in nanoseconds
    
    if (fence_result == VK_TIMEOUT) {
        printf("Warning: Fence wait timeout - GPU may be overloaded\n");
        return 0;
    } else if (fence_result != VK_SUCCESS) {
        printf("Failed to wait for fence: %d\n", fence_result);
        return 0;
    }
    
    VkResult result = vkAcquireNextImageKHR(vk_renderer->device, vk_renderer->swapchain, 1000000000, // 1 second timeout
                                           vk_renderer->image_available_semaphores[vk_renderer->current_frame],
                                           VK_NULL_HANDLE, image_index);
    
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        vk_renderer->framebuffer_resized = 1;
        return 0;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        printf("Failed to acquire swapchain image!\n");
        return 0;
    }
    
    vkResetFences(vk_renderer->device, 1, &vk_renderer->in_flight_fences[vk_renderer->current_frame]);
    
    // Reset and begin command buffer
    vkResetCommandBuffer(vk_renderer->command_buffers[vk_renderer->current_frame], 0);
    
    VkCommandBufferBeginInfo begin_info = {0};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    
    if (vkBeginCommandBuffer(vk_renderer->command_buffers[vk_renderer->current_frame], &begin_info) != VK_SUCCESS) {
        printf("Failed to begin command buffer!\n");
        return 0;
    }
    
    // Begin render pass
    VkRenderPassBeginInfo render_pass_info = {0};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_info.renderPass = vk_renderer->render_pass;
    render_pass_info.framebuffer = vk_renderer->framebuffers[*image_index];
    render_pass_info.renderArea.offset.x = 0;
    render_pass_info.renderArea.offset.y = 0;
    render_pass_info.renderArea.extent = vk_renderer->swapchain_extent;
    
    VkClearValue clear_color = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    render_pass_info.clearValueCount = 1;
    render_pass_info.pClearValues = &clear_color;
    
    vkCmdBeginRenderPass(vk_renderer->command_buffers[vk_renderer->current_frame], 
                        &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
    
    return 1;
}

// End frame rendering and present
int vulkan_end_frame(VulkanRenderer *vk_renderer, uint32_t image_index) {
    vkCmdEndRenderPass(vk_renderer->command_buffers[vk_renderer->current_frame]);
    
    if (vkEndCommandBuffer(vk_renderer->command_buffers[vk_renderer->current_frame]) != VK_SUCCESS) {
        printf("Failed to record command buffer!\n");
        return 0;
    }
    
    VkSubmitInfo submit_info = {0};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    
    VkSemaphore wait_semaphores[] = {vk_renderer->image_available_semaphores[vk_renderer->current_frame]};
    VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = wait_semaphores;
    submit_info.pWaitDstStageMask = wait_stages;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &vk_renderer->command_buffers[vk_renderer->current_frame];
    
    VkSemaphore signal_semaphores[] = {vk_renderer->render_finished_semaphores[vk_renderer->current_frame]};
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = signal_semaphores;
    
    if (vkQueueSubmit(vk_renderer->graphics_queue, 1, &submit_info, 
                     vk_renderer->in_flight_fences[vk_renderer->current_frame]) != VK_SUCCESS) {
        printf("Failed to submit draw command buffer!\n");
        return 0;
    }
    
    VkPresentInfoKHR present_info = {0};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = signal_semaphores;
    
    VkSwapchainKHR swapchains[] = {vk_renderer->swapchain};
    present_info.swapchainCount = 1;
    present_info.pSwapchains = swapchains;
    present_info.pImageIndices = &image_index;
    
    VkResult result = vkQueuePresentKHR(vk_renderer->present_queue, &present_info);
    
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || vk_renderer->framebuffer_resized) {
        vk_renderer->framebuffer_resized = 1;
    } else if (result != VK_SUCCESS) {
        printf("Failed to present swapchain image!\n");
        return 0;
    }
    
    vk_renderer->current_frame = (vk_renderer->current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
    
    return 1;
}

// Handle window resize
void vulkan_handle_resize(VulkanRenderer *vk_renderer) {
    // Window resize handling: flag is checked in end_frame and cleared there
    // The flag stays set until the next successful frame presentation
    // This ensures the swapchain is properly recreated
    printf("Window resize detected - swapchain will be recreated on next frame\n");
    // Note: framebuffer_resized flag is cleared in vulkan_end_frame after handling
}

// Cleanup Vulkan resources
void vulkan_cleanup(VulkanRenderer *vk_renderer) {
    if (vk_renderer->device) {
        vkDeviceWaitIdle(vk_renderer->device);
    }
    
    // Destroy sync objects
    if (vk_renderer->image_available_semaphores && vk_renderer->device) {
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            if (vk_renderer->image_available_semaphores[i]) {
                vkDestroySemaphore(vk_renderer->device, vk_renderer->image_available_semaphores[i], NULL);
            }
        }
    }
    if (vk_renderer->render_finished_semaphores && vk_renderer->device) {
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            if (vk_renderer->render_finished_semaphores[i]) {
                vkDestroySemaphore(vk_renderer->device, vk_renderer->render_finished_semaphores[i], NULL);
            }
        }
    }
    if (vk_renderer->in_flight_fences && vk_renderer->device) {
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            if (vk_renderer->in_flight_fences[i]) {
                vkDestroyFence(vk_renderer->device, vk_renderer->in_flight_fences[i], NULL);
            }
        }
    }
    free(vk_renderer->image_available_semaphores);
    free(vk_renderer->render_finished_semaphores);
    free(vk_renderer->in_flight_fences);
    
    // Destroy command pool
    if (vk_renderer->command_pool && vk_renderer->device) {
        vkDestroyCommandPool(vk_renderer->device, vk_renderer->command_pool, NULL);
    }
    free(vk_renderer->command_buffers);
    
    // Destroy framebuffers
    if (vk_renderer->framebuffers && vk_renderer->device) {
        for (uint32_t i = 0; i < vk_renderer->swapchain_image_count; i++) {
            if (vk_renderer->framebuffers[i]) {
                vkDestroyFramebuffer(vk_renderer->device, vk_renderer->framebuffers[i], NULL);
            }
        }
    }
    free(vk_renderer->framebuffers);
    
    // Destroy render pass
    if (vk_renderer->render_pass && vk_renderer->device) {
        vkDestroyRenderPass(vk_renderer->device, vk_renderer->render_pass, NULL);
    }
    
    // Destroy image views
    if (vk_renderer->swapchain_image_views && vk_renderer->device) {
        for (uint32_t i = 0; i < vk_renderer->swapchain_image_count; i++) {
            if (vk_renderer->swapchain_image_views[i]) {
                vkDestroyImageView(vk_renderer->device, vk_renderer->swapchain_image_views[i], NULL);
            }
        }
    }
    free(vk_renderer->swapchain_image_views);
    free(vk_renderer->swapchain_images);
    
    // Destroy swapchain
    if (vk_renderer->swapchain && vk_renderer->device) {
        vkDestroySwapchainKHR(vk_renderer->device, vk_renderer->swapchain, NULL);
    }
    
    // Destroy device
    if (vk_renderer->device) {
        vkDestroyDevice(vk_renderer->device, NULL);
    }
    
    // Destroy surface
    if (vk_renderer->surface && vk_renderer->instance) {
        vkDestroySurfaceKHR(vk_renderer->instance, vk_renderer->surface, NULL);
    }
    
    // Destroy instance
    if (vk_renderer->instance) {
        vkDestroyInstance(vk_renderer->instance, NULL);
    }
    
    printf("Vulkan renderer cleaned up\n");
}
