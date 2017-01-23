// Copyright (c) 2017 nyorain
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt

#pragma once

#include <vpp/fwd.hpp>
#include <vpp/resource.hpp>
#include <vpp/shader.hpp>
#include <vpp/util/span.hpp>
#include <vpp/util/stringParam.hpp>

#include <vector>
#include <functional>

namespace vpp {

//TODO: rework. Does not offer all flexibility needed. stride and stuff atm not changeable
//for graphics pipelines. Abolish those wrapper structs and replace them with initializer funcs.
//VkVertexInputBindingDescription: describes a vertex buffer binding.
//VkVertexInputAttributeDescription: describes a vertexShader <in> variable and associates it
//	with a vertex buffer binding.

///Defines the different vertex inputs a pipeline takes.
class VertexBufferLayout
{
public:
	std::vector<vk::Format> attributes;
	unsigned int binding {0};
};

///RAII vulkan pipeline layout wrapper.
///A pipeline layout specifies all descriptor layouts and push constant ranges an
///application can use.
///Multiple pipelines can share the same pipeline layout.
class PipelineLayout : public ResourceHandle<vk::PipelineLayout>
{
public:
	PipelineLayout() = default;
	PipelineLayout(const Device& dev, const vk::PipelineLayoutCreateInfo& info);
	PipelineLayout(const Device& dev,
		nytl::Span<const std::reference_wrapper<DescriptorSetLayout>> layouts,
		nytl::Span<const vk::PushConstantRange> ranges);
	~PipelineLayout();

	PipelineLayout(PipelineLayout&& lhs) noexcept { swap(lhs); }
	PipelineLayout& operator=(PipelineLayout&& lhs) noexcept { swap(lhs); return *this; }
};

///RAII vulkan pipeline cache wrapper.
///A pipeline cache can be used to increase pipeline compilation time.
///The application can save driver-specific cache data from created pipelines to disk
///and then load them during the next execution and pass them to the pipeline constructor
///which will decrease pipeline build time.
class PipelineCache : public ResourceHandle<vk::PipelineCache>
{
public:
	PipelineCache() = default;
	PipelineCache(const Device& dev);

	///Creates the pipeline cache with the given initial data.
	PipelineCache(const Device& dev, nytl::Span<const std::uint8_t> data);

	///Loads the pipeline cache data from a file.
	///\exception std::runtime_error if opening the file fails
	PipelineCache(const Device& dev, nytl::StringParam filename);
	~PipelineCache();

	PipelineCache(PipelineCache&& other) noexcept { swap(other); }
	PipelineCache& operator=(PipelineCache other) noexcept { swap(other); return *this; }
};

///\{
///Saves a pipeline cache to the given filename.
///\exception std::runtime_error if opening/writing the file fails
void save(vk::Device dev, vk::PipelineCache cache, nytl::StringParam filename);
inline void save(const PipelineCache& cache, nytl::StringParam file)
	{ save(cache.device(), cache, file); }
///\}

///XXX: resourceRef: pipeline layout
///RAII Vulkan pipeline wrapper.
///A pipeline is basically a collection of draw/compute information that contains e.g.
///the vertex and descriptor layouts, used shaders and different state information.
class Pipeline : public ResourceHandle<vk::Pipeline> {
public:
	Pipeline() = default;
	Pipeline(const Device& dev, vk::Pipeline pipeline) : ResourceHandle(dev, pipeline) {}
	~Pipeline();

	Pipeline(Pipeline&& lhs) noexcept { swap(lhs); }
	Pipeline& operator=(Pipeline&& lhs) noexcept { swap(lhs); return *this; }
};

}
