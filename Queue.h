#pragma once

#include <compare>
#include <initializer_list>
#include <memory>
#include <new>
#include <stdexcept>
#include <utility>

namespace abouttt
{

template <typename T>
class Queue
{
public:
	Queue() noexcept
		: Queue(0)
	{
	}

	explicit Queue(size_t capacity)
		: mData(capacity > 0 ? static_cast<T*>(::operator new(sizeof(T) * capacity)) : nullptr)
		, mFront(0)
		, mRear(0)
		, mCount(0)
		, mCapacity(capacity)
	{
	}

	Queue(std::initializer_list<T> ilist)
		: mData(static_cast<T*>(::operator new(sizeof(T)* ilist.size())))
		, mFront(0)
		, mRear(ilist.size() % ilist.size())
		, mCount(ilist.size())
		, mCapacity(ilist.size())
	{
		std::uninitialized_copy(ilist.begin(), ilist.end(), mData);
	}

	Queue(const Queue& other)
		: mData(static_cast<T*>(::operator new(sizeof(T)* other.mCount)))
		, mFront(0)
		, mRear(other.mCount % (other.mCount == 0 ? 1 : other.mCount))
		, mCount(other.mCount)
		, mCapacity(other.mCount)
	{
		for (size_t i = 0; i < other.mCount; ++i)
		{
			std::construct_at(mData + i, other.mData[(other.mFront + i) % other.mCapacity]);
		}
	}

	Queue(Queue&& other) noexcept
		: mData(std::exchange(other.mData, nullptr))
		, mFront(std::exchange(other.mFront, 0))
		, mRear(std::exchange(other.mRear, 0))
		, mCount(std::exchange(other.mCount, 0))
		, mCapacity(std::exchange(other.mCapacity, 0))
	{
	}

	~Queue()
	{
		clearAndDeallocate();
	}

public:
	Queue& operator=(const Queue& other)
	{
		if (this != &other)
		{
			Queue temp(other);
			Swap(temp);
		}
		return *this;
	}

	Queue& operator=(Queue&& other) noexcept
	{
		if (this != &other)
		{
			clearAndDeallocate();
			mData = std::exchange(other.mData, nullptr);
			mFront = std::exchange(other.mFront, 0);
			mRear = std::exchange(other.mRear, 0);
			mCount = std::exchange(other.mCount, 0);
			mCapacity = std::exchange(other.mCapacity, 0);
		}
		return *this;
	}

	Queue& operator=(std::initializer_list<T> ilist)
	{
		Queue temp(ilist);
		Swap(temp);
		return *this;
	}

	auto operator<=>(const Queue& other) const
	{
		if (mCount != other.mCount)
		{
			return mCount <=> other.mCount;
		}

		for (size_t i = 0; i < mCount; ++i)
		{
			const T& a = mData[(mFront + i) % mCapacity];
			const T& b = other.mData[(other.mFront + i) % other.mCapacity];
			if (auto cmp = a <=> b; cmp != 0)
			{
				return cmp;
			}
		}

		return std::strong_ordering::equal;
	}

	bool operator==(const Queue& other) const
	{
		if (mCount != other.mCount)
		{
			return false;
		}

		for (size_t i = 0; i < mCount; ++i)
		{
			if (mData[(mFront + i) % mCapacity] != other.mData[(other.mFront + i) % other.mCapacity])
			{
				return false;
			}
		}

		return true;
	}

public:
	void Clear() noexcept
	{
		if (mData)
		{
			if (mFront < mRear)
			{
				std::destroy_n(mData + mFront, mCount);
			}
			else
			{
				std::destroy_n(mData + mFront, mCapacity - mFront);
				std::destroy_n(mData, mRear);
			}
			mFront = 0;
			mRear = 0;
			mCount = 0;
		}
	}

	bool Contains(const T& value) const
	{
		for (size_t i = 0; i < mCount; ++i)
		{
			if (mData[(mFront + i) % mCapacity] == value)
			{
				return true;
			}
		}
		return false;
	}

	size_t Count() const noexcept
	{
		return mCount;
	}

	void Dequeue()
	{
		checkEmpty();
		std::destroy_at(mData + mFront);
		mFront = (mFront + 1) % mCapacity;
		--mCount;
	}

	template <typename... Args>
	void Emplace(Args&&... args)
	{
		ensureCapacity(mCount + 1);
		std::construct_at(mData + mRear, std::forward<Args>(args)...);
		mRear = (mRear + 1) % mCapacity;
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

	T& Peek()
	{
		checkEmpty();
		return mData[mFront];
	}

	const T& Peek() const
	{
		checkEmpty();
		return mData[mFront];
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

	void Swap(Queue& other) noexcept
	{
		std::swap(mData, other.mData);
		std::swap(mFront, other.mFront);
		std::swap(mRear, other.mRear);
		std::swap(mCount, other.mCount);
		std::swap(mCapacity, other.mCapacity);
	}

private:
	void checkEmpty() const
	{
		if (mCount == 0)
		{
			throw std::out_of_range("Queue is empty");
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
			if (mData && newCount > 0)
			{
				if (mFront < mRear)
				{
					std::uninitialized_move_n(mData + mFront, newCount, newData);
				}
				else
				{
					size_t frontPart = std::min(newCount, mCapacity - mFront);
					std::uninitialized_move_n(mData + mFront, frontPart, newData);

					size_t remaining = newCount - frontPart;
					if (remaining > 0)
					{
						std::uninitialized_move_n(mData, remaining, newData + frontPart);
					}
				}
			}
		}
		catch (...)
		{
			::operator delete(newData);
			throw;
		}

		clearAndDeallocate();

		mData = newData;
		mFront = 0;
		mRear = newCount % newCapacity;
		mCount = newCount;
		mCapacity = newCapacity;
	}

	void clearAndDeallocate() noexcept
	{
		if (mData)
		{
			Clear();
			::operator delete(mData);
			mData = nullptr;
		}
	}

private:
	T* mData;
	size_t mFront;
	size_t mRear;
	size_t mCount;
	size_t mCapacity;
};

} // namespace abouttt
