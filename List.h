#pragma once

#include <cassert>
#include <initializer_list>
#include <memory>
#include <stdexcept>
#include <utility>

template<typename T, typename Allocator = std::allocator<T>>
class List
{
private:
	struct Node;
	using AllocTraits = std::allocator_traits<Allocator>;
	using NodeAllocator = typename AllocTraits::template rebind_alloc<Node>;
	using NodeAllocTraits = std::allocator_traits<NodeAllocator>;

private:
	struct NodeBase
	{
		NodeBase* prev = nullptr;
		NodeBase* next = nullptr;
	};

	struct Node : NodeBase
	{
		T value;

		template<typename... Args>
		Node(Args&&... args)
			: value(std::forward<Args>(args)...)
		{
		}
	};

public:
	List(const Allocator& alloc = Allocator())
		: _sentinel()
		, _size(0)
		, _allocator(alloc)
		, _node_allocator(alloc)
	{
		_sentinel.next = &_sentinel;
		_sentinel.prev = &_sentinel;
	}

	List(size_t count, const Allocator& alloc = Allocator())
		: List(alloc)
	{
		for (size_t i = 0; i < count; i++)
		{
			emplace_back();
		}
	}

	List(size_t count, const T& value, const Allocator& alloc = Allocator())
		: List(alloc)
	{
		for (size_t i = 0; i < count; i++)
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
		: List(AllocTraits::select_on_container_copy_construction(other._allocator))
	{
		_node_allocator = other._node_allocator;

		for (NodeBase* cur = other._sentinel.next; cur != &other._sentinel; cur = cur->next)
		{
			push_back(static_cast<Node*>(cur)->value);
		}
	}

	List(List&& other)
		: _sentinel()
		, _size(0)
		, _allocator(std::move(other._allocator))
		, _node_allocator(std::move(other._node_allocator))
	{
		if (!other.empty())
		{
			_sentinel.next = other._sentinel.next;
			_sentinel.prev = other._sentinel.prev;
			_sentinel.next->prev = &_sentinel;
			_sentinel.prev->next = &_sentinel;
			_size = other._size;
		}

		other._sentinel.next = &other._sentinel;
		other._sentinel.prev = &other._sentinel;
		other._size = 0;
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
			clear();

			if constexpr (AllocTraits::propagate_on_container_copy_assignment::value)
			{
				_allocator = other._allocator;
				_node_allocator = other._node_allocator;
			}

			for (NodeBase* cur = other._sentinel.next; cur != &other._sentinel; cur = cur->next)
			{
				push_back(static_cast<Node*>(cur)->value);
			}
		}

		return *this;
	}

	List& operator=(List&& other)
	{
		if (this != &other)
		{
			clear();

			if constexpr (AllocTraits::propagate_on_container_move_assignment::value)
			{
				_allocator = std::move(other._allocator);
				_node_allocator = std::move(other._node_allocator);
			}

			if (!other.empty())
			{
				_sentinel.next = other._sentinel.next;
				_sentinel.prev = other._sentinel.prev;
				_sentinel.next->prev = &_sentinel;
				_sentinel.prev->next = &_sentinel;
				_size = other._size;
			}

			other._sentinel.next = &other._sentinel;
			other._sentinel.prev = &other._sentinel;
			other._size = 0;
		}

		return *this;
	}

	void assign(size_t count, const T& value)
	{
		clear();
		for (size_t i = 0; i < count; i++)
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
		return _allocator;
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
		return NodeAllocTraits::max_size(_node_allocator);
	}

public: // Modifiers
	void clear()
	{
		NodeBase* cur = _sentinel.next;
		while (cur != &_sentinel)
		{
			NodeBase* next = cur->next;
			destroy_node(static_cast<Node*>(cur));
			cur = next;
		}

		_sentinel.next = &_sentinel;
		_sentinel.prev = &_sentinel;
		_size = 0;
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
			throw std::out_of_range("List insert position out of range");
		}

