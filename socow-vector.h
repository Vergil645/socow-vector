#pragma once
#include <cstddef>

template <typename T, size_t SMALL_SIZE>
struct socow_vector {
    using iterator = T*;
    using const_iterator = T const*;
    using small_array =
        std::array<std::aligned_storage_t<sizeof(T), alignof(T)>, SMALL_SIZE>;

    socow_vector() : _stat_buffer_1() {}

    socow_vector(socow_vector const& other) : socow_vector() {
        if (other._is_small) {
            _stat_buffer_1 = other._stat_buffer_1;
        } else {
            _is_small = false;
            _dyn_buffer_1 = other._dyn_buffer_1;
            _dyn_buffer_1->_copy_count++;
        }
    }

    socow_vector& operator=(socow_vector const& other) {
        socow_vector(other).swap(*this);
        return *this;
    }

    ~socow_vector() {
        if (!_is_small) {
            if (_dyn_buffer_1->_copy_count == 1) {
                delete _dyn_buffer_1;
            } else {
                _dyn_buffer_1->_copy_count--;
            }
        }
    }

    T& operator[](size_t i) {
        if (!_is_small && _dyn_buffer_1->_copy_count != 1) {
            change_capacity(capacity());
        }
        return _is_small ? *reinterpret_cast<T*>(&_stat_buffer_1._data[i])
                         : _dyn_buffer_1->_data[i];
    }

    T const& operator[](size_t i) const {
        return _is_small ? *reinterpret_cast<T const*>(&_stat_buffer_1._data[i])
                         : _dyn_buffer_1->_data[i];
    }

    T* data() {
        if (!_is_small && _dyn_buffer_1->_copy_count != 1) {
            change_capacity(capacity());
        }
        return _is_small ? reinterpret_cast<T*>(&_stat_buffer_1._data[0])
                         : _dyn_buffer_1->_data;
    }

    T const* data() const {
        return _is_small ? reinterpret_cast<T const*>(&_stat_buffer_1._data[0])
                         : _dyn_buffer_1->_data;
    }

    size_t size() const {
        return _is_small ? _stat_buffer_1._size : _dyn_buffer_1->_size;
    }

    T& front() {
        // more easy?
        if (!_is_small && _dyn_buffer_1->_copy_count != 1) {
            change_capacity(capacity());
        }
        return _is_small ? *reinterpret_cast<T*>(&_stat_buffer_1._data[0])
                         : _dyn_buffer_1->_data[0];
    }

    T const& front() const {
        // more easy?
        return _is_small ? *reinterpret_cast<T const*>(&_stat_buffer_1._data[0])
                         : _dyn_buffer_1->_data[0];
    }

    T& back() {
        if (!_is_small && _dyn_buffer_1->_copy_count != 1) {
            change_capacity(capacity());
        }
        return _is_small
                 ? *reinterpret_cast<T*>(&_stat_buffer_1._data[size() - 1])
                 : _dyn_buffer_1->_data[size() - 1];
    }

    T const& back() const {
        return _is_small ? *reinterpret_cast<T const*>(
                               &_stat_buffer_1._data[size() - 1])
                         : _dyn_buffer_1->_data[size() - 1];
    }

    void push_back(T const& value) {
        if (size() == capacity()) {
            size_t val_pos = &value >= get_begin()
                               ? &value - get_begin()
                               : std::numeric_limits<size_t>::max();
            change_capacity(capacity() * 2);
            new (get_end())
                T(val_pos < size() ? _dyn_buffer_1->_data[val_pos] : value);
            // more easy?
            _dyn_buffer_1->_size++;
        } else if (_is_small) {
            new (&_stat_buffer_1._data[size()]) T(value);
            _stat_buffer_1._size++;
        } else {
            size_t val_pos = &value >= get_begin()
                               ? &value - get_begin()
                               : std::numeric_limits<size_t>::max();
            if (_dyn_buffer_1->_copy_count != 1) {
                change_capacity(capacity());
            }
            new (get_end())
                T(val_pos < size() ? _dyn_buffer_1->_data[val_pos] : value);
            _dyn_buffer_1->_size++;
        }
    }

    void pop_back() {
        if (!_is_small && _dyn_buffer_1->_copy_count != 1) {
            change_capacity(capacity());
        }
        // more easy?
        if (_is_small) {
            _stat_buffer_1._size--;
            reinterpret_cast<T*>(&_stat_buffer_1._data[size()])->~T();
        } else {
            _dyn_buffer_1->_size--;
            _dyn_buffer_1->_data[size()].~T();
        }
    }

