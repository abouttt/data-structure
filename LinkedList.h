#pragma once

#include <compare>
#include <initializer_list>
#include <utility>

#include "MemoryUtil.h"

namespace abouttt
{

template <typename T>
class LinkedListNode;

template <typename T, typename Allocator = std::allocator<T>>
class LinkedList
{
private:
	using AllocTraits = std::allocator_traits<Allocator>;
	using Node = LinkedListNode<T>;
	using NodeAllocatorType = typename AllocTraits::template rebind_alloc<Node>;
	using NodeAllocTraits = std::allocator_traits<NodeAllocatorType>;
	using NodePointer = typename NodeAllocTraits::pointer;

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
	LinkedList() noexcept(noexcept(Allocator()))
		: LinkedList(Allocator())
	{
	}

	explicit LinkedList(const Allocator& alloc) noexcept
		: mNodeAlloc(alloc)
		, mHead(nullptr)
		, mTail(nullptr)
		, mCount(0)
	{
	}

	LinkedList(SizeType count, const T& value, const Allocator& alloc = Allocator())
		: LinkedList(alloc)
	{
		for (SizeType i = 0; i < count; ++i)
		{
			AddTail(value);
		}
	}

	LinkedList(std::initializer_list<T> ilist, const Allocator& alloc = Allocator())
		: LinkedList(alloc)
	{
		for (const T& value : ilist)
		{
			AddTail(value);
		}
	}

	LinkedList(const LinkedList& other)
		: LinkedList(other, AllocTraits::select_on_container_copy_construction(other.mAlloc))
	{
	}

	LinkedList(const LinkedList& other, const Allocator& alloc)
		: LinkedList(alloc)
	{
		for (Node* node = other.mHead; node; node = node->mNext)
		{
			AddTail(node->mValue);
		}
	}

	LinkedList(LinkedList&& other) noexcept
		: LinkedList(std::move(other), std::move(other.mNodeAlloc))
	{
	}

	LinkedList(LinkedList&& other, const Allocator& alloc)
		: LinkedList(alloc)
	{
		if (mNodeAlloc == other.mNodeAlloc)
		{
			exchangeFrom(other);
		}
		else
		{
			for (Node* node = other.mHead; node; node = node->mNext)
			{
				AddTail(std::move(node->mValue));
			}

			other.Clear();
		}
	}

	~LinkedList()
	{
		Clear();
	}

public:
	LinkedList& operator=(const LinkedList& other)
	{
		if (this == &other)
		{
			return *this;
		}

		Clear();

		if constexpr (AllocTraits::propagate_on_container_copy_assignment::value)
		{
			mNodeAlloc = other.mNodeAlloc;
		}

		for (Node* node = other.mHead; node; node = node->mNext)
		{
			AddTail(node->mValue);
		}

		return *this;
	}

	LinkedList& operator=(LinkedList&& other) noexcept(
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
			mNodeAlloc = std::move(other.mNodeAlloc);
			exchangeFrom(other);
		}
		else
		{
			if (mNodeAlloc == other.mNodeAlloc)
			{
				exchangeFrom(other);
			}
			else
			{
				for (Node* node = other.mHead; node; node = node->mNext)
				{
					AddTail(std::move(node->mValue));
				}

				other.Clear();
			}
		}

		return *this;
	}

	LinkedList& operator=(std::initializer_list<T> ilist)
	{
		Clear();

		for (const T& value : ilist)
		{
			AddTail(value);
		}

		return *this;
	}

	auto operator<=>(const LinkedList& other) const
	{
		Node* lhs = mHead;
		Node* rhs = other.mHead;

		while (lhs && rhs)
		{
			if (auto cmp = lhs->mValue <=> rhs->mValue; cmp != 0)
			{
				return cmp;
			}

			lhs = lhs->mNext;
			rhs = rhs->mNext;
		}

		return mCount <=> other.mCount;
	}

	bool operator==(const LinkedList& other) const
	{
		if (mCount != other.mCount)
		{
			return false;
		}

		Node* lhs = mHead;
		Node* rhs = other.mHead;

		while (lhs && rhs)
		{
			if (lhs->mValue != rhs->mValue)
			{
				return false;
			}

			lhs = lhs->mNext;
			rhs = rhs->mNext;
		}

		return true;
	}

public:
	void AddHead(const T& value)
	{
		EmplaceHead(value);
	}

	void AddHead(T&& value)
	{
		EmplaceHead(std::move(value));
	}

	bool AddHead(NodePointer newNode)
	{
		return Insert(newNode, mHead);
	}

	void AddTail(const T& value)
	{
		EmplaceTail(value);
	}

	void AddTail(T&& value)
	{
		EmplaceTail(std::move(value));
	}

	bool AddTail(NodePointer newNode)
	{
		return Insert(newNode, nullptr);
	}

	void Clear() noexcept
	{
		Node* current = mHead;
		while (current)
		{
			Node* next = current->mNext;
			mem::Delete(mNodeAlloc, current);
			current = next;
		}

		mHead = nullptr;
		mTail = nullptr;
		mCount = 0;
	}

	bool Contains(const T& value) const
	{
		return Find(value) != nullptr;
	}

	template <typename Predicate>
	bool ContainsIf(Predicate pred) const
	{
		return FindIf(pred) != nullptr;
	}

	SizeType Count() const noexcept
	{
		return mCount;
	}

	template<typename... Args>
	NodePointer Emplace(NodePointer before, Args&&... args)
	{
		NodePointer newNode = mem::New(mNodeAlloc, std::forward<Args>(args)...);
		return Insert(newNode, before) ? newNode : nullptr;
	}

