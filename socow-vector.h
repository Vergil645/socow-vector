#pragma once
#include <cstddef>
#include <type_traits>
#include <array>

template <typename T, size_t SMALL_SIZE>
struct socow_vector {
    using iterator = T*;
    using const_iterator = T const*;

    socow_vector() : stat_buf_() {}

    socow_vector(socow_vector const& other) : socow_vector() {
        if (other.is_static_) {
            stat_buf_ = other.stat_buf_;
        } else {
            is_static_ = false;
            destruct_stat_buffer();
            dyn_buf_ = other.dyn_buf_;
            ++dyn_buf_->copy_count_;
        }
    }

    socow_vector& operator=(socow_vector const& other) {
        socow_vector(other).swap(*this);
        return *this;
    }

    ~socow_vector() {
        if (is_static_) {
            destruct_stat_buffer();
        } else {
            destruct_dyn_buffer();
        }
    }

    T& operator[](size_t i) {
        if (is_static_) {
            return *stat_buf_.get_ptr(i);
        } else {
            copy_dyn_buffer();
            return dyn_buf_->data_[i];
        }
    }

    T const& operator[](size_t i) const {
        return is_static_ ? *stat_buf_.get_ptr(i) : dyn_buf_->data_[i];
    }

    T* data() {
        if (is_static_) {
            return stat_buf_.get_ptr(0);
        } else {
            copy_dyn_buffer();
            return dyn_buf_->data_;
        }
    }

    T const* data() const {
        return is_static_ ? stat_buf_.get_ptr(0) : dyn_buf_->data_;
    }

    size_t size() const {
        return is_static_ ? stat_buf_.size_ : dyn_buf_->size_;
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
        if (is_static_) {
            new (&stat_buf_.data_[stat_buf_.size_++]) T(value);
        } else {
            copy_dyn_buffer();
            new (dyn_buf_->data_ + dyn_buf_->size_)
                T(val_pos < dyn_buf_->size_ ? dyn_buf_->data_[val_pos] : value);
            dyn_buf_->size_++;
        }
    }

    void pop_back() {
        if (is_static_) {
            --stat_buf_.size_;
            stat_buf_.get_ptr(stat_buf_.size_)->~T();
        } else {
            copy_dyn_buffer();
            --dyn_buf_->size_;
            dyn_buf_->data_[size()].~T();
        }
    }

    bool empty() const {
        return size() == 0;
    }

    size_t capacity() const {
        return is_static_ ? SMALL_SIZE : dyn_buf_->capacity_;
    }

    void reserve(size_t new_cap) {
        if (capacity() < new_cap) {
            rebuild_storage(new_cap);
        } else if (!is_static_) {
            copy_dyn_buffer();
        }
    }

    void shrink_to_fit() {
        if (size() != capacity()) {
            rebuild_storage(size());
        }
    }

    void clear() {
        if (is_static_) {
            stat_buf_.clear();
        } else {
            copy_dyn_buffer();
            dyn_buf_->clear();
        }
    }

    void swap(socow_vector& other) {
        if (is_static_ && other.is_static_) {
            std::swap(stat_buf_, other.stat_buf_);
        } else if (is_static_ && !other.is_static_) {
            swap_stat_dyn(*this, other);
        } else if (other.is_static_) {
            swap_stat_dyn(other, *this);
        } else {
            std::swap(dyn_buf_, other.dyn_buf_);
        }
        std::swap(is_static_, other.is_static_);
    }

    iterator begin() {
        if (is_static_) {
            return stat_buf_.get_ptr(0);
        } else {
            copy_dyn_buffer();
            return dyn_buf_->data_;
        }
    }

    iterator end() {
        return begin() + size();
    }

    const_iterator begin() const {
        return is_static_ ? stat_buf_.get_ptr(0) : dyn_buf_->data_;
    }

    const_iterator end() const {
        return is_static_ ? stat_buf_.get_ptr(0) + stat_buf_.size_
                          : dyn_buf_->data_ + dyn_buf_->size_;
    }

