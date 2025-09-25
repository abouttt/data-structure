#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <limits>
#include <memory>
#include <type_traits>
#include <utility>

namespace abouttt
{
namespace mem
{

template <typename Alloc>
using AllocTraits = std::allocator_traits<Alloc>;

template <typename Alloc>
using AllocPointer = typename AllocTraits<Alloc>::pointer;

template <typename Alloc>
using ValueType = typename AllocTraits<Alloc>::value_type;

template <typename Alloc>
using SizeType = typename AllocTraits<Alloc>::size_type;

// --- allocation helpers ---

template <typename Alloc>
[[nodiscard]] inline AllocPointer<Alloc> Allocate(Alloc& alloc, SizeType<Alloc> count)
{
	return count > 0 ? AllocTraits<Alloc>::allocate(alloc, count) : AllocPointer<Alloc>{};
}

template <typename Alloc>
inline void Deallocate(Alloc& alloc, AllocPointer<Alloc> ptr, SizeType<Alloc> count)
{
	if (ptr)
	{
		AllocTraits<Alloc>::deallocate(alloc, ptr, count);
	}
}

// --- construct/destroy helpers ---

template <typename Alloc, typename Ptr, typename... Args>
inline void ConstructAt(Alloc& alloc, Ptr ptr, Args&&... args)
{
	AllocTraits<Alloc>::construct(alloc, ptr, std::forward<Args>(args)...);
}

template <typename Alloc, typename Ptr>
inline void DestroyAt(Alloc& alloc, Ptr ptr)
{
	AllocTraits<Alloc>::destroy(alloc, ptr);
}

template <typename Alloc, typename FwdIt>
inline void Destroy(Alloc& alloc, FwdIt first, FwdIt last)
{
	for (auto it = first; it != last; ++it)
	{
		DestroyAt(alloc, it);
	}
}

template <typename Alloc, typename FwdIt>
inline FwdIt DestroyN(Alloc& alloc, FwdIt first, SizeType<Alloc> count)
{
	auto current = first;
	for (SizeType<Alloc> i = 0; i < count; ++i, ++current)
	{
		DestroyAt(alloc, current);
	}
	return current;
}

template <typename Alloc, typename BidIt>
inline void DestroyBackward(Alloc& alloc, BidIt first, BidIt last)
{
	auto current = last;
	while (current != first)
	{
		--current;
		DestroyAt(alloc, current);
	}
}

template <typename Alloc, typename BidIt>
inline BidIt DestroyBackwardN(Alloc& alloc, BidIt last, SizeType<Alloc> count)
{
	auto current = last;
	for (SizeType<Alloc> i = 0; i < count; ++i)
	{
		--current;
		DestroyAt(alloc, current);
	}
	return current;
}

template <typename Alloc, typename T>
inline void DestroyCircular(Alloc& alloc, T* buffer, SizeType<Alloc> start, SizeType<Alloc> count, SizeType<Alloc> capacity)
{
	if (!buffer || capacity == 0 || count == 0)
	{
		return;
	}

	start %= capacity;
	SizeType<Alloc> end = start + count;

	if (end <= capacity)
	{
		DestroyN(alloc, buffer + start, count);
	}
	else
	{
		SizeType<Alloc> firstPart = capacity - start;
		DestroyN(alloc, buffer + start, firstPart);
		DestroyN(alloc, buffer, end - capacity);
	}
}

// --- single-object helpers ---

template <typename Alloc, typename... Args>
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

template <typename Alloc>
inline void Delete(Alloc& alloc, AllocPointer<Alloc> ptr)
{
	if (ptr)
	{
		DestroyAt(alloc, ptr);
		Deallocate(alloc, ptr, 1);
	}
}

// --- uninitialized algorithms ---

template <typename Alloc, typename InIt, typename FwdIt>
inline FwdIt UninitializedCopy(Alloc& alloc, InIt first, InIt last, FwdIt dest)
{
	auto current = dest;
	try
	{
		for (; first != last; ++first, ++current)
		{
			ConstructAt(alloc, current, *first);
		}
		return current;
	}
	catch (...)
	{
		Destroy(alloc, dest, current);
		throw;
	}
}

template <typename Alloc, typename InIt, typename FwdIt>
inline FwdIt UninitializedCopyN(Alloc& alloc, InIt first, SizeType<Alloc> count, FwdIt dest)
{
	auto current = dest;
	try
	{
		for (SizeType<Alloc> i = 0; i < count; ++i, ++first, ++current)
		{
			ConstructAt(alloc, current, *first);
		}
		return current;
	}
	catch (...)
	{
		Destroy(alloc, dest, current);
		throw;
	}
}

template <typename Alloc, typename FwdIt>
inline void UninitializedDefaultConstruct(Alloc& alloc, FwdIt first, FwdIt last)
{
	auto current = first;
	try
	{
		for (; current != last; ++current)
		{
			ConstructAt(alloc, current);
		}
	}
	catch (...)
	{
		Destroy(alloc, first, current);
		throw;
	}
}

template <typename Alloc, typename FwdIt>
inline FwdIt UninitializedDefaultConstructN(Alloc& alloc, FwdIt first, SizeType<Alloc> count)
{
	auto current = first;
	try
	{
		for (SizeType<Alloc> i = 0; i < count; ++i, ++current)
		{
			ConstructAt(alloc, current);
		}
		return current;
	}
	catch (...)
	{
		Destroy(alloc, first, current);
		throw;
	}
}

template <typename Alloc, typename FwdIt, typename T>
inline void UninitializedFill(Alloc& alloc, FwdIt first, FwdIt last, const T& value)
{
	auto current = first;
	try
	{
		for (; current != last; ++current)
		{
			ConstructAt(alloc, current, value);
		}
	}
	catch (...)
	{
		Destroy(alloc, first, current);
		throw;
	}
}

template <typename Alloc, typename FwdIt, typename T>
inline FwdIt UninitializedFillN(Alloc& alloc, FwdIt first, SizeType<Alloc> count, const T& value)
{
	auto current = first;
	try
	{
		for (SizeType<Alloc> i = 0; i < count; ++i, ++current)
		{
			ConstructAt(alloc, current, value);
		}
		return current;
	}
	catch (...)
	{
		Destroy(alloc, first, current);
		throw;
	}
}

template <typename Alloc, typename InIt, typename FwdIt>
inline FwdIt UninitializedMove(Alloc& alloc, InIt first, InIt last, FwdIt dest)
{
	auto current = dest;
	try
	{
		for (; first != last; ++first, ++current)
		{
			ConstructAt(alloc, current, std::move(*first));
		}
		return current;
	}
	catch (...)
	{
		Destroy(alloc, dest, current);
		throw;
	}
}

template <typename Alloc, typename InIt, typename FwdIt>
inline FwdIt UninitializedMoveN(Alloc& alloc, InIt first, SizeType<Alloc> count, FwdIt dest)
{
	auto current = dest;
	try
	{
		for (SizeType<Alloc> i = 0; i < count; ++i, ++first, ++current)
		{
			ConstructAt(alloc, current, std::move(*first));
		}
		return current;
	}
	catch (...)
	{
		Destroy(alloc, dest, current);
		throw;
	}
}

template <typename Alloc, typename FwdIt>
inline void UninitializedValueConstruct(Alloc& alloc, FwdIt first, FwdIt last)
{
	auto current = first;
	try
	{
		for (; current != last; ++current)
		{
			ConstructAt(alloc, current, ValueType<Alloc>{});
		}
	}
	catch (...)
	{
		Destroy(alloc, first, current);
		throw;
	}
}

template <typename Alloc, typename FwdIt>
inline FwdIt UninitializedValueConstructN(Alloc& alloc, FwdIt first, SizeType<Alloc> count)
{
	auto current = first;
	try
	{
		for (SizeType<Alloc> i = 0; i < count; ++i, ++current)
		{
			ConstructAt(alloc, current, ValueType<Alloc>{});
		}
		return current;
	}
	catch (...)
	{
		Destroy(alloc, first, current);
		throw;
	}
}

template <typename Alloc, typename InIt, typename FwdIt>
inline FwdIt UninitializedMoveOrCopy(Alloc& alloc, InIt first, InIt last, FwdIt dest)
{
	using T = std::iter_value_t<InIt>;
	if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>)
	{
		return UninitializedMove(alloc, first, last, dest);
	}
	else
	{
		return UninitializedCopy(alloc, first, last, dest);
	}
}

template <typename Alloc, typename InIt, typename FwdIt>
inline FwdIt UninitializedMoveOrCopyN(Alloc& alloc, InIt first, SizeType<Alloc> count, FwdIt dest)
{
	using T = std::iter_value_t<InIt>;
	if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>)
	{
		return UninitializedMoveN(alloc, first, count, dest);
	}
	else
	{
		return UninitializedCopyN(alloc, first, count, dest);
	}
}

