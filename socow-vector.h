#pragma once
#include <array>
#include <cstddef>
#include <type_traits>

template <typename T, size_t SMALL_SIZE>
struct socow_vector {
    using iterator = T*;
    using const_iterator = T const*;
    using small_array =
        std::array<std::aligned_storage_t<sizeof(T), alignof(T)>, SMALL_SIZE>;

    socow_vector() : stat_buf_() {}

    socow_vector(socow_vector const& other) : size_(other.size_) {
        if (other.is_static()) {
            new (&stat_buf_) static_storage(other.stat_buf_, other.size());
        } else {
            new (&dyn_buf_) dynamic_storage(other.dyn_buf_);
        }
    }

    socow_vector& operator=(socow_vector const& other) {
        socow_vector(other).swap(*this);
        return *this;
    }

    ~socow_vector() {
        if (is_static()) {
            destruct_stat_buffer(stat_buf_, size());
        } else {
            destruct_dyn_buffer(dyn_buf_, size());
        }
    }

    T& operator[](size_t i) {
        if (is_static()) {
            return *stat_buf_.get_ptr(i);
        } else {
            copy_dyn_buffer();
            return dyn_buf_.data_[i];
        }
    }

    T const& operator[](size_t i) const {
        return is_static() ? *stat_buf_.get_ptr(i) : dyn_buf_.data_[i];
    }

    T* data() {
        if (is_static()) {
            return stat_buf_.get_ptr(0);
        } else {
            copy_dyn_buffer();
            return dyn_buf_.data_;
        }
    }

    T const* data() const {
        return is_static() ? stat_buf_.get_ptr(0) : dyn_buf_.data_;
    }

    size_t size() const {
        return size_ >> 1;
    }

    T& front() {
        return *begin();
    }

    T const& front() const {
        return *begin();
    }

    T& back() {
        return *(end() - 1);
    }

    T const& back() const {
        return *(end() - 1);
    }

    void push_back(T const& value) {
        size_t val_pos = &value >= get_begin()
                           ? &value - get_begin()
                           : std::numeric_limits<size_t>::max();
        if (size() == capacity()) {
            rebuild_storage(capacity() * 2);
        }
        if (is_static()) {
            new (&stat_buf_.data_[size()]) T(value);
        } else {
            copy_dyn_buffer();
            new (dyn_buf_.data_ + size())
                T(val_pos < size() ? dyn_buf_.data_[val_pos] : value);
        }
        size_ += 2;
    }

    void pop_back() {
        if (is_static()) {
            stat_buf_.get_ptr(size() - 1)->~T();
        } else {
            copy_dyn_buffer();
            dyn_buf_.data_[size() - 1].~T();
        }
        size_ -= 2;
    }

    bool empty() const {
        return size() == 0;
    }

    size_t capacity() const {
        return is_static() ? SMALL_SIZE : dyn_buf_.capacity_;
    }

    void reserve(size_t new_cap) {
        if (capacity() < new_cap) {
            rebuild_storage(new_cap);
        } else if (!is_static()) {
            copy_dyn_buffer();
        }
    }

    void shrink_to_fit() {
        if (size() != capacity()) {
            rebuild_storage(size());
        }
    }

    void clear() {
        if (is_static()) {
            stat_buf_.clear(size());
        } else {
            copy_dyn_buffer();
            dyn_buf_.clear(size());
        }
        size_ = size_ % 2;
    }

    void swap(socow_vector& other) {
        if (is_static() && other.is_static()) {
            static_storage tmp(stat_buf_, size());
            destruct_stat_buffer(stat_buf_, size());
            new (&stat_buf_) static_storage(other.stat_buf_, other.size());
            destruct_stat_buffer(other.stat_buf_, other.size());
            new (&other.stat_buf_) static_storage(tmp, size());
            tmp.clear(size());
        } else if (is_static() && !other.is_static()) {
            swap_stat_dyn(*this, other);
        } else if (other.is_static()) {
            swap_stat_dyn(other, *this);
        } else {
            dyn_buf_.swap(other.dyn_buf_);
        }
        std::swap(size_, other.size_);
    }

    iterator begin() {
        if (is_static()) {
            return stat_buf_.get_ptr(0);
        } else {
            copy_dyn_buffer();
            return dyn_buf_.data_;
        }
    }

    iterator end() {
        return begin() + size();
    }

    const_iterator begin() const {
        return is_static() ? stat_buf_.get_ptr(0) : dyn_buf_.data_;
    }

    const_iterator end() const {
        return begin() + size();
    }

    iterator insert(const_iterator pos, T const& value) {
        size_t pos_index = pos - get_begin();
        push_back(value);
        if (is_static()) {
            for (size_t i = size() - 1; i != pos_index; i--) {
                std::swap(stat_buf_.data_[i - 1], stat_buf_.data_[i]);
            }
        } else {
            for (size_t i = size() - 1; i != pos_index; i--) {
                std::swap(dyn_buf_.data_[i - 1], dyn_buf_.data_[i]);
            }
        }
        return get_begin() + pos_index;
    }

    iterator erase(const_iterator pos) {
        return pos == get_end() ? end() : erase(pos, pos + 1);
    }

