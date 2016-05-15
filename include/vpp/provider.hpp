#pragma once

#include <vpp/vk.hpp>
#include <vpp/allocator.hpp>
#include <vpp/commandBuffer.hpp>

#include <vector>
#include <string>
#include <memory>
#include <mutex>
#include <thread>

namespace vpp
{

///Able to provide DeviceMemory for the calling thread.
class DeviceMemoryProvider : public Resource
{
public:
	DeviceMemoryProvider(const Device& dev);
	~DeviceMemoryProvider() = default;

	///Returns a DeviceMemoryAllocator that can be used for allocating memory in the calling thread.
	///Sett DeviceMemoryAllocator for more information.
	DeviceMemoryAllocator& get();

protected:
	std::map<std::thread::id, DeviceMemoryAllocator> allocatorMap_;
	std::mutex mutex_;
};

//Able to efficiently provide commandBuffer of all types for all threads by holding an internal
//pool of commandPools.
class CommandBufferProvider : public Resource
{
public:
	CommandBufferProvider(const Device& dev);
	~CommandBufferProvider() = default;

	///Allocates a commandBuffer for the calling thread that matches the given requirements.
	CommandBuffer get(std::uint32_t qfamily, vk::CommandPoolCreateFlags flags = {},
		vk::CommandBufferLevel lvl = vk::CommandBufferLevel::Primary);

	///Allocates a commandBuffer that matches the given flags.
	// CommandBuffer get(vk::QueueFlags qflags, vk::CommandPoolCreateFlags flags = {},
		// vk::CommandBufferLevel lvl = vk::CommandBufferLevel::Primary);

	//Allocates multiple commandBuffers for the calling thread with the given requirements.
	///Might be more efficient than multiple calls to get.
	std::vector<CommandBuffer> get(std::uint32_t qfamily, unsigned int count,
		vk::CommandPoolCreateFlags flags = {},
		vk::CommandBufferLevel lvl = vk::CommandBufferLevel::Primary);

protected:
	std::map<std::thread::id, std::vector<CommandPool>> commandPools_;
	std::mutex mutex_;
};

}
