#pragma once

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <limits>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

namespace abouttt
{
namespace mem
{

template<typename Alloc>
using AllocTraits = std::allocator_traits<Alloc>;

template<typename Alloc>
using AllocPointer = typename AllocTraits<Alloc>::pointer;

template<typename Alloc>
using ValueType = typename AllocTraits<Alloc>::value_type;

template<typename Alloc, typename FwdIt>
class DestroyGuard
{
public:
	DestroyGuard(Alloc& alloc, FwdIt first) noexcept
		: mAlloc(std::addressof(alloc))
		, mFirst(first)
		, mCurrent(first)
		, mbActive(true)
	{
	}

	~DestroyGuard() noexcept
	{
		if (mbActive)
		{
			for (FwdIt it = mFirst; it != mCurrent; ++it)
			{
				AllocTraits<Alloc>::destroy(*mAlloc, std::to_address(it));
			}
		}
	}

	DestroyGuard(const DestroyGuard&) = delete;
	DestroyGuard(DestroyGuard&&) = delete;
	DestroyGuard& operator=(const DestroyGuard&) = delete;
	DestroyGuard& operator=(DestroyGuard&&) = delete;

public:
	void Advance() noexcept
	{
		++mCurrent;
	}

	void Advance(size_t n) noexcept
	{
		std::advance(mCurrent, n);
	}

	void Release() noexcept
	{
		mbActive = false;
	}

