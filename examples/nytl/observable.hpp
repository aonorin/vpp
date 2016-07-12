/*
 * The MIT License (MIT)
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
///\brief Base classes for making types observable or observe the lifetime of objects.

#pragma once

#include <vector>
#include <algorithm>
#include <atomic>
#include <mutex>

namespace nytl
{

class Observable;

///\ingroup utility
///Base class for classes that observe the lifetime of other Observeable objects.
class Observer
{
	friend class Observable;
	virtual void observeableDestruction(Observable&) = 0;
};

///\ingroup utility
///\brief Utility class to make the objects lifetime observable.
///\details Base class that can be derived from if the lifetime of objects of this class should be
///observeable by others. Also see nytl::Observer and nytl::ObservingPtr.
class Observable
{
protected:
	std::vector<Observer*> observer_;
	std::mutex mutex_;

public:
	~Observable()
	{
		//no lock guard needed. Undefined behavior if destructor and accesing run at the same time
		for(auto& obs : observer_)
			obs->observeableDestruction(*this);
	}

	void addObserver(Observer& obs)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		observer_.push_back(&obs);
	}
	bool removeObserver(Observer& obs)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return (observer_.cend() == std::remove(observer_.begin(), observer_.end(), &obs));
	}
	bool moveObserver(Observer& oldone, Observer& newone)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		auto it = std::find(observer_.begin(), observer_.end(), &oldone);
		if(it == observer_.cend()) return 0;
		*it = &newone;
		return 1;
	}
};

///\ingroup utility
///\brief Smart pointer class that observes the lifetime of its object.
///\details Basically a smart pointer that does always know, whether the object it points to is 
//alive or not. Does only work with objects of classes that are derived from nytl::Observeable.
///Semantics are related to std::unique_ptr.
template <typename T>
class ObservingPtr : public Observer
{
private:
	std::atomic<T*> object_ {nullptr};
	virtual void observableDestruction(Observable&) override { object_ = nullptr; }

public:
	ObservingPtr() = default;
	ObservingPtr(T* obj) : object_(obj) { if(object_) object_->addObserver(*this); }
	ObservingPtr(T& obj) : object_(&obj) { object_->addObserver(*this); }
	~ObservingPtr(){ if(object_) object_->removeObserver(*this); }

	ObservingPtr(const ObservingPtr& other) : object_(other.object_)
		{ object_->addObserver(*this); }
	ObservingPtr& operator=(const ObservingPtr& other)
		{ reset(other.object_); return *this; }

	ObservingPtr(ObservingPtr&& other) noexcept : object_(other.object_)
		{ if(object_) object_->moveObserver(other, *this); other.object_ = nullptr; }
	ObservingPtr& operator=(ObservingPtr&& other) noexcept
		{ 
			reset(); 
			object_ = other.object_; 
			if(object_) object_->moveObserver(other, *this); 
			other.object_ = nullptr; 
			return *this; 
		}

	void reset(T* obj = nullptr)
		{ 
			if(obj) obj->addObserver(*this); 
			if(object_) object_->removeObserver(*this); 
			object_ = obj;  
		}
	void reset(T& obj)
		{ 
			obj.addObserver(*this); 
			if(object_) object_->removeObserver(*this);
			object_ = &obj;  
		}

	T* get() { return object_; }

	T& operator*(){ return *object_; }
	const T& operator*() const { return *object_; }

	T* operator->(){ return object_; }
	const T* operator->() const { return object_; }

	operator bool() const { return (object_ != nullptr); }
	void swap(ObservingPtr& other) noexcept 
	{
		if(object_) object_->moveObserver(*this, other);
		if(other.object_) other.object_->moveObserver(other, *this);
		std::swap(object_, other.object_);
	}
};

}
