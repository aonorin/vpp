// Copyright (c) 2017 nyorain
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt

#pragma once

///TMP utility.
using Expand = int[];
namespace detail {

///Function for matrix and vector size mapping.
template<unsigned int S>
constexpr unsigned int vecAlign()
{
	static_assert(S > 0 && S < 5, "Invalid matrix or vector size");
	return (S == 3) ? 4 : S;
}

///Rounds up the given align to a multiple of 16 if the layout is std140
constexpr unsigned int roundAlign(unsigned int align, bool std140)
{
	return std140 ? std::ceil(align / 16.f) * 16.f : align;
}


///Utility to get the member type of a pointer to member object (Used with decltype).
template<typename T> struct TypeDummy { using type = T; };
template<typename T, typename M> constexpr TypeDummy<M> memPtr(M T::*);

///Utility template class that can be used to write an object of type T to a buffer update.
///Implementations of this class for different ShaderTypes must implement the following
///member functions:
/// - template<typename T, typename O> static void call(O& op, T& obj);
/// - template<typename T> static unsigned int align(bool std140); constexpr
/// - template<typename O> static void size(O& op); [optional; if possible] constexpr
///The first function is used by BufferUpdate/BufferReader to read/write the actual data
///while the second function is used by BufferSizer to determine the size that would be needed
///to fit the given data on the buffer.
template<typename VT, ShaderType V = VT::type>
struct BufferApplier;

///Checks that the given BufferApplier specialization has the size function available.
template<typename T>
struct HasSizeFunction;

///Utility function using the different BufferApplier specializations.
///Prefer to use this function instead of directly calling the BufferApplier Structure.
template<typename O, typename T, typename VT = VulkanType<T>>
void bufferApply(O& op, T&& obj) { BufferApplier<VT>::call(op, obj); }

///Utillity function to using the different BufferApplier::size implementations.
template<typename VT, typename O>
constexpr void bufferSize(O& op)
{
	static_assert(HasSizeFunction<BufferApplier<VT>>::value,
		"You can call this version of BufferSizer::add only with static sized types. "
		"Try to use the neededSize function with actual objects instead. "
		"Read its documentation for more information. ");
	BufferApplier<VT>::size(op);
}

///Utility function to use the different BufferApplier::align implementations.
template<typename VT, typename T = void> constexpr
unsigned int bufferAlign(bool std140) { return BufferApplier<VT>::template align<T>(std140); }

///Utility template class used to write the members of a structure to a buffer update.
///\tparam VT VulkanType of the structure
template<typename VT, typename Seq = std::make_index_sequence<
	std::tuple_size<decltype(VT::members)>::value>> struct MembersOp;

template<typename VT, std::size_t... I>
struct MembersOp<VT, std::index_sequence<I...>>
{
	static constexpr decltype(VT::members) members = VT::members;

	//call
	template<typename O, typename T>
	static void call(O& op, T&& obj)
	{
		auto sa = VT::align ? align(op.std140()) : 0u;
		if(sa) op.align(sa);
		(void)Expand{(bufferApply(op, obj.*(std::get<I>(members))), 0) ...};
		if(sa) op.align(sa);
	}

	//waiting for constexpr lambdas...
	//applySize - utility for size
	template<typename MP, typename O> constexpr static void applySize(const MP& memptr, O& op)
		{ bufferSize<VulkanType<typename decltype(memPtr(memptr))::type>>(op); }

	//applyAlign - utility for align
	template<typename MP> constexpr static unsigned int applyAlign(const MP& memptr, bool std140)
	{
		using V = typename decltype(memPtr(memptr))::type;
		return bufferAlign<VulkanType<V>, V>(std140);
	}

	//size
	template<typename O>
	constexpr static void size(O& op)
	{
		auto sa = VT::align ? align(op.std140()) : 0u;
		if(sa) op.align(sa);
		(void)Expand{(applySize(std::get<I>(members), op), 0) ...};
		if(sa) op.nextOffsetAlign(sa);
	}

