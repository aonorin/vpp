#include <vpp/framebuffer.hpp>
#include <vpp/memory.hpp>
#include <vpp/vk.hpp>
#include <vpp/renderPass.hpp>
#include <vpp/utility/range.hpp>

namespace vpp
{

//Framebuffer
Framebuffer::Framebuffer(const Device& dev, vk::RenderPass rp, const vk::Extent2D& size,
	AttachmentsInfo attachments, const ExtAttachments& ext)
{
	std::vector<vk::ImageCreateInfo> imgInfo(attachments.size());
	std::vector<vk::ImageViewCreateInfo> viewInfo(attachments.size());
	for(auto i = 0u; i < attachments.size(); ++i)
	{
		imgInfo[i] = attachments[i].first;
		viewInfo[i] = attachments[i].second;
	}

	create(dev, size, imgInfo);
	init(rp, viewInfo, ext);
}

Framebuffer::~Framebuffer()
{
	if(vkFramebuffer()) vk::destroyFramebuffer(vkDevice(), vkFramebuffer(), nullptr);
	attachments_.clear();
	framebuffer_ = {};
}

Framebuffer::Framebuffer(Framebuffer&& other) noexcept
{
	swap(*this, other);
}
Framebuffer& Framebuffer::operator=(Framebuffer&& other) noexcept
{
	this->~Framebuffer();
	swap(*this, other);
	return *this;
}

void swap(Framebuffer& a, Framebuffer& b) noexcept
{
	using std::swap;

	swap(a.framebuffer_, b.framebuffer_);
	swap(a.attachments_, b.attachments_);
	swap(a.device_, b.device_);
}

void Framebuffer::create(const Device& dev, const vk::Extent2D& size,
	const std::vector<vk::ImageCreateInfo>& imgInfo)
{
	Resource::init(dev);
	size_ = size;

	attachments_.reserve(imgInfo.size());
	for(auto& attinfo : imgInfo) {
		auto imageInfo = attinfo;
		imageInfo.extent = {size.width, size.height, 1};

		attachments_.emplace_back();
		attachments_.back().create(device(), imageInfo);
	}
}

void Framebuffer::init(vk::RenderPass rp, const std::vector<vk::ImageViewCreateInfo>& viewInfo,
	const ExtAttachments& extAttachments)
{
	if(viewInfo.size() < attachments_.size())
		throw std::logic_error("vpp::Framebuffer::init: to few viewInfos");

	for(std::size_t i(0); i < attachments_.size(); ++i)
		attachments_[i].init(viewInfo[i]);

	//framebuffer
	//attachments
	std::vector<vk::ImageView> attachments;
	attachments.resize(attachments_.size() + extAttachments.size());

	for(auto& extView : extAttachments)
	{
		if(extView.first > attachments.size())
			throw std::logic_error("bpp::Framebuffer: invalid external Attachment id given");

		attachments[extView.first] = extView.second;
	}

	std::size_t currentID = 0;
	for(auto& buf : attachments_)
	{
		while(attachments[currentID]) ++currentID;

		attachments[currentID] = buf.vkImageView();
		++currentID;
	}

	//createinfo
	vk::FramebufferCreateInfo createInfo;
	createInfo.renderPass = rp;
	createInfo.attachmentCount = attachments.size();
	createInfo.pAttachments = attachments.data();
	createInfo.width = size_.width;
	createInfo.height = size_.height;
	createInfo.layers = 1; ///XXX: should be paramterized?

	framebuffer_ = vk::createFramebuffer(vkDevice(), createInfo);
}

//static utility
Framebuffer::AttachmentsInfo Framebuffer::parseRenderPass(const RenderPass& rp, const vk::Extent2D& s)
{
	AttachmentsInfo ret;
	ret.reserve(rp.attachments().size());

	for(std::size_t i(0); i < rp.attachments().size(); ++i)
	{
		ViewableImage::CreateInfo fbaInfo;
		fbaInfo.first.format = rp.attachments()[i].format;
		fbaInfo.first.extent = {s.width, s.height, 1};
		fbaInfo.second.format = rp.attachments()[i].format;

		vk::ImageUsageFlags usage {};
		vk::ImageAspectFlags aspect {};

		for(auto& sub : rp.subpasses())
		{
			if(sub.pDepthStencilAttachment && sub.pDepthStencilAttachment->attachment == i)
			{
				usage |= vk::ImageUsageBits::depthStencilAttachment;
				aspect |= vk::ImageAspectBits::depth | vk::ImageAspectBits::stencil;
			}

			for(auto& ref : makeRange(sub.pInputAttachments, sub.inputAttachmentCount))
			{
				if(ref.attachment == i)
				{
					usage |= vk::ImageUsageBits::inputAttachment;
					aspect |= vk::ImageAspectBits::depth;
				}
			}

			for(auto& ref : makeRange(sub.pColorAttachments, sub.colorAttachmentCount))
			{
				if(ref.attachment == i)
				{
					usage |= vk::ImageUsageBits::colorAttachment;
					aspect |= vk::ImageAspectBits::color;
				}
			}
		}

		fbaInfo.first.usage = usage;
		fbaInfo.second.vkHandle().subresourceRange.aspectMask = aspect;

		ret.push_back(fbaInfo);
	}

	return ret;
}

}
