#pragma once
#include <cstddef>

template <typename T, size_t SMALL_SIZE>
struct socow_vector {
    using iterator = T*;
    using const_iterator = T const*;

    socow_vector() : _stat_buffer() {}

    socow_vector(socow_vector const& other) : socow_vector() {
        _size = other._size;
        if (other._is_small) {
            _stat_buffer = other._stat_buffer;
        } else {
            _is_small = false;
            // destruct(_stat_buffer);
            _dyn_buffer = other._dyn_buffer;
            _dyn_buffer->_copy_count++;
        }
    }

    socow_vector& operator=(socow_vector const& other) {
        socow_vector(other).swap(*this);
        return *this;
    }

    ~socow_vector() {
        if (_is_small) {
            clear_prefix(_stat_buffer, _size);
        } else {
            if (_dyn_buffer->_copy_count == 1) {
                clear_prefix(*_dyn_buffer, _size);
                delete _dyn_buffer;
            } else {
                _dyn_buffer->_copy_count--;
            }
        }
    }

    T& operator[](size_t i) {
        if (!_is_small && _dyn_buffer->_copy_count != 1) {
            change_capacity(capacity());
        }
        return _is_small ? *reinterpret_cast<T*>(&_stat_buffer[i])
                         : _dyn_buffer->_data[i];
    }

    T const& operator[](size_t i) const {
        return _is_small ? *reinterpret_cast<T const*>(&_stat_buffer[i])
                         : _dyn_buffer->_data[i];
    }

    T* data() {
        if (!_is_small && _dyn_buffer->_copy_count != 1) {
            change_capacity(capacity());
        }
        return _is_small ? reinterpret_cast<T*>(&_stat_buffer[0])
                         : _dyn_buffer->_data;
    }

    T const* data() const {
        return _is_small ? reinterpret_cast<T const*>(&_stat_buffer[0])
                         : _dyn_buffer->_data;
    }

    size_t size() const {
        return _size;
    }

    T& front() {
        if (!_is_small && _dyn_buffer->_copy_count != 1) {
            change_capacity(capacity());
        }
        return _is_small ? *reinterpret_cast<T*>(&_stat_buffer[0])
                         : _dyn_buffer->_data[0];
    }

    T const& front() const {
        return _is_small ? *reinterpret_cast<T const*>(&_stat_buffer[0])
                         : _dyn_buffer->_data[0];
    }

    T& back() {
        if (!_is_small && _dyn_buffer->_copy_count != 1) {
            change_capacity(capacity());
        }
        return _is_small ? *reinterpret_cast<T*>(&_stat_buffer[_size - 1])
                         : _dyn_buffer->_data[_size - 1];
    }

    T const& back() const {
        return _is_small ? *reinterpret_cast<T const*>(&_stat_buffer[_size - 1])
                         : _dyn_buffer->_data[_size - 1];
    }

    void push_back(T const& value) {
        if (_size == capacity()) {
            size_t val_pos = &value >= get_begin()
                               ? &value - get_begin()
                               : std::numeric_limits<size_t>::max();
            change_capacity(capacity() * 2);
            new (get_end())
                T(val_pos < _size ? _dyn_buffer->_data[val_pos] : value);
        } else if (_is_small) {
            new (&_stat_buffer[_size]) T(value);
        } else {
            size_t val_pos = &value >= get_begin()
                               ? &value - get_begin()
                               : std::numeric_limits<size_t>::max();
            if (_dyn_buffer->_copy_count != 1) {
                change_capacity(capacity());
            }
            new (get_end())
                T(val_pos < _size ? _dyn_buffer->_data[val_pos] : value);
        }
        _size++;
    }

    void pop_back() {
        if (!_is_small && _dyn_buffer->_copy_count != 1) {
            change_capacity(capacity());
        }
        _size--;
        if (_is_small) {
            reinterpret_cast<T*>(&_stat_buffer[_size])->~T();
        } else {
            _dyn_buffer->_data[_size].~T();
        }
    }

    bool empty() const {
        return _size == 0;
    }

    size_t capacity() const {
        return _is_small ? SMALL_SIZE : _dyn_buffer->_capacity;
    }

    void reserve(size_t new_cap) {
        if (capacity() < new_cap) {
            change_capacity(new_cap);
        } else if (!_is_small && _dyn_buffer->_copy_count != 1) {
            change_capacity(capacity());
        }
    }

    void shrink_to_fit() {
        if (_size != capacity()) {
            change_capacity(_size);
        }
    }

