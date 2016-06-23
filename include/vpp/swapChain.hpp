#pragma once

#include <vpp/fwd.hpp>
#include <vpp/resource.hpp>

#include <vector>
#include <string>
#include <memory>

namespace vpp
{

///TODO: more settings, especially related to blocking, presentation mode (vsync) etc.
///TODO: synchronization for acquire and present commands. Handle acquire -> out_of_date
///Represents Vulkan swap chain and associated images/frameBuffers.
class SwapChain : public Resource
{
public:
    struct RenderBuffer
    {
        vk::Image image;
        vk::ImageView imageView;
    };

public:
	SwapChain() = default;
	///Constructs the swap chain for a given vulkan surface.
	///Note that the given size is only used if the backend does not provide
	///a surface size (e.g. wayland backend). Usually the swapChain will have the same size
	///as the underlaying native surface, then the size parameter is ignored.
	SwapChain(const Device& device, vk::SurfaceKHR surface, const vk::Extent2D& size);
    ~SwapChain();

	SwapChain(SwapChain&& other) noexcept;
	SwapChain& operator=(SwapChain other) noexcept;

	///Resizes the swapchain to the given size. Should be called if the native window of the
	///underlaying surface handle changes it size to make sure the swapchain fills the
	///whole window. May invalidate all images and imageViews retrived before the resize.
	///Note that the size paramter is only used when the backend does not provide a fixed
	///surface size. Otherwise the swapchain will simply be resized to the current surface
	///size and the given size paramter is ignored.
	void resize(const vk::Extent2D& size);

	///Acquires the next swapchain image (i.e. the next render buffer).
	///\param sem Semaphore to be signaled when acquiring is complete or nullHandle.
	///\param fence Fence to be signaled when acquiring is complete or nullHandle.
	///\return The id of the newly acquired image.
	///May block depending on present mode. TODO.
    unsigned int acquireNextImage(vk::Semaphore sem = {}, vk::Fence fence = {}) const;

	///Queues commands to present the image with the given id on the given queue. TODO
	///\param wait The semaphore to wait on before presenting (usually signaled at the end
	///of all rendering commands for this image). Can be nullHandle.
    void present(const Queue& queue, unsigned int image, vk::Semaphore wait = {}) const;

    const vk::SwapchainKHR& vkSwapChain() const { return swapChain_; }
	const vk::SurfaceKHR& vkSurface() const { return surface_; }

	vk::Format format() const { return format_; }
	vk::ColorSpaceKHR colorSpace() const { return colorSpace_; }
	vk::Extent2D size() const;
    const std::vector<RenderBuffer>& renderBuffers() const { return buffers_; }

	operator vk::SwapchainKHR() const { return vkSwapChain(); }
	friend void swap(SwapChain& a, SwapChain& b) noexcept;

protected:
	void initSwapChain();
    void queryFormats();
    vk::SwapchainCreateInfoKHR swapChainCreateInfo();
	void destroyBuffers();

protected:
    vk::SwapchainKHR swapChain_ {};
    vk::Format format_;
    vk::ColorSpaceKHR colorSpace_;

	unsigned int width_ = 0;
	unsigned int height_ = 0;

	vk::SurfaceKHR surface_ {};
	std::vector<RenderBuffer> buffers_;
};

}
