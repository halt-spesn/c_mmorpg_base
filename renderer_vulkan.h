#ifndef RENDERER_VULKAN_H
#define RENDERER_VULKAN_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <vulkan/vulkan.h>

// Maximum number of frames in flight
#define MAX_FRAMES_IN_FLIGHT 2

typedef struct {
    VkInstance instance;
    VkPhysicalDevice physical_device;
    VkDevice device;
    VkQueue graphics_queue;
    VkQueue present_queue;
    VkSurfaceKHR surface;
    VkSwapchainKHR swapchain;
    VkFormat swapchain_image_format;
    VkExtent2D swapchain_extent;
    VkImage *swapchain_images;
    VkImageView *swapchain_image_views;
    uint32_t swapchain_image_count;
    VkRenderPass render_pass;
    VkPipelineLayout pipeline_layout;
    VkPipeline graphics_pipeline;
    VkFramebuffer *framebuffers;
    VkCommandPool command_pool;
    VkCommandBuffer *command_buffers;
    VkSemaphore *image_available_semaphores;
    VkSemaphore *render_finished_semaphores;
    VkFence *in_flight_fences;
    uint32_t current_frame;
    uint32_t graphics_family_index;
    uint32_t present_family_index;
    int framebuffer_resized;
} VulkanRenderer;

// Initialize Vulkan renderer
int vulkan_init(SDL_Window *window, VulkanRenderer *vk_renderer);

// Cleanup Vulkan resources
void vulkan_cleanup(VulkanRenderer *vk_renderer);

// Begin frame rendering
int vulkan_begin_frame(VulkanRenderer *vk_renderer, uint32_t *image_index);

// End frame rendering and present
int vulkan_end_frame(VulkanRenderer *vk_renderer, uint32_t image_index);

// Handle window resize
void vulkan_handle_resize(VulkanRenderer *vk_renderer);

#endif // RENDERER_VULKAN_H