    void clear() {
        if (_is_small) {
            clear_prefix(_stat_buffer, _size);
        } else {
            if (_dyn_buffer->_copy_count != 1) {
                change_capacity(capacity());
            }
            clear_prefix(*_dyn_buffer, _size);
        }
        _size = 0;
    }

    void swap(socow_vector& other) {
        if (_is_small) {
            std::array<std::aligned_storage_t<sizeof(T), alignof(T)>,
                       SMALL_SIZE>
                tmp;
            for (size_t i = 0; i != _size; i++) {
                try {
                    new (&tmp[i]) T(*reinterpret_cast<T*>(&_stat_buffer[i]));
                } catch (...) {
                    clear_prefix(tmp, i);
                    throw;
                }
            }
            clear_prefix(_stat_buffer, _size);
            if (other._is_small) {
                for (size_t i = 0; i != other._size; i++) {
                    try {
                        new (&_stat_buffer[i])
                            T(*reinterpret_cast<T*>(&other._stat_buffer[i]));
                    } catch (...) {
                        clear_prefix(tmp, _size);
                        throw;
                    }
                }
                clear_prefix(other._stat_buffer, other._size);
                for (size_t i = 0; i != _size; i++) {
                    try {
                        new (&other._stat_buffer[i])
                            T(*reinterpret_cast<T*>(&tmp[i]));
                    } catch (...) {
                        clear_prefix(tmp, _size);
                        throw;
                    }
                }
                clear_prefix(tmp, _size);
                std::swap(_size, other._size);
            } else {
                _dyn_buffer = other._dyn_buffer;
                for (size_t i = 0; i != _size; i++) {
                    try {
                        new (&other._stat_buffer[i])
                            T(*reinterpret_cast<T*>(&tmp[i]));
                    } catch (...) {
                        clear_prefix(tmp, _size);
                        throw;
                    }
                }
                clear_prefix(tmp, _size);
                std::swap(_size, other._size);
                std::swap(_is_small, other._is_small);
            }
        } else {
            if (other._is_small) {
                std::array<std::aligned_storage_t<sizeof(T), alignof(T)>,
                           SMALL_SIZE>
                    tmp;
                for (size_t i = 0; i != other._size; i++) {
                    try {
                        new (&tmp[i])
                            T(*reinterpret_cast<T*>(&other._stat_buffer[i]));
                    } catch (...) {
                        clear_prefix(tmp, i);
                        throw;
                    }
                }
                clear_prefix(other._stat_buffer, other._size);
                other._dyn_buffer = _dyn_buffer;
                for (size_t i = 0; i != other._size; i++) {
                    try {
                        new (&_stat_buffer[i])
                            T(*reinterpret_cast<T*>(&tmp[i]));
                    } catch (...) {
                        clear_prefix(tmp, other._size);
                        throw;
                    }
                }
                clear_prefix(tmp, other._size);
                std::swap(_size, other._size);
                std::swap(_is_small, other._is_small);
            } else {
                std::swap(_size, other._size);
                std::swap(_dyn_buffer, other._dyn_buffer);
            }
        }
    }

    iterator begin() {
        if (!_is_small && _dyn_buffer->_copy_count != 1) {
            change_capacity(capacity());
        }
        return _is_small ? reinterpret_cast<T*>(&_stat_buffer[0])
                         : _dyn_buffer->_data;
    }

    iterator end() {
        return begin() + _size;
    }

    const_iterator begin() const {
        return _is_small ? reinterpret_cast<T const*>(&_stat_buffer[0])
                         : _dyn_buffer->_data;
    }

    const_iterator end() const {
        return _is_small ? reinterpret_cast<T const*>(&_stat_buffer[0]) + _size
                         : _dyn_buffer->_data + _size;
    }

