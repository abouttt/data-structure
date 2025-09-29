#pragma once

#include <stdexcept>
#include <utility>

#include "MemoryUtil.h"

namespace abouttt
{

template <typename T, typename Allocator = std::allocator<T>>
class Stack
{
private:
	using AllocTraits = std::allocator_traits<Allocator>;

public:
	using ValueType = T;
	using AllocatorType = Allocator;
	using SizeType = typename AllocTraits::size_type;
	using Reference = T&;
	using ConstReference = const T&;
	using Pointer = typename AllocTraits::pointer;

public:
	Stack() noexcept(noexcept(Allocator()))
		: Stack(Allocator())
	{
	}

	explicit Stack(const Allocator& alloc) noexcept
		: mAlloc(alloc)
		, mData(nullptr)
		, mCount(0)
		, mCapacity(0)
	{
	}

	explicit Stack(SizeType capacity, const Allocator& alloc = Allocator())
		: mAlloc(alloc)
		, mData(mem::Allocate(mAlloc, capacity))
		, mCount(0)
		, mCapacity(capacity)
	{
	}

	Stack(const Stack& other)
		: Stack(other, AllocTraits::select_on_container_copy_construction(other.mAlloc))
	{
	}

	Stack(const Stack& other, const Allocator& alloc)
		: Stack(other.mCount, alloc)
	{
		mem::UninitializedCopyN(mAlloc, other.mData, other.mCount, mData);
		mCount = other.mCount;
	}

	Stack(Stack&& other) noexcept
		: Stack(std::move(other), std::move(other.mAlloc))
	{
	}

	Stack(Stack&& other, const Allocator& alloc)
		: Stack(alloc)
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

	~Stack()
	{
		clearAndDeallocate();
	}

public:
	Stack& operator=(const Stack& other)
	{
		if (this == &other)
		{
			return *this;
		}

		Clear();

		if constexpr (AllocTraits::propagate_on_container_copy_assignment::value)
		{
			mAlloc = other.mAlloc;
		}

		if (other.mCount > 0)
		{
			ensureCapacity(other.mCount);
			mem::UninitializedCopyN(mAlloc, other.mData, other.mCount, mData);
			mCount = other.mCount;
		}

		return *this;
	}

	Stack& operator=(Stack&& other) noexcept(
		AllocTraits::propagate_on_container_move_assignment::value ||
		AllocTraits::is_always_equal::value)
	{
		if (this == &other)
		{
			return *this;
		}

		Clear();

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

	template <typename... Args>
	Reference Emplace(Args&&... args)
	{
		ensureCapacity(mCount + 1);
		mem::ConstructAt(mAlloc, mData + mCount, std::forward<Args>(args)...);
		return mData[mCount++];
	}

	AllocatorType GetAllocator() const
	{
		return mAlloc;
	}

	bool IsEmpty() const noexcept
	{
		return mCount == 0;
	}

	void Pop()
	{
		checkEmpty();
		--mCount;
		mem::DestroyAt(mAlloc, mData + mCount);
	}

	void Push(const T& value)
	{
		Emplace(value);
	}

	void Push(T&& value)
	{
		Emplace(std::move(value));
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

	void Swap(Stack& other) noexcept(
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

		std::swap(mData, other.mData);
		std::swap(mCount, other.mCount);
		std::swap(mCapacity, other.mCapacity);
	}

	Reference Top()
	{
		checkEmpty();
		return mData[mCount - 1];
	}

	ConstReference Top() const
	{
		checkEmpty();
		return mData[mCount - 1];
	}

private:
	void checkEmpty()
	{
		if (mCount == 0)
		{
			throw std::underflow_error("Stack is empty");
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

	void exchangeFrom(Stack& other) noexcept
	{
		mData = std::exchange(other.mData, nullptr);
		mCount = std::exchange(other.mCount, 0);
		mCapacity = std::exchange(other.mCapacity, 0);
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
	AllocatorType mAlloc;
	Pointer mData;
	SizeType mCount;
	SizeType mCapacity;
};

} // namespace abouttt