	FwdIt Current() const noexcept
	{
		return mCurrent;
	}

private:
	Alloc* const mAlloc;
	FwdIt const mFirst;
	FwdIt mCurrent;
	bool mbActive;
};

template<typename Alloc>
[[nodiscard]] inline AllocPointer<Alloc> Allocate(Alloc& alloc, size_t count)
{
	return count > 0 ? AllocTraits<Alloc>::allocate(alloc, count) : AllocPointer<Alloc>{};
}

template<typename Alloc>
inline void Deallocate(Alloc& alloc, AllocPointer<Alloc> ptr, size_t count) noexcept
{
	if (ptr)
	{
		AllocTraits<Alloc>::deallocate(alloc, ptr, count);
	}
}

template<typename Alloc, typename It, typename... Args>
inline void ConstructAt(Alloc& alloc, It it, Args&&... args)
{
	AllocTraits<Alloc>::construct(alloc, std::to_address(it), std::forward<Args>(args)...);
}

template<typename Alloc, typename It>
inline void DestroyAt(Alloc& alloc, It it) noexcept
{
	AllocTraits<Alloc>::destroy(alloc, std::to_address(it));
}

template<typename Alloc, typename FwdIt>
inline void Destroy(Alloc& alloc, FwdIt first, FwdIt last) noexcept
{
	for (; first != last; ++first)
	{
		DestroyAt(alloc, first);
	}
}

template<typename Alloc, typename FwdIt>
inline FwdIt DestroyN(Alloc& alloc, FwdIt first, size_t count) noexcept
{
	for (size_t i = 0; i < count; ++i, ++first)
	{
		DestroyAt(alloc, first);
	}
	return first;
}

template<typename Alloc, typename BidIt>
inline void DestroyBackward(Alloc& alloc, BidIt first, BidIt last) noexcept
{
	while (last != first)
	{
		--last;
		DestroyAt(alloc, last);
	}
}

template<typename Alloc, typename BidIt>
inline BidIt DestroyBackwardN(Alloc& alloc, BidIt last, size_t count) noexcept
{
	for (size_t i = 0; i < count; ++i)
	{
		--last;
		DestroyAt(alloc, last);
	}
	return last;
}

template<typename Alloc, typename T>
inline void DestroyCircular(Alloc& alloc, T* buffer, size_t capacity, size_t start, size_t count) noexcept
{
	if (capacity == 0 || count == 0)
	{
		return;
	}

	start %= capacity;
	size_t end = start + count;
	if (end <= capacity)
	{
		DestroyN(alloc, buffer + start, count);
	}
	else
	{
		size_t firstPart = capacity - start;
		DestroyN(alloc, buffer + start, firstPart);
		DestroyN(alloc, buffer, end - capacity);
	}
}

template<typename Alloc, typename... Args>
[[nodiscard]] inline AllocPointer<Alloc> New(Alloc& alloc, Args&&... args)
{
	AllocPointer<Alloc> ptr = Allocate<Alloc>(alloc, 1);
	try
	{
		ConstructAt(alloc, ptr, std::forward<Args>(args)...);
		return ptr;
	}
	catch (...)
	{
		Deallocate(alloc, ptr, 1);
		throw;
	}
}

template<typename Alloc>
inline void Delete(Alloc& alloc, AllocPointer<Alloc> ptr) noexcept
{
	if (ptr)
	{
		DestroyAt(alloc, ptr);
		Deallocate(alloc, ptr, 1);
	}
}

template<typename Alloc, typename InIt, typename FwdIt>
inline FwdIt UninitializedCopy(Alloc& alloc, InIt first, InIt last, FwdIt dest)
{
	DestroyGuard<Alloc, FwdIt> guard(alloc, dest);
	for (; first != last; ++first, ++dest)
	{
		ConstructAt(alloc, dest, *first);
		guard.Advance();
	}
	guard.Release();
	return dest;
}

template<typename Alloc, typename InIt, typename FwdIt>
inline FwdIt UninitializedCopyN(Alloc& alloc, InIt first, size_t count, FwdIt dest)
{
	DestroyGuard<Alloc, FwdIt> guard(alloc, dest);
	for (size_t i = 0; i < count; ++i, ++first, ++dest)
	{
		ConstructAt(alloc, dest, *first);
		guard.Advance();
	}
	guard.Release();
	return dest;
}

template<typename Alloc, typename FwdIt>
inline void UninitializedDefaultConstruct(Alloc& alloc, FwdIt first, FwdIt last)
{
	DestroyGuard<Alloc, FwdIt> guard(alloc, first);
	for (; first != last; ++first)
	{
		AllocTraits<Alloc>::construct(alloc, std::to_address(first));
		guard.Advance();
	}
	guard.Release();
}

template<typename Alloc, typename FwdIt>
inline FwdIt UninitializedDefaultConstructN(Alloc& alloc, FwdIt first, size_t count)
{
	DestroyGuard<Alloc, FwdIt> guard(alloc, first);
	FwdIt cur = first;
	for (size_t i = 0; i < count; ++i, ++cur)
	{
		AllocTraits<Alloc>::construct(alloc, std::to_address(cur));
		guard.Advance();
	}
	guard.Release();
	return cur;
}

template<typename Alloc, typename FwdIt, typename T>
inline void UninitializedFill(Alloc& alloc, FwdIt first, FwdIt last, const T& value)
{
	DestroyGuard<Alloc, FwdIt> guard(alloc, first);
	for (; first != last; ++first)
	{
		ConstructAt(alloc, first, value);
		guard.Advance();
	}
	guard.Release();
}

template<typename Alloc, typename FwdIt, typename T>
inline FwdIt UninitializedFillN(Alloc& alloc, FwdIt first, size_t count, const T& value)
{
	DestroyGuard<Alloc, FwdIt> guard(alloc, first);
	FwdIt cur = first;
	for (size_t i = 0; i < count; ++i, ++cur)
	{
		ConstructAt(alloc, cur, value);
		guard.Advance();
	}
	guard.Release();
	return cur;
}

template<typename Alloc, typename InIt, typename FwdIt>
inline FwdIt UninitializedMove(Alloc& alloc, InIt first, InIt last, FwdIt dest)
{
	DestroyGuard<Alloc, FwdIt> guard(alloc, dest);
	for (; first != last; ++first, ++dest)
	{
		ConstructAt(alloc, dest, std::move(*first));
		guard.Advance();
	}
	guard.Release();
	return dest;
}

template<typename Alloc, typename InIt, typename FwdIt>
inline FwdIt UninitializedMoveN(Alloc& alloc, InIt first, size_t count, FwdIt dest)
{
	DestroyGuard<Alloc, FwdIt> guard(alloc, dest);
	for (size_t i = 0; i < count; ++i, ++first, ++dest)
	{
		ConstructAt(alloc, dest, std::move(*first));
		guard.Advance();
	}
	guard.Release();
	return dest;
}

template<typename Alloc, typename FwdIt>
inline void UninitializedValueConstruct(Alloc& alloc, FwdIt first, FwdIt last)
{
	DestroyGuard<Alloc, FwdIt> guard(alloc, first);
	for (; first != last; ++first)
	{
		AllocTraits<Alloc>::construct(alloc, std::to_address(first), ValueType<Alloc>{});
		guard.Advance();
	}
	guard.Release();
}

template<typename Alloc, typename FwdIt>
inline FwdIt UninitializedValueConstructN(Alloc& alloc, FwdIt first, size_t count)
{
	DestroyGuard<Alloc, FwdIt> guard(alloc, first);
	FwdIt cur = first;
	for (size_t i = 0; i < count; ++i, ++cur)
	{
		AllocTraits<Alloc>::construct(alloc, std::to_address(cur), ValueType<Alloc>{});
		guard.Advance();
	}
	guard.Release();
	return cur;
}

template<typename Alloc, typename InIt, typename FwdIt>
inline FwdIt UninitializedMoveOrCopy(Alloc& alloc, InIt first, InIt last, FwdIt dest)
{
	using T = std::iter_value_t<FwdIt>;
	if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>)
	{
		return UninitializedMove<Alloc>(alloc, first, last, dest);
	}
	else
	{
		return UninitializedCopy<Alloc>(alloc, first, last, dest);
	}
}

template<typename Alloc, typename InIt, typename FwdIt>
inline FwdIt UninitializedMoveOrCopyN(Alloc& alloc, InIt first, size_t count, FwdIt dest)
{
	using T = std::iter_value_t<FwdIt>;
	if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>)
	{
		return UninitializedMoveN<Alloc>(alloc, first, count, dest);
	}
	else
	{
		return UninitializedCopyN<Alloc>(alloc, first, count, dest);
	}
}

