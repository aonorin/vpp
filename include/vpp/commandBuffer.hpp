#pragma once

//experimental feature, may not be fully working

#include <vpp/fwd.hpp>
#include <vpp/resource.hpp>

#include <memory>
#include <map>
#include <vector>

namespace vpp
{

//Document somewhere the synchorinzation requirements of these classes.
//e.g. CommandBuffers must be destroyed in the same thread as they were constructed.

class CommandPool;
namespace fwd { extern const vk::CommandBufferLevel commandBufferLevelPrimary; }

///XXX: this class really needed?
//CommandBuffer
class CommandBuffer : public ResourceHandleReference<vk::CommandBuffer, CommandBuffer>
{
public:
	CommandBuffer() = default;
	CommandBuffer(vk::CommandBuffer buffer, const CommandPool& pool);
	~CommandBuffer();

	CommandBuffer(CommandBuffer&& other) noexcept = default;
	CommandBuffer& operator=(CommandBuffer&& other) noexcept = default;

	const CommandPool& commandPool() const { return *commandPool_; }
	const CommandPool& resourceRef() const { return *commandPool_; }

protected:
	const CommandPool* commandPool_ {};
};

//CommandPool
//XXX: needed?
class CommandPool : public ResourceHandle<vk::CommandPool>
{
public:
	CommandPool() = default;
	CommandPool(const Device& dev, std::uint32_t qfam, vk::CommandPoolCreateFlags flags = {});
	~CommandPool();

	CommandPool(CommandPool&& other) noexcept = default;
	CommandPool& operator=(CommandPool&& other) noexcept = default;

	std::vector<CommandBuffer> allocate(std::size_t count,
		vk::CommandBufferLevel lvl = fwd::commandBufferLevelPrimary);
	CommandBuffer allocate(vk::CommandBufferLevel lvl = fwd::commandBufferLevelPrimary);

	void reset(vk::CommandPoolResetFlags flags) const;

	const std::uint32_t& queueFamily() const { return qFamily_; }
	const vk::CommandPoolCreateFlags& flags() const { return flags_; }

protected:
	vk::CommandPoolCreateFlags flags_ {};
	std::uint32_t qFamily_ {};
};

}
