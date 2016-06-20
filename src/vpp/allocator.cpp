#include <vpp/allocator.hpp>
#include <vpp/vk.hpp>
#include <cmath>
#include <algorithm>

namespace vpp
{

//Entry
MemoryEntry::MemoryEntry(DeviceMemory* memory, const Allocation& alloc)
	: memory_(memory), allocation_(alloc)
{
}

MemoryEntry::MemoryEntry(MemoryEntry&& other) noexcept
{
	swap(*this, other);
}

MemoryEntry& MemoryEntry::operator=(MemoryEntry&& other) noexcept
{
	this->~MemoryEntry();
	swap(*this, other);
	return *this;
}

MemoryEntry::~MemoryEntry()
{
	if(!allocated() && allocator_) allocator_->removeRequest(*this);
	else if(allocated()) memory_->free(allocation_);

	allocation_ = {};
	allocator_ = nullptr;
}

void swap(MemoryEntry& a, MemoryEntry& b) noexcept
{
	using std::swap;

	//signal the allocator (if there is any) that the entry has been moved
	//since the allocator stores references (addresses) of the entries
	if(!a.allocated() && a.allocator_) a.allocator_->moveEntry(a, b);
	if(!b.allocated() && b.allocator_) b.allocator_->moveEntry(b, a);

	//correclty swap the anonymous union
	//can be proably be done more elegant...

	//backup the memory or allocator values of a
	auto memTmp = a.allocated() ? a.memory() : nullptr;
	auto allocTmp = a.allocated() ? nullptr : a.allocator();

	//correclty "swap" the unions
	if(b.allocated()) a.memory_ = b.memory_;
	else a.allocator_ = b.allocator_;

	if(a.allocated()) b.memory_ = memTmp;
	else b.allocator_ = allocTmp;

	//swap allocations
	swap(a.allocation_, b.allocation_);
}

MemoryMapView MemoryEntry::map() const
{
	return memory()->map(allocation());
}

//Allocator
DeviceMemoryAllocator::DeviceMemoryAllocator(const Device& dev) : Resource(dev)
{
}

DeviceMemoryAllocator::~DeviceMemoryAllocator()
{
	if(device_) allocate();
}

DeviceMemoryAllocator::DeviceMemoryAllocator(DeviceMemoryAllocator&& other) noexcept
{
	swap(*this, other);
}
DeviceMemoryAllocator& DeviceMemoryAllocator::operator=(DeviceMemoryAllocator&& other) noexcept
{
	if(device_) allocate();
	swap(*this, other);
	return *this;
}

void swap(DeviceMemoryAllocator& a, DeviceMemoryAllocator& b) noexcept
{
	using std::swap;

	swap(a.requirements_, b.requirements_);
	swap(a.device_, b.device_);
	swap(a.memories_, b.memories_);
}

void DeviceMemoryAllocator::request(vk::Buffer requestor, const vk::MemoryRequirements& reqs,
	MemoryEntry& entry)
{
	if(!reqs.size)
		throw std::logic_error("vpp::DeviceMemAllocator::request: allocation size of 0 not allowed");

	entry = {};
	entry.allocator_ = this;

	Requirement req;
	req.type = RequirementType::buffer;
	req.size = reqs.size;
	req.alignment = reqs.alignment;
	req.memoryTypes = reqs.memoryTypeBits;
	req.buffer = requestor;
	req.entry = &entry;

	requirements_.push_back(req);
}

void DeviceMemoryAllocator::request(vk::Image requestor, const vk::MemoryRequirements& reqs,
	vk::ImageTiling tiling, MemoryEntry& entry)
{
	if(reqs.size == 0)
		throw std::logic_error("vpp::DeviceMemAllocator::request: allocation size of 0 not allowed");

	entry = {};
	entry.allocator_ = this;

	using ReqType = RequirementType;

	Requirement req;
	req.type = (tiling == vk::ImageTiling::linear) ? ReqType::linearImage : ReqType::optimalImage;
	req.size = reqs.size;
	req.alignment = reqs.alignment;
	req.memoryTypes = reqs.memoryTypeBits;
	req.image = requestor;
	req.entry = &entry;

	requirements_.push_back(req);
}

bool DeviceMemoryAllocator::removeRequest(const MemoryEntry& entry)
{
	auto it = findReq(entry);
	if(it == requirements_.end()) return false;
	requirements_.erase(it);
	return true;
}

bool DeviceMemoryAllocator::moveEntry(const MemoryEntry& oldOne, MemoryEntry& newOne)
{
	auto req = findReq(oldOne);
	if(req == requirements_.end()) return false;

	req->entry = &newOne;
	return true;
}

DeviceMemory* DeviceMemoryAllocator::findMem(Requirement& req)
{
	for(auto& mem : memories_)
	{
		if(!(req.memoryTypes & (1 << mem->typeIndex()))) continue;
		auto allocation = mem->allocatable(req.size, req.alignment, toAllocType(req.type));
		if(allocation.size == 0) continue;

		mem->allocSpecified(allocation.offset, allocation.size, toAllocType(req.type));

		//can be allocated on memory, allocate and bind it
		if(req.type == RequirementType::buffer)
			vk::bindBufferMemory(vkDevice(), req.buffer, mem->vkDeviceMemory(), allocation.offset);
		else
			vk::bindImageMemory(vkDevice(), req.image, mem->vkDeviceMemory(), allocation.offset);

		return mem.get();
	}

	return nullptr;
}

DeviceMemoryAllocator::Requirements::iterator DeviceMemoryAllocator::findReq(const MemoryEntry& entry)
{
	return std::find_if(requirements_.begin(), requirements_.end(), [&](const auto& r)
		{ return r.entry == &entry; });
}

//TODO: all 4 allocate functions can be improved.
void DeviceMemoryAllocator::allocate()
{
	auto map = queryTypes();
	for(auto& type : map) allocate(type.first, type.second);
	requirements_.clear(); //all requirements can be removed
}

bool DeviceMemoryAllocator::allocate(const MemoryEntry& entry)
{
	auto req = findReq(entry);
	if(req == requirements_.end()) return false;

	//this function makes sure the given entry is allocated
	//first of all try to find a free spot in the already existent memories
	if(findMem(*req))
	{
		requirements_.erase(req);
		return true;
	}

	//finding free memory failed, so query the memory type with the most requests and on
	//which the given entry can be allocated and then alloc and bind all reqs for this type
	//XXX: rather use here also queryTypes? could be more efficient seen for the whole amount
	//	allocations that have to be done. At the moment it will chose the type bit that has
	//	the highest amount of stored requirements, but some other type may be better (?) to reduce
	//	the amount of different allocations that have to be done.
	auto type = findBestType(req->memoryTypes);
	allocate(type);

	return true;
}

void DeviceMemoryAllocator::allocate(unsigned int type)
{
	std::vector<Requirement*> reqs;

	for(auto& req : requirements_)
		if(suppportsType(req, type)) reqs.push_back(&req);

	allocate(type, reqs);

	//remove allocated reqs
	//TODO: use efficient algorithm
	for(auto& req : reqs)
	{
		for(auto it = requirements_.begin(); it != requirements_.end();)
		{
			if(req == &(*it)) it = requirements_.erase(it);
			else ++it;
		}
	}
}


void DeviceMemoryAllocator::allocate(unsigned int type, const std::vector<Requirement*>& requirements)
{
	auto gran = device().properties().limits.bufferImageGranularity;

	vk::DeviceSize offset = 0;
	bool applyGran = false;

	std::vector<std::pair<Requirement*, unsigned int>> offsets;
	offsets.reserve(requirements.size());

	//iterate through all reqs and place the ones that may be allocated on the given type
	//there. First all linear resources, then all optimal resources.
	for(auto& req : requirements)
	{
		if(req->type != RequirementType::buffer)
		{
			applyGran = true;
			continue;
		}

		offset = std::ceil(offset / req->alignment) * req->alignment; //apply alignment
		offsets.push_back({req, offset});
		offset += req->size;
	}

	//apply granularity if there were already resources placed and there are ones to be placed
	if(offset > 0 && applyGran) offset = std::ceil(offset / gran) * gran;

	//now all optimal resources
	for(auto& req : requirements)
	{
		offset = std::ceil(offset / req->alignment) * req->alignment;
		offsets.push_back({req, offset});
		offset += req->size;
	}

	//now the needed size is known and the requirements to be allocated have their offsets
	//the last offset value now equals the needed size
	auto mem = std::make_unique<DeviceMemory>(device(), offset, type);

	//bind and alloc all to be allocated resources
	for(auto& res : offsets)
	{
		auto& req = *res.first;
		auto& offset = res.second;
		auto& entry = *req.entry;

		entry.allocation_ = mem->allocSpecified(offset, req.size, toAllocType(req.type));
		entry.memory_ = mem.get();

		auto isBuffer = (res.first->type == RequirementType::buffer);
		if(isBuffer) vk::bindBufferMemory(vkDevice(), res.first->buffer, mem->vkDeviceMemory(), offset);
		else vk::bindImageMemory(vkDevice(), res.first->image, mem->vkDeviceMemory(), offset);
	}

	memories_.push_back(std::move(mem));
}

std::map<unsigned int, std::vector<DeviceMemoryAllocator::Requirement*>>
DeviceMemoryAllocator::queryTypes()
{
	//XXX: probably one of the places where a custom host allocator would really speed things up
	//XXX: probably there is some really easy and trivial algorithm for it... pls find it...

	//this function implements an algorithm to choose the best type for each requirement from
	//its typebits, so that in the end there will be as few allocations as possible needed.

	//vector to return, holds requirements that have a type
	std::map<unsigned int, std::vector<Requirement*>> ret;

	//map holding the current count of the different types
	std::map<unsigned int, std::vector<Requirement*>> occurences;

	//function to count the occurences of the different memory types and store them in
	//the occurences map
	auto countOccurences = [&]()
			{
				occurences.clear();
				for(auto& req : requirements_)
					for(auto i = 0u; i < 32; ++i)
						if(suppportsType(req, i)) occurences[i].push_back(&req);
			};

	//initial occurences count
	countOccurences();

	//while there are requirements left that have not been moved into ret
	while(!occurences.empty())
	{
		//find the least occuring type
		//bestID is after this the memory type with the fewest requirements
		auto best = requirements_.size() + 1; //cant be bigger than that
		std::uint32_t bestID = 0u;
		for(auto& occ : occurences)
		{
			if(occ.second.size() < best)
			{
				best = occ.second.size();
				bestID = occ.first;
			}
		}

		//function to determine if other types besides the given are supported
		auto othersSupported = [](const Requirement& req, unsigned int type)
			{
				for(auto i = 0u; i < 32 && i != type; ++i)
					if(suppportsType(req, i)) return true;
				return false;
			};

		//remove the type with the fewest occurences
		//here check if any of the reqs that can be allocated on bestID can ONLY be allocated
		//on it. Then there has to be an allocatin of type bestID made, otherwise the type
		//wont be considered for allocations any further.
		bool canBeRemoved = true;
		for(auto& req : occurences[bestID])
			if(!othersSupported(*req, bestID)) canBeRemoved = false;

		//if it can be removed there must be at least one allocation for the given type
		//push all reqs that can be allocated on the type to ret and remove them from occurences
		if(canBeRemoved)
		{
			//remove the bestID type bit from all requirement type bits, to reduce the problems complexity
			for(auto& req : occurences[bestID])
				req->memoryTypes &= (~bestID);

			//erase the occurences for bestID
			occurences.erase(occurences.find(bestID));
		}
		else
		{
			//set the requirements typebits to 0, indicating that it has a matching type
			//this makes it no longer have any effect on countOccurences which makes sence,
			//since we "removed" it
			//one could alternatively really remove this from the requirements_ vector, but
			//this would be less efficient
			for(auto& req : occurences[bestID]) req->memoryTypes = 0;

			//insert the requirements that will be allocated on bestID into ret
			ret[bestID] = std::move(occurences[bestID]);

			//explicitly remove the occurences for the chosen bit.
			//the other references to the now removed since type found requirements stay, but
			//sice their memoryTypes member is 0, they will no longer be counted
			occurences.erase(occurences.find(bestID));

			//the occurences have to be recounted since the occurences for type bits other than
			//best bestID (which will have 0 now) do not count longer (they have a type)
			countOccurences();
		}

	}

	return ret;
}

std::vector<DeviceMemory*> DeviceMemoryAllocator::memories() const
{
	std::vector<DeviceMemory*> ret;
	ret.reserve(memories_.size());
	for(auto& mem : memories_) ret.push_back(mem.get());
	return ret;
}

AllocationType DeviceMemoryAllocator::toAllocType(RequirementType type)
{
	switch(type)
	{
		case RequirementType::buffer:
		case RequirementType::linearImage: return AllocationType::linear;
		case RequirementType::optimalImage: return AllocationType::optimal;
		default: return AllocationType::none;
	}
}

unsigned int DeviceMemoryAllocator::findBestType(std::uint32_t typeBits) const
{
	auto best = 0;
	auto bestID = 0;

	for(auto i = 0u; i < 32; ++i)
	{
		//start with one, so even if there are no matching reqs at all, at least a supported
		//type bit is returned.
		auto count = 1;
		if(!suppportsType(typeBits, i)) continue;
		for(auto& req : requirements_) if(suppportsType(req, i)) ++count;

		if(count > best)
		{
			best = count;
			bestID = i;
		}
	}

	return bestID;
}

bool DeviceMemoryAllocator::suppportsType(std::uint32_t typeBits, unsigned int type)
{
	return (typeBits & (1 << type));
}

bool DeviceMemoryAllocator::suppportsType(const Requirement& req, unsigned int type)
{
	return suppportsType(req.memoryTypes, type);
}

}