	template<typename... Args>
	NodePointer EmplaceHead(Args&&... args)
	{
		return Emplace(mHead, std::forward<Args>(args)...);
	}

	template<typename... Args>
	NodePointer EmplaceTail(Args&&... args)
	{
		return Emplace(nullptr, std::forward<Args>(args)...);
	}

	NodePointer Find(const T& value) const
	{
		for (Node* current = mHead; current; current = current->mNext)
		{
			if (current->mValue == value)
			{
				return current;
			}
		}

		return nullptr;
	}

	template <typename Predicate>
	NodePointer FindIf(Predicate pred) const
	{
		for (Node* current = mHead; current; current = current->mNext)
		{
			if (pred(current->mValue))
			{
				return current;
			}
		}

		return nullptr;
	}

	NodePointer FindLast(const T& value) const
	{
		for (Node* current = mTail; current; current = current->mPrev)
		{
			if (current->mValue == value)
			{
				return current;
			}
		}

		return nullptr;
	}

	template <typename Predicate>
	NodePointer FindLastIf(Predicate pred) const
	{
		for (Node* current = mTail; current; current = current->mPrev)
		{
			if (pred(current->mValue))
			{
				return current;
			}
		}

		return nullptr;
	}

	AllocatorType GetAllocator() const
	{
		return AllocatorType(mNodeAlloc);
	}

	NodePointer GetHead() const noexcept
	{
		return mHead;
	}

	NodePointer GetTail() const noexcept
	{
		return mTail;
	}

	void Insert(const T& value, NodePointer before = nullptr)
	{
		Emplace(before, value);
	}

	void Insert(T&& value, NodePointer before = nullptr)
	{
		Emplace(before, std::move(value));
	}

	bool Insert(NodePointer newNode, NodePointer before = nullptr)
	{
		if (!newNode)
		{
			return false;
		}

		if (!before)
		{
			newNode->mPrev = mTail;
			newNode->mNext = nullptr;

			if (mTail)
			{
				mTail->mNext = newNode;
			}
			else
			{
				mHead = newNode;
			}
			mTail = newNode;
		}
		else
		{
			newNode->mPrev = before->mPrev;
			newNode->mNext = before;

			if (before->mPrev)
			{
				before->mPrev->mNext = newNode;
			}
			else
			{
				mHead = newNode;
			}
			before->mPrev = newNode;
		}

		++mCount;

		return true;
	}

	bool IsEmpty() const noexcept
	{
		return mCount == 0;
	}

	bool Remove(const T& value)
	{
		NodePointer node = Find(value);
		return node ? Remove(node) : false;
	}

	bool Remove(NodePointer node)
	{
		if (!node || IsEmpty())
		{
			return false;
		}

		Node* prevNode = node->mPrev;
		Node* nextNode = node->mNext;

		if (prevNode)
		{
			prevNode->mNext = nextNode;
		}
		else
		{
			mHead = nextNode;
		}

		if (nextNode)
		{
			nextNode->mPrev = prevNode;
		}
		else
		{
			mTail = prevNode;
		}

		mem::Delete(mNodeAlloc, node);
		--mCount;

		return true;
	}

	template <typename Predicate>
	SizeType RemoveAll(Predicate pred)
	{
		SizeType removedCount = 0;
		Node* current = mHead;

		while (current)
		{
			Node* next = current->mNext;
			if (pred(current->mValue))
			{
				Remove(current);
				++removedCount;
			}

			current = next;
		}

		return removedCount;
	}

	void Resize(SizeType newSize)
	{
		Resize(newSize, T());
	}

	void Resize(SizeType newSize, const T& value)
	{
		if (newSize > mCount)
		{
			SizeType diff = newSize - mCount;
			for (SizeType i = 0; i < diff; ++i)
			{
				AddTail(value);
			}
		}
		else
		{
			while (mCount > newSize)
			{
				Remove(mTail);
			}
		}
	}

	void Swap(LinkedList& other) noexcept(
		NodeAllocTraits::propagate_on_container_swap::value ||
		NodeAllocTraits::is_always_equal::value)
	{
		if (this == &other)
		{
			return;
		}

		if constexpr (NodeAllocTraits::propagate_on_container_swap::value)
		{
			std::swap(mNodeAlloc, other.mNodeAlloc);
		}

		std::swap(mHead, other.mHead);
		std::swap(mTail, other.mTail);
		std::swap(mCount, other.mCount);
	}

private:
	void exchangeFrom(LinkedList& other) noexcept
	{
		mHead = std::exchange(mHead, nullptr);
		mTail = std::exchange(mTail, nullptr);
		mCount = std::exchange(mCount, 0);
	}

private:
	NodeAllocatorType mNodeAlloc;
	NodePointer mHead;
	NodePointer mTail;
	SizeType mCount;
};

template <typename T>
class LinkedListNode
{
public:
	template <typename U, typename Alloc>
	friend class LinkedList;

public:
	template<typename... Args>
	explicit LinkedListNode(Args&&... args)
		: mValue(std::forward<Args>(args)...)
		, mNext(nullptr)
		, mPrev(nullptr)
	{
	}

public:
	T& GetValue() noexcept
	{
		return mValue;
	}

	const T& GetValue() const noexcept
	{
		return mValue;
	}

	LinkedListNode* GetNextNode() noexcept
	{
		return mNext;
	}

	const LinkedListNode* GetNextNode() const noexcept
	{
		return mNext;
	}

	LinkedListNode* GetPrevNode() noexcept
	{
		return mPrev;
	}

	const LinkedListNode* GetPrevNode() const noexcept
	{
		return mPrev;
	}

private:
	T mValue;
	LinkedListNode* mNext;
	LinkedListNode* mPrev;
};

} // namespace abouttt
