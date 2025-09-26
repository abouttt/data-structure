#pragma once

#include <algorithm>
#include <compare>
#include <initializer_list>
#include <stdexcept>

#include "MemoryUtil.h"

namespace abouttt
{

template <typename T, typename Allocator = std::allocator<T>>
class Array
{
private:
	using AllocTraits = std::allocator_traits<Allocator>;

public:
	using ValueType = T;
	using AllocatorType = Allocator;
	using SizeType = typename AllocTraits::size_type;
	using DifferenceType = typename AllocTraits::difference_type;
	using Reference = T&;
	using ConstReference = const T&;
	using Pointer = typename AllocTraits::pointer;
	using ConstPointer = typename AllocTraits::const_pointer;

public:
	static constexpr SizeType NPOS = static_cast<SizeType>(-1);

public:
	Array() noexcept(noexcept(Allocator()))
		: Array(Allocator())
	{
	}

	explicit Array(const Allocator& alloc) noexcept
		: mAlloc(alloc)
		, mData(nullptr)
		, mCount(0)
		, mCapacity(0)
	{
	}

	explicit Array(SizeType capacity, const Allocator& alloc = Allocator())
		: mAlloc(alloc)
		, mData(mem::Allocate(mAlloc, capacity))
		, mCount(0)
		, mCapacity(capacity)
	{
	}

	Array(SizeType count, const T& value, const Allocator& alloc = Allocator())
		: Array(count, alloc)
	{
		mem::UninitializedFillN(mAlloc, mData, count, value);
		mCount = count;
	}

	Array(const T* ptr, SizeType count, const Allocator& alloc = Allocator())
		: Array(count, alloc)
	{
		mem::UninitializedCopyN(mAlloc, ptr, count, mData);
		mCount = count;
	}

	Array(std::initializer_list<T> ilist, const Allocator& alloc = Allocator())
		: Array(ilist.size(), alloc)
	{
		mem::UninitializedCopy(mAlloc, ilist.begin(), ilist.end(), mData);
		mCount = ilist.size();
	}

	Array(const Array& other)
		: Array(other, AllocTraits::select_on_container_copy_construction(other.mAlloc))
	{
	}

	Array(const Array& other, const Allocator& alloc)
		: Array(other.mCount, alloc)
	{
		mem::UninitializedCopyN(mAlloc, other.mData, other.mCount, mData);
		mCount = other.mCount;
	}

	Array(Array&& other) noexcept
		: Array(std::move(other), std::move(other.mAlloc))
	{
	}

	Array(Array&& other, const Allocator& alloc)
		: Array(alloc)
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
			}
		}
	}

	~Array()
	{
		clearAndDeallocate();
	}

public:
	Array& operator=(const Array& other)
	{
		if (this == &other)
		{
			return *this;
		}

		if constexpr (AllocTraits::propagate_on_container_copy_assignment::value)
		{
			if (mAlloc != other.mAlloc)
			{
				clearAndDeallocate();
				mAlloc = other.mAlloc;
			}
		}

		assignImpl(other.mData, other.mCount);

		return *this;
	}

	Array& operator=(Array&& other) noexcept(
		AllocTraits::propagate_on_container_move_assignment::value ||
		AllocTraits::is_always_equal::value)
	{
		if (this == &other)
		{
			return *this;
		}

		if constexpr (AllocTraits::propagate_on_container_move_assignment::value)
		{
			clearAndDeallocate();
			mAlloc = std::move(other.mAlloc);
			exchangeFrom(other);
		}
		else
		{
			if (mAlloc == other.mAlloc)
			{
				clearAndDeallocate();
				exchangeFrom(other);
			}
			else
			{
				moveAssignImpl(other);
			}
		}

		return *this;
	}

	Array& operator=(std::initializer_list<T> ilist)
	{
		assignImpl(ilist.begin(), ilist.size());
		return *this;
	}

	Reference operator[](SizeType index)
	{
		checkRange(index);
		return mData[index];
	}

	ConstReference operator[](SizeType index) const
	{
		checkRange(index);
		return mData[index];
	}

	bool operator==(const Array& other) const
	{
		return mCount == other.mCount && std::equal(mData, mData + mCount, other.mData);
	}

	auto operator<=>(const Array& other) const
	{
		return std::lexicographical_compare_three_way(
			mData, mData + mCount,
			other.mData, other.mData + other.mCount
		);
	}

