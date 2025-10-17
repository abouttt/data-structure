#pragma once

#include <algorithm>
#include <compare>
#include <initializer_list>
#include <type_traits>
#include <utility>

namespace abouttt
{

template <typename T>
class LinkedListNode;

template <typename T, bool IsConst>
class LinkedListIterator;

template <typename T>
class LinkedList
{
public:
	using Iterator = LinkedListIterator<T, false>;
	using ConstIterator = LinkedListIterator<T, true>;

public:
	LinkedList() noexcept
		: mHead(nullptr)
		, mTail(nullptr)
		, mCount(0)
	{
	}

	LinkedList(std::initializer_list<T> ilist)
		: LinkedList()
	{
		for (const T& value : ilist)
		{
			AddTail(value);
		}
	}

	LinkedList(const LinkedList& other)
		: LinkedList()
	{
		for (LinkedListNode<T>* node = other.mHead; node != nullptr; node = node->mNext)
		{
			AddTail(node->mValue);
		}
	}

	LinkedList(LinkedList&& other) noexcept
		: mHead(std::exchange(other.mHead, nullptr))
		, mTail(std::exchange(other.mTail, nullptr))
		, mCount(std::exchange(other.mCount, 0))
	{
	}

	~LinkedList()
	{
		Clear();
	}

public:
	LinkedList& operator=(const LinkedList& other)
	{
		if (this != &other)
		{
			LinkedList temp(other);
			Swap(temp);
		}
		return *this;
	}

	LinkedList& operator=(LinkedList&& other) noexcept
	{
		if (this != &other)
		{
			Clear();
			mHead = std::exchange(other.mHead, nullptr);
			mTail = std::exchange(other.mTail, nullptr);
			mCount = std::exchange(other.mCount, 0);
		}
		return *this;
	}

	LinkedList& operator=(std::initializer_list<T> ilist)
	{
		LinkedList temp(ilist);
		Swap(temp);
		return *this;
	}

	auto operator<=>(const LinkedList& other) const
	{
		LinkedListNode<T>* a = mHead;
		LinkedListNode<T>* b = other.mHead;

		while (a && b)
		{
			if (auto cmp = a->mValue <=> b->mValue; cmp != 0)
			{
				return cmp;
			}
			a = a->mNext;
			b = b->mNext;
		}

		return !a && !b ? std::strong_ordering::equal :
			!a ? std::strong_ordering::less :
			std::strong_ordering::greater;
	}

