#pragma once

#include <algorithm>
#include <compare>
#include <initializer_list>
#include <memory>
#include <new>
#include <stdexcept>
#include <utility>

namespace abouttt
{

template <typename T>
class Stack
{
public:
	Stack() noexcept
		: Stack(0)
	{
	}

	explicit Stack(size_t capacity)
		: mData(capacity > 0 ? static_cast<T*>(::operator new(sizeof(T) * capacity)) : nullptr)
		, mCount(0)
		, mCapacity(capacity)
	{
	}

	Stack(std::initializer_list<T> ilist)
		: Stack(ilist.size())
	{
		std::uninitialized_copy(ilist.begin(), ilist.end(), mData);
		mCount = ilist.size();
	}

	Stack(const Stack& other)
		: Stack(other.mCount)
	{
		std::uninitialized_copy(other.mData, other.mData + other.mCount, mData);
		mCount = other.mCount;
	}

	Stack(Stack&& other) noexcept
		: mData(std::exchange(other.mData, nullptr))
		, mCount(std::exchange(other.mCount, 0))
		, mCapacity(std::exchange(other.mCapacity, 0))
	{
	}

	~Stack()
	{
		clearAndDeallocate();
	}

public:
	Stack& operator=(const Stack& other)
	{
		if (this != &other)
		{
			Stack temp(other);
			Swap(temp);
		}
		return *this;
	}

	Stack& operator=(Stack&& other) noexcept
	{
		if (this != &other)
		{
			clearAndDeallocate();
			mData = std::exchange(other.mData, nullptr);
			mCount = std::exchange(other.mCount, 0);
			mCapacity = std::exchange(other.mCapacity, 0);
		}
		return *this;
	}

	Stack& operator=(std::initializer_list<T> ilist)
	{
		Stack temp(ilist);
		Swap(temp);
		return *this;
	}

	auto operator<=>(const Stack& other) const
	{
		return std::lexicographical_compare_three_way(
			mData, mData + mCount,
			other.mData, other.mData + other.mCount
		);
	}

	bool operator==(const Stack& other) const
	{
		return mCount == other.mCount && std::equal(mData, mData + mCount, other.mData);
	}

public:
	void Clear() noexcept
	{
		std::destroy_n(mData, mCount);
		mCount = 0;
	}

	bool Contains(const T& value) const
	{
		return std::find(mData, mData + mCount, value) != (mData + mCount);
	}

	size_t Count() const noexcept
	{
		return mCount;
	}

	template <typename... Args>
	void Emplace(Args&&... args)
	{
		ensureCapacity(mCount + 1);
		std::construct_at(mData + mCount, std::forward<Args>(args)...);
		++mCount;
	}

	bool IsEmpty() const noexcept
	{
		return mCount == 0;
	}

	T& Peek()
	{
		checkEmpty();
		return mData[mCount - 1];
	}

	const T& Peek() const
	{
		checkEmpty();
		return mData[mCount - 1];
	}

	void Pop()
	{
		checkEmpty();
		--mCount;
		std::destroy_at(mData + mCount);
	}

	void Push(const T& value)
	{
		Emplace(value);
	}

	void Push(T&& value)
	{
		Emplace(std::move(value));
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

	void Swap(Stack& other) noexcept
	{
		std::swap(mData, other.mData);
		std::swap(mCount, other.mCount);
		std::swap(mCapacity, other.mCapacity);
	}

private:
	void checkEmpty() const
	{
		if (mCount == 0)
		{
			throw std::out_of_range("Stack is empty");
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

	void reallocate(size_t newCapacity)
	{
		if (newCapacity == mCapacity)
		{
			return;
		}

		if (newCapacity == 0)
		{
			clearAndDeallocate();
			return;
		}

		T* newData = static_cast<T*>(::operator new(sizeof(T) * newCapacity));
		size_t newCount = std::min(mCount, newCapacity);

		try
		{
			std::uninitialized_move_n(mData, newCount, newData);
		}
		catch (...)
		{
			::operator delete(newData);
			throw;
		}

		clearAndDeallocate();

		mData = newData;
		mCount = newCount;
		mCapacity = newCapacity;
	}

	void clearAndDeallocate() noexcept
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
	T* mData;
	size_t mCount;
	size_t mCapacity;
};

} // namespace abouttt
