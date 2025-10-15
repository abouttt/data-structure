#pragma once

#include <algorithm>
#include <functional>
#include <initializer_list>
#include <memory>
#include <new>
#include <stdexcept>
#include <utility>

namespace abouttt
{

template <typename T, typename Compare = std::less<T>>
class PriorityQueue
{
public:
	PriorityQueue() noexcept
		: PriorityQueue(0, Compare())
	{
	}

	explicit PriorityQueue(const Compare& comp) noexcept
		: PriorityQueue(0, comp)
	{
	}

	explicit PriorityQueue(size_t capacity)
		: PriorityQueue(capacity, Compare())
	{
	}

	PriorityQueue(size_t capacity, const Compare& comp)
		: mCompare(comp)
		, mData(capacity > 0 ? static_cast<T*>(::operator new(sizeof(T) * capacity)) : nullptr)
		, mCount(0)
		, mCapacity(capacity)
	{
	}

	PriorityQueue(std::initializer_list<T> ilist)
		: PriorityQueue(ilist, Compare())
	{
	}

	PriorityQueue(std::initializer_list<T> ilist, const Compare& comp)
		: PriorityQueue(ilist.size(), comp)
	{
		std::uninitialized_copy(ilist.begin(), ilist.end(), mData);
		mCount = ilist.size();
		makeHeap();
	}

	PriorityQueue(const PriorityQueue& other)
		: PriorityQueue(other.mCount, other.mCompare)
	{
		std::uninitialized_copy(other.mData, other.mData + other.mCount, mData);
		mCount = other.mCount;
		makeHeap();
	}

	PriorityQueue(PriorityQueue&& other) noexcept
		: mCompare(std::move(other.mCompare))
		, mData(std::exchange(other.mData, nullptr))
		, mCount(std::exchange(other.mCount, 0))
		, mCapacity(std::exchange(other.mCapacity, 0))
	{
	}

	~PriorityQueue()
	{
		cleanup();
	}

public:
	PriorityQueue& operator=(const PriorityQueue& other)
	{
		if (this != &other)
		{
			PriorityQueue temp(other);
			Swap(temp);
		}
		return *this;
	}

	PriorityQueue& operator=(PriorityQueue&& other) noexcept
	{
		if (this != &other)
		{
			cleanup();
			mCompare = std::move(other.mCompare);
			mData = std::exchange(other.mData, nullptr);
			mCount = std::exchange(other.mCount, 0);
			mCapacity = std::exchange(other.mCapacity, 0);
		}
		return *this;
	}

	PriorityQueue& operator=(std::initializer_list<T> ilist)
	{
		PriorityQueue temp(ilist);
		Swap(temp);
		return *this;
	}

public:
	void Clear() noexcept
	{
		std::destroy_n(mData, mCount);
		mCount = 0;
	}

	size_t Count() const noexcept
	{
		return mCount;
	}

	void Dequeue()
	{
		checkEmpty();
		std::swap(mData[0], mData[mCount - 1]);
		std::destroy_at(mData + mCount - 1);
		--mCount;
		if (mCount > 0)
		{
			heapifyDown(0);
		}
	}

	template <typename... Args>
	void Emplace(Args&&... args)
	{
		ensureCapacity(mCount + 1);
		std::construct_at(mData + mCount, std::forward<Args>(args)...);
		heapifyUp(mCount);
		++mCount;
	}

	void Enqueue(const T& value)
	{
		Emplace(value);
	}

	void Enqueue(T&& value)
	{
		Emplace(std::move(value));
	}

	bool IsEmpty() const noexcept
	{
		return mCount == 0;
	}

	const T& Peek() const
	{
		checkEmpty();
		return mData[0];
	}

	void Reserve(size_t newCapacity)
	{
		if (newCapacity > mCapacity)
		{
			reallocate(newCapacity);
		}
	}

	void Shrink()
	{
		if (mCapacity > mCount)
		{
			reallocate(mCount);
		}
	}

	void Swap(PriorityQueue& other) noexcept
	{
		std::swap(mCompare, other.mCompare);
		std::swap(mData, other.mData);
		std::swap(mCount, other.mCount);
		std::swap(mCapacity, other.mCapacity);
	}

private:
	void checkEmpty() const
	{
		if (mCount == 0)
		{
			throw std::out_of_range("Priority queue is empty");
		}
	}

	void ensureCapacity(size_t minCapacity)
	{
		if (minCapacity > mCapacity)
		{
			size_t grow = mCapacity + (mCapacity >> 1); // Grow by 1.5x
			size_t newCapacity = std::max(minCapacity, mCapacity == 0 ? 8 : grow);
			reallocate(newCapacity);
		}
	}

	void heapifyUp(size_t index)
	{
		T value = std::move(mData[index]);

		while (index > 0)
		{
			size_t parent = (index - 1) / 2;
			if (!mCompare(mData[parent], value))
			{
				break;
			}
			mData[index] = std::move(mData[parent]);
			index = parent;
		}

		mData[index] = std::move(value);
	}

	void heapifyDown(size_t index)
	{
		T temp = std::move(mData[index]);
		size_t child;

		while ((child = index * 2 + 1) < mCount)
		{
			if (child + 1 < mCount && mCompare(mData[child], mData[child + 1]))
			{
				++child;
			}

			if (!mCompare(temp, mData[child]))
			{
				break;
			}

			mData[index] = std::move(mData[child]);
			index = child;
		}

		mData[index] = std::move(temp);
	}

	void makeHeap()
	{
		if (mCount <= 1)
		{
			return;
		}

		for (size_t i = mCount / 2; i-- > 0;)
		{
			heapifyDown(i);
		}
	}

	void reallocate(size_t newCapacity)
	{
		if (newCapacity == mCapacity)
		{
			return;
		}

		if (newCapacity == 0)
		{
			cleanup();
			return;
		}

		T* newData = static_cast<T*>(::operator new(sizeof(T) * newCapacity));
		size_t newCount = std::min(mCount, newCapacity);

		if (mData)
		{
			std::uninitialized_move_n(mData, newCount, newData);
			std::destroy_n(mData, mCount);
			::operator delete(mData);
		}

		mData = newData;
		mCount = newCount;
		mCapacity = newCapacity;
	}

	void cleanup() noexcept
	{
		if (mData)
		{
			std::destroy_n(mData, mCount);
			::operator delete(mData);
			mData = nullptr;
			mCount = 0;
			mCapacity = 0;
		}
	}

private:
	Compare mCompare;
	T* mData;
	size_t mCount;
	size_t mCapacity;
};

} // namespace abouttt