    iterator erase(const_iterator first, const_iterator last) {
        size_t first_index = first - get_begin();
        size_t cnt = last - first;
        if (cnt == 0) {
            return begin() + first_index;
        } else {
            if (is_static()) {
                for (size_t i = first_index; i != size() - cnt; i++) {
                    std::swap(stat_buf_.data_[i], stat_buf_.data_[i + cnt]);
                }
            } else {
                copy_dyn_buffer();
                for (size_t i = first_index; i != size() - cnt; i++) {
                    std::swap(dyn_buf_.data_[i], dyn_buf_.data_[i + cnt]);
                }
            }
            for (size_t i = 0; i != cnt; i++) {
                pop_back();
            }
            return begin() + first_index;
        }
    }

private:
    size_t size_{1};

    struct static_storage {
        small_array data_;

        static_storage() = default;

        static_storage(static_storage const& other, size_t cnt)
            : static_storage() {
            for (size_t i = 0; i != cnt; i++) {
                try {
                    new (&data_[i]) T(*other.get_ptr(i));
                } catch (...) {
                    clear(i);
                    throw;
                }
            }
        }

        ~static_storage() = default;

        T* get_ptr(size_t i) {
            return reinterpret_cast<T*>(&data_[i]);
        }

        T const* get_ptr(size_t i) const {
            return reinterpret_cast<T const*>(&data_[i]);
        }

        void clear(size_t cnt) {
            for (size_t i = 0; i != cnt; i++) {
                get_ptr(i)->~T();
            }
        }
    };

    struct dynamic_storage {
        size_t capacity_{0};
        size_t* copy_count_;
        T* data_{nullptr};

        dynamic_storage() : copy_count_(new size_t(1)) {}

        explicit dynamic_storage(size_t cap)
            : capacity_(cap), copy_count_(new size_t(1)),
              data_(static_cast<T*>(operator new(capacity_ * sizeof(T)))) {}

        dynamic_storage(dynamic_storage const& other)
            : capacity_(other.capacity_), copy_count_(other.copy_count_),
              data_(other.data_) {
            ++(*copy_count_);
        }

        ~dynamic_storage() = default;

        void swap(dynamic_storage& other) {
            std::swap(capacity_, other.capacity_);
            std::swap(copy_count_, other.copy_count_);
            std::swap(data_, other.data_);
        }

        void clear(size_t cnt) {
            for (size_t i = 0; i != cnt; i++) {
                data_[i].~T();
            }
        }
    };

    union {
        static_storage stat_buf_;
        dynamic_storage dyn_buf_;
    };

    bool is_static() {
        return size_ % 2 != 0;
    }

    bool is_static() const {
        return size_ % 2 != 0;
    }

    static void clear_dyn_buffer(dynamic_storage& buf, size_t buf_size) {
        if (*buf.copy_count_ == 1) {
            delete buf.copy_count_;
            buf.clear(buf_size);
            operator delete(buf.data_);
        } else {
            --(*buf.copy_count_);
        }
    }

    static void destruct_stat_buffer(static_storage& buf, size_t buf_size) {
        buf.clear(buf_size);
        buf.~static_storage();
    }

    static void destruct_dyn_buffer(dynamic_storage& buf, size_t buf_size) {
        clear_dyn_buffer(buf, buf_size);
        buf.~dynamic_storage();
    }

    void copy_dyn_buffer() {
        if (*dyn_buf_.copy_count_ != 1) {
            rebuild_storage(capacity());
        }
    }

    T& get_elem(size_t i) {
        return is_static() ? *stat_buf_.get_ptr(i) : dyn_buf_.data_[i];
    }

    iterator get_begin() {
        return is_static() ? stat_buf_.get_ptr(0) : dyn_buf_.data_;
    }

    iterator get_end() {
        return get_begin() + size();
    }

    static void swap_stat_dyn(socow_vector& stat, socow_vector& dyn) {
        static_storage tmp(stat.stat_buf_, stat.size());
        destruct_stat_buffer(stat.stat_buf_, stat.size());
        new (&stat.dyn_buf_) dynamic_storage(dyn.dyn_buf_);
        destruct_dyn_buffer(dyn.dyn_buf_, dyn.size());
        new (&dyn.stat_buf_) static_storage(tmp, stat.size());
        tmp.clear(stat.size());
    }

    void rebuild_storage(size_t new_cap) {
        if (new_cap <= SMALL_SIZE && !is_static()) {
            static_storage new_stat_buffer;
            for (size_t i = 0; i != size(); i++) {
                try {
                    new (&new_stat_buffer.data_[i]) T(dyn_buf_.data_[i]);
                } catch (...) {
                    new_stat_buffer.clear(i);
                    throw;
                }
            }
            destruct_dyn_buffer(dyn_buf_, size());
            new (&stat_buf_) static_storage(new_stat_buffer, size());
            ++size_;
            new_stat_buffer.clear(size());
        } else if (new_cap > SMALL_SIZE) {
            dynamic_storage new_dyn_buffer(new_cap);
            for (size_t i = 0; i != size(); i++) {
                try {
                    new (new_dyn_buffer.data_ + i) T(get_elem(i));
                } catch (...) {
                    clear_dyn_buffer(new_dyn_buffer, i);
                    throw;
                }
            }
            if (is_static()) {
                destruct_stat_buffer(stat_buf_, size());
                --size_;
            } else {
                destruct_dyn_buffer(dyn_buf_, size());
            }
            new (&dyn_buf_) dynamic_storage(new_dyn_buffer);
            clear_dyn_buffer(new_dyn_buffer, size());
        }
    }
};
