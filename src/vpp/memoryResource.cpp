#include <vpp/memoryResource.hpp>
#include <vpp/vk.hpp>
#include <stdexcept>

namespace vpp
{

void MemoryResource::assureMemory() const
{
	if(!memoryEntry().allocated()) memoryEntry().allocate();
}

MemoryMapView MemoryResource::memoryMap() const
{
	if(!mappable()) throw std::logic_error("vpp::MemoryResource::memoryMap: not mappable");

	assureMemory();
	return memoryEntry().map();
}

bool MemoryResource::mappable() const
{
	if(!memoryEntry().memory()) return false;
	return memoryEntry().memory()->properties() & vk::MemoryPropertyBits::hostVisible;
}

}