    bool empty() const {
        return size() == 0;
    }

    size_t capacity() const {
        return _is_small ? SMALL_SIZE : _dyn_buffer_1->_capacity;
    }

    void reserve(size_t new_cap) {
        if (capacity() < new_cap) {
            change_capacity(new_cap);
        } else if (!_is_small && _dyn_buffer_1->_copy_count != 1) {
            // ?!
            change_capacity(capacity());
        }
    }

    void shrink_to_fit() {
        if (size() != capacity()) {
            change_capacity(size());
        }
    }

    void clear() {
        if (_is_small) {
            _stat_buffer_1.clear_prefix(size());
            _stat_buffer_1._size = 0;
        } else {
            if (_dyn_buffer_1->_copy_count != 1) {
                change_capacity(capacity());
            }
            _dyn_buffer_1->clear_prefix(size());
            _dyn_buffer_1->_size = 0;
        }
    }

    void swap(socow_vector& other) {
        if (_is_small) {
            static_storage tmp = _stat_buffer_1;
            if (other._is_small) {
                // more easy?
                _stat_buffer_1 = other._stat_buffer_1;
                other._stat_buffer_1 = tmp;
            } else {
                _dyn_buffer_1 = other._dyn_buffer_1;
                other._stat_buffer_1 = tmp;
                std::swap(_is_small, other._is_small);
            }
        } else {
            if (other._is_small) {
                static_storage tmp = other._stat_buffer_1;
                other._dyn_buffer_1 = _dyn_buffer_1;
                _stat_buffer_1 = tmp;
                std::swap(_is_small, other._is_small);
            } else {
                std::swap(_dyn_buffer_1, other._dyn_buffer_1);
            }
        }
    }

    iterator begin() {
        if (!_is_small && _dyn_buffer_1->_copy_count != 1) {
            change_capacity(capacity());
        }
        return _is_small ? reinterpret_cast<T*>(&_stat_buffer_1._data[0])
                         : _dyn_buffer_1->_data;
    }

    iterator end() {
        return begin() + size();
    }

    const_iterator begin() const {
        return _is_small ? reinterpret_cast<T const*>(&_stat_buffer_1._data[0])
                         : _dyn_buffer_1->_data;
    }

    const_iterator end() const {
        return _is_small
                 ? reinterpret_cast<T const*>(&_stat_buffer_1._data[0]) + size()
                 : _dyn_buffer_1->_data + size();
    }

    iterator insert(const_iterator pos, T const& value) {
        size_t pos_index = pos - get_begin();
        push_back(value);
        if (_is_small) {
            for (size_t i = size() - 1; i != pos_index; i--) {
                std::swap(_stat_buffer_1._data[i - 1], _stat_buffer_1._data[i]);
            }
        } else {
            for (size_t i = size() - 1; i != pos_index; i--) {
                std::swap(_dyn_buffer_1->_data[i - 1], _dyn_buffer_1->_data[i]);
            }
        }
        return get_begin() + pos_index;
    }

    iterator erase(const_iterator pos) {
        return pos == get_end() ? end() : erase(pos, pos + 1);
    }

    iterator erase(const_iterator first, const_iterator last) {
        size_t first_index = first - get_begin();
        size_t count = last - first;
        if (!_is_small && _dyn_buffer_1->_copy_count != 1) {
            change_capacity(capacity());
        }
        if (count == 0) {
            return const_cast<iterator>(get_begin() + first_index);
        } else {
            if (_is_small) {
                for (size_t i = first_index; i != size() - count; i++) {
                    std::swap(_stat_buffer_1._data[i],
                              _stat_buffer_1._data[i + count]);
                }
            } else {
                for (size_t i = first_index; i != size() - count; i++) {
                    std::swap(_dyn_buffer_1->_data[i],
                              _dyn_buffer_1->_data[i + count]);
                }
            }
            for (size_t i = 0; i != count; i++) {
                pop_back();
            }
            return get_begin() + first_index;
        }
    }

private:
    struct dynamic_storage;

    struct static_storage {
        static_storage() = default;

        static_storage(static_storage const& other) : _size(other._size) {
            for (size_t i = 0; i != _size; i++) {
                try {
                    new (&_data[i]) T(*reinterpret_cast<T const*>(&other._data[i]));
                } catch (...) {
                    clear_prefix(i);
                    throw;
                }
            }
        }

