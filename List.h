#pragma once

#include <cassert>
#include <initializer_list>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace abouttt
{

template<typename T, typename Allocator = std::allocator<T>>
class List
{
private:
	struct NodeBase
	{
		NodeBase* prev = nullptr;
		NodeBase* next = nullptr;
	};

	struct Node : public NodeBase
	{
		T value;

		template<typename... Args>
		Node(Args&&... args)
			: value(std::forward<Args>(args)...)
		{
		}
	};

private:
	using AllocatorTraits = std::allocator_traits<Allocator>;
	using NodeAllocator = typename AllocatorTraits::template rebind_alloc<Node>;
	using NodeAllocTraits = std::allocator_traits<NodeAllocator>;

public:
	List(const Allocator& alloc = Allocator())
		: _sentinel()
		, _size(0)
		, _nodeAlloc(alloc)
	{
		_sentinel.next = &_sentinel;
		_sentinel.prev = &_sentinel;
	}

	List(size_t count, const T& value = T(), const Allocator& alloc = Allocator())
		: List(alloc)
	{
		for (size_t i = 0; i < count; ++i)
		{
			push_back(value);
		}
	}

	List(std::initializer_list<T> init, const Allocator& alloc = Allocator())
		: List(alloc)
	{
		for (const auto& val : init)
		{
			push_back(val);
		}
	}

	List(const List& other)
		: List(NodeAllocTraits::select_on_container_copy_construction(other.get_allocator()))
	{
		for (Node* cur = static_cast<Node*>(other._sentinel.next); cur != &other._sentinel; cur = static_cast<Node*>(cur->next))
		{
			push_back(cur->value);
		}
	}

	List(List&& other)
		: _sentinel()
		, _size(other._size)
		, _nodeAlloc(std::move(other._nodeAlloc))
	{
		if (_size > 0)
		{
			_sentinel.next = other._sentinel.next;
			_sentinel.prev = other._sentinel.prev;

			_sentinel.next->prev = &_sentinel;
			_sentinel.prev->next = &_sentinel;
		}
		else
		{
			_sentinel.next = &_sentinel;
			_sentinel.prev = &_sentinel;
		}

		other.reset_to_empty();
	}

	~List()
	{
		clear();
	}

public:
	List& operator=(const List& other)
	{
		if (this != &other)
		{
			List temp(other);
			swap(temp);
		}

		return *this;
	}

	List& operator=(List&& other)
	{
		if (this != &other)
		{
			clear();

			_size = other._size;
			_nodeAlloc = std::move(other._nodeAlloc);

			if (_size > 0)
			{
				_sentinel.next = other._sentinel.next;
				_sentinel.prev = other._sentinel.prev;

				_sentinel.next->prev = &_sentinel;
				_sentinel.prev->next = &_sentinel;
			}
			else
			{
				_sentinel.next = &_sentinel;
				_sentinel.prev = &_sentinel;
			}

			other.reset_to_empty();
		}

		return *this;
	}

	List& operator=(std::initializer_list<T> ilist)
	{
		clear();
		for (const auto& val : ilist)
		{
			push_back(val);
		}

		return *this;
	}

	void assign(size_t count, const T& value)
	{
		clear();
		for (size_t i = 0; i < count; ++i)
		{
			push_back(value);
		}
	}

	void assign(std::initializer_list<T> ilist)
	{
		clear();
		for (const auto& val : ilist)
		{
			push_back(val);
		}
	}

	Allocator get_allocator() const
	{
		return _nodeAlloc;
	}

public: // Element access
	T& front()
	{
		assert(!empty() && "front() called on empty List");
		return static_cast<Node*>(_sentinel.next)->value;
	}

	const T& front() const
	{
		assert(!empty() && "front() called on empty List");
		return static_cast<Node*>(_sentinel.next)->value;
	}

	T& back()
	{
		assert(!empty() && "back() called on empty List");
		return static_cast<Node*>(_sentinel.prev)->value;
	}

	const T& back() const
	{
		assert(!empty() && "back() called on empty List");
		return static_cast<Node*>(_sentinel.prev)->value;
	}

public: // Capacity
	bool empty() const
	{
		return _size == 0;
	}

	size_t size() const
	{
		return _size;
	}

	size_t max_size() const
	{
		return NodeAllocTraits::max_size(_nodeAlloc);
	}

public: // Modifiers
	void clear()
	{
		while (!empty())
		{
			pop_back();
		}
	}

	template<typename... Args>
	T& emplace(size_t pos, Args&&... args)
	{
		if (pos > _size)
		{
			throw std::out_of_range("Invalid List subscript");
		}

		NodeBase* nextNode = (pos == _size) ? &_sentinel : node_at(pos);
		NodeBase* prevNode = nextNode->prev;

		Node* newNode = create_node(std::forward<Args>(args)...);
		link(prevNode, newNode, nextNode);
		++_size;

		return newNode->value;
	}

	void insert(size_t pos, const T& value)
	{
		emplace(pos, value);
	}

	void insert(size_t pos, T&& value)
	{
		emplace(pos, std::move(value));
	}

	void insert(size_t pos, size_t count, const T& value)
	{
		if (pos > _size)
		{
			throw std::out_of_range("Invalid List subscript");
		}

		if (count == 0)
		{
			return;
		}

		NodeBase* nextNode = (pos == _size) ? &_sentinel : node_at(pos);
		NodeBase* prevNode = nextNode->prev;

		for (size_t i = 0; i < count; ++i)
		{
			Node* newNode = create_node(value);
			link(prevNode, newNode, nextNode);
			prevNode = newNode;
		}

		_size += count;
	}

	void insert(size_t pos, std::initializer_list<T> ilist)
	{
		if (pos > _size)
		{
			throw std::out_of_range("Invalid List subscript");
		}

		NodeBase* nextNode = (pos == _size) ? &_sentinel : node_at(pos);
		NodeBase* prevNode = nextNode->prev;

		for (const auto& val : ilist)
		{
			Node* newNode = create_node(val);
			link(prevNode, newNode, nextNode);
			prevNode = newNode;
		}

		_size += ilist.size();
	}

	void erase(size_t pos)
	{
		if (pos >= _size)
		{
			throw std::out_of_range("Invalid List subscript");
		}

		NodeBase* node = node_at(pos);
		unlink(node);
		destroy_node(static_cast<Node*>(node));
		--_size;
	}

	void erase(size_t first, size_t last)
	{
		if (first > last || last > _size)
		{
			throw std::out_of_range("Invalid List subscript");
		}

		if (first == last)
		{
			return;
		}

		NodeBase* firstNode = node_at(first);
		NodeBase* lastNode = (last == _size) ? &_sentinel : node_at(last);

		NodeBase* prevNode = firstNode->prev;
		NodeBase* cur = firstNode;

		while (cur != lastNode)
		{
			NodeBase* to_delete = cur;
			cur = cur->next;
			destroy_node(static_cast<Node*>(to_delete));
		}

		prevNode->next = lastNode;
		lastNode->prev = prevNode;
		_size -= (last - first);
	}

	template<typename... Args>
	T& emplace_back(Args&&... args)
	{
		Node* newNode = create_node(std::forward<Args>(args)...);
		link(_sentinel.prev, newNode, &_sentinel);
		++_size;
		return newNode->value;
	}

	void push_back(const T& value)
	{
		emplace_back(value);
	}

	void push_back(T&& value)
	{
		emplace_back(std::move(value));
	}

	void pop_back()
	{
		if (empty())
		{
			throw std::out_of_range("pop_back() called on empty List");
		}

		NodeBase* node = _sentinel.prev;
		unlink(node);
		destroy_node(static_cast<Node*>(node));
		--_size;
	}

	template<typename... Args>
	T& emplace_front(Args&&... args)
	{
		Node* newNode = create_node(std::forward<Args>(args)...);
		link(&_sentinel, newNode, _sentinel.next);
		++_size;
		return newNode->value;
	}

	void push_front(const T& value)
	{
		emplace_front(value);
	}

	void push_front(T&& value)
	{
		emplace_front(std::move(value));
	}

	void pop_front()
	{
		if (empty())
		{
			throw std::out_of_range("pop_front() called on empty List");
		}

		NodeBase* node = _sentinel.next;
		unlink(node);
		destroy_node(static_cast<Node*>(node));
		--_size;
	}

	void resize(size_t count, const T& value = T())
	{
		while (_size > count)
		{
			pop_back();
		}

		while (_size < count)
		{
			push_back(value);
		}
	}

	void swap(List& other)
	{
		std::swap(_size, other._size);
		if constexpr (NodeAllocTraits::propagate_on_container_swap::value)
		{
			std::swap(_nodeAlloc, other._nodeAlloc);
		}

		if (_size > 0 || other._size > 0)
		{
			std::swap(_sentinel.next, other._sentinel.next);
			std::swap(_sentinel.prev, other._sentinel.prev);

			_sentinel.next->prev = &_sentinel;
			_sentinel.prev->next = &_sentinel;

			other._sentinel.next->prev = &other._sentinel;
			other._sentinel.prev->next = &other._sentinel;
		}
	}

public:
	bool operator==(const List& other) const
	{
		if (_size != other._size)
		{
			return false;
		}

		auto it1 = _sentinel.next;
		auto it2 = other._sentinel.next;

		while (it1 != &_sentinel)
		{
			if (static_cast<Node*>(it1)->value != static_cast<Node*>(it2)->value)
			{
				return false;
			}

			it1 = it1->next;
			it2 = it2->next;
		}

		return true;
	}

	bool operator!=(const List& other) const
	{
		return !(*this == other);
	}

private:
	template<typename... Args>
	Node* create_node(Args&&... args)
	{
		Node* node = NodeAllocTraits::allocate(_nodeAlloc, 1);

		try
		{
			NodeAllocTraits::construct(_nodeAlloc, node, std::forward<Args>(args)...);
		}
		catch (...)
		{
			NodeAllocTraits::deallocate(_nodeAlloc, node, 1);
			throw;
		}

		return node;
	}

	void destroy_node(Node* node)
	{
		NodeAllocTraits::destroy(_nodeAlloc, node);
		NodeAllocTraits::deallocate(_nodeAlloc, node, 1);
	}

	void link(NodeBase* prev, NodeBase* node, NodeBase* next)
	{
		prev->next = node;
		node->prev = prev;
		node->next = next;
		next->prev = node;
	}

	void unlink(NodeBase* node)
	{
		node->prev->next = node->next;
		node->next->prev = node->prev;
	}

	void reset_to_empty()
	{
		_sentinel.next = &_sentinel;
		_sentinel.prev = &_sentinel;
		_size = 0;
	}

	NodeBase* node_at(size_t pos) const
	{
		NodeBase* cur;

		if (pos < _size / 2)
		{
			cur = _sentinel.next;
			for (size_t i = 0; i < pos; ++i)
			{
				cur = cur->next;
			}
		}
		else
		{
			cur = &_sentinel;
			for (size_t i = 0; i < _size - pos; ++i)
			{
				cur = cur->prev;
			}
		}

		return cur;
	}

private:
	NodeBase _sentinel;
	size_t _size = 0;
	NodeAllocator _nodeAlloc;
};

} // namespace abouttt
