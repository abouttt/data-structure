#pragma once

#include <cassert>
#include <initializer_list>
#include <memory>
#include <utility>
#include <stdexcept>

template<typename T>
class Vector
{
private:
	using AllocTraits = std::allocator_traits<std::allocator<T>>;

public:
	Vector()
		: _data(nullptr)
		, _size(0)
		, _capacity(0)
		, _allocator()
	{
	}

	Vector(size_t count)
		: _data(nullptr)
		, _size(count)
		, _capacity(count)
		, _allocator()
	{
		if (count > 0)
		{
			_data = AllocTraits::allocate(_allocator, _capacity);
			std::uninitialized_value_construct_n(_data, _size);
		}
	}

	Vector(size_t count, const T& value)
		: _data(nullptr)
		, _size(count)
		, _capacity(count)
		, _allocator()
	{
		if (count > 0)
		{
			_data = AllocTraits::allocate(_allocator, _capacity);
			std::uninitialized_fill_n(_data, _size, value);
		}
	}

	Vector(std::initializer_list<T> init)
		: _data(nullptr)
		, _size(init.size())
		, _capacity(init.size())
		, _allocator()
	{
		if (_size > 0)
		{
			_data = AllocTraits::allocate(_allocator, _capacity);
			std::uninitialized_copy(init.begin(), init.end(), _data);
		}
	}

	Vector(const Vector& other)
		: _data(nullptr)
		, _size(other._size)
		, _capacity(other._size)
		, _allocator(other._allocator)
	{
		if (_size > 0)
		{
			_data = AllocTraits::allocate(_allocator, _capacity);
			std::uninitialized_copy(other._data, other._data + other._size, _data);
		}
	}

	Vector(Vector&& other)
		: _data(other._data)
		, _size(other._size)
		, _capacity(other._capacity)
		, _allocator(std::move(other._allocator))
	{
		other._data = nullptr;
		other._size = 0;
		other._capacity = 0;
	}

	~Vector()
	{
		destroy_elements();
		AllocTraits::deallocate(_allocator, _data, _capacity);
	}

public:
	Vector& operator=(const Vector& other)
	{
		if (this != &other)
		{
			Vector temp(other);
			swap(temp);
		}

		return *this;
	}

	Vector& operator=(Vector&& other)
	{
		if (this != &other)
		{
			destroy_elements();
			AllocTraits::deallocate(_allocator, _data, _capacity);

			_data = other._data;
			_size = other._size;
			_capacity = other._capacity;

			other._data = nullptr;
			other._size = 0;
			other._capacity = 0;
		}

		return *this;
	}

	Vector& operator=(std::initializer_list<T> ilist)
	{
		assign(ilist);
		return *this;
	}

	void assign(size_t count, const T& value)
	{
		clear();

		if (count > _capacity)
		{
			Vector temp(count, value);
			swap(temp);
		}
		else
		{
			std::uninitialized_fill_n(_data, count, value);
			_size = count;
		}
	}

	void assign(std::initializer_list<T> ilist)
	{
		clear();

		if (ilist.size() > _capacity)
		{
			Vector temp(ilist);
			swap(temp);
		}
		else
		{
			std::uninitialized_copy(ilist.begin(), ilist.end(), _data);
			_size = ilist.size();
		}
	}

public: // Element access
	T& at(size_t pos)
	{
		if (pos >= _size)
		{
			throw std::out_of_range("Vector at position out of range");
		}

		return _data[pos];
	}

	const T& at(size_t pos) const
	{
		if (pos >= _size)
		{
			throw std::out_of_range("Vector at position out of range");
		}

		return _data[pos];
	}

	T& operator[](size_t pos)
	{
		return _data[pos];
	}

	const T& operator[](size_t pos) const
	{
		return _data[pos];
	}

	T& front()
	{
		assert(!empty() && "front() called on empty Vector");
		return _data[0];
	}

	const T& front() const
	{
		assert(!empty() && "front() called on empty Vector");
		return _data[0];
	}

	T& back()
	{
		assert(!empty() && "back() called on empty Vector");
		return _data[_size - 1];
	}

	const T& back() const
	{
		assert(!empty() && "back() called on empty Vector");
		return _data[_size - 1];
	}

	T* data()
	{
		return _data;
	}

	const T* data() const
	{
		return _data;
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

	void reserve(size_t new_cap)
	{
		if (new_cap > _capacity)
		{
			reallocate(new_cap);
		}
	}

	size_t capacity() const
	{
		return _capacity;
	}

	void shrink_to_fit()
	{
		if (_size < _capacity)
		{
			reallocate(_size);
		}
	}

public: // Modifiers
	void clear()
	{
		destroy_elements();
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
			throw std::out_of_range("Vector insert position out of range");
		}

		if (count == 0)
		{
			return;
		}

		size_t new_size = _size + count;
		if (new_size > _capacity)
		{
			size_t new_cap = std::max(_capacity * 2, new_size);
			reserve(new_cap);
		}

		std::move_backward(_data + pos, _data + _size, _data + new_size);
		std::uninitialized_fill_n(_data + pos, count, value);
		_size = new_size;
	}