    iterator insert(const_iterator pos, T const& value) {
        size_t pos_index = pos - get_begin();
        push_back(value);
        if (is_static_) {
            for (size_t i = size() - 1; i != pos_index; i--) {
                std::swap(stat_buf_.data_[i - 1], stat_buf_.data_[i]);
            }
        } else {
            for (size_t i = size() - 1; i != pos_index; i--) {
                std::swap(dyn_buf_->data_[i - 1], dyn_buf_->data_[i]);
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
            if (is_static_) {
                for (size_t i = first_index; i != stat_buf_.size_ - cnt; i++) {
                    std::swap(stat_buf_.data_[i], stat_buf_.data_[i + cnt]);
                }
            } else {
                copy_dyn_buffer();
                for (size_t i = first_index; i != dyn_buf_->size_ - cnt; i++) {
                    std::swap(dyn_buf_->data_[i], dyn_buf_->data_[i + cnt]);
                }
            }
            for (size_t i = 0; i != cnt; i++) {
                pop_back();
            }
            return begin() + first_index;
        }
    }

private:
    struct dynamic_storage;

    struct static_storage {
        size_t size_{0};
        std::array<std::aligned_storage_t<sizeof(T), alignof(T)>, SMALL_SIZE>
            data_;

        static_storage() = default;

        static_storage(static_storage const& other) : static_storage() {
            for (size_t i = 0; i != other.size_; i++) {
                new (&data_[i]) T(*other.get_ptr(i));
                ++size_;
            }
        }

        static_storage& operator=(static_storage const& other) {
            clear();
            for (size_t i = 0; i != other.size_; i++) {
                new (&data_[i]) T(*other.get_ptr(i));
                ++size_;
            }
            return *this;
        }

        ~static_storage() {
            clear();
        }

        T* get_ptr(size_t i) {
            return reinterpret_cast<T*>(&data_[i]);
        }

        T const* get_ptr(size_t i) const {
            return reinterpret_cast<T const*>(&data_[i]);
        }

        void clear() {
            for (size_t i = 0; i != size_; i++) {
                get_ptr(i)->~T();
            }
            size_ = 0;
        }
    };

    struct dynamic_storage {
        size_t size_{0};
        size_t capacity_{0};
        size_t copy_count_{1};
        T* data_{nullptr};

        dynamic_storage() = default;

        explicit dynamic_storage(size_t cap)
            : capacity_(cap),
              data_(static_cast<T*>(operator new(capacity_ * sizeof(T)))) {}

        ~dynamic_storage() {
            clear();
            operator delete(data_);
        }

        void clear() {
            for (size_t i = 0; i != size_; i++) {
                data_[i].~T();
            }
            size_ = 0;
        }
    };

    bool is_static_{true};
    union {
        static_storage stat_buf_;
        dynamic_storage* dyn_buf_;
    };

    void destruct_stat_buffer() {
        stat_buf_.~static_storage();
    }

    void destruct_dyn_buffer() {
        if (dyn_buf_->copy_count_ == 1) {
            delete dyn_buf_;
        } else {
            --dyn_buf_->copy_count_;
        }
    }

    void copy_dyn_buffer() {
        if (dyn_buf_->copy_count_ != 1) {
            rebuild_storage(capacity());
        }
    }

    T& get_elem(size_t i) {
        return is_static_ ? *stat_buf_.get_ptr(i) : dyn_buf_->data_[i];
    }

    iterator get_begin() {
        return is_static_ ? stat_buf_.get_ptr(0) : dyn_buf_->data_;
    }

    iterator get_end() {
        return get_begin() + size();
    }

    static void swap_stat_dyn(socow_vector& stat, socow_vector& dyn) {
        static_storage tmp = stat.stat_buf_;
        stat.destruct_stat_buffer();
        stat.dyn_buf_ = dyn.dyn_buf_;
        new (&dyn.stat_buf_) static_storage(tmp);
    }

    void rebuild_storage(size_t new_cap) {
        if (new_cap <= SMALL_SIZE && !is_static_) {
            static_storage new_stat_buffer;
            for (size_t i = 0; i != size(); i++) {
                new (&new_stat_buffer.data_[i]) T(dyn_buf_->data_[i]);
                ++new_stat_buffer.size_;
            }
            destruct_dyn_buffer();
            new (&stat_buf_) static_storage(new_stat_buffer);
            is_static_ = true;
        } else if (new_cap > SMALL_SIZE) {
            auto* new_dyn_buffer = new dynamic_storage(new_cap);
            for (size_t i = 0; i != size(); i++) {
                try {
                    new (new_dyn_buffer->data_ + i) T(get_elem(i));
                    ++new_dyn_buffer->size_;
                } catch (...) {
                    delete new_dyn_buffer;
                    throw;
                }
            }
            if (is_static_) {
                destruct_stat_buffer();
                dyn_buf_ = new_dyn_buffer;
                is_static_ = false;
            } else {
                destruct_dyn_buffer();
                dyn_buf_ = new_dyn_buffer;
            }
        }
    }
};