// --- simple wrappers around std algorithms ---

template <typename InIt, typename OutIt>
constexpr OutIt Copy(InIt first, InIt last, OutIt dest)
{
	return std::copy(first, last, dest);
}

template <typename InIt, typename Diff, typename OutIt>
constexpr OutIt CopyN(InIt first, Diff count, OutIt dest)
{
	return std::copy_n(first, count, dest);
}

template <typename FwdIt, typename T>
constexpr void Fill(FwdIt first, FwdIt last, const T& value)
{
	std::fill(first, last, value);
}

template <typename OutIt, typename Diff, typename T>
constexpr OutIt FillN(OutIt first, Diff count, const T& value)
{
	return std::fill_n(first, count, value);
}

template <typename InIt, typename OutIt>
constexpr OutIt Move(InIt first, InIt last, OutIt dest)
{
	return std::move(first, last, dest);
}

template <typename BidIt1, typename BidIt2>
constexpr BidIt2 MoveBackward(BidIt1 first, BidIt1 last, BidIt2 dest)
{
	return std::move_backward(first, last, dest);
}

// --- reallocation helpers ---

template <typename Alloc>
[[nodiscard]] inline AllocPointer<Alloc> Reallocate(
	Alloc& alloc,
	AllocPointer<Alloc> buffer,
	SizeType<Alloc> count,
	SizeType<Alloc> capacity,
	SizeType<Alloc> newCapacity)
{

	if (newCapacity == 0)
	{
		if (buffer)
		{
			DestroyN(alloc, buffer, count);
			Deallocate(alloc, buffer, capacity);
		}
		return AllocPointer<Alloc>{};
	}

	if (capacity == newCapacity)
	{
		return buffer;
	}

	AllocPointer<Alloc> newBuffer = Allocate<Alloc>(alloc, newCapacity);
	SizeType<Alloc> elementsToMove = std::min(count, newCapacity);

	try
	{
		if (buffer && elementsToMove > 0)
		{
			UninitializedMoveOrCopyN(alloc, buffer, elementsToMove, newBuffer);
		}
	}
	catch (...)
	{
		Deallocate(alloc, newBuffer, newCapacity);
		throw;
	}

	if (buffer)
	{
		DestroyN(alloc, buffer, elementsToMove);
		if (count > newCapacity)
		{
			DestroyN(alloc, buffer + elementsToMove, count - elementsToMove);
		}
		Deallocate(alloc, buffer, capacity);
	}

	return newBuffer;
}

template <typename Alloc>
[[nodiscard]] inline AllocPointer<Alloc> ReallocateCircular(
	Alloc& alloc,
	AllocPointer<Alloc> buffer,
	SizeType<Alloc> start,
	SizeType<Alloc> count,
	SizeType<Alloc> capacity,
	SizeType<Alloc> newCapacity)
{

	if (newCapacity == 0)
	{
		if (buffer)
		{
			DestroyCircular(alloc, buffer, start, count, capacity);
			Deallocate(alloc, buffer, capacity);
		}
		return AllocPointer<Alloc>{};
	}

	if (newCapacity == capacity)
	{
		return buffer;
	}

	AllocPointer<Alloc> newBuffer = Allocate<Alloc>(alloc, newCapacity);
	SizeType<Alloc> elementsToMove = std::min(count, newCapacity);

	if (buffer && elementsToMove > 0)
	{
		try
		{
			start %= capacity;
			SizeType<Alloc> firstPart = std::min(elementsToMove, capacity - start);
			SizeType<Alloc> secondPart = elementsToMove - firstPart;
			AllocPointer<Alloc> cur = newBuffer;

			if (firstPart > 0)
			{
				cur = UninitializedMoveOrCopyN(alloc, buffer + start, firstPart, cur);
			}
			if (secondPart > 0)
			{
				UninitializedMoveOrCopyN(alloc, buffer, secondPart, cur);
			}
		}
		catch (...)
		{
			Deallocate(alloc, newBuffer, newCapacity);
			throw;
		}
	}

	if (buffer)
	{
		DestroyCircular(alloc, buffer, start, elementsToMove, capacity);
		if (count > newCapacity)
		{
			SizeType<Alloc> excessStart = (start + elementsToMove) % capacity;
			SizeType<Alloc> excessCount = count - elementsToMove;
			DestroyCircular(alloc, buffer, excessStart, excessCount, capacity);
		}
		Deallocate(alloc, buffer, capacity);
	}

	return newBuffer;
}

// --- growth policy helpers ---

inline size_t GrowCapacity(size_t current, size_t minRequired, double factor = 1.5, size_t initCap = 8)
{
	if (minRequired == 0)
	{
		return current;
	}

	if (current == 0)
	{
		return std::max(initCap, minRequired);
	}

	factor = std::max(factor, 1.0);

	constexpr size_t maxSize = std::numeric_limits<size_t>::max();
	if (current > (maxSize - 1) / factor)
	{
		return maxSize;
	}

	size_t candidate = static_cast<size_t>(std::ceil(static_cast<double>(current) * factor));
	if (candidate < current)
	{
		return maxSize;
	}

	return std::max(candidate, minRequired);
}

} // namespace mem
} // namespace abouttt
