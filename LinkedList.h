#pragma once

#include <algorithm>
#include <compare>
#include <initializer_list>
#include <utility>

namespace abouttt
{

template <typename T>
class LinkedListNode;

template <typename T>
class LinkedList
{
public:
	LinkedList()
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

		return mCount <=> other.mCount;
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

	bool AddHead(LinkedListNode<T>* node)
	{
		return Insert(node, mHead);
	}

	void AddTail(const T& value)
	{
		EmplaceTail(value);
	}

	void AddTail(T&& value)
	{
		EmplaceTail(std::move(value));
	}

	bool AddTail(LinkedListNode<T>* node)
	{
		return Insert(node, nullptr);
	}

	void Clear() noexcept
	{
		LinkedListNode<T>* current = mHead;
		while (current)
		{
			LinkedListNode<T>* next = current->mNext;
			delete current;
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

	size_t Count() const noexcept
	{
		return mCount;
	}

	template <typename... Args>
	LinkedListNode<T>* Emplace(LinkedListNode<T>* before, Args&&... args)
	{
		LinkedListNode<T>* newNode = new LinkedListNode<T>(std::forward<Args>(args)...);
		return Insert(newNode, before) ? newNode : nullptr;
	}

	template <typename... Args>
	LinkedListNode<T>* EmplaceHead(Args&&... args)
	{
		return Emplace(mHead, std::forward<Args>(args)...);
	}

	template <typename... Args>
	LinkedListNode<T>* EmplaceTail(Args&&... args)
	{
		return Emplace(nullptr, std::forward<Args>(args)...);
	}

	LinkedListNode<T>* Find(const T& value) const
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

	template <typename Predicate>
	LinkedListNode<T>* FindIf(Predicate pred) const
	{
		for (LinkedListNode<T>* node = mHead; node != nullptr; node = node->mNext)
		{
			if (pred(node->mValue))
			{
				return node;
			}
		}
		return nullptr;
	}

	LinkedListNode<T>* FindLast(const T& value) const
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

	template <typename Predicate>
	LinkedListNode<T>* FindLastIf(Predicate pred) const
	{
		for (LinkedListNode<T>* node = mTail; node != nullptr; node = node->mPrev)
		{
			if (pred(node->mValue))
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
		Emplace(before, value);
	}

	void Insert(T&& value, LinkedListNode<T>* before)
	{
		Emplace(before, std::move(value));
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

	bool Remove(LinkedListNode<T>* node)
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

		delete node;
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

public:
	template <typename... Args>
	LinkedListNode(Args&&... args)
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

} // namespace abouttt
