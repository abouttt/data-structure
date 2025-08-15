#pragma once

#include <algorithm>
#include <concepts>
#include <initializer_list>
#include <iterator>
#include <new>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace abouttt
{

template<typename T>
class DynamicArray
{
public:
	DynamicArray() noexcept
		: mData(nullptr)
		, mSize(0)
		, mCapacity(0)
	{
	}

	explicit DynamicArray(size_t capacity)
		: DynamicArray()
	{
		allocate(mCapacity);
	}

	DynamicArray(std::initializer_list<T> values)
		: mData(nullptr)
		, mSize(values.size())
		, mCapacity(values.size())
	{
		allocate(mCapacity);
		std::uninitialized_copy(values.begin(), values.end(), mData);
	}

	DynamicArray(const DynamicArray& other)
		: mData(nullptr)
		, mSize(other.mSize)
		, mCapacity(other.mSize)
	{
		allocate(mCapacity);
		std::uninitialized_copy_n(other.mData, mSize, mData);
	}

	DynamicArray(DynamicArray&& other) noexcept
		: mData(other.mData)
		, mSize(other.mSize)
		, mCapacity(other.mCapacity)
	{
		other.mData = nullptr;
		other.mSize = 0;
		other.mCapacity = 0;
	}

	~DynamicArray()
	{
		deallocate();
	}

public:
	T& operator[](size_t index)
	{
		return mData[index];
	}

	const T& operator[](size_t index) const
	{
		return mData[index];
	}

	DynamicArray& operator=(const DynamicArray& other)
	{
		if (this != &other)
		{
			DynamicArray temp(other);
			Swap(temp);
		}

		return *this;
	}

	DynamicArray& operator=(DynamicArray&& other) noexcept
	{
		if (this != &other)
		{
			deallocate();

			mData = other.mData;
			mSize = other.mSize;
			mCapacity = other.mCapacity;

			other.mData = nullptr;
			other.mSize = 0;
			other.mCapacity = 0;
		}

		return *this;
	}

	bool operator==(const DynamicArray& other) const
	{
		if (mSize != other.mSize)
		{
			return false;
		}

		return std::equal(mData, mData + mSize, other.mData);
	}

	bool operator!=(const DynamicArray& other) const
	{
		return !(*this == other);
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

	void AddRange(std::initializer_list<T> values)
	{
		size_t count = values.size();
		if (count == 0)
		{
			return;
		}

		EnsureCapacity(mSize + count);
		std::uninitialized_copy(values.begin(), values.end(), mData + mSize);
		mSize += count;
	}

	T& At(size_t index)
	{
		if (index >= mSize)
		{
			throw std::out_of_range("Index out of range");
		}

		return mData[index];
	}

	const T& At(size_t index) const
	{
		if (index >= mSize)
		{
			throw std::out_of_range("Index out of range");
		}

		return mData[index];
	}

	size_t Capacity() const noexcept
	{
		return mCapacity;
	}

	void Clear() noexcept
	{
		if constexpr (!std::is_trivially_destructible_v<T>)
		{
			std::destroy_n(mData, mSize);
		}

		mSize = 0;
	}

	bool Contains(const T& value) const
	{
		for (size_t i = 0; i < mSize; i++)
		{
			if (mData[i] == value)
			{
				return true;
			}
		}

		return false;
	}

	T* Data() noexcept
	{
		return mData;
	}

	const T* Data() const noexcept
	{
		return mData;
	}

	void EnsureCapacity(size_t capacity)
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

	template<typename... Args>
	T& Emplace(Args&&... args)
	{
		EnsureCapacity(mSize + 1);

		T* ptr = std::construct_at(mData + mSize, std::forward<Args>(args)...);
		mSize++;

		return *ptr;
	}

	template<typename... Args>
	T& EmplaceAt(size_t index, Args&&... args)
	{
		if (index > mSize)
		{
			throw std::out_of_range("Index out of range");
		}

		if (index == mSize)
		{
			return Emplace(std::forward<Args>(args)...);
		}

		EnsureCapacity(mSize + 1);

		std::construct_at(mData + mSize, std::move(mData[mSize - 1]));
		std::move_backward(mData + index, mData + mSize - 1, mData + mSize);
		mData[index] = T(std::forward<Args>(args)...);
		mSize++;

		return mData[index];
	}

	bool Empty() const noexcept
	{
		return mSize == 0;
	}

	size_t Find(const T& value) const
	{
		return Find(0, mSize, value);
	}

	size_t Find(size_t startIndex, const T& value) const
	{
		return Find(startIndex, mSize - startIndex, value);
	}

	size_t Find(size_t startIndex, size_t count, const T& value) const
	{
		if (startIndex + count > mSize)
		{
			throw std::out_of_range("Index out of range");
		}

		for (size_t i = startIndex; i < startIndex + count; i++)
		{
			if (mData[i] == value)
			{
				return i;
			}
		}

		return mSize;
	}

	template<std::predicate<T> Predicate>
	size_t FindIf(Predicate&& pred) const
	{
		return FindIf(0, mSize, pred);
	}

	template<std::predicate<T> Predicate>
	size_t FindIf(size_t startIndex, Predicate&& pred) const
	{
		return FindIf(startIndex, mSize - startIndex, pred);
	}

	template<std::predicate<T> Predicate>
	size_t FindIf(size_t startIndex, size_t count, Predicate&& pred) const
	{
		if (startIndex + count > mSize)
		{
			throw std::out_of_range("Index out of range");
		}

		for (size_t i = startIndex; i < startIndex + count; i++)
		{
			if (pred(mData[i]))
			{
				return i;
			}
		}

		return mSize;
	}

	size_t FindLast(const T& value) const
	{
		return FindLast(0, mSize, value);
	}

	size_t FindLast(size_t startIndex, const T& value) const
	{
		return FindLast(startIndex, mSize - startIndex, value);
	}

	size_t FindLast(size_t startIndex, size_t count, const T& value) const
	{
		if (startIndex + count > mSize)
		{
			throw std::out_of_range("Index out of range");
		}

		for (size_t i = startIndex + count; i > startIndex; i--)
		{
			if (mData[i - 1] == value)
			{
				return i - 1;
			}
		}

		return mSize;
	}

	template<std::predicate<T> Predicate>
	size_t FindLastIf(Predicate&& pred) const
	{
		return FindLastIf(0, mSize, pred);
	}

	template<std::predicate<T> Predicate>
	size_t FindLastIf(size_t startIndex, Predicate&& pred) const
	{
		return FindLastIf(startIndex, mSize - startIndex, pred);
	}

	template<std::predicate<T> Predicate>
	size_t FindLastIf(size_t startIndex, size_t count, Predicate&& pred) const
	{
		if (startIndex + count > mSize)
		{
			throw std::out_of_range("Index out of range");
		}

		for (size_t i = startIndex + count; i > startIndex; i--)
		{
			if (pred(mData[i - 1]))
			{
				return i - 1;
			}
		}

		return mSize;
	}

	template<typename Func>
	void ForEach(Func&& func) const
	{
		for (size_t i = 0; i < mSize; i++)
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

	void InsertRange(size_t index, std::initializer_list<T> values)
	{
		if (index > mSize)
		{
			throw std::out_of_range("Index out of range");
		}

		size_t count = values.size();
		if (count == 0)
		{
			return;
		}

		EnsureCapacity(mSize + count);

		if (index < mSize)
		{
			std::move_backward(mData + index, mData + mSize, mData + mSize + count);
		}

		std::uninitialized_copy_n(values.begin(), count, mData + index);

		mSize += count;
	}

	bool Remove(const T& value)
	{
		for (size_t i = 0; i < mSize; i++)
		{
			if (mData[i] == value)
			{
				RemoveAt(i);
				return true;
			}
		}

		return false;
	}

	template<std::predicate<T> Predicate>
	size_t RemoveAll(Predicate&& pred)
	{
		T* newEnd = std::remove_if(mData, mData + mSize, std::forward<Predicate>(pred));
		if (newEnd == mData + mSize)
		{
			return 0;
		}

		size_t originalSize = mSize;
		size_t newSize = std::distance(mData, newEnd);

		if constexpr (!std::is_trivially_destructible_v<T>)
		{
			std::destroy(newEnd, mData + mSize);
		}

		mSize = newSize;
		return originalSize - newSize;
	}

	bool RemoveAt(size_t index)
	{
		if (index >= mSize)
		{
			throw std::out_of_range("Index out of range");
		}

		std::destroy_at(mData + index);

		if (index < mSize - 1)
		{
			std::move(mData + index + 1, mData + mSize, mData + index);
		}

		mSize--;
		return true;
	}

	void RemoveRange(size_t index, size_t count)
	{
		if (index + count > mSize)
		{
			throw std::out_of_range("Index out of range");
		}

		if (count == 0)
		{
			return;
		}

		std::destroy_n(mData + index, count);

		if (index + count < mSize)
		{
			std::move(mData + index + count, mData + mSize, mData + index);
		}

		mSize -= count;
	}

	void Reverse()
	{
		if (mSize <= 1)
		{
			return;
		}

		std::reverse(mData, mData + mSize);
	}

	void Shrink()
	{
		if (mSize < mCapacity)
		{
			reallocate(mSize);
		}
	}

	size_t Size() const noexcept
	{
		return mSize;
	}

	DynamicArray Slice(size_t index, size_t count) const
	{
		if (index + count > mSize)
		{
			throw std::out_of_range("Index out of range");
		}

		DynamicArray result(count);
		std::uninitialized_copy_n(mData + index, count, result.mData);
		result.mSize = count;

		return result;
	}

	void Sort()
	{
		if (mSize <= 1)
		{
			return;
		}

		std::sort(mData, mData + mSize);
	}

	template<typename Compare>
	void Sort(Compare&& comparison)
	{
		if (mSize <= 1)
		{
			return;
		}

		std::sort(mData, mData + mSize, std::forward<Compare>(comparison));
	}

	template<typename Compare>
	void Sort(size_t index, size_t count, Compare&& comparison)
	{
		if (index + count > mSize)
		{
			throw std::out_of_range("Index out of range");
		}

		if (count <= 1)
		{
			return;
		}

		std::sort(mData + index, mData + index + count, std::forward<Compare>(comparison));
	}

	void Swap(DynamicArray& other) noexcept
	{
		std::swap(mData, other.mData);
		std::swap(mSize, other.mSize);
		std::swap(mCapacity, other.mCapacity);
	}

private:
	void reallocate(size_t newCapacity)
	{
		if (newCapacity == mCapacity)
		{
			return;
		}

		if (newCapacity < mSize)
		{
			return;
		}

		T* newData = nullptr;
		if (newCapacity > 0)
		{
			newData = static_cast<T*>(::operator new(sizeof(T) * newCapacity, std::align_val_t{ alignof(T) }));
		}

		try
		{
			if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>)
			{
				std::uninitialized_move_n(mData, mSize, newData);
			}
			else
			{
				std::uninitialized_copy_n(mData, mSize, newData);
			}
		}
		catch (...)
		{
			::operator delete(newData, std::align_val_t{ alignof(T) });
			throw;
		}

		deallocate_internal();

		mData = newData;
		mCapacity = newCapacity;
	}

	void allocate(size_t capacity)
	{
		if (capacity == 0)
		{
			return;
		}

		mData = static_cast<T*>(::operator new(sizeof(T) * capacity, std::align_val_t{ alignof(T) }));
		mCapacity = capacity;
	}

	void deallocate() noexcept
	{
		deallocate_internal();

		mData = nullptr;
		mSize = 0;
		mCapacity = 0;
	}

	void deallocate_internal() noexcept
	{
		if (!mData)
		{
			return;
		}

		if constexpr (!std::is_trivially_destructible_v<T>)
		{
			std::destroy_n(mData, mSize);
		}

		::operator delete(mData, std::align_val_t{ alignof(T) });
	}

private:
	T* mData;
	size_t mSize;
	size_t mCapacity;
};

} // namespace abouttt
