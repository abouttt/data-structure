#pragma once

#include <algorithm>
#include <compare>
#include <initializer_list>
#include <new>
#include <memory>
#include <stdexcept>
#include <utility>

namespace abouttt
{

template<typename T>
class Array
{
public:
	Array() noexcept
		: mData(nullptr)
		, mCount(0)
		, mCapacity(0)
	{
	}

	explicit Array(size_t capacity)
		: Array()
	{
		allocate(capacity);
	}

	Array(const T& value, size_t count)
		: Array(count)
	{
		std::uninitialized_fill_n(mData, count, value);
		mCount = count;
	}

	Array(std::initializer_list<T> init)
		: Array(init.size())
	{
		std::uninitialized_copy(init.begin(), init.end(), mData);
		mCount = init.size();
	}

	Array(const Array& other)
		: Array(other.mCount)
	{
		std::uninitialized_copy_n(other.mData, other.mCount, mData);
		mCount = other.mCount;
	}

	Array(Array&& other) noexcept
		: mData(other.mData)
		, mCount(other.mCount)
		, mCapacity(other.mCapacity)
	{
		other.mData = nullptr;
		other.mCapacity = 0;
		other.mCount = 0;
	}

	~Array()
	{
		deallocate();
	}

public:
	Array& operator=(const Array& other)
	{
		if (this != &other)
		{
			Array temp(other);
			Swap(temp);
		}

		return *this;
	}

	Array& operator=(Array&& other) noexcept
	{
		if (this != &other)
		{
			deallocate();

			mData = std::exchange(other.mData, nullptr);
			mCount = std::exchange(other.mCount, 0);
			mCapacity = std::exchange(other.mCapacity, 0);
		}

		return *this;
	}

	T& operator[](size_t index)
	{
		return mData[index];
	}

	const T& operator[](size_t index) const
	{
		return mData[index];
	}