public:
	void Add(const T& value)
	{
		Emplace(value);
	}

	void Add(T&& value)
	{
		Emplace(std::move(value));
	}

	void Append(const Array& source)
	{
		Insert(mCount, source.mData, source.mCount);
	}

	void Append(std::initializer_list<T> ilist)
	{
		Insert(mCount, ilist.begin(), ilist.size());
	}

	void Append(const T* ptr, SizeType count)
	{
		Insert(mCount, ptr, count);
	}

	SizeType Capacity() const noexcept
	{
		return mCapacity;
	}

	void Clear() noexcept
	{
		mem::DestroyN(mAlloc, mData, mCount);
		mCount = 0;
	}

	bool Contains(const T& value) const
	{
		return Find(value) != NPOS;
	}

	template <typename Predicate>
	bool ContainsIf(Predicate pred) const
	{
		return FindIf(pred) != NPOS;
	}

	SizeType Count() const noexcept
	{
		return mCount;
	}

	Pointer Data() noexcept
	{
		return mData;
	}

	ConstPointer Data() const noexcept
	{
		return mData;
	}

	template <typename... Args>
	Reference Emplace(Args&&... args)
	{
		ensureCapacity(mCount + 1);
		mem::ConstructAt(mAlloc, mData + mCount, std::forward<Args>(args)...);
		return mData[mCount++];
	}

	template <typename... Args>
	SizeType EmplaceAt(SizeType index, Args&&... args)
	{
		checkRange(index, true);

		if (index == mCount)
		{
			Emplace(std::forward<Args>(args)...);
			return index;
		}

		ensureCapacity(mCount + 1);

		mem::ConstructAt(mAlloc, mData + mCount, std::move(mData[mCount - 1]));
		mem::MoveBackward(mData + index, mData + mCount - 1, mData + mCount);
		mem::DestroyAt(mAlloc, mData + index);
		mem::ConstructAt(mAlloc, mData + index, std::forward<Args>(args)...);
		++mCount;

		return index;
	}

	SizeType Find(const T& value) const
	{
		for (SizeType i = 0; i < mCount; ++i)
		{
			if (mData[i] == value)
			{
				return i;
			}
		}

		return NPOS;
	}

	template <typename Predicate>
	SizeType FindIf(Predicate pred) const
	{
		for (SizeType i = 0; i < mCount; ++i)
		{
			if (pred(mData[i]))
			{
				return i;
			}
		}

		return NPOS;
	}

	SizeType FindLast(const T& value) const
	{
		for (SizeType i = mCount; i > 0; --i)
		{
			if (mData[i - 1] == value)
			{
				return i - 1;
			}
		}

		return NPOS;
	}

	template <typename Predicate>
	SizeType FindLastIf(Predicate pred) const
	{
		for (SizeType i = mCount; i > 0; --i)
		{
			if (pred(mData[i - 1]))
			{
				return i - 1;
			}
		}

		return NPOS;
	}

	AllocatorType GetAllocator() const
	{
		return mAlloc;
	}

	SizeType Insert(SizeType index, const T& value)
	{
		return EmplaceAt(index, value);
	}

	SizeType Insert(SizeType index, T&& value)
	{
		return EmplaceAt(index, std::move(value));
	}

	SizeType Insert(SizeType index, const Array& source)
	{
		return Insert(index, source.mData, source.mCount);
	}

	SizeType Insert(SizeType index, std::initializer_list<T> ilist)
	{
		return Insert(index, ilist.begin(), ilist.size());
	}

	SizeType Insert(SizeType index, const T* ptr, SizeType count)
	{
		checkRange(index, true);

		if (!ptr || count == 0)
		{
			return NPOS;
		}

		ensureCapacity(mCount + count);

		SizeType elementsToMove = mCount - index;
		if (elementsToMove > 0)
		{
			if (elementsToMove > count)
			{
				mem::UninitializedMoveN(mAlloc, mData + mCount - count, count, mData + mCount);
				mem::MoveBackward(mData + index, mData + mCount - count, mData + mCount);
				mem::CopyN(ptr, count, mData + index);
			}
			else
			{
				mem::UninitializedMoveN(mAlloc, mData + index, elementsToMove, mData + index + count);
				mem::CopyN(ptr, elementsToMove, mData + index);
				mem::UninitializedCopyN(mAlloc, ptr + elementsToMove, count - elementsToMove, mData + mCount);
			}
		}
		else
		{
			mem::UninitializedCopyN(mAlloc, ptr, count, mData + mCount);
		}

		mCount += count;

		return index;
	}

	bool IsEmpty() const noexcept
	{
		return mCount == 0;
	}

	bool Remove(const T& value)
	{
		SizeType index = Find(value);
		if (index != NPOS)
		{
			RemoveAt(index);
			return true;
		}

		return false;
	}

	template <typename Predicate>
	SizeType RemoveAll(Predicate pred)
	{
		if (mCount == 0)
		{
			return 0;
		}

		Pointer newEnd = std::remove_if(mData, mData + mCount, pred);
		if (newEnd == mData + mCount)
		{
			return 0;
		}

		SizeType removedCount = (mData + mCount) - newEnd;
		mem::Destroy(mAlloc, newEnd, mData + mCount);
		mCount -= removedCount;

		return removedCount;
	}

	void RemoveAt(SizeType index)
	{
		checkRange(index);
		mem::Move(mData + index + 1, mData + mCount, mData + index);
		mem::DestroyAt(mAlloc, mData + mCount - 1);
		--mCount;
	}

	void Reserve(SizeType newCapacity)
	{
		if (newCapacity > mCapacity)
		{
			mData = mem::Reallocate(mAlloc, mData, mCount, mCapacity, newCapacity);
			mCapacity = newCapacity;
		}
	}

	void Resize(SizeType newSize)
	{
		Resize(newSize, T());
	}

	void Resize(SizeType newSize, const T& value)
	{
		if (newSize > mCount)
		{
			ensureCapacity(newSize);
			mem::UninitializedFillN(mAlloc, mData + mCount, newSize - mCount, value);
		}
		else if (newSize < mCount)
		{
			mem::DestroyN(mAlloc, mData + newSize, mCount - newSize);
		}

		mCount = newSize;
	}

	void Shrink()
	{
		if (mCapacity > mCount)
		{
			mData = mem::Reallocate(mAlloc, mData, mCount, mCapacity, mCount);
			mCapacity = mCount;
		}
	}

	void Sort()
	{
		Sort(std::less<T>());
	}

	template <typename Compare>
	void Sort(Compare comp)
	{
		std::sort(mData, mData + mCount, comp);
	}

	void Swap(Array& other) noexcept(
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

private:
	void checkRange(SizeType index, bool bAllowEnd = false) const
	{
		if ((bAllowEnd && index > mCount) || (!bAllowEnd && index >= mCount))
		{
			throw std::out_of_range("Array index out of range");
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

	void exchangeFrom(Array& other) noexcept
	{
		mData = std::exchange(other.mData, nullptr);
		mCount = std::exchange(other.mCount, 0);
		mCapacity = std::exchange(other.mCapacity, 0);
	}

	void assignImpl(const T* newPtr, SizeType newCount)
	{
		if (newCount > mCapacity)
		{
			clearAndDeallocate();

			SizeType newCapacity = mem::GrowCapacity(mCapacity, newCount);
			mData = mem::Allocate(mAlloc, newCapacity);
			mCapacity = newCapacity;
			mem::UninitializedCopyN(mAlloc, newPtr, newCount, mData);
		}
		else if (newCount > mCount)
		{
			mem::CopyN(newPtr, mCount, mData);
			mem::UninitializedCopyN(mAlloc, newPtr + mCount, newCount - mCount, mData + mCount);
		}
		else
		{
			mem::CopyN(newPtr, newCount, mData);
			mem::Destroy(mAlloc, mData + newCount, mData + mCount);
		}

		mCount = newCount;
	}

	void moveAssignImpl(Array& other)
	{
		if (other.mCount == 0)
		{
			Clear();
			return;
		}

		if (mCapacity >= other.mCount)
		{
			Clear();
			mem::UninitializedMoveN(mAlloc, other.mData, other.mCount, mData);
		}
		else
		{
			clearAndDeallocate();

			mData = mem::Allocate(mAlloc, other.mCount);
			mCapacity = other.mCount;
			mem::UninitializedMoveN(mAlloc, other.mData, other.mCount, mData);
		}

		mCount = other.mCount;
		other.Clear();
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
