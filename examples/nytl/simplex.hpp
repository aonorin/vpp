/* The MIT License (MIT)
 *
 * Copyright (c) 2016 Jan Kelling
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

///\file
///\brief Includes the unspecialized Simplex template class with operations and typedefs.

#pragma once

#include <nytl/vec.hpp>
#include <nytl/mat.hpp>
#include <nytl/scalar.hpp>
#include <nytl/linearSolver.hpp>
#include <nytl/bits/tmpUtil.inl>

#include <vector>
#include <type_traits>
#include <utility>
#include <cmath>
#include <stdexcept>
#include <string>
#include <cassert>

namespace nytl
{

///Short enable-if typedef
template<std::size_t D, std::size_t A> using DimMatch = typename std::enable_if<D >= A>::type;

///\ingroup math
///\brief Templated abstraction of the Simplex concept.
///\details The Simplex<D, P, A> template class defines an unique area with \c A dimensions
///of \c P precision in an \c D dimensional space.
///So e.g. Simplex<3, float, 2> describes a Triangle in a 3-dimensional space.
///This template class does only works if D >= A, since the dimension of the area
///can not be higher than the dimension of the space that contains this area.
///The area is called unique, since it does have a variable number of points defining it; always
///enough to describe exactly one, unambigous area with the given dimension and precision in
///the given space.
template<std::size_t D, typename P = float, std::size_t A = D>
class Simplex : public DeriveDummy<DimMatch<D, A>>
{
public:
	static constexpr std::size_t dim = D;
	static constexpr std::size_t simplexDim = A;

	using Precision = P;
    using VecType = Vec<D, P>;
	using Size = std::size_t;

	//stl
    using value_type = Precision;
	using size_type = Size;

public:
	Vec<A + 1, VecType> points_ {};

public:
	template<typename... Args, typename = typename
		std::enable_if_t<
			std::is_convertible<
				std::tuple<Args...>,
				TypeTuple<VecType, A + 1>
			>::value
		>>		
	Simplex(Args&&... args) noexcept : points_{std::forward<Args>(args)...} {}
	Simplex() noexcept = default;

	///Returns the size of the area (e.g. for a 3-dimensional Simplex this would be the volume)
	double size() const;

	///Returns the center point of the area.
	VecType center() const;

	///Returns whether the defined Simplex is valid (i.e. size > 0).
	bool valid() const;

	///Converts the object into a Vec of points. 
	///Can be used to acces (read/change/manipulate) the points.
	Vec<A + 1, VecType>& points(){ return points_; }

	///Converts the object into a const Vec of poitns. 
	///Can be used to const_iterate/read the points.
	const Vec<A + 1, VecType>& points() const { return points_; }

	///Converts the object to a Simplex with a different dimension or precision.
	///Note that the area dimension A cannot be changed, only the space dimension D.
	///Works only if the new D is still greater equal A.
	template<std::size_t OD, typename OP> 
		operator Simplex<OD, OP, A>() const;
};

///\ingroup math
///Describes a RectRegion of multiple Simplexes. 
template<std::size_t D, typename P = float, std::size_t A = D>
class SimplexRegion : public DeriveDummy<DimMatch<D, A>>
{
public:
	static constexpr std::size_t dim = D;
	static constexpr std::size_t simplexDim = A;

	using SimplexType = Simplex<D, P, A>;
	using SimplexRegionType = SimplexRegion<D, P, A>;
	using VectorType = std::vector<SimplexType>;
	using Size = typename VectorType::size_type;

	//stl
    using value_type = SimplexType;
	using size_type = Size;

public:
	VectorType simplices_;

public:
	///Adds a Simplex to this RectRegion. Effectively only adds the part of the Simplex that
	///is not already part of the RectRegion.
	void add(const SimplexType& simplex);

	///Makes this RectRegion object the union of itself and the argument-given RectRegion.
	void add(const SimplexRegionType& simplexRegion);

	///Adds a Simplex without checking for intersection
	void addNoCheck(const SimplexType& simplex) { simplices_.push_back(simplex); }

	///Adds a SimplexRegion without checking for intersection
	void addNoCheck(const SimplexRegionType& simplexRegion) 
		{ simplices_.insert(simplices_.cend(), simplexRegion.cbegin(), simplexRegion.cend()); }

	///Subtracts a Simplex from this SimplexRegion. 
	///Effectively checks every Simplex of this SimplexRegio for intersection and resizes 
	///it if needed.
	void subtract(const SimplexType& simplex);

	///Subtracts the given simplexRegion from this object.
	void subtract(const SimplexRegionType& simplexregion);

	///Returns the total size of the region.
	double size() const;

	///Returns the number of simplexes this SimplexRegion contains.
	Size count() const { return simplices().size(); }

	///Returns a Vector with the given Simplexes.
	const VectorType& simplices() const { return simplices_; }

	///Returns a Vector with the given Simplexes.
	VectorType& simplices() { return simplices_; }

	///Converts this object to a SimplexRegion object of different precision and/or space dimension.
	template<std::size_t OD, typename OP> 
		operator SimplexRegion<OD, OP, A>() const;
};

//To get the additional features for each specialization, include the corresponding headers:
//#include <nytl/line.hpp>
template<std::size_t D, typename P = float> using Line = Simplex<D, P, 1>;
//#include <nytl/triangle.hpp>
template<std::size_t D, typename P = float> using Triangle = Simplex<D, P, 2>;
//#include <nytl/tetrahedron.hpp>
template<std::size_t D, typename P = float> using Tetrahedron = Simplex<D, P, 3>;

//operators/utility
#include <nytl/bits/simplexRegion.inl>
#include <nytl/bits/simplex.inl>

}

