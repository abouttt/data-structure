#pragma once

#include <algorithm>
#include <stdexcept>
#include <utility>

#include "MemoryUtil.h"

namespace abouttt
{

template <typename T, typename Compare = std::less<T>, typename Allocator = std::allocator<T>>
class PriorityQueue
{
private:
	using AllocTraits = std::allocator_traits<Allocator>;

public:
	using ValueCompare = Compare;
	using ValueType = T;
	using AllocatorType = Allocator;
	using SizeType = typename AllocTraits::size_type;
	using Reference = T&;
	using ConstReference = const T&;
	using Pointer = typename AllocTraits::pointer;

public:
	PriorityQueue()
		: PriorityQueue(Compare(), Allocator())
	{
	}

	explicit PriorityQueue(const Compare& comp)
		: PriorityQueue(comp, Allocator())
	{
	}

	explicit PriorityQueue(const Allocator& alloc)
		: PriorityQueue(Compare(), alloc)
	{
	}

	explicit PriorityQueue(const Compare& comp, const Allocator& alloc)
		: mComp(comp)
		, mAlloc(alloc)
		, mData(nullptr)
		, mCount(0)
		, mCapacity(0)
	{
	}

	PriorityQueue(SizeType capacity, const Compare& comp = Compare(), const Allocator& alloc = Allocator())
		: mComp(comp)
		, mAlloc(alloc)
		, mData(mem::Allocate(mAlloc, capacity))
		, mCount(0)
		, mCapacity(capacity)
	{
	}

	PriorityQueue(const PriorityQueue& other)
		: PriorityQueue(other, AllocTraits::select_on_container_copy_construction(other.mAlloc))
	{
	}

	PriorityQueue(const PriorityQueue& other, const Allocator& alloc)
		: PriorityQueue(other.mCount, other.mComp, alloc)
	{
		mem::UninitializedCopyN(mAlloc, other.mData, other.mCount, mData);
		mCount = other.mCount;
		std::make_heap(mData, mData + mCount, mComp);
	}

	PriorityQueue(PriorityQueue&& other) noexcept
		: PriorityQueue(std::move(other), std::move(other.mAlloc))
	{
	}

	PriorityQueue(PriorityQueue&& other, const Allocator& alloc)
		: PriorityQueue(std::move(other.mComp), alloc)
	{
		if (mAlloc == other.mAlloc)
		{
			exchangeFrom(other);
		}
		else
		{
			if (other.mCount > 0)
			{
				mData = mem::Allocate(mAlloc, other.mCount);
				mCount = other.mCount;
				mCapacity = other.mCount;
				mem::UninitializedMoveN(mAlloc, other.mData, other.mCount, mData);

				other.Clear();
			}
		}
	}

	~PriorityQueue()
	{
		clearAndDeallocate();
	}

public:
	PriorityQueue& operator=(const PriorityQueue& other)
	{
		if (this == &other)
		{
			return *this;
		}

		Clear();

		mComp = other.mComp;

		if constexpr (AllocTraits::propagate_on_container_copy_assignment::value)
		{
			mAlloc = other.mAlloc;
		}

		if (other.mCount > 0)
		{
			ensureCapacity(other.mCount);
			mem::UninitializedCopyN(mAlloc, other.mData, other.mCount, mData);
			mCount = other.mCount;
			std::make_heap(mData, mData + mCount, mComp);
		}

		return *this;
	}

	PriorityQueue& operator=(PriorityQueue&& other) noexcept(
		AllocTraits::propagate_on_container_move_assignment::value ||
		AllocTraits::is_always_equal::value)
	{
		if (this == &other)
		{
			return *this;
		}

		Clear();

		mComp = std::move(other.mComp);

		if constexpr (AllocTraits::propagate_on_container_move_assignment::value)
		{
			mAlloc = std::move(other.mAlloc);
			exchangeFrom(other);
		}
		else
		{
			if (mAlloc == other.mAlloc)
			{
				exchangeFrom(other);
			}
			else
			{
				if (other.mCount > 0)
				{
					ensureCapacity(other.mCount);
					mem::UninitializedMoveN(mAlloc, other.mData, other.mCount, mData);
					mCount = other.mCount;
				}

				other.Clear();
			}
		}

		return *this;
	}

public:
	SizeType Capacity() const noexcept
	{
		return mCapacity;
	}