		NodeBase* succ = node_at(pos);
		for (size_t i = 0; i < count; i++)
		{
			Node* new_node = create_node(value);
			link_before(succ, new_node);
			_size++;
		}
	}

	void insert(size_t pos, std::initializer_list<T> ilist)
	{
		if (pos > _size)
		{
			throw std::out_of_range("List insert position out of range");
		}

		NodeBase* succ = node_at(pos);
		for (const T& val : ilist)
		{
			Node* new_node = create_node(val);
			link_before(succ, new_node);
			_size++;
		}
	}

	template<typename... Args>
	void emplace(size_t pos, Args&&... args)
	{
		if (pos > _size)
		{
			throw std::out_of_range("List emplace position out of range");
		}

		NodeBase* succ = node_at(pos);
		Node* new_node = create_node(std::forward<Args>(args)...);
		link_before(succ, new_node);
		_size++;
	}

	void erase(size_t pos)
	{
		if (pos >= _size)
		{
			throw std::out_of_range("List erase position out of range");
		}

		NodeBase* target = node_at(pos);
		unlink(target);
		destroy_node(static_cast<Node*>(target));
		_size--;
	}

	void push_back(const T& value)
	{
		emplace(_size, value);
	}

	void push_back(T&& value)
	{
		emplace(_size, std::move(value));
	}

	template<typename... Args>
	T& emplace_back(Args&&... args)
	{
		Node* new_node = create_node(std::forward<Args>(args)...);
		link_before(&_sentinel, new_node);
		_size++;

		return new_node->value;
	}

	void pop_back()
	{
		assert(!empty() && "pop_back() called on empty List");

		NodeBase* node = _sentinel.prev;
		unlink(node);
		destroy_node(static_cast<Node*>(node));
		_size--;
	}

	void push_front(const T& value)
	{
		emplace_front(value);
	}

	void push_front(T&& value)
	{
		emplace_front(std::move(value));
	}

	template<typename... Args>
	T& emplace_front(Args&&... args)
	{
		Node* new_node = create_node(std::forward<Args>(args)...);
		link_before(_sentinel.next, new_node);
		_size++;

		return new_node->value;
	}

	void pop_front()
	{
		assert(!empty() && "pop_front() called on empty List");

		NodeBase* node = _sentinel.next;
		unlink(node);
		destroy_node(static_cast<Node*>(node));
		_size--;
	}

	void resize(size_t count)
	{
		if (_size > count)
		{
			size_t diff = _size - count;
			for (size_t i = 0; i < diff; ++i)
			{
				pop_back();
			}
		}
		else if (_size < count)
		{
			size_t diff = count - _size;
			for (size_t i = 0; i < diff; ++i)
			{
				emplace_back();
			}
		}
	}

	void resize(size_t count, const T& value)
	{
		if (_size > count)
		{
			size_t diff = _size - count;
			for (size_t i = 0; i < diff; ++i)
			{
				pop_back();
			}
		}
		else if (_size < count)
		{
			size_t diff = count - _size;
			for (size_t i = 0; i < diff; ++i)
			{
				emplace_back(value);
			}
		}
	}

	void swap(List& other)
	{
		if (this == &other)
		{
			return;
		}

		std::swap(_sentinel.next, other._sentinel.next);
		std::swap(_sentinel.prev, other._sentinel.prev);
		std::swap(_size, other._size);

		if (_sentinel.next)
		{
			_sentinel.next->prev = &_sentinel;
		}
		if (_sentinel.prev)
		{
			_sentinel.prev->next = &_sentinel;
		}

		if (other._sentinel.next)
		{
			other._sentinel.next->prev = &other._sentinel;
		}
		if (other._sentinel.prev)
		{
			other._sentinel.prev->next = &other._sentinel;
		}

		if constexpr (AllocTraits::propagate_on_container_swap::value)
		{
			std::swap(_allocator, other._allocator);
			std::swap(_node_allocator, other._node_allocator);
		}
	}

public:
	bool operator==(const List& other)
	{
		if (_size != other._size)
		{
			return false;
		}

		NodeBase* a = _sentinel.next;
		NodeBase* b = other._sentinel.next;

		while (a != &_sentinel)
		{
			if (static_cast<Node*>(a)->value != static_cast<Node*>(b)->value)
			{
				return false;
			}

			a = a->next;
			b = b->next;
		}

		return true;
	}

	bool operator!=(const List& other)
	{
		return !(*this == other);
	}

private:
	template<typename... Args>
	Node* create_node(Args&&... args)
	{
		Node* node = NodeAllocTraits::allocate(_node_allocator, 1);

		try
		{
			NodeAllocTraits::construct(_node_allocator, node, std::forward<Args>(args)...);
		}
		catch (...)
		{
			NodeAllocTraits::deallocate(_node_allocator, node, 1);
			throw;
		}

		return node;
	}

	void destroy_node(Node* node)
	{
		NodeAllocTraits::destroy(_node_allocator, node);
		NodeAllocTraits::deallocate(_node_allocator, node, 1);
	}

	NodeBase* node_at(size_t pos)
	{
		NodeBase* cur;

		if (pos < _size / 2)
		{
			cur = _sentinel.next;
			for (size_t i = 0; i < pos; i++)
			{
				cur = cur->next;
			}
		}
		else
		{
			cur = &_sentinel;
			for (size_t i = 0; i < _size - pos; i++)
			{
				cur = cur->prev;
			}
		}

		return cur;
	}

	void link_before(NodeBase* successor, NodeBase* new_node)
	{
		NodeBase* prev = successor->prev;
		prev->next = new_node;
		new_node->prev = prev;
		new_node->next = successor;
		successor->prev = new_node;
	}

	void unlink(NodeBase* node)
	{
		node->prev->next = node->next;
		node->next->prev = node->prev;
	}

private:
	NodeBase _sentinel;
	size_t _size;
	Allocator _allocator;
	NodeAllocator _node_allocator;
};