	//align
	constexpr static unsigned int align(bool std140)
	{
		auto sa = 0u;
		(void)Expand{(sa = std::max(sa, applyAlign(std::get<I>(members), std140)), 0)...};
		return roundAlign(sa, std140);
	}
};

//otherwise we get an undefined reference
template<typename VT, std::size_t... I> constexpr
decltype(VT::members) MembersOp<VT, std::index_sequence<I...>>::members;

///Default specialization for scalar
template<typename VT>
struct BufferApplier<VT, ShaderType::scalar>
{
	template<typename T, typename O>
	static void call(O& op, T&& obj)
	{
		op.align(align<void>(op.std140()));
		op.operate(&obj, sizeof(obj));
	}

	template<typename O>
	constexpr static void size(O& op)
	{
		op.align(align<void>(op.std140()));
		op.operate(nullptr, 4 + VT::size64 * 4);
	}

	template<typename T>
	static constexpr unsigned int align(bool std140)
	{
		return 4 + VT::size64 * 4;
	}
};

///Default specialization for vec
template<typename VT>
struct BufferApplier<VT, ShaderType::vec>
{
	//call
	template<typename T, typename O>
	static void call(O& op, T&& obj)
	{
		op.align(align<void>(op.std140()));
		op.operate(&obj, sizeof(obj));
	}

	//size
	template<typename O>
	constexpr static void size(O& op)
	{
		op.align(align<void>(op.std140()));
		op.operate(nullptr, VT::dimension * (4 + VT::size64 * 4));
	}

	//align
	template<typename T>
	static constexpr unsigned int align(bool)
	{
		return detail::vecAlign<VT::dimension>() * (4 + VT::size64 * 4);
	}
};

///Specialization for custom shader types.
template<typename VT>
struct BufferApplier<VT, ShaderType::custom>
{
	using Impl = typename VT::impl;

	template<typename O, typename T> static void call(O& op, T&& obj) { Impl::call(op, obj); }
	template<typename O> static constexpr void size(O& op) { Impl::size(op); }
	template<typename T> static constexpr auto align(bool std140)
		{ return Impl::template align<T>(std140); }
};

///Specialization for raw buffers.
///Does not implementation BufferApplier::size since it depends on the given object.
template<typename VT>
struct BufferApplier<VT, ShaderType::buffer>
{
	//call
	template<typename O, typename T,
		typename = decltype(std::declval<T>().size()),
		typename = decltype(std::declval<T>().data())>
	static void call(O& op, T&& obj) { op.operate(obj.data(), obj.size()); }

	//align
	template<typename T>
	static constexpr unsigned int align(bool) { return 0u; }
};

///Specialization for matrix types.
template<typename VT>
struct BufferApplier<VT, ShaderType::mat>
{
	//utility shortcuts
	static constexpr auto major = VT::major;
	static constexpr auto minor = VT::minor;
	static constexpr auto transpose = VT::transpose;
	static constexpr auto csize = 4 + VT::size64 * 4;

	static_assert(major > 1 && minor > 1 && major < 5 && minor < 5, "Invalid matrix dimensions!");

	//call
	template<typename O, typename T>
	static void call(O& op, T&& obj)
	{
		using Value = std::remove_cv_t<std::remove_reference_t<decltype(obj[0][0])>>;
		static_assert(std::is_same<Value, float>::value || std::is_same<Value, double>::value,
			"Only floating point matrix types are supported!");

		auto sa = roundAlign(minor * csize, op.std140());
		op.align(sa);

		//raw write sometimes possible
		if(minor != 3 && sizeof(obj) == major * minor * csize && !transpose)
		{
			op.operate(&obj, sizeof(obj));
		}
		else if(!transpose)
		{
			for(auto mj = 0u; mj < major; ++mj)
			{
				op.align(sa);
				for(auto mn = 0u; mn < minor; ++mn) op.operate(&obj[mj][mn], csize);
			}
		}
		else
		{
			for(auto mn = 0u; mn < minor; ++mn)
			{
				op.align(sa);
				for(auto mj = 0u; mj < major; ++mj) op.operate(&obj[mj][mn], csize);
			}
		}

		//the member following is rounded up to the base alignment
		op.nextOffsetAlign(sa);
	}

