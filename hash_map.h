#include <iostream>
#include <iomanip>
#include <functional>
#include <string>
#include <unordered_set>
#include <list>
#include <vector>

using namespace std;

template<class KeyType, class ValueType, class Hash = hash<KeyType> > class HashMap{
public:
    enum class Status {
        ALIVE, DEAD, FREE
    };

    class iterator {
    public:
        iterator() = default;

        iterator(HashMap <KeyType, ValueType, Hash>* map): map_(map), id_(0) { skip_dead_and_free(); }
        iterator(HashMap <KeyType, ValueType, Hash>* map, size_t id): map_(map), id_(id) {}

        pair<const KeyType, ValueType> &operator*() {
            return *get_key_const_pointer(map_->data_[id_]);
        }

        pair<const KeyType, ValueType> *operator->() {
            return get_key_const_pointer(map_->data_[id_]);
        }

        iterator &operator++() {
            ++id_;
            skip_dead_and_free();
            return *this;
        }

        iterator operator++(int) {
            auto result = *this;
            ++(*this);
            return result;
        }

        bool operator==(const iterator &other) const {
            return this->map_ == other.map_ && this->id_ == other.id_;
        }

        bool operator!=(const iterator &other) const {
            return !(this->operator==(other));
        }

        size_t get_id() const {
            return id_;
        }
    private:
        HashMap <KeyType, ValueType, Hash> *map_ = nullptr;
        size_t id_ = 0;

        void skip_dead_and_free() {
            while (id_ < map_->buffer_size_ && map_->status_[id_] != Status::ALIVE) {
                ++id_;
            }
        }

        auto get_key_const_pointer(pair <KeyType, ValueType>& it) {
            return reinterpret_cast<pair <const KeyType, ValueType>*> (&it);
        }
    };

    class const_iterator {
    public:
        const_iterator() = default;

        const_iterator(const HashMap <KeyType, ValueType, Hash>* map): map_(map), id_(0) { skip_dead_and_free(); }
        const_iterator(const HashMap <KeyType, ValueType, Hash>* map, size_t id): map_(map), id_(id) {}

        const pair<const KeyType, ValueType> &operator*() {
            return *get_key_const_pointer(map_->data_[id_]);
        }

        const pair<const KeyType, ValueType> *operator->() {
            return get_key_const_pointer(map_->data_[id_]);
        }

        const_iterator &operator++() {
            ++id_;
            skip_dead_and_free();
            return *this;
        }

        const_iterator operator++(int) {
            auto result = *this;
            ++(*this);
            return result;
        }

        bool operator==(const const_iterator &other) const {
            return this->map_ == other.map_ && this->id_ == other.id_;
        }

        bool operator!=(const const_iterator &other) const {
            return !(this->operator==(other));
        }

        size_t get_id() const {
            return id_;
        }
    private:
        const HashMap <KeyType, ValueType, Hash> *map_ = nullptr;
        size_t id_ = 0;

        void skip_dead_and_free() {
            while (id_ < map_->buffer_size_ && map_->status_[id_] != Status::ALIVE) {
                ++id_;
            }
        }

        auto get_key_const_pointer(const pair <KeyType, ValueType>& it) {
            return reinterpret_cast<const pair <const KeyType, ValueType>*> (&it);
        }
    };

    HashMap(): hasher_(Hash()) {
        buffer_size_ = DEFAULT_SIZE_;
        data_.resize(DEFAULT_SIZE_);
        status_.resize(DEFAULT_SIZE_, Status::FREE);
        psl_.resize(DEFAULT_SIZE_, 0);
    }

    explicit HashMap(Hash hasher): hasher_(hasher) {
        buffer_size_ = DEFAULT_SIZE_;
        data_.resize(DEFAULT_SIZE_);
        status_.resize(DEFAULT_SIZE_, Status::FREE);
        psl_.resize(DEFAULT_SIZE_, 0);
    }

    HashMap(const HashMap& other){
        *this = other;
    }

    HashMap& operator = (const HashMap& other) {
        hasher_ = other.hasher_;
        size_ = other.size_;
        dead_ = other.dead_;
        buffer_size_ = other.buffer_size_;
        data_ = other.data_;
        status_ = other.status_;
        psl_ = other.psl_;
        return *this;
    }

    template<class IteratorType>
    HashMap(IteratorType begin, IteratorType end) : HashMap() {
        for (auto it = begin; it != end; ++it) {
            insert(*it);
        }
    }

    HashMap(initializer_list<pair <KeyType, ValueType>> list): HashMap() {
        for (auto it = list.begin(); it != list.end(); ++it) {
            insert(*it);
        }
    }

    void insert(pair<KeyType, ValueType> new_pair) {
        maybe_resize();
        KeyType key = new_pair.first;

        size_t id = hasher_(key) % buffer_size_;
        size_t psl = 0;
        for (size_t _ = 0; _ < buffer_size_; ++_) {
            if (status_[id] == Status::FREE) {
                data_[id] = new_pair;
                psl_[id] = psl;
                status_[id] = Status::ALIVE;
                size_++;
                return;
            }

            if (status_[id] == Status::ALIVE) {
                if (data_[id].first == key)
                    return;
                if (psl_[id] < psl) {
                    swap(data_[id], new_pair);
                    swap(psl_[id], psl);
                }
            }

            ++psl;
            id = (id + 1) % buffer_size_;
        }
    }

    void erase(const KeyType& key) {
        auto it = find(key);
        if (it == end())
            return;
        status_[it.get_id()] = Status::DEAD;
        ++dead_;
        --size_;
    }

    iterator find(const KeyType& key) {
        size_t id = hasher_(key) % buffer_size_;
        for (size_t _ = 0; _ < buffer_size_; ++_) {
            if (status_[id] == Status::ALIVE && data_[id].first == key)
                return iterator(this, id);
            if (status_[id] == Status::FREE)
                return iterator(this, buffer_size_);
            id = (id + 1) % buffer_size_;
        }
        return iterator(this, buffer_size_);
    }

    const_iterator find(const KeyType& key) const {
        size_t id = hasher_(key) % buffer_size_;
        for (size_t _ = 0; _ < buffer_size_; ++_) {
            if (status_[id] == Status::ALIVE && data_[id].first == key)
                return const_iterator(this, id);
            if (status_[id] == Status::FREE)
                return const_iterator(this, buffer_size_);
            id = (id + 1) % buffer_size_;
        }
        return const_iterator(this, buffer_size_);
    }

    size_t size() const {
        return size_;
    }

    bool empty() const {
        return size_ == 0;
    }

    Hash hash_function() const {
        return hasher_;
    }

    iterator begin() {
        return iterator(this);
    }

    const_iterator begin() const {
        return const_iterator(this);
    }

    iterator end() {
        return iterator(this, buffer_size_);
    }

    const_iterator end() const {
        return const_iterator(this, buffer_size_);
    }

    ValueType& operator [](KeyType key){
        iterator it = find(key);
        if (it == end()) {
            insert({key, ValueType()});
            it = find(key);
        }
        return it->second;
    }

    const ValueType& at(const KeyType& key) const{
        const_iterator it = find(key);
        if (it == end())
            throw out_of_range("");
        return it->second;
    }

    void clear() {
        *this = HashMap(hasher_);
    }

private:
    Hash hasher_ = Hash();
    size_t size_ = 0;
    size_t dead_ = 0;
    size_t buffer_size_ = 128;
    size_t DEFAULT_SIZE_ = 128;
    const double LOAD_FACTOR_ = 0.3;

    vector<pair <KeyType, ValueType>> data_;
    vector <Status> status_;
    vector <size_t> psl_;

    void maybe_resize() {
        if (buffer_size_ * LOAD_FACTOR_ < size_ + dead_) {
            buffer_size_ *= 2;
            rebuild();
        }
    }

    void rebuild() {
        vector <pair <KeyType, ValueType>> data_copy = data_;
        vector <Status> status_copy = status_;

        data_.resize(buffer_size_);
        status_.assign(buffer_size_, Status::FREE);
        psl_.resize(buffer_size_);
        size_ = 0;
        dead_ = 0;

        for (size_t i = 0; i < data_copy.size(); ++i) {
            if (status_copy[i] == Status::ALIVE)
                insert(data_copy[i]);
        }
    }
};