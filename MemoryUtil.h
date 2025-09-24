#pragma once

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <iterator>
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

template <typename Alloc, typename FwdIt>
class DestroyGuard
{
public:
	DestroyGuard(Alloc& alloc, FwdIt first) noexcept
		: mAlloc(std::addressof(alloc))
		, mFirst(first)
		, mCurrent(first)
		, mActive(true)
	{
	}

	~DestroyGuard() noexcept
	{
		if (!mActive)
		{
			return;
		}

		for (FwdIt it = mFirst; it != mCurrent; ++it)
		{
			AllocTraits<Alloc>::destroy(*mAlloc, std::to_address(it));
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

	void Advance(SizeType<Alloc> n) noexcept
	{
		if constexpr (std::random_access_iterator<FwdIt>)
		{
			mCurrent += static_cast<ptrdiff_t>(n);
		}
		else
		{
			std::advance(mCurrent, static_cast<ptrdiff_t>(n));
		}
	}

	FwdIt Current() const noexcept
	{
		return mCurrent;
	}

	void SetCurrent(FwdIt it) noexcept
	{
		mCurrent = it;
	}

	void Release() noexcept
	{
		mActive = false;
	}

private:
	Alloc* const mAlloc;
	FwdIt const mFirst;
	FwdIt mCurrent;
	bool mActive;
};

// --- allocation helpers ---

template <typename Alloc>
[[nodiscard]] inline AllocPointer<Alloc> Allocate(Alloc& alloc, SizeType<Alloc> count)
{
	return count > 0 ? AllocTraits<Alloc>::allocate(alloc, count) : AllocPointer<Alloc>{};
}

template <typename Alloc>
inline void Deallocate(Alloc& alloc, AllocPointer<Alloc> ptr, SizeType<Alloc> count) noexcept
{
	if (std::to_address(ptr))
	{
		AllocTraits<Alloc>::deallocate(alloc, ptr, count);
	}
}

// --- construct/destroy helpers ---

template <typename Alloc, typename It, typename... Args>
inline void ConstructAt(Alloc& alloc, It it, Args&&... args)
{
	AllocTraits<Alloc>::construct(alloc, std::to_address(it), std::forward<Args>(args)...);
}

template <typename Alloc, typename It>
inline void DestroyAt(Alloc& alloc, It it) noexcept
{
	AllocTraits<Alloc>::destroy(alloc, std::to_address(it));
}

template <typename Alloc, typename FwdIt>
inline void Destroy(Alloc& alloc, FwdIt first, FwdIt last) noexcept
{
	for (; first != last; ++first)
	{
		DestroyAt(alloc, first);
	}
}

template <typename Alloc, typename FwdIt>
inline FwdIt DestroyN(Alloc& alloc, FwdIt first, SizeType<Alloc> count) noexcept
{
	for (SizeType<Alloc> i = 0; i < count; ++i, ++first)
	{
		DestroyAt(alloc, first);
	}
	return first;
}

template <typename Alloc, typename BidIt>
inline void DestroyBackward(Alloc& alloc, BidIt first, BidIt last) noexcept
{
	while (last != first)
	{
		--last;
		DestroyAt(alloc, last);
	}
}

template <typename Alloc, typename BidIt>
inline BidIt DestroyBackwardN(Alloc& alloc, BidIt last, SizeType<Alloc> count) noexcept
{
	for (SizeType<Alloc> i = 0; i < count; ++i)
	{
		--last;
		DestroyAt(alloc, last);
	}
	return last;
}

template <typename Alloc, typename T>
inline void DestroyCircular(Alloc& alloc, T* buffer, SizeType<Alloc> start, SizeType<Alloc> count, SizeType<Alloc> capacity) noexcept
{
	if (capacity == 0 || count == 0)
	{
		return;
	}

	assert(count <= capacity && "DestroyCircular: count must not exceed capacity");

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
inline void Delete(Alloc& alloc, AllocPointer<Alloc> ptr) noexcept
{
	if (std::to_address(ptr))
	{
		DestroyAt(alloc, ptr);
		Deallocate(alloc, ptr, 1);
	}
}

// --- uninitialized algorithms (use guards for strong exception safety) ---

template <typename Alloc, typename InIt, typename FwdIt>
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

template <typename Alloc, typename InIt, typename FwdIt>
inline FwdIt UninitializedCopyN(Alloc& alloc, InIt first, SizeType<Alloc> count, FwdIt dest)
{
	DestroyGuard<Alloc, FwdIt> guard(alloc, dest);
	FwdIt cur = dest;
	for (SizeType<Alloc> i = 0; i < count; ++i, ++first, ++cur)
	{
		ConstructAt(alloc, cur, *first);
		guard.Advance();
	}
	guard.Release();
	return cur;
}

template <typename Alloc, typename FwdIt>
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

template <typename Alloc, typename FwdIt>
inline FwdIt UninitializedDefaultConstructN(Alloc& alloc, FwdIt first, SizeType<Alloc> count)
{
	DestroyGuard<Alloc, FwdIt> guard(alloc, first);
	FwdIt cur = first;
	for (SizeType<Alloc> i = 0; i < count; ++i, ++cur)
	{
		AllocTraits<Alloc>::construct(alloc, std::to_address(cur));
		guard.Advance();
	}
	guard.Release();
	return cur;
}

template <typename Alloc, typename FwdIt, typename T>
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

template <typename Alloc, typename FwdIt, typename T>
inline FwdIt UninitializedFillN(Alloc& alloc, FwdIt first, SizeType<Alloc> count, const T& value)
{
	DestroyGuard<Alloc, FwdIt> guard(alloc, first);
	FwdIt cur = first;
	for (SizeType<Alloc> i = 0; i < count; ++i, ++cur)
	{
		ConstructAt(alloc, cur, value);
		guard.Advance();
	}
	guard.Release();
	return cur;
}

template <typename Alloc, typename InIt, typename FwdIt>
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

template <typename Alloc, typename InIt, typename FwdIt>
inline FwdIt UninitializedMoveN(Alloc& alloc, InIt first, SizeType<Alloc> count, FwdIt dest)
{
	DestroyGuard<Alloc, FwdIt> guard(alloc, dest);
	FwdIt cur = dest;
	for (SizeType<Alloc> i = 0; i < count; ++i, ++first, ++cur)
	{
		ConstructAt(alloc, cur, std::move(*first));
		guard.Advance();
	}
	guard.Release();
	return cur;
}

template <typename Alloc, typename FwdIt>
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

template <typename Alloc, typename FwdIt>
inline FwdIt UninitializedValueConstructN(Alloc& alloc, FwdIt first, SizeType<Alloc> count)
{
	DestroyGuard<Alloc, FwdIt> guard(alloc, first);
	FwdIt cur = first;
	for (SizeType<Alloc> i = 0; i < count; ++i, ++cur)
	{
		AllocTraits<Alloc>::construct(alloc, std::to_address(cur), ValueType<Alloc>{});
		guard.Advance();
	}
	guard.Release();
	return cur;
}

template <typename Alloc, typename InIt, typename FwdIt>
inline FwdIt UninitializedMoveOrCopy(Alloc& alloc, InIt first, InIt last, FwdIt dest)
{
	using T = std::iter_value_t<InIt>;
	if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>)
	{
		return UninitializedMove<Alloc>(alloc, first, last, dest);
	}
	else
	{
		return UninitializedCopy<Alloc>(alloc, first, last, dest);
	}
}

template <typename Alloc, typename InIt, typename FwdIt>
inline FwdIt UninitializedMoveOrCopyN(Alloc& alloc, InIt first, SizeType<Alloc> count, FwdIt dest)
{
	using T = std::iter_value_t<InIt>;

	if constexpr (std::is_trivially_copyable_v<T> && std::contiguous_iterator<InIt> && std::contiguous_iterator<FwdIt>)
	{
		std::memmove(std::to_address(dest), std::to_address(first), static_cast<size_t>(count) * sizeof(T));
		return std::next(dest, static_cast<ptrdiff_t>(count));
	}
	else if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>)
	{
		return UninitializedMoveN<Alloc>(alloc, first, count, dest);
	}
	else
	{
		return UninitializedCopyN<Alloc>(alloc, first, count, dest);
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
	if (capacity == newCapacity)
	{
		return buffer;
	}

	if (newCapacity == 0)
	{
		if (std::to_address(buffer))
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
		if (std::to_address(buffer) && count > 0)
		{
			SizeType<Alloc> moveCount = std::min(count, newCapacity);
			auto rawOld = std::to_address(buffer);
			auto newEnd = UninitializedMoveOrCopyN<Alloc>(alloc, rawOld, moveCount, newBuffer);
			guard.SetCurrent(newEnd);
		}
		guard.Release();
	}
	catch (...)
	{
		Deallocate(alloc, newBuffer, newCapacity);
		throw;
	}

	if (std::to_address(buffer))
	{
		DestroyN(alloc, buffer, count);
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
	if (capacity == newCapacity)
	{
		return buffer;
	}

	if (newCapacity == 0)
	{
		if (std::to_address(buffer))
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
		if (std::to_address(buffer) && count > 0)
		{
			assert(count <= capacity && "ReallocateCircular: count must not exceed capacity");

			start %= capacity;
			SizeType<Alloc> moveCount = std::min(count, newCapacity);
			auto rawOld = std::to_address(buffer);

			SizeType<Alloc> firstPart = std::min(moveCount, capacity - start);
			SizeType<Alloc> secondPart = moveCount - firstPart;

			auto cur = newBuffer;
			if (firstPart > 0)
			{
				cur = UninitializedMoveOrCopyN(alloc, rawOld + start, firstPart, cur);
			}
			if (secondPart > 0)
			{
				cur = UninitializedMoveOrCopyN(alloc, rawOld, secondPart, cur);
			}
			guard.SetCurrent(cur);
		}
		guard.Release();
	}
	catch (...)
	{
		Deallocate(alloc, newBuffer, newCapacity);
		throw;
	}

	if (std::to_address(buffer))
	{
		DestroyCircular(alloc, std::to_address(buffer), capacity, start, count);
		Deallocate(alloc, buffer, capacity);
	}

	return newBuffer;
}

// --- growth policy helpers ---

inline size_t GrowCapacity(size_t current, size_t minRequired, double factor = 1.5, size_t initCap = 8)
{
	factor = std::max(factor, 1.0);
	size_t candidate = current == 0 ? initCap : static_cast<size_t>(std::ceil(static_cast<double>(current) * factor));
	return std::max(candidate, minRequired);
}

} // namespace mem
} // namespace abouttt