        explicit static_storage(dynamic_storage const& other)
            : _size(other._size) {
            for (size_t i = 0; i != _size; i++) {
                try {
                    new (&_data[i]) T(other._data[i]);
                } catch (...) {
                    clear_prefix(i);
                    throw;
                }
            }
        }

        static_storage& operator=(static_storage const& other) {
            clear_prefix(_size);
            _size = other._size;
            for (size_t i = 0; i != _size; i++) {
                try {
                    new (&_data[i]) T(*reinterpret_cast<T const*>(&other._data[i]));
                } catch (...) {
                    clear_prefix(i);
                    throw;
                }
            }
            return *this;
        }

        ~static_storage() {
            clear_prefix(_size);
        }

        size_t _size{0};
        small_array _data;

        void clear_prefix(size_t prefix_size) {
            for (size_t i = 0; i != prefix_size; i++) {
                reinterpret_cast<T*>(&_data[i])->~T();
            }
        }
    };

    struct dynamic_storage {
        dynamic_storage() = default;

        dynamic_storage(dynamic_storage const& other)
            : _size(other._size), _capacity(other._capacity),
              _copy_count(other._copy_count),
              _data(static_cast<T*>(operator new(_capacity * sizeof(T)))) {
            for (size_t i = 0; i != _size; i++) {
                try {
                    new (_data + i) T(other._data[i]);
                } catch (...) {
                    clear_prefix(i);
                    delete _data;
                    throw;
                }
            }
        }

        explicit dynamic_storage(size_t cap)
            : _capacity(cap),
              _data(static_cast<T*>(operator new(_capacity * sizeof(T)))) {}

        ~dynamic_storage() {
            clear_prefix(_size);
            operator delete(_data);
        }

        size_t _size{0};
        size_t _capacity{0};
        size_t _copy_count{1};
        T* _data{nullptr};

        void clear_prefix(size_t prefix_size) {
            for (size_t i = 0; i != prefix_size; i++) {
                _data[i].~T();
            }
        }
    };

    bool _is_small{true};
    union {
        static_storage _stat_buffer_1;
        dynamic_storage* _dyn_buffer_1;
    };

    T& get_elem(size_t i) {
        return _is_small ? *reinterpret_cast<T*>(&_stat_buffer_1._data[i])
                         : _dyn_buffer_1->_data[i];
    }

    iterator get_begin() {
        return _is_small ? reinterpret_cast<T*>(&_stat_buffer_1._data[0])
                         : _dyn_buffer_1->_data;
    }

    iterator get_end() {
        return get_begin() + size();
    }

    void change_capacity(size_t new_cap) {
        if (_is_small) {
            if (new_cap <= SMALL_SIZE) {
                return;
            } else {
                auto* new_dyn_buffer = new dynamic_storage(new_cap);
                for (size_t i = 0; i != size(); i++) {
                    try {
                        new (new_dyn_buffer->_data + i) T(get_elem(i));
                        new_dyn_buffer->_size++;
                    } catch (...) {
                        delete new_dyn_buffer;
                        throw;
                    }
                }
                _stat_buffer_1.~static_storage();
                _dyn_buffer_1 = new_dyn_buffer;
                _is_small = false;
            }
        } else {
            if (new_cap <= SMALL_SIZE) {
                static_storage new_stat_buffer;
                for (size_t i = 0; i != size(); i++) {
                    try {
                        new (&new_stat_buffer._data[i]) T(get_elem(i));
                        new_stat_buffer._size++;
                    } catch (...) {
                        throw;
                    }
                }
                if (_dyn_buffer_1->_copy_count == 1) {
                    delete _dyn_buffer_1;
                } else {
                    _dyn_buffer_1->_copy_count--;
                }
                _stat_buffer_1 = new_stat_buffer;
                _is_small = true;
            } else {
                auto* new_dyn_buffer = new dynamic_storage(new_cap);
                for (size_t i = 0; i != size(); i++) {
                    try {
                        new (new_dyn_buffer->_data + i) T(get_elem(i));
                        new_dyn_buffer->_size++;
                    } catch (...) {
                        delete new_dyn_buffer;
                        throw;
                    }
                }
                if (_dyn_buffer_1->_copy_count == 1) {
                    delete _dyn_buffer_1;
                } else {
                    _dyn_buffer_1->_copy_count--;
                }
                _dyn_buffer_1 = new_dyn_buffer;
            }
        }
    }
};