    iterator insert(const_iterator pos, T const& value) {
        size_t pos_index = pos - get_begin();
        push_back(value);
        if (_is_small) {
            for (size_t i = _size - 1; i != pos_index; i--) {
                std::swap(_stat_buffer[i - 1], _stat_buffer[i]);
            }
        } else {
            for (size_t i = _size - 1; i != pos_index; i--) {
                std::swap(_dyn_buffer->_data[i - 1], _dyn_buffer->_data[i]);
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
        if (!_is_small && _dyn_buffer->_copy_count != 1) {
            change_capacity(capacity());
        }
        if (count == 0) {
            return const_cast<iterator>(get_begin() + first_index);
        } else {
            if (_is_small) {
                for (size_t i = first_index; i != _size - count; i++) {
                    std::swap(_stat_buffer[i], _stat_buffer[i + count]);
                }
            } else {
                for (size_t i = first_index; i != _size - count; i++) {
                    std::swap(_dyn_buffer->_data[i],
                              _dyn_buffer->_data[i + count]);
                }
            }
            for (size_t i = 0; i != count; i++) {
                pop_back();
            }
            return get_begin() + first_index;
        }
    }

private:
    struct dynamic_storage {
        dynamic_storage() : _data(nullptr) {}

        explicit dynamic_storage(size_t cap) : dynamic_storage() {
            _capacity = cap;
            _data = static_cast<T*>(operator new(cap * sizeof(T)));
        }

        ~dynamic_storage() {
            operator delete(_data);
        }

        size_t _capacity{0};
        uint32_t _copy_count{0};
        T* _data;
    };

    bool _is_small{true};
    size_t _size{0};
    union {
        std::array<std::aligned_storage_t<sizeof(T), alignof(T)>, SMALL_SIZE>
            _stat_buffer;
        dynamic_storage* _dyn_buffer;
    };

    T& get_elem(size_t i) {
        return _is_small ? *reinterpret_cast<T*>(&_stat_buffer[i])
                         : _dyn_buffer->_data[i];
    }

    iterator get_begin() {
        return _is_small ? reinterpret_cast<T*>(&_stat_buffer[0])
                         : _dyn_buffer->_data;
    }

    iterator get_end() {
        return get_begin() + _size;
    }

    template <typename S>
    static void destruct(S& obj) {
        obj.~S();
    }

    static void
    clear_prefix(std::array<std::aligned_storage_t<sizeof(T), alignof(T)>,
                            SMALL_SIZE>& data,
                 size_t prefix_size) {
        for (size_t i = 0; i != prefix_size; i++) {
            reinterpret_cast<T*>(&data[i])->~T();
        }
    }

    static void clear_prefix(dynamic_storage& data, size_t prefix_size) {
        for (size_t i = 0; i != prefix_size; i++) {
            data._data[i].~T();
        }
    }

    void change_capacity(size_t new_cap) {
        if (_is_small) {
            if (new_cap <= SMALL_SIZE) {
                return;
            } else {
                auto* new_dyn_buffer = new dynamic_storage(new_cap);
                for (size_t i = 0; i != _size; i++) {
                    try {
                        new (new_dyn_buffer->_data + i) T(get_elem(i));
                    } catch (...) {
                        clear_prefix(*new_dyn_buffer, i);
                        delete new_dyn_buffer;
                        throw;
                    }
                }
                clear_prefix(_stat_buffer, _size);
                _dyn_buffer = new_dyn_buffer;
                _dyn_buffer->_copy_count++;
                _is_small = false;
            }
        } else {
            if (new_cap <= SMALL_SIZE) {
                std::array<std::aligned_storage_t<sizeof(T), alignof(T)>,
                           SMALL_SIZE>
                    new_stat_buffer;
                for (size_t i = 0; i != _size; i++) {
                    try {
                        new (&new_stat_buffer[i]) T(get_elem(i));
                    } catch (...) {
                        clear_prefix(new_stat_buffer, i);
                        throw;
                    }
                }
                if (_dyn_buffer->_copy_count == 1) {
                    clear_prefix(*_dyn_buffer, _size);
                    delete _dyn_buffer;
                } else {
                    _dyn_buffer->_copy_count--;
                }
                for (size_t i = 0; i != _size; i++) {
                    try {
                        new (&_stat_buffer[i])
                            T(*reinterpret_cast<T*>(&new_stat_buffer[i]));
                    } catch (...) {
                        clear_prefix(_stat_buffer, i);
                        clear_prefix(new_stat_buffer, _size);
                        throw;
                    }
                }
                clear_prefix(new_stat_buffer, _size);
                _is_small = true;
            } else {
                auto* new_dyn_buffer = new dynamic_storage(new_cap);
                for (size_t i = 0; i != _size; i++) {
                    try {
                        new (new_dyn_buffer->_data + i) T(get_elem(i));
                    } catch (...) {
                        clear_prefix(*new_dyn_buffer, i);
                        delete new_dyn_buffer;
                        throw;
                    }
                }
                if (_dyn_buffer->_copy_count == 1) {
                    clear_prefix(*_dyn_buffer, _size);
                    delete _dyn_buffer;
                } else {
                    _dyn_buffer->_copy_count--;
                }
                _dyn_buffer = new_dyn_buffer;
                _dyn_buffer->_copy_count++;
            }
        }
    }
};
