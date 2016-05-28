// Copyright © 2016 nyorain
//
// Permission is hereby granted, free of charge, to any person
// obtaining a copy of this software and associated documentation
// files (the “Software”), to deal in the Software without
// restriction, including without limitation the rights to use,
// copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following
// conditions:
//
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
// OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.

#pragma once

#include "enums.hpp"
#include <stdexcept>
#include <string>
#include <iostream>

namespace vk
{

///Default exception class that will be thrown when a thow-checked vulkan function fails.
///This exception will carry the return vulkan result code.
class VulkanError : public std::runtime_error
{
public:
	VulkanError(Result err, const std::string& msg = "") : std::runtime_error(msg), error(err) {}
	const vk::Result error;
};

namespace call
{

inline std::string resultErrorMsg(Result result)
{
    switch(result)
    {
	    case Result::success: return "Success";
	    case Result::notReady: return "NotReady";
	    case Result::timeout: return "Timeout";
	    case Result::eventSet: return "EventSet";
	    case Result::eventReset: return "EventReset";
	    case Result::incomplete: return "Incomplete";
	    case Result::errorOutOfHostMemory: return "ErrorOutOfHostMemory";
	    case Result::errorOutOfDeviceMemory: return "ErrorOutOfDeviceMemory";
	    case Result::errorInitializationFailed: return "ErrorInitializationFailed";
	    case Result::errorDeviceLost: return "ErrorDeviceLost";
	    case Result::errorMemoryMapFailed: return "ErrorMemoryMapFailed";
	    case Result::errorLayerNotPresent: return "ErrorLayerNotPresent";
	    case Result::errorExtensionNotPresent: return "ErrorExtensionNotPresent";
	    case Result::errorFeatureNotPresent: return "ErrorFeatureNotPresent";
	    case Result::errorIncompatibleDriver: return "ErrorIncompatibleDriver";
	    case Result::errorTooManyObjects: return "ErrorTooManyObjects";
	    case Result::errorFormatNotSupported: return "ErrorFormatNotSupported";
	    case Result::errorSurfaceLostKHR: return "ErrorSurfaceLostKHR";
	    case Result::errorNativeWindowInUseKHR: return "ErrorNativeWindowInUseKHR";
	    case Result::suboptimalKHR: return "SuboptimalKHR";
	    case Result::errorOutOfDateKHR: return "ErrorOutOfDateKHR";
	    case Result::errorIncompatibleDisplayKHR: return "ErrorIncompatibleDisplayKHR";
	    case Result::errorValidationFailedEXT: return "ErrorValidationFailedEXT";
	    default: return "unknown";
    }
}

//throw
inline vk::Result checkResultThrow(vk::Result result, const char* function, const char* called)
{
	if(static_cast<std::uint64_t>(result) >= 0)
		return result;

	auto msg = resultErrorMsg(result);
	auto ecode = static_cast<unsigned int>(result);
	const std::string err = "Vulkan Error Code " + std::to_string(ecode) + ": " + msg +
		"\t\nin function " + function + " ,calling " + called;

    std::cerr << "vk::call::checkResultThrow will throw " << err << std::endl;
	throw VulkanError(result, err);
}

//warn
inline vk::Result checkResultWarn(vk::Result result, const char* function, const char* called)
{
	if(static_cast<std::uint64_t>(result) >= 0)
		return result;

	auto msg = resultErrorMsg(result);
	auto ecode = static_cast<unsigned int>(result);
	const std::string err = "Vulkan Error Code " + std::to_string(ecode) + ": " + msg +
		"\n\tin function " + function + " ,calling " + called;

	std::cerr << "vk::call::checkResultWarn: " << err << std::endl;
}

} //namespace call
} //namespace vk

//macro for querying the current function name on all platforms
#if defined(__GNUC__) || defined(__clang__)
 #define VPP_FUNC_NAME __PRETTY_FUNCTION__
#elif defined(_MSC_VER)
 #define VPP_FUNC_NAME __FUNCTION__
#else
 #define VPP_FUNC_NAME __func__
#endif

//call warn and throw macros
#define VPP_CALL_W(x) ::vk::call::checkResultWarn(static_cast<vk::Result>(x), VPP_FUNC_NAME, #x)
#define VPP_CALL_T(x) ::vk::call::checkResultThrow(static_cast<vk::Result>(x), VPP_FUNC_NAME, #x)

//default selection
#if !defined(VPP_CALL_THROW) && !defined(VPP_CALL_WARN) && !defined(VPP_CALL_NOCHECK)
 #ifdef NDEBUG
  #define VPP_CALL_WARN
 #else
  #define VPP_CALL_THROW
 #endif
#endif

//default call macro based on given option macros (throw/warn/nocheck)
#ifdef VPP_CALL_THROW
 #define VPP_CALL(x) VPP_CALL_T(x)
#elif defined(VPP_CALL_WARN)
 #define VPP_CALL(x) VPP_CALL_W(x)
#else
 #define VPP_CALL(x) x
#endif