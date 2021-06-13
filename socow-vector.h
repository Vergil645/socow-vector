#pragma once
#include <array>
#include <cstddef>
#include <new>
#include <type_traits>

template <typename T, size_t SMALL_SIZE>
struct socow_vector {
private:
    struct static_storage;
    struct dynamic_storage;

public:
    using iterator = T*;
    using const_iterator = T const*;

    socow_vector();

    socow_vector(socow_vector const& other);

    socow_vector& operator=(socow_vector const& other);

    ~socow_vector();

    T& operator[](size_t i);

    T const& operator[](size_t i) const;

    T* data();

    T const* data() const;

    size_t size() const;

    T& front();

    T const& front() const;

    T& back();

    T const& back() const;

    void push_back(T const& value);

    void pop_back();

    bool empty() const;

    size_t capacity() const;

    void reserve(size_t new_cap);

    void shrink_to_fit();

    void clear();

    void swap(socow_vector& other);

    iterator begin();

    const_iterator begin() const;

    iterator end();

    const_iterator end() const;

    iterator insert(const_iterator pos, T const& value);

    iterator erase(const_iterator pos);

    iterator erase(const_iterator first, const_iterator last);

private:
    size_t size_{1};
    union {
        static_storage stat_buf_;
        dynamic_storage dyn_buf_;
    };

    bool is_static();

    bool is_static() const;

    static void clear_storage(dynamic_storage& buf, size_t buf_size);

    static void destruct_storage(static_storage& buf, size_t buf_size);

    static void destruct_storage(dynamic_storage& buf, size_t buf_size);

    void copy_storage();

    T& get_elem(size_t i);

    iterator get_begin();

    iterator get_end();

    static void swap_stat_dyn(socow_vector& stat_vec, socow_vector& dyn_vec);

    void rebuild_storage(size_t new_cap);
};

template <typename T, size_t SMALL_SIZE>
struct socow_vector<T, SMALL_SIZE>::static_storage {
    static_storage();

    static_storage(static_storage const& other, size_t stor_size);

    T* data();

    T const* data() const;

    void clear(size_t stor_size);

private:
    std::array<std::aligned_storage_t<sizeof(T), alignof(T)>, SMALL_SIZE> data_;
};

template <typename T, size_t SMALL_SIZE>
struct socow_vector<T, SMALL_SIZE>::dynamic_storage {
private:
    struct metadata;

public:
    explicit dynamic_storage(size_t cap);

    dynamic_storage(dynamic_storage const& other);

    size_t capacity() const;

    size_t& ref_count();

    T* data();

    T const* data() const;

    void swap(dynamic_storage& other);

    void clear(size_t stor_size);

private:
    metadata* all_data_;

    static constexpr std::align_val_t meta_al =
        static_cast<std::align_val_t>(alignof(metadata));

    friend struct socow_vector;
};

template <typename T, size_t SMALL_SIZE>
struct socow_vector<T, SMALL_SIZE>::dynamic_storage::metadata {
private:
    size_t capacity_;
    size_t ref_count_;
    T data_[];

    friend struct socow_vector::dynamic_storage;
};

/// SOCOW VECTOR
template <typename T, size_t SMALL_SIZE>
socow_vector<T, SMALL_SIZE>::socow_vector() : stat_buf_() {}

template <typename T, size_t SMALL_SIZE>
socow_vector<T, SMALL_SIZE>::socow_vector(socow_vector const& other)
    : size_(other.size_) {
    if (other.is_static()) {
        new (&stat_buf_) static_storage(other.stat_buf_, other.size());
    } else {
        new (&dyn_buf_) dynamic_storage(other.dyn_buf_);
    }
}

template <typename T, size_t SMALL_SIZE>
socow_vector<T, SMALL_SIZE>&
socow_vector<T, SMALL_SIZE>::operator=(socow_vector const& other) {
    socow_vector(other).swap(*this);
    return *this;
}

template <typename T, size_t SMALL_SIZE>
socow_vector<T, SMALL_SIZE>::~socow_vector() {
    if (is_static()) {
        destruct_storage(stat_buf_, size());
    } else {
        destruct_storage(dyn_buf_, size());
    }
}

template <typename T, size_t SMALL_SIZE>
T& socow_vector<T, SMALL_SIZE>::operator[](size_t i) {
    return data()[i];
}

template <typename T, size_t SMALL_SIZE>
T const& socow_vector<T, SMALL_SIZE>::operator[](size_t i) const {
    return data()[i];
}

template <typename T, size_t SMALL_SIZE>
T* socow_vector<T, SMALL_SIZE>::data() {
    copy_storage();
    return is_static() ? stat_buf_.data() : dyn_buf_.data();
}

template <typename T, size_t SMALL_SIZE>
T const* socow_vector<T, SMALL_SIZE>::data() const {
    return is_static() ? stat_buf_.data() : dyn_buf_.data();
}

template <typename T, size_t SMALL_SIZE>
size_t socow_vector<T, SMALL_SIZE>::size() const {
    return size_ >> 1;
}

template <typename T, size_t SMALL_SIZE>
T& socow_vector<T, SMALL_SIZE>::front() {
    return *begin();
}

template <typename T, size_t SMALL_SIZE>
T const& socow_vector<T, SMALL_SIZE>::front() const {
    return *begin();
}

template <typename T, size_t SMALL_SIZE>
T& socow_vector<T, SMALL_SIZE>::back() {
    return *(end() - 1);
}

template <typename T, size_t SMALL_SIZE>
T const& socow_vector<T, SMALL_SIZE>::back() const {
    return *(end() - 1);
}

template <typename T, size_t SMALL_SIZE>
void socow_vector<T, SMALL_SIZE>::push_back(T const& value) {
    size_t val_pos = &value >= get_begin() ? &value - get_begin()
                                           : std::numeric_limits<size_t>::max();
    size_t cur_size = size(), cur_cap = capacity();
    if (cur_size == cur_cap) {
        rebuild_storage(cur_cap << 1);
    }
    T* cur_data = data();
    new (cur_data + cur_size) T(val_pos < cur_size ? cur_data[val_pos] : value);
    size_ += 2;
}

template <typename T, size_t SMALL_SIZE>
void socow_vector<T, SMALL_SIZE>::pop_back() {
    data()[size() - 1].~T();
    size_ -= 2;
}

template <typename T, size_t SMALL_SIZE>
bool socow_vector<T, SMALL_SIZE>::empty() const {
    return size() == 0;
}

template <typename T, size_t SMALL_SIZE>
size_t socow_vector<T, SMALL_SIZE>::capacity() const {
    return is_static() ? SMALL_SIZE : dyn_buf_.capacity();
}

template <typename T, size_t SMALL_SIZE>
void socow_vector<T, SMALL_SIZE>::reserve(size_t new_cap) {
    if (capacity() < new_cap) {
        rebuild_storage(new_cap);
    } else {
        copy_storage();
    }
}

template <typename T, size_t SMALL_SIZE>
void socow_vector<T, SMALL_SIZE>::shrink_to_fit() {
    if (size() != capacity()) {
        rebuild_storage(size());
    }
}

template <typename T, size_t SMALL_SIZE>
void socow_vector<T, SMALL_SIZE>::clear() {
    copy_storage();
    is_static() ? stat_buf_.clear(size()) : dyn_buf_.clear(size());
    size_ = size_ % 2;
}

template <typename T, size_t SMALL_SIZE>
void socow_vector<T, SMALL_SIZE>::swap(socow_vector& other) {
    if (is_static() && other.is_static()) {
        size_t cur_size = size(), other_size = other.size();
        static_storage tmp(stat_buf_, cur_size);
        destruct_storage(stat_buf_, cur_size);
        new (&stat_buf_) static_storage(other.stat_buf_, other_size);
        destruct_storage(other.stat_buf_, other_size);
        new (&other.stat_buf_) static_storage(tmp, cur_size);
        tmp.clear(cur_size);
    } else if (is_static() && !other.is_static()) {
        swap_stat_dyn(*this, other);
    } else if (other.is_static()) {
        swap_stat_dyn(other, *this);
    } else {
        dyn_buf_.swap(other.dyn_buf_);
    }
    std::swap(size_, other.size_);
}

template <typename T, size_t SMALL_SIZE>
typename socow_vector<T, SMALL_SIZE>::iterator
socow_vector<T, SMALL_SIZE>::begin() {
    return data();
}

template <typename T, size_t SMALL_SIZE>
typename socow_vector<T, SMALL_SIZE>::iterator
socow_vector<T, SMALL_SIZE>::end() {
    return begin() + size();
}

template <typename T, size_t SMALL_SIZE>
typename socow_vector<T, SMALL_SIZE>::const_iterator
socow_vector<T, SMALL_SIZE>::begin() const {
    return data();
}

template <typename T, size_t SMALL_SIZE>
typename socow_vector<T, SMALL_SIZE>::const_iterator
socow_vector<T, SMALL_SIZE>::end() const {
    return begin() + size();
}

template <typename T, size_t SMALL_SIZE>
typename socow_vector<T, SMALL_SIZE>::iterator
socow_vector<T, SMALL_SIZE>::insert(const_iterator pos, T const& value) {
    size_t pos_index = pos - get_begin();
    push_back(value);
    T* cur_data = data();
    for (size_t i = size() - 1; i != pos_index; i--) {
        std::swap(cur_data[i - 1], cur_data[i]);
    }
    return get_begin() + pos_index;
}

template <typename T, size_t SMALL_SIZE>
typename socow_vector<T, SMALL_SIZE>::iterator
socow_vector<T, SMALL_SIZE>::erase(const_iterator pos) {
    return pos == get_end() ? end() : erase(pos, pos + 1);
}

template <typename T, size_t SMALL_SIZE>
typename socow_vector<T, SMALL_SIZE>::iterator
socow_vector<T, SMALL_SIZE>::erase(const_iterator first, const_iterator last) {
    size_t first_index = first - get_begin();
    size_t cnt = last - first;
    if (cnt == 0) {
        return begin() + first_index;
    } else {
        T* cur_data = data();
        for (size_t i = first_index; i != size() - cnt; i++) {
            std::swap(cur_data[i], cur_data[i + cnt]);
        }
        for (size_t i = 0; i != cnt; i++) {
            pop_back();
        }
        return get_begin() + first_index;
    }
}

template <typename T, size_t SMALL_SIZE>
bool socow_vector<T, SMALL_SIZE>::is_static() {
    return size_ % 2 != 0;
}

template <typename T, size_t SMALL_SIZE>
bool socow_vector<T, SMALL_SIZE>::is_static() const {
    return size_ % 2 != 0;
}

template <typename T, size_t SMALL_SIZE>
void socow_vector<T, SMALL_SIZE>::clear_storage(
    socow_vector::dynamic_storage& buf, size_t buf_size) {
    if (buf.ref_count() == 1) {
        buf.clear(buf_size);
        operator delete(buf.all_data_, dynamic_storage::meta_al);
    } else {
        --buf.ref_count();
    }
}

template <typename T, size_t SMALL_SIZE>
void socow_vector<T, SMALL_SIZE>::destruct_storage(
    socow_vector::static_storage& buf, size_t buf_size) {
    buf.clear(buf_size);
    buf.~static_storage();
}

template <typename T, size_t SMALL_SIZE>
void socow_vector<T, SMALL_SIZE>::destruct_storage(
    socow_vector::dynamic_storage& buf, size_t buf_size) {
    clear_storage(buf, buf_size);
    buf.~dynamic_storage();
}

template <typename T, size_t SMALL_SIZE>
void socow_vector<T, SMALL_SIZE>::copy_storage() {
    if (!is_static() && dyn_buf_.ref_count() != 1) {
        if (size() > SMALL_SIZE) {
            if ((size() << 2) > capacity()) {
                rebuild_storage(capacity());
            } else {
                rebuild_storage((capacity() + 1) >> 1);
            }
        } else {
            rebuild_storage(SMALL_SIZE);
        }
    }
}

template <typename T, size_t SMALL_SIZE>
T& socow_vector<T, SMALL_SIZE>::get_elem(size_t i) {
    return is_static() ? stat_buf_.data()[i] : dyn_buf_.data()[i];
}

template <typename T, size_t SMALL_SIZE>
typename socow_vector<T, SMALL_SIZE>::iterator
socow_vector<T, SMALL_SIZE>::get_begin() {
    return is_static() ? stat_buf_.data() : dyn_buf_.data();
}

template <typename T, size_t SMALL_SIZE>
typename socow_vector<T, SMALL_SIZE>::iterator
socow_vector<T, SMALL_SIZE>::get_end() {
    return get_begin() + size();
}

template <typename T, size_t SMALL_SIZE>
void socow_vector<T, SMALL_SIZE>::swap_stat_dyn(socow_vector& stat_vec,
                                                socow_vector& dyn_vec) {
    dynamic_storage tmp = dyn_vec.dyn_buf_;
    destruct_storage(dyn_vec.dyn_buf_, dyn_vec.size());
    try {
        new (&dyn_vec.stat_buf_)
            static_storage(stat_vec.stat_buf_, stat_vec.size());
    } catch (...) {
        new (&dyn_vec.dyn_buf_) dynamic_storage(tmp);
        --dyn_vec.dyn_buf_.ref_count();
        throw;
    }
    destruct_storage(stat_vec.stat_buf_, stat_vec.size());
    new (&stat_vec.dyn_buf_) dynamic_storage(tmp);
    --stat_vec.dyn_buf_.ref_count();
}

template <typename T, size_t SMALL_SIZE>
void socow_vector<T, SMALL_SIZE>::rebuild_storage(size_t new_cap) {
    if (new_cap <= SMALL_SIZE && !is_static()) {
        static_storage new_stat_buf;
        for (size_t i = 0; i != size(); i++) {
            try {
                new (new_stat_buf.data() + i) T(dyn_buf_.data()[i]);
            } catch (...) {
                new_stat_buf.clear(i);
                throw;
            }
        }
        destruct_storage(dyn_buf_, size());
        new (&stat_buf_) static_storage(new_stat_buf, size());
        ++size_;
        new_stat_buf.clear(size());
    } else if (new_cap > SMALL_SIZE) {
        dynamic_storage new_dyn_buf(new_cap);
        for (size_t i = 0; i != size(); i++) {
            try {
                new (new_dyn_buf.data() + i) T(get_elem(i));
            } catch (...) {
                clear_storage(new_dyn_buf, i);
                throw;
            }
        }
        if (is_static()) {
            destruct_storage(stat_buf_, size());
            --size_;
        } else {
            destruct_storage(dyn_buf_, size());
        }
        new (&dyn_buf_) dynamic_storage(new_dyn_buf);
        --dyn_buf_.ref_count();
    }
}

/// STATIC STORAGE
template <typename T, size_t SMALL_SIZE>
socow_vector<T, SMALL_SIZE>::static_storage::static_storage() = default;

template <typename T, size_t SMALL_SIZE>
socow_vector<T, SMALL_SIZE>::static_storage::static_storage(
    static_storage const& other, size_t stor_size)
    : static_storage() {
    for (size_t i = 0; i != stor_size; i++) {
        try {
            new (&data_[i]) T(other.data()[i]);
        } catch (...) {
            clear(i);
            throw;
        }
    }
}

template <typename T, size_t SMALL_SIZE>
T* socow_vector<T, SMALL_SIZE>::static_storage::data() {
    return reinterpret_cast<T*>(&data_[0]);
}

template <typename T, size_t SMALL_SIZE>
T const* socow_vector<T, SMALL_SIZE>::static_storage::data() const {
    return reinterpret_cast<T const*>(&data_[0]);
}

template <typename T, size_t SMALL_SIZE>
void socow_vector<T, SMALL_SIZE>::static_storage::clear(size_t stor_size) {
    for (size_t i = 0; i != stor_size; i++) {
        data()[i].~T();
    }
}

/// DYNAMIC STORAGE
template <typename T, size_t SMALL_SIZE>
socow_vector<T, SMALL_SIZE>::dynamic_storage::dynamic_storage(size_t cap)
    : all_data_(static_cast<metadata*>(operator new(
          sizeof(metadata) + cap * sizeof(T), meta_al))) {
    all_data_->capacity_ = cap;
    all_data_->ref_count_ = 1;
}

template <typename T, size_t SMALL_SIZE>
socow_vector<T, SMALL_SIZE>::dynamic_storage::dynamic_storage(
    dynamic_storage const& other)
    : all_data_(other.all_data_) {
    ++all_data_->ref_count_;
}

template <typename T, size_t SMALL_SIZE>
size_t socow_vector<T, SMALL_SIZE>::dynamic_storage::capacity() const {
    return all_data_->capacity_;
}

template <typename T, size_t SMALL_SIZE>
size_t& socow_vector<T, SMALL_SIZE>::dynamic_storage::ref_count() {
    return all_data_->ref_count_;
}

template <typename T, size_t SMALL_SIZE>
T* socow_vector<T, SMALL_SIZE>::dynamic_storage::data() {
    return all_data_->data_;
}

template <typename T, size_t SMALL_SIZE>
T const* socow_vector<T, SMALL_SIZE>::dynamic_storage::data() const {
    return all_data_->data_;
}

template <typename T, size_t SMALL_SIZE>
void socow_vector<T, SMALL_SIZE>::dynamic_storage::swap(
    dynamic_storage& other) {
    std::swap(all_data_, other.all_data_);
}

template <typename T, size_t SMALL_SIZE>
void socow_vector<T, SMALL_SIZE>::dynamic_storage::clear(size_t stor_size) {
    for (size_t i = 0; i != stor_size; i++) {
        all_data_->data_[i].~T();
    }
}
