#include <example.hpp>

#include <vpp/image.hpp>
#include <vpp/descriptor.hpp>
#include <vpp/graphicsPipeline.hpp>
#include <vpp/vk.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define DDF_FONT_IMPLEMENTATION
#include "ddf_font.h"

class TextureData : public vpp::Resource
{
public:
	TextureData(App& app) : app_(app) {}
	~TextureData() {}

	void init()
	{
		if(initialized_) return;
		vpp::Resource::init(app_.context.device());

		//image
		{
			// //load image 1
			// int width1, height1, comp1;
			// auto data1 = stbi_load("image1.png", &width1, &height1, &comp1, 4);
			// if(!data1) throw std::runtime_error("Failed to load image image1.png");
			//
			// //load image 2
			// int width2, height2, comp2;
			// auto data2 = stbi_load("test.png", &width2, &height2, &comp2, 4);
			// if(!data2) throw std::runtime_error("Failed to load image image2.png");

			// int width, height, comp;
			// auto data = stbi_load("test.png", &width, &height, &comp, 4);
			// if(!data) throw std::runtime_error("Failed to load image image2.png");

			//assert(comp = 4); //assert rgba
			// vk::Extent3D extent;
			// extent.width = width;
			// extent.height = height;
			// extent.depth = 1;
			//
			// //texture
			// auto info = vpp::ViewableImage::defaultColor2D();
			// info.imgInfo.extent = extent;
			// info.imgInfo.tiling = vk::ImageTiling::optimal;
			// info.imgInfo.format = vk::Format::r8g8b8a8Unorm;
			// info.viewInfo.format = vk::Format::r8g8b8a8Unorm;
			// info.memoryFlags = vk::MemoryPropertyBits::hostVisible;
			// texture_ = {device(), info};

			// std::cout << texture_.image().size() << "\n";
			// std::cout << width * height * comp << "\n";

			// fill(texture_.image(), *data, vk::Format::r8g8b8a8Unorm, vk::ImageLayout::undefined,
			// 	extent, {vk::ImageAspectBits::color, 0, 0})->finish();

			// extent.width = width2;
			// extent.height = height2;
			//
			// fill(texture_.image(), *data2, vk::Format::r8g8b8a8Unorm, vk::ImageLayout::undefined,
			// 	extent, {vk::ImageAspectBits::color, 0, 0}, {width1, height1, 0})->finish();



			ddf_font font;
			if(!ddf_font_create(&font, "font.ddf"))
			{
				printf("ddf_font error %d", ddf_get_last_error());
				exit(-1);
			}

			auto width = font.texture_width;
			auto height = font.texture_height;
			uint8_t* data = font.texture_data;

			std::cout << width << " -- " << height << "\n";

			std::vector<uint8_t> data2(width * height * 4, 255);
			for(auto h = 0u; h < height; ++h)
			{
				for(auto w = 0u; w < width; ++w)
				{
					auto pos = h * width + w;
					std::memcpy(data2.data() + pos * 4, data + pos * 3, 3);
					// std::cout << static_cast<uint32_t>(*data2.data() + pos * 4) << "\n";
				}
			}

			vk::Extent3D extent;
			extent.width = width;
			extent.height = height;
			extent.depth = 1;

			//texture
			auto info = vpp::ViewableImage::defaultColor2D();
			info.imgInfo.extent = extent;
			info.imgInfo.tiling = vk::ImageTiling::linear;
			info.imgInfo.format = vk::Format::r8g8b8a8Unorm;
			info.viewInfo.format = vk::Format::r8g8b8a8Unorm;
			info.memoryFlags = vk::MemoryPropertyBits::hostVisible;
			texture_ = {device(), info};

			fill(texture_.image(), *data2.data(),
				vk::Format::r8g8b8a8Unorm, vk::ImageLayout::undefined,
				extent, {vk::ImageAspectBits::color, 0, 0})->finish();


			//sampler
			vk::SamplerCreateInfo samplerInfo;
			samplerInfo.magFilter = vk::Filter::linear;
			samplerInfo.minFilter = vk::Filter::linear;
			samplerInfo.mipmapMode = vk::SamplerMipmapMode::nearest;
			samplerInfo.addressModeU = vk::SamplerAddressMode::repeat;
			samplerInfo.addressModeV = vk::SamplerAddressMode::repeat;
			samplerInfo.addressModeW = vk::SamplerAddressMode::repeat;
			samplerInfo.mipLodBias = 0;
			samplerInfo.anisotropyEnable = true;
			samplerInfo.maxAnisotropy = 16;
			samplerInfo.compareEnable = false;
			samplerInfo.compareOp = {};
			samplerInfo.minLod = 0;
			samplerInfo.maxLod = 0.25;
			samplerInfo.borderColor = vk::BorderColor::floatTransparentBlack;
			samplerInfo.unnormalizedCoordinates = false;
			sampler_ = vk::createSampler(device(), samplerInfo);
		}

		//descriptor
		vpp::DescriptorSetLayout layout;
		{
			//pool
			vk::DescriptorPoolSize typeCounts[1];
			typeCounts[0].type = vk::DescriptorType::combinedImageSampler;
			typeCounts[0].descriptorCount = 1;

			vk::DescriptorPoolCreateInfo info;
			info.poolSizeCount = 1;
			info.pPoolSizes = typeCounts;
			info.maxSets = 1;

			descriptorPool_ = {device(), info};

			//set
			layout = {device(), {vpp::descriptorBinding(vk::DescriptorType::combinedImageSampler,
				vk::ShaderStageBits::fragment)}};

			descriptorSet_ = vpp::DescriptorSet(layout, descriptorPool_);

			//write
			vpp::DescriptorSetUpdate update(descriptorSet_);
			update.imageSampler({{sampler_, texture_.vkImageView(), vk::ImageLayout::general}});
		}

		//pipeline
		{
			pipelineLayout_ = {device(), {layout}};

			vpp::GraphicsPipelineBuilder builder(device(), app_.renderPass);
			builder.layout = pipelineLayout_;
			builder.dynamicStates = {vk::DynamicState::viewport, vk::DynamicState::scissor};

			builder.shader.stage("texture.vert.spv", {vk::ShaderStageBits::vertex});
			builder.shader.stage("texture.frag.spv", {vk::ShaderStageBits::fragment});

			builder.states.blendAttachments[0].blendEnable = true;
			builder.states.blendAttachments[0].colorBlendOp = vk::BlendOp::add;
			builder.states.blendAttachments[0].srcColorBlendFactor = vk::BlendFactor::srcAlpha;
			builder.states.blendAttachments[0].dstColorBlendFactor =
				vk::BlendFactor::oneMinusSrcAlpha;
			builder.states.blendAttachments[0].srcAlphaBlendFactor = vk::BlendFactor::one;
			builder.states.blendAttachments[0].dstAlphaBlendFactor = vk::BlendFactor::zero;
			builder.states.blendAttachments[0].alphaBlendOp = vk::BlendOp::add;

			builder.states.rasterization.cullMode = vk::CullModeBits::none;
			builder.states.inputAssembly.topology = vk::PrimitiveTopology::triangleList;

			pipeline_ = builder.build();
		}


		initialized_ = true;
	}

protected:
	friend class TextureRenderer;