	bool operator==(const Array& other) const
	{
		if (mCount != other.mCount)
		{
			return false;
		}

		return std::equal(mData, mData + mCount, other.mData);
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

	void AddRange(std::initializer_list<T> init)
	{
		AddRange(init.begin(), init.size());
	}

	void AddRange(const Array& array)
	{
		if (this == &array)
		{
			Array temp(array);
			AddRange(temp.mData, temp.mCount);
		}
		else
		{
			AddRange(array.mData, array.mCount);
		}
	}

	void AddRange(const T* values, size_t count)
	{
		if (!values || count == 0)
		{
			return;
		}

		ensureCapacity(mCount + count);

		std::uninitialized_copy_n(values, count, mData + mCount);
		mCount += count;
	}

	T& At(size_t index)
	{
		checkRange(index);
		return mData[index];
	}

	const T& At(size_t index) const
	{
		checkRange(index);
		return mData[index];
	}

	size_t Capacity() const noexcept
	{
		return mCapacity;
	}

	void Clear() noexcept
	{
		std::destroy_n(mData, mCount);
		mCount = 0;
	}

	bool Contains(const T& value) const
	{
		return Find(value) != mCount;
	}

	template<typename Predicate>
	bool ContainsIf(Predicate pred) const
	{
		return FindIf(pred) != mCount;
	}

	size_t Count() const noexcept
	{
		return mCount;
	}

	T* Data() noexcept
	{
		return mData;
	}

	const T* Data() const noexcept
	{
		return mData;
	}

	template<typename... Args>
	T& Emplace(Args&&... args)
	{
		ensureCapacity(mCount + 1);

		T* newElement = std::construct_at(mData + mCount, std::forward<Args>(args)...);
		mCount++;

		return *newElement;
	}

	template<typename... Args>
	T& EmplaceAt(size_t index, Args&&... args)
	{
		checkRangeForInsert(index);

		if (index == mCount)
		{
			return Emplace(std::forward<Args>(args)...);
		}

		ensureCapacity(mCount + 1);

		std::move_backward(mData + index, mData + mCount, mData + mCount + 1);
		std::construct_at(mData + index, std::forward<Args>(args)...);
		mCount++;

		return mData[index];
	}

	size_t Find(const T& value) const
	{
		return Find(value, 0, mCount);
	}

	size_t Find(const T& value, size_t index) const
	{
		return Find(value, index, mCount - index);
	}

	size_t Find(const T& value, size_t index, size_t count) const
	{
		checkRange(index, count);

		if (count == 0)
		{
			return mCount;
		}

		for (size_t i = index; i < index + count; i++)
		{
			if (mData[i] == value)
			{
				return i;
			}
		}

		return mCount;
	}

	template<typename Predicate>
	size_t FindIf(Predicate pred) const
	{
		return FindIf(0, mCount, pred);
	}

	template<typename Predicate>
	size_t FindIf(size_t startIndex, Predicate pred) const
	{
		return FindIf(startIndex, mCount - startIndex, pred);
	}

	template<typename Predicate>
	size_t FindIf(size_t startIndex, size_t count, Predicate pred) const
	{
		checkRange(startIndex, count);

		if (count == 0)
		{
			return mCount;
		}

		for (size_t i = startIndex; i < startIndex + count; i++)
		{
			if (pred(mData[i]))
			{
				return i;
			}
		}

		return mCount;
	}

	size_t FindLast(const T& value) const
	{
		return FindLast(value, 0, mCount);
	}

	size_t FindLast(const T& value, size_t index) const
	{
		return FindLast(value, index, mCount - index);
	}

	size_t FindLast(const T& value, size_t index, size_t count) const
	{
		checkRange(index, count);

		if (count == 0)
		{
			return mCount;
		}

		for (size_t i = index + count; i > index; i--)
		{
			if (mData[i - 1] == value)
			{
				return i - 1;
			}
		}

		return mCount;
	}

	template<typename Predicate>
	size_t FindLastIf(Predicate pred) const
	{
		return FindLastIf(0, mCount, pred);
	}

	template<typename Predicate>
	size_t FindLastIf(size_t startIndex, Predicate pred) const
	{
		return FindLastIf(startIndex, mCount - startIndex, pred);
	}

	template<typename Predicate>
	size_t FindLastIf(size_t startIndex, size_t count, Predicate pred) const
	{
		checkRange(startIndex, count);

		if (count == 0)
		{
			return mCount;
		}

		for (size_t i = startIndex + count; i > startIndex; i--)
		{
			if (pred(mData[i - 1]))
			{
				return i - 1;
			}
		}

		return mCount;
	}

	template<typename Function>
	void ForEach(Function func)
	{
		for (size_t i = 0; i < mCount; i++)
		{
			func(mData[i]);
		}
	}

	void Insert(size_t index, const T& value)
	{
		EmplaceAt(index, value);
	}

	void Insert(size_t index, T&& value)
	{
		EmplaceAt(index, std::move(value));
	}

	void InsertRange(size_t index, std::initializer_list<T> init)
	{
		InsertRange(index, init.begin(), init.size());
	}

	void InsertRange(size_t index, const Array& array)
	{
		if (this == &array)
		{
			Array temp(array);
			InsertRange(index, temp.mData, temp.mCount);
		}
		else
		{
			InsertRange(index, array.mData, array.mCount);
		}
	}

	void InsertRange(size_t index, const T* values, size_t count)
	{
		checkRangeForInsert(index);

		if (!values || count == 0)
		{
			return;
		}

		ensureCapacity(mCount + count);

		if (mCount - index > 0)
		{
			std::move_backward(mData + index, mData + mCount, mData + mCount + count);
		}

		std::uninitialized_copy_n(values, count, mData + index);
		mCount += count;
	}

	bool Remove(const T& value)
	{
		size_t index = Find(value);
		if (index != mCount)
		{
			RemoveAt(index);
			return true;
		}

		return false;
	}

	template<typename Predicate>
	size_t RemoveAll(Predicate pred)
	{
		T* newEnd = std::remove_if(mData, mData + mCount, pred);
		size_t removedCount = std::distance(newEnd, mData + mCount);
		if (removedCount > 0)
		{
			std::destroy(newEnd, mData + mCount);
			mCount -= removedCount;
		}

		return removedCount;
	}

	void RemoveAt(size_t index)
	{
		checkRange(index);

		if (index < mCount - 1)
		{
			std::move(mData + index + 1, mData + mCount, mData + index);
		}

		std::destroy_at(mData + mCount - 1);
		mCount--;
	}

	void RemoveRange(size_t index, size_t count)
	{
		checkRange(index, count);

		if (count == 0)
		{
			return;
		}

		if (index + count < mCount)
		{
			std::move(mData + index + count, mData + mCount, mData + index);
		}

		std::destroy(mData + mCount - count, mData + mCount);
		mCount -= count;
	}

	void Reserve(size_t capacity)
	{
		if (capacity > mCapacity)
		{
			reallocate(capacity);
		}
	}

	void Reverse()
	{
		Reverse(0, mCount);
	}

	void Reverse(size_t index, size_t count)
	{
		checkRange(index, count);

		if (count > 1)
		{
			std::reverse(mData + index, mData + index + count);
		}
	}

	void Shrink()
	{
		if (mCapacity > mCount)
		{
			if (mCount > 0)
			{
				reallocate(mCount);
			}
			else
			{
				deallocate();
			}
		}
	}

	Array Slice(size_t index, size_t count) const
	{
		checkRange(index, count);

		Array result(count);

		if (count > 0)
		{
			std::uninitialized_copy_n(mData + index, count, result.mData);
			result.mCount = count;
		}

		return result;
	}

	void Sort()
	{
		Sort(0, mCount, std::less<T>{});
	}

	template<typename Compare>
	void Sort(Compare comp)
	{
		Sort(0, mCount, comp);
	}

	template<typename Compare>
	void Sort(size_t index, size_t count, Compare comp)
	{
		checkRange(index, count);

		if (count > 1)
		{
			std::sort(mData + index, mData + index + count, comp);
		}
	}

	void Swap(Array& other) noexcept
	{
		std::swap(mData, other.mData);
		std::swap(mCount, other.mCount);
		std::swap(mCapacity, other.mCapacity);
	}

private:
	void checkRange(size_t index, size_t count = 1) const
	{
		if (index >= mCount || count > mCount - index)
		{
			throw std::out_of_range("Index or count out of range");
		}
	}

	void checkRangeForInsert(size_t index) const
	{
		if (index > mCount)
		{
			throw std::out_of_range("Index for insert out of range");
		}
	}

	void ensureCapacity(size_t capacity)
	{
		if (capacity <= mCapacity)
		{
			return;
		}

		size_t newCapacity = mCapacity == 0 ? 8 : mCapacity * 2;
		if (newCapacity < capacity)
		{
			newCapacity = capacity;
		}

		reallocate(newCapacity);
	}

	void allocate(size_t capacity)
	{
		if (capacity == 0)
		{
			return;
		}

		deallocate();

		mData = static_cast<T*>(::operator new(sizeof(T) * capacity, std::align_val_t{ alignof(T) }));
		mCapacity = capacity;
	}

	void reallocate(size_t newCapacity)
	{
		if (newCapacity == mCapacity)
		{
			return;
		}

		if (newCapacity == 0)
		{
			return;
		}

		T* newData = static_cast<T*>(::operator new(sizeof(T) * newCapacity, std::align_val_t{ alignof(T) }));
		size_t newCount = std::min(newCapacity, mCount);

		if (mData)
		{
			std::uninitialized_move_n(mData, newCount, newData);
			std::destroy_n(mData, mCount);
			::operator delete(mData, std::align_val_t{ alignof(T) });
		}

		mData = newData;
		mCount = newCount;
		mCapacity = newCapacity;
	}

	void deallocate()
	{
		if (mData)
		{
			std::destroy_n(mData, mCount);
			::operator delete(mData, std::align_val_t{ alignof(T) });

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