template<typename InIt, typename OutIt>
constexpr OutIt Copy(InIt first, InIt last, OutIt dest)
{
	return std::copy(first, last, dest);
}

template<typename InIt, typename OutIt>
constexpr OutIt CopyN(InIt first, size_t count, OutIt dest)
{
	return std::copy_n(first, count, dest);
}

template<typename FwdIt, typename T>
constexpr void Fill(FwdIt first, FwdIt last, const T& value)
{
	std::fill(first, last, value);
}

template<typename OutIt, typename T>
constexpr OutIt FillN(OutIt first, size_t count, const T& value)
{
	return std::fill_n(first, count, value);
}

template<typename InIt, typename OutIt>
constexpr OutIt Move(InIt first, InIt last, OutIt dest)
{
	return std::move(first, last, dest);
}

template<typename BidIt1, typename BidIt2>
constexpr BidIt2 MoveBackward(BidIt1 first, BidIt1 last, BidIt2 dest)
{
	return std::move_backward(first, last, dest);
}

template<typename Alloc>
[[nodiscard]] inline AllocPointer<Alloc> Reallocate(
	Alloc& alloc,
	AllocPointer<Alloc> buffer,
	size_t count,
	size_t capacity,
	size_t newCapacity)
{
	if (capacity == newCapacity)
	{
		return buffer;
	}

	if (newCapacity == 0)
	{
		if (buffer)
		{
			DestroyN(alloc, buffer, count);
			Deallocate(alloc, buffer, capacity);
		}
		return AllocPointer<Alloc>{};
	}

	AllocPointer<Alloc> newBuffer = Allocate<Alloc>(alloc, newCapacity);
	DestroyGuard<Alloc, AllocPointer<Alloc>> guard(alloc, newBuffer);

	try
	{
		if (buffer && count > 0)
		{
			size_t moveCount = std::min(count, newCapacity);
			auto rawOld = std::to_address(buffer);
			UninitializedMoveOrCopyN(alloc, rawOld, moveCount, newBuffer);
			guard.Advance(moveCount);
		}
		guard.Release();
	}
	catch (...)
	{
		Deallocate(alloc, newBuffer, newCapacity);
		throw;
	}

	if (buffer)
	{
		DestroyN(alloc, buffer, count);
		Deallocate(alloc, buffer, capacity);
	}

	return newBuffer;
}

template<typename Alloc>
[[nodiscard]] inline AllocPointer<Alloc> ReallocateCircular(
	Alloc& alloc,
	AllocPointer<Alloc> buffer,
	size_t capacity,
	size_t start,
	size_t count,
	size_t newCapacity)
{
	if (capacity == newCapacity)
	{
		return buffer;
	}

	if (newCapacity == 0)
	{
		if (buffer)
		{
			DestroyCircular(alloc, std::to_address(buffer), capacity, start, count);
			Deallocate(alloc, buffer, capacity);
		}
		return AllocPointer<Alloc>{};
	}

	AllocPointer<Alloc> newBuffer = Allocate<Alloc>(alloc, newCapacity);
	DestroyGuard<Alloc, AllocPointer<Alloc>> guard(alloc, newBuffer);

	try
	{
		if (buffer && count > 0)
		{
			start %= capacity;
			size_t moveCount = std::min(count, newCapacity);
			size_t beginIdx = start;
			size_t firstPart = std::min(moveCount, capacity - beginIdx);
			size_t secondPart = moveCount - firstPart;
			auto rawOld = std::to_address(buffer);

			if (firstPart > 0)
			{
				UninitializedMoveOrCopyN(alloc, rawOld + beginIdx, firstPart, newBuffer);
				guard.Advance(firstPart);
			}

			if (secondPart > 0)
			{
				UninitializedMoveOrCopyN(alloc, rawOld, secondPart, newBuffer + firstPart);
				guard.Advance(secondPart);
			}
		}
		guard.Release();
	}
	catch (...)
	{
		Deallocate(alloc, newBuffer, newCapacity);
		throw;
	}

	if (buffer)
	{
		DestroyCircular(alloc, std::to_address(buffer), capacity, start, count);
		Deallocate(alloc, buffer, capacity);
	}

	return newBuffer;
}

constexpr size_t GrowCapacity(size_t current, size_t minRequired) noexcept
{
	constexpr size_t maxCapacity = std::numeric_limits<size_t>::max();
	if (minRequired > maxCapacity)
	{
		return maxCapacity;
	}

	if (current == 0)
	{
		return std::max(minRequired, size_t(8));
	}

	if (current > maxCapacity / 2)
	{
		return std::max(minRequired, maxCapacity);
	}

	size_t grown = current + current / 2;
	if (grown < current)
	{
		return std::max(minRequired, maxCapacity);
	}

	return std::max(grown, minRequired);
}

} // namespace mem
} // namespace abouttt