	App& app_;
	vpp::ViewableImage texture_;
	vpp::Pipeline pipeline_;
	vpp::PipelineLayout pipelineLayout_;
	vpp::DescriptorSet descriptorSet_;
	vpp::DescriptorPool descriptorPool_;
	vk::Sampler sampler_;
	bool initialized_ = false;
};

class TextureRenderer : public vpp::RendererBuilder
{
public:
	TextureRenderer(TextureData& xdata) : data_(xdata) {}
	~TextureRenderer() = default;

	TextureData& data() { return data_; }

	void init(vpp::SwapChainRenderer& renderer) override
	{
		data_.init();
		renderer.record();
	}
	void build(unsigned int, const vpp::RenderPassInstance& instance) override
	{
		auto cmdBuffer = instance.vkCommandBuffer();
		vk::cmdBindPipeline(cmdBuffer, vk::PipelineBindPoint::graphics,
			data().pipeline_);
		vk::cmdBindDescriptorSets(cmdBuffer, vk::PipelineBindPoint::graphics,
			data().pipelineLayout_, 0, {data().descriptorSet_}, {});
		vk::cmdDraw(cmdBuffer, 6, 1, 0, 0);
	}
	std::vector<vk::ClearValue> clearValues(unsigned int) override
	{
		std::vector<vk::ClearValue> ret(2, vk::ClearValue{});
		ret[0].color = {{0.f, 0.f, 0.f, 1.0f}};
		ret[1].depthStencil = {1.f, 0};
		return ret;
	}

protected:
	TextureData& data_;
};

int main(int argc, char** argv)
{
	App app;
	TextureData data(app);
	initApp(app, [&](){ return std::make_unique<TextureRenderer>(data); });
	mainLoop(app, [](){});
}