	void insert(size_t pos, std::initializer_list<T> ilist)
	{
		if (pos > _size)
		{
			throw std::out_of_range("Vector insert position out of range");
		}

		size_t count = ilist.size();
		if (count == 0)
		{
			return;
		}

		size_t new_size = _size + count;
		if (new_size > _capacity)
		{
			size_t new_cap = std::max(_capacity * 2, new_size);
			reserve(new_cap);
		}

		std::move_backward(_data + pos, _data + _size, _data + new_size);
		std::uninitialized_copy(ilist.begin(), ilist.end(), _data + pos);
		_size = new_size;
	}

	template<typename... Args>
	void emplace(size_t pos, Args&&... args)
	{
		if (pos > _size)
		{
			throw std::out_of_range("Vector emplace position out of range");
		}

		if (_size == _capacity)
		{
			size_t new_cap = (_capacity == 0) ? 1 : _capacity * 2;
			reserve(new_cap);
		}

		std::move_backward(_data + pos, _data + _size, _data + _size + 1);
		AllocTraits::construct(_allocator, _data + pos, std::forward<Args>(args)...);
		_size++;
	}

	void erase(size_t pos)
	{
		if (pos >= _size)
		{
			throw std::out_of_range("Vector erase position out of range");
		}

		AllocTraits::destroy(_allocator, _data + pos);
		std::move(_data + pos + 1, _data + _size, _data + pos);
		_size--;
	}

	void erase(size_t pos, size_t count)
	{
		if (pos + count > _size)
		{
			throw std::out_of_range("Vector erase range out of range");
		}

		for (size_t i = 0; i < count; ++i)
		{
			AllocTraits::destroy(_allocator, _data + pos + i);
		}

		std::move(_data + pos + count, _data + _size, _data + pos);
		_size -= count;
	}

	void push_back(const T& value)
	{
		emplace_back(value);
	}

	void push_back(T&& value)
	{
		emplace_back(std::move(value));
	}

	template<typename... Args>
	T& emplace_back(Args&&... args)
	{
		if (_size == _capacity)
		{
			reserve((_capacity == 0) ? 1 : _capacity * 2);
		}

		AllocTraits::construct(_allocator, _data + _size, std::forward<Args>(args)...);
		_size++;

		return _data[_size - 1];
	}

	void pop_back()
	{
		assert(!empty() && "pop_back() called on empty Vector");

		_size--;
		AllocTraits::destroy(_allocator, _data + _size);
	}

	void resize(size_t count)
	{
		if (count < _size)
		{
			erase(count, _size - count);
		}
		else if (count > _size)
		{
			reserve(count);
			for (size_t i = _size; i < count; ++i)
			{
				AllocTraits::construct(_allocator, _data + i);
			}
		}

		_size = count;
	}

	void resize(size_t count, const T& value)
	{
		if (count < _size)
		{
			erase(count, _size - count);
		}
		else if (count > _size)
		{
			reserve(count);
			for (size_t i = _size; i < count; ++i)
			{
				AllocTraits::construct(_allocator, _data + i, value);
			}
		}

		_size = count;
	}

	void swap(Vector& other)
	{
		std::swap(_data, other._data);
		std::swap(_size, other._size);
		std::swap(_capacity, other._capacity);
		std::swap(_allocator, other._allocator);
	}

public:
	bool operator==(const Vector& other) const
	{
		if (_size != other._size)
		{
			return false;
		}

		return std::equal(_data, _data + _size, other._data);
	}

	bool operator!=(const Vector& other) const
	{
		return !(*this == other);
	}

private:
	void reallocate(size_t new_cap)
	{
		T* new_data = AllocTraits::allocate(_allocator, new_cap);

		size_t new_size = std::min(_size, new_cap);
		for (size_t i = 0; i < new_size; i++)
		{
			AllocTraits::construct(_allocator, new_data + i, std::move(_data[i]));
		}

		destroy_elements();
		AllocTraits::deallocate(_allocator, _data, _capacity);

		_data = new_data;
		_size = new_size;
		_capacity = new_cap;
	}

	void destroy_elements()
	{
		for (size_t i = 0; i < _size; i++)
		{
			AllocTraits::destroy(_allocator, _data + i);
		}
	}

private:
	T* _data;
	size_t _size;
	size_t _capacity;
	std::allocator<T> _allocator;
};
