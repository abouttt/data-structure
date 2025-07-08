#pragma once

#include <algorithm>
#include <cassert>
#include <initializer_list>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace abouttt
{
	template<typename T, typename Allocator = std::allocator<T>>
	class Vector
	{
	private:
		using AllocTraits = std::allocator_traits<Allocator>;

	public:
		Vector(const Allocator& alloc = Allocator())
			: _data(nullptr)
			, _size(0)
			, _capacity(0)
			, _allocator(alloc)
		{
		}

		Vector(size_t count, const T& value = T(), const Allocator& alloc = Allocator())
			: _size(count)
			, _capacity(count)
			, _allocator(alloc)
		{
			_data = AllocTraits::allocate(_allocator, count);
			for (size_t i = 0; i < count; ++i)
			{
				AllocTraits::construct(_allocator, _data + i, value);
			}
		}

		Vector(std::initializer_list<T> init, const Allocator& alloc = Allocator())
			: _size(init.size())
			, _capacity(init.size())
			, _allocator(alloc)
		{
			_data = AllocTraits::allocate(_allocator, init.size());
			size_t i = 0;
			for (const auto& item : init)
			{
				AllocTraits::construct(_allocator, _data + i++, item);
			}
		}

		Vector(const Vector& other)
			: _size(other._size)
			, _capacity(other._capacity)
			, _allocator(AllocTraits::select_on_container_copy_construction(other._allocator))
		{
			_data = AllocTraits::allocate(_allocator, other._capacity);
			for (size_t i = 0; i < _size; ++i)
			{
				AllocTraits::construct(_allocator, _data + i, other._data[i]);
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
			clear();
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
				clear();
				AllocTraits::deallocate(_allocator, _data, _capacity);

				_data = other._data;
				_size = other._size;
				_capacity = other._capacity;
				_allocator = std::move(other._allocator);

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
			reserve(count);

			for (size_t i = 0; i < count; ++i)
			{
				emplace_back(value);
			}
		}

		void assign(std::initializer_list<T> ilist)
		{
			clear();
			reserve(ilist.size());

			for (const auto& item : ilist)
			{
				emplace_back(item);
			}
		}

		Allocator get_allocator() const
		{
			return _allocator;
		}

	public: // Element access
		T& at(size_t pos)
		{
			if (pos >= _size)
			{
				throw std::out_of_range("invalid vector subscript");
			}

			return _data[pos];
		}

		const T& at(size_t pos) const
		{
			if (pos >= _size)
			{
				throw std::out_of_range("invalid vector subscript");
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

		size_t max_size() const
		{
			return AllocTraits::max_size(_allocator);
		}

		void reserve(size_t new_cap)
		{
			if (new_cap <= _capacity)
			{
				return;
			}

			T* new_data = AllocTraits::allocate(_allocator, new_cap);

			for (size_t i = 0; i < _size; ++i)
			{
				AllocTraits::construct(_allocator, new_data + i, std::move(_data[i]));
				AllocTraits::destroy(_allocator, _data + i);
			}

			AllocTraits::deallocate(_allocator, _data, _capacity);
			_data = new_data;
			_capacity = new_cap;
		}

		size_t capacity() const
		{
			return _capacity;
		}

		void shrink_to_fit()
		{
			if (_size == _capacity)
			{
				return;
			}

			if (_size == 0)
			{
				AllocTraits::deallocate(_allocator, _data, _capacity);
				_data = nullptr;
				_capacity = 0;
				return;
			}

			T* new_data = AllocTraits::allocate(_allocator, _size);

			for (size_t i = 0; i < _size; ++i)
			{
				AllocTraits::construct(_allocator, new_data + i, std::move(_data[i]));
				AllocTraits::destroy(_allocator, _data + i);
			}

			AllocTraits::deallocate(_allocator, _data, _capacity);
			_data = new_data;
			_capacity = _size;
		}

	public: // Modifiers
		void clear()
		{
			for (size_t i = 0; i < _size; ++i)
			{
				AllocTraits::destroy(_allocator, _data + i);
			}

			_size = 0;
		}

		void insert(size_t pos, const T& value)
		{
			if (pos > _size)
			{
				throw std::out_of_range("invalid vector subscript");
			}

			check_capacity();

			for (size_t i = _size; i-- > pos;)
			{
				T temp = std::move(_data[i]);
				AllocTraits::destroy(_allocator, _data + i);
				AllocTraits::construct(_allocator, _data + i + 1, std::move(temp));
			}

			AllocTraits::construct(_allocator, _data + pos, value);
			++_size;
		}

		void insert(size_t pos, T&& value)
		{
			if (pos > _size)
			{
				throw std::out_of_range("invalid vector subscript");
			}

			check_capacity();

			for (size_t i = _size; i-- > pos;)
			{
				T temp = std::move(_data[i]);
				AllocTraits::destroy(_allocator, _data + i);
				AllocTraits::construct(_allocator, _data + i + 1, std::move(temp));
			}

			AllocTraits::construct(_allocator, _data + pos, std::move(value));
			++_size;
		}

		void insert(size_t pos, size_t count, const T& value)
		{
			if (pos > _size)
			{
				throw std::out_of_range("invalid vector subscript");
			}

			if (count == 0)
			{
				return;
			}

			check_capacity(count);

			for (size_t i = _size + count; i-- > pos + count;)
			{
				AllocTraits::construct(_allocator, _data + i, std::move(_data[i - count]));
				AllocTraits::destroy(_allocator, _data + i - count);
			}

			for (size_t i = pos; i < pos + count; ++i)
			{
				AllocTraits::construct(_allocator, _data + i, value);
			}

			_size += count;
		}

		void insert(size_t pos, std::initializer_list<T> ilist)
		{
			if (pos > _size)
			{
				throw std::out_of_range("invalid vector subscript");
			}

			size_t count = ilist.size();
			check_capacity(count);

			for (size_t i = _size + count; i-- > pos + count;)
			{
				T temp = std::move(_data[i - count]);
				AllocTraits::destroy(_allocator, _data + i - count);
				AllocTraits::construct(_allocator, _data + i, std::move(temp));
			}

			size_t i = pos;
			for (const auto& item : ilist)
			{
				AllocTraits::construct(_allocator, _data + i++, item);
			}

			_size += count;
		}

		template<typename... Args>
		T& emplace(size_t pos, Args&&... args)
		{
			if (pos > _size)
			{
				throw std::out_of_range("invalid vector subscript");
			}

			check_capacity();

			for (size_t i = _size; i > pos; --i)
			{
				AllocTraits::construct(_allocator, _data + i, std::move(_data[i - 1]));
				AllocTraits::destroy(_allocator, _data + i - 1);
			}

			AllocTraits::construct(_allocator, _data + pos, std::forward<Args>(args)...);
			++_size;
			return _data[pos];
		}

		void erase(size_t pos)
		{
			if (pos >= _size)
			{
				throw std::out_of_range("invalid vector subscript");
			}

			AllocTraits::destroy(_allocator, _data + pos);

			for (size_t i = pos; i < _size - 1; ++i)
			{
				AllocTraits::construct(_allocator, _data + i, std::move(_data[i + 1]));
				AllocTraits::destroy(_allocator, _data + i + 1);
			}

			--_size;
		}

		void erase(size_t first, size_t last)
		{
			if (first > last || last > _size)
			{
				throw std::out_of_range("invalid vector subscript");
			}

			for (size_t i = first; i < last; ++i)
			{
				AllocTraits::destroy(_allocator, _data + i);
			}

			size_t count = last - first;
			for (size_t i = first; i < _size - count; ++i)
			{
				AllocTraits::construct(_allocator, _data + i, std::move(_data[i + count]));
				AllocTraits::destroy(_allocator, _data + i + count);
			}

			_size -= count;
		}

		void push_back(const T& value)
		{
			check_capacity();
			AllocTraits::construct(_allocator, _data + _size, value);
			++_size;
		}

		void push_back(T&& value)
		{
			check_capacity();
			AllocTraits::construct(_allocator, _data + _size, std::move(value));
			++_size;
		}

		template<typename... Args>
		T& emplace_back(Args&&... args)
		{
			check_capacity();
			AllocTraits::construct(_allocator, _data + _size, std::forward<Args>(args)...);
			++_size;
			return _data[_size - 1];
		}

		void pop_back()
		{
			if (_size == 0)
			{
				return;
			}

			AllocTraits::destroy(_allocator, _data + _size - 1);
			--_size;
		}

		void resize(size_t count)
		{
			if (count < _size)
			{
				for (size_t i = count; i < _size; ++i)
				{
					AllocTraits::destroy(_allocator, _data + i);
				}
			}
			else if (count > _size)
			{
				reserve(count);
				for (size_t i = _size; i < count; ++i)
				{
					AllocTraits::construct(_allocator, _data + i, T());
				}
			}

			_size = count;
		}

		void resize(size_t count, const T& value)
		{
			if (count < _size)
			{
				for (size_t i = count; i < _size; ++i)
				{
					AllocTraits::destroy(_allocator, _data + i);
				}
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
			if (AllocTraits::propagate_on_container_swap::value)
			{
				std::swap(_allocator, other._allocator);
			}
		}

	public:
		bool operator==(const Vector& other) const
		{
			if (_size != other._size)
			{
				return false;
			}

			for (size_t i = 0; i < _size; ++i)
			{
				if (_data[i] != other._data[i])
				{
					return false;
				}
			}

			return true;
		}

		bool operator!=(const Vector& other) const
		{
			return !(*this == other);
		}

	private:
		void check_capacity(size_t count = 0)
		{
			if (count != 0)
			{
				if (_size + count > _capacity)
				{
					size_t new_cap = std::max(_size + count, _capacity * 2);
					reserve(new_cap);
				}
			}
			else
			{
				if (_size == _capacity)
				{
					size_t new_cap = _capacity == 0 ? 1 : _capacity * 2;
					reserve(new_cap);
				}
			}
		}

	private:
		T* _data;
		size_t _size;
		size_t _capacity;
		Allocator _allocator;
	};
} // namespace abouttt