	void Clear() noexcept
	{
		mem::DestroyN(mAlloc, mData, mCount);
		mCount = 0;
	}

	SizeType Count() const noexcept
	{
		return mCount;
	}

	void Dequeue()
	{
		checkEmpty();

		if (mCount == 1)
		{
			mem::DestroyAt(mAlloc, mData);
			--mCount;
		}
		else
		{
			mem::DestroyAt(mAlloc, mData);
			mem::ConstructAt(mAlloc, mData, std::move(mData[mCount - 1]));
			mem::DestroyAt(mAlloc, mData + mCount - 1);
			--mCount;
			heapifyDown(0);
		}
	}

	template <typename... Args>
	void Emplace(Args&&... args)
	{
		ensureCapacity(mCount + 1);
		mem::ConstructAt(mAlloc, mData + mCount, std::forward<Args>(args)...);
		++mCount;
		heapifyUp(mCount - 1);
	}

	void Enqueue(const T& value)
	{
		Emplace(value);
	}

	void Enqueue(T&& value)
	{
		Emplace(std::move(value));
	}

	AllocatorType GetAllocator() const
	{
		return mAlloc;
	}

	ValueCompare GetCompare() const
	{
		return mComp;
	}

	bool IsEmpty() const noexcept
	{
		return mCount == 0;
	}

	ConstReference Peek() const
	{
		checkEmpty();
		return mData[0];
	}

	void Reserve(SizeType newCapacity)
	{
		if (newCapacity > mCapacity)
		{
			mData = mem::Reallocate(mAlloc, mData, mCount, mCapacity, newCapacity);
			mCapacity = newCapacity;
		}
	}

	void Shrink()
	{
		if (mCapacity > mCount)
		{
			mData = mem::Reallocate(mAlloc, mData, mCount, mCapacity, mCount);
			mCapacity = mCount;
		}
	}

	void Swap(PriorityQueue& other) noexcept(
		AllocTraits::propagate_on_container_swap::value ||
		AllocTraits::is_always_equal::value)
	{
		if (this == &other)
		{
			return;
		}

		if constexpr (AllocTraits::propagate_on_container_swap::value)
		{
			std::swap(mAlloc, other.mAlloc);
		}

		std::swap(mComp, other.mComp);
		std::swap(mData, other.mData);
		std::swap(mCount, other.mCount);
		std::swap(mCapacity, other.mCapacity);
	}

private:
	void checkEmpty() const
	{
		if (mCount == 0)
		{
			throw std::underflow_error("PriorityQueue is empty");
		}
	}

	void ensureCapacity(SizeType capacity)
	{
		if (capacity > mCapacity)
		{
			SizeType newCapacity = mem::GrowCapacity(mCapacity, capacity);
			Reserve(newCapacity);
		}
	}

	void exchangeFrom(PriorityQueue& other) noexcept
	{
		mData = std::exchange(other.mData, nullptr);
		mCount = std::exchange(other.mCount, 0);
		mCapacity = std::exchange(other.mCapacity, 0);
	}

	void heapifyUp(SizeType index)
	{
		while (index > 0)
		{
			SizeType parent = (index - 1) / 2;
			if (mComp(mData[parent], mData[index]))
			{
				std::swap(mData[parent], mData[index]);
				index = parent;
			}
			else
			{
				break;
			}
		}
	}

	void heapifyDown(SizeType index)
	{
		while (true)
		{
			SizeType leftChild = 2 * index + 1;
			SizeType rightChild = 2 * index + 2;
			SizeType largest = index;

			if (leftChild < mCount && mComp(mData[largest], mData[leftChild]))
			{
				largest = leftChild;
			}

			if (rightChild < mCount && mComp(mData[largest], mData[rightChild]))
			{
				largest = rightChild;
			}

			if (largest != index)
			{
				std::swap(mData[index], mData[largest]);
				index = largest;
			}
			else
			{
				break;
			}
		}
	}

	void clearAndDeallocate()
	{
		if (mData)
		{
			Clear();
			mem::Deallocate(mAlloc, mData, mCapacity);
			mData = nullptr;
			mCapacity = 0;
		}
	}

private:
	ValueCompare mComp;
	AllocatorType mAlloc;
	Pointer mData;
	SizeType mCount;
	SizeType mCapacity;
};

} // namespace abouttt