	bool operator==(const LinkedList& other) const
	{
		if (mCount != other.mCount)
		{
			return false;
		}

		LinkedListNode<T>* a = mHead;
		LinkedListNode<T>* b = other.mHead;

		while (a && b)
		{
			if (a->mValue != b->mValue)
			{
				return false;
			}
			a = a->mNext;
			b = b->mNext;
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

	bool AddHead(LinkedListNode<T>* newNode)
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

	bool AddTail(LinkedListNode<T>* newNode)
	{
		return Insert(newNode, nullptr);
	}

	void Clear() noexcept
	{
		while (mHead)
		{
			auto next = mHead->mNext;
			delete mHead;
			mHead = next;
		}
		mTail = nullptr;
		mCount = 0;
	}

	bool Contains(const T& value) const
	{
		return Find(value) != nullptr;
	}

	size_t Count() const noexcept
	{
		return mCount;
	}

	template <typename... Args>
	LinkedListNode<T>* EmplaceHead(Args&&... args)
	{
		LinkedListNode<T>* newNode = new LinkedListNode<T>(std::forward<Args>(args)...);
		return Insert(newNode, mHead) ? newNode : nullptr;
	}

	template <typename... Args>
	LinkedListNode<T>* EmplaceTail(Args&&... args)
	{
		LinkedListNode<T>* newNode = new LinkedListNode<T>(std::forward<Args>(args)...);
		return Insert(newNode, nullptr) ? newNode : nullptr;
	}

	LinkedListNode<T>* Find(const T& value)
	{
		return const_cast<LinkedListNode<T>*>(static_cast<const LinkedList*>(this)->Find(value));
	}

	const LinkedListNode<T>* Find(const T& value) const
	{
		for (LinkedListNode<T>* node = mHead; node != nullptr; node = node->mNext)
		{
			if (node->mValue == value)
			{
				return node;
			}
		}
		return nullptr;
	}

	LinkedListNode<T>* FindLast(const T& value)
	{
		return const_cast<LinkedListNode<T>*>(static_cast<const LinkedList*>(this)->FindLast(value));
	}

	const LinkedListNode<T>* FindLast(const T& value) const
	{
		for (LinkedListNode<T>* node = mTail; node != nullptr; node = node->mPrev)
		{
			if (node->mValue == value)
			{
				return node;
			}
		}
		return nullptr;
	}

	LinkedListNode<T>* Head() const noexcept
	{
		return mHead;
	}

	void Insert(const T& value, LinkedListNode<T>* before)
	{
		LinkedListNode<T>* newNode = new LinkedListNode<T>(value);
		Insert(newNode, before);
	}

	void Insert(T&& value, LinkedListNode<T>* before)
	{
		LinkedListNode<T>* newNode = new LinkedListNode<T>(std::move(value));
		Insert(newNode, before);
	}

	bool Insert(LinkedListNode<T>* newNode, LinkedListNode<T>* before)
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
		LinkedListNode<T>* node = Find(value);
		return node ? Remove(node) : false;
	}

	bool Remove(LinkedListNode<T>* node, bool bDelete = true)
	{
		if (!node)
		{
			return false;
		}

		LinkedListNode<T>* prev = node->mPrev;
		LinkedListNode<T>* next = node->mNext;

		if (prev)
		{
			prev->mNext = next;
		}
		else
		{
			mHead = next;
		}

		if (next)
		{
			next->mPrev = prev;
		}
		else
		{
			mTail = prev;
		}

		if (bDelete)
		{
			delete node;
		}
		else
		{
			node->mPrev = nullptr;
			node->mNext = nullptr;
		}

		--mCount;

		return true;
	}

	LinkedListNode<T>* Tail() const noexcept
	{
		return mTail;
	}

	void Swap(LinkedList& other) noexcept
	{
		std::swap(mHead, other.mHead);
		std::swap(mTail, other.mTail);
		std::swap(mCount, other.mCount);
	}

public: // Iterators for range-based loop support.
	Iterator begin() noexcept
	{
		return Iterator(mHead);
	}

	ConstIterator begin() const noexcept
	{
		return ConstIterator(mHead);
	}

	Iterator end() noexcept
	{
		return Iterator(nullptr);
	}

	ConstIterator end() const noexcept
	{
		return ConstIterator(nullptr);
	}

private:
	LinkedListNode<T>* mHead;
	LinkedListNode<T>* mTail;
	size_t mCount;
};

template <typename T>
class LinkedListNode
{
public:
	friend class LinkedList<T>;
	template <typename U, bool IsConst>
	friend class LinkedListIterator;

public:
	template <typename... Args>
	explicit LinkedListNode(Args&&... args)
		: mValue(std::forward<Args>(args)...)
		, mNext(nullptr)
		, mPrev(nullptr)
	{
	}

public:
	T& Value() noexcept
	{
		return mValue;
	}

	const T& Value() const noexcept
	{
		return mValue;
	}

	LinkedListNode* Next() noexcept
	{
		return mNext;
	}

	const LinkedListNode* Next() const noexcept
	{
		return mNext;
	}

	LinkedListNode* Prev() noexcept
	{
		return mPrev;
	}

	const LinkedListNode* Prev() const noexcept
	{
		return mPrev;
	}

private:
	T mValue;
	LinkedListNode* mNext;
	LinkedListNode* mPrev;
};

template <typename T, bool IsConst>
class LinkedListIterator
{
public:
	using NodeType = std::conditional_t<IsConst, const LinkedListNode<T>, LinkedListNode<T>>;
	using ValueType = std::conditional_t<IsConst, const T, T>;

public:
	explicit LinkedListIterator(NodeType* node) noexcept
		: mNode(node)
	{
	}

public:
	ValueType& operator*() const noexcept
	{
		return mNode->mValue;
	}

	ValueType* operator->() const noexcept
	{
		return &mNode->mValue;
	}

	LinkedListIterator& operator++() noexcept
	{
		mNode = mNode ? mNode->mNext : nullptr;
		return *this;
	}

	LinkedListIterator operator++(int) noexcept
	{
		LinkedListIterator temp(*this);
		++(*this);
		return temp;
	}

	bool operator==(const LinkedListIterator& other) const noexcept
	{
		return mNode == other.mNode;
	}

	bool operator!=(const LinkedListIterator& other) const noexcept
	{
		return mNode != other.mNode;
	}

private:
	NodeType* mNode;
};

} // namespace abouttt
