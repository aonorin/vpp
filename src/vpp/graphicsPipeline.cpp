// Copyright (c) 2017 nyorain
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt

#include <vpp/graphicsPipeline.hpp>
#include <vpp/shader.hpp>
#include <vpp/image.hpp>
#include <vpp/vk.hpp>

#include <utility>
#include <algorithm>

namespace vpp {

GraphicsPipelineBuilder::GraphicsPipelineBuilder(vk::RenderPass rp,
	unsigned int xsubpass) : renderPass(rp), subpass(xsubpass)
{
	dynamicStates = {vk::DynamicState::viewport, vk::DynamicState::scissor};

	//fill
	states.inputAssembly.topology = vk::PrimitiveTopology::triangleList;

	states.rasterization.polygonMode = vk::PolygonMode::fill;
	states.rasterization.cullMode = vk::CullModeBits::none;
	states.rasterization.frontFace = vk::FrontFace::counterClockwise;
	states.rasterization.depthClampEnable = true;
	states.rasterization.rasterizerDiscardEnable = false;
	states.rasterization.depthBiasEnable = false;
	states.rasterization.lineWidth = 1.f;

	//blendAttachments
	states.blendAttachments.emplace_back();
	states.blendAttachments.back().blendEnable = false;
	states.blendAttachments.back().colorWriteMask =
		vk::ColorComponentBits::r |
		vk::ColorComponentBits::g |
		vk::ColorComponentBits::b |
		vk::ColorComponentBits::a;

	//stencil
	vk::StencilOpState stencil;
	stencil.failOp = vk::StencilOp::keep;
	stencil.passOp = vk::StencilOp::keep;
	stencil.compareOp = vk::CompareOp::always;

	states.depthStencil.depthTestEnable = true;
	states.depthStencil.depthWriteEnable = true;
	states.depthStencil.depthCompareOp = vk::CompareOp::lessOrEqual;
	states.depthStencil.depthBoundsTestEnable = false;
	states.depthStencil.stencilTestEnable = false;
	states.depthStencil.back = stencil;
	states.depthStencil.front = stencil;

	//multisample
	states.multisample.pSampleMask = nullptr;
	states.multisample.rasterizationSamples = vk::SampleCountBits::e1;
}

Pipeline GraphicsPipelineBuilder::build(const Device& dev, vk::PipelineCache cache)
{
	auto info = parse();
	vk::Pipeline pipeline;
	vk::createGraphicsPipelines(dev, cache, 1, info, nullptr, pipeline);
	return Pipeline(dev, pipeline);
}

vk::GraphicsPipelineCreateInfo GraphicsPipelineBuilder::parse()
{
	vk::GraphicsPipelineCreateInfo ret;

	//reset
	bindingDescriptions_.clear();
	attributeDescriptions_.clear();

	//Binding description
	std::size_t attributeCount = 0;
	for(auto& layout : vertexBufferLayouts)
		attributeCount += layout.attributes.size();

	bindingDescriptions_.reserve(vertexBufferLayouts.size());
	attributeDescriptions_.reserve(attributeCount);

	for(auto& layout : vertexBufferLayouts)
	{
		std::size_t location = 0;
		std::size_t offset = 0;

		for(auto& attribute : layout.attributes)
		{
			attributeDescriptions_.emplace_back();
			attributeDescriptions_.back().location = location++;
			attributeDescriptions_.back().binding = layout.binding;
			attributeDescriptions_.back().format = attribute;
			attributeDescriptions_.back().offset = offset;
			offset += formatSize(attribute);
		}

		bindingDescriptions_.emplace_back();
		bindingDescriptions_.back().binding = layout.binding;
		bindingDescriptions_.back().stride = offset;
		bindingDescriptions_.back().inputRate = vk::VertexInputRate::vertex; ///XXX: paramterize
	}

	vertexInfo_.vertexBindingDescriptionCount = bindingDescriptions_.size();
	vertexInfo_.pVertexBindingDescriptions = bindingDescriptions_.data();
	vertexInfo_.vertexAttributeDescriptionCount = attributeDescriptions_.size();
	vertexInfo_.pVertexAttributeDescriptions = attributeDescriptions_.data();

	//update states with data
	colorBlend_.attachmentCount = states.blendAttachments.size();
	colorBlend_.pAttachments = states.blendAttachments.data();

	viewport_.viewportCount = states.viewports.size();
	viewport_.pViewports = states.viewports.data();
	viewport_.scissorCount = states.scissors.size();
	viewport_.pScissors = states.scissors.data();

	if(std::find(dynamicStates.begin(), dynamicStates.end(), vk::DynamicState::scissor)
		!= dynamicStates.end() && !states.scissors.size()) viewport_.scissorCount = 1;

	if(std::find(dynamicStates.begin(), dynamicStates.end(), vk::DynamicState::viewport)
		!= dynamicStates.end() && !states.viewports.size()) viewport_.viewportCount = 1;

	//dynamic state
	if(!dynamicStates.empty())
	{
		dynamicState_.pDynamicStates = dynamicStates.data();
		dynamicState_.dynamicStateCount = dynamicStates.size();
		ret.pDynamicState = &dynamicState_;
	}

	stageInfos_ = shader.vkStageInfos();
	ret.stageCount = stageInfos_.size();
	ret.pStages = stageInfos_.data();

	ret.pViewportState = &viewport_;
	ret.layout = layout;
	ret.pVertexInputState = &vertexInfo_;
	ret.renderPass = renderPass;
	ret.subpass = subpass;
	ret.pInputAssemblyState = &states.inputAssembly;
	ret.pRasterizationState = &states.rasterization;
	ret.pColorBlendState = &colorBlend_;
	ret.pMultisampleState = &states.multisample;
	ret.pDepthStencilState = &states.depthStencil;
	ret.pTessellationState = nullptr;

	return ret;
}

// std::vector<Pipeline> createGraphicsPipelines(const Device& dev,
// 	nytl::Span<const vk::GraphicsPipelineCreateInfo> infos, vk::PipelineCache cache)
// {
// 	auto pipelines = vk::createGraphicsPipelines(dev, cache, infos);
// 	std::vector<Pipeline> ret;
// 	ret.reserve(pipelines.size());
// 	for(auto& p : pipelines) ret.emplace_back(dev, p);
// 	return ret;
// }
//
// std::vector<Pipeline> createGraphicsPipelines(
// 	nytl::Span<const std::reference_wrapper<GraphicsPipelineBuilder>>& builder,
// 	vk::PipelineCache cache)
// {
// 	if(builder.empty()) return {};
//
// 	std::vector<vk::GraphicsPipelineCreateInfo> infos;
// 	infos.reserve(builder.size());
// 	for(auto& b : builder) infos.push_back(b.get().parse());
//
// 	return createGraphicsPipelines(builder.front().get().shader.device(), infos, cache);
// }

}