	//size
	template<typename O>
	static constexpr void size(O& op)
	{
		auto sa = roundAlign(minor * csize, op.std140());
		op.align(sa);
		op.operate(nullptr, major * roundAlign(minor * csize, op.std140()));
		op.nextOffsetAlign(sa);
	}

	template<typename T>
	constexpr static unsigned int align(bool std140)
	{
		return detail::roundAlign(minor * csize, std140);
	}
};

///Specialization for structures.
template<typename VT>
struct BufferApplier<VT, ShaderType::structure>
{
	template<typename O, typename T>
	static void call(O& op, T&& obj)
	{
		MembersOp<VT>::call(op, obj);
	}

	template<typename O>
	static constexpr void size(O& op)
	{
		MembersOp<VT>::size(op);
	}

	template<typename T>
	static constexpr unsigned int align(bool std140)
	{
		return MembersOp<VT>::align(std140);
	}
};

///Containers specialization.
template<typename VT>
struct BufferApplier<VT, ShaderType::none>
{

	template<
		typename O, typename T,
		typename B = decltype(*std::begin(std::declval<T>())),
		typename E = decltype(*std::end(std::declval<T>())),
		ShaderType V = VulkanType<B>::type>
	static void call(O& op, T&& obj)
	{
		auto sa = roundAlign(bufferAlign<VulkanType<B>, B>(op.std140()), op.std140());
		op.align(sa);

		for(auto& a : obj)
		{
			if(op.std140()) op.align(sa);
			bufferApply(op, a);
		}

		op.nextOffsetAlign(sa);
	}

	template<typename T>
	static constexpr unsigned int align(bool std140)
	{
		using Value = decltype(*std::begin(std::declval<T>()));
		return roundAlign(bufferAlign<VulkanType<Value>, Value>(std140), std140);
	}
};

template<typename T>
struct HasSizeFunction
{
	template <typename C> static char
		test(decltype(std::declval<C>().size(std::declval<BufferSizer&>()))*);
    template <typename C> static long test(...);

    enum { value = sizeof(test<T>(0)) == sizeof(char) };
};

} //namespace detail

template<typename B> template<typename T>
void BufferOperator<B>::addSingle(const T& obj)
{
	auto& bself = *static_cast<B*>(this);
	detail::bufferApply(bself, obj);
}

template<typename B> template<typename... T>
void BufferOperator<B>::add(const T&... objs)
{
	(void)Expand{(addSingle(objs), 0)...};
}

template<typename... T>
constexpr void BufferSizer::add()
{
	(void)Expand{(detail::bufferSize<VulkanType<T>>(*this), 0)...};
}

constexpr void BufferSizer::operate(const void*, std::size_t size)
{
	offset_ = std::max(nextOffset_, offset_) + size;
}

template<typename... T> WorkPtr read(const Buffer& buf, BufferLayout align, T&... args)
{
	///WorkImpl that will store references to the given args and write the buffer data into
	///it once it was retrieved
	struct WorkImpl : public Work<void> {
		WorkImpl(const Buffer& buf, BufferLayout align, T&... args)
			: buffer_(buf), retrieveWork_(retrieve(buf)), align_(align), args_(args...) {}
		~WorkImpl() { finish(); }

		void submit() override { retrieveWork_->submit(); }
		void wait() override { retrieveWork_->wait(); }
		State state() override
		{
			if(!retrieveWork_) return WorkBase::State::finished;
			auto state = retrieveWork_->state();
			return (state == WorkBase::State::finished) ? WorkBase::State::executed : state;
		}
		void finish() override
		{
			if(!retrieveWork_) return;

			retrieveWork_->finish();
			BufferReader reader(buffer_.device(), align_, retrieveWork_->data());
			nytl::apply([&](auto&... args){ reader.add(args...); }, args_); //std::apply c++17
			retrieveWork_.reset();
		}

		const Buffer& buffer_;
		DataWorkPtr retrieveWork_;
		BufferLayout align_;
		std::tuple<T&...> args_;
	};

	return std::make_unique<WorkImpl>(buf, align, args...);
}
