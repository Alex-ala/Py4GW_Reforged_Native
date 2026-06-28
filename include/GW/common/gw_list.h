#pragma once

#include "base/error_handling.h"

#include <cstddef>
#include <cstdint>

namespace GW {

template <typename T>
struct GwLink {
    bool IsLinked() const { return prev_link != this; }

    void Unlink()
    {
        RemoveFromList();
        RemoveFromList();

        next_node = reinterpret_cast<T*>(reinterpret_cast<size_t>(this) + 1);
        prev_link = this;
    }

    T* Prev()
    {
        T* prev_node = prev_link->prev_link->next_node;
        if (reinterpret_cast<size_t>(prev_node) & 1) {
            return nullptr;
        }
        return prev_node;
    }

    T* Next()
    {
        if (reinterpret_cast<size_t>(next_node) & 1) {
            return nullptr;
        }
        return next_node;
    }

    GwLink* NextLink()
    {
        const size_t offset = reinterpret_cast<size_t>(this) - (reinterpret_cast<size_t>(prev_link->next_node) & ~1U);
        return reinterpret_cast<GwLink*>((reinterpret_cast<size_t>(next_node) & ~1U) + offset);
    }

    GwLink* PrevLink() { return prev_link; }

protected:
    GwLink* prev_link;
    T* next_node;

    void RemoveFromList()
    {
        NextLink()->prev_link = prev_link;
        prev_link->next_node = next_node;
    }
};

template <typename T>
struct GwList {
    class iterator {
    public:
        using difference_type = std::ptrdiff_t;
        using value_type = T;

        iterator()
            : current(nullptr)
            , first(nullptr)
        {
        }
        explicit iterator(GwLink<T>* node, GwLink<T>* first_node = nullptr)
            : current(node)
            , first(first_node)
        {
        }

        T& operator*() { return *current->Next(); }
        T* operator->() { return current->Next(); }
        T& operator*() const { return *current->Next(); }
        T* operator->() const { return current->Next(); }

        iterator& operator++()
        {
            if (current->NextLink() == first && first != nullptr) {
                iteration++;
            }
            current = current->NextLink();
            return *this;
        }

        iterator operator++(int)
        {
            iterator it(current);
            ++*this;
            return it;
        }

        bool operator==(const iterator& other) const { return current == other.current && iteration == other.iteration; }
        bool operator!=(const iterator& other) const { return !(*this == other); }

    private:
        GwLink<T>* current = nullptr;
        GwLink<T>* first = nullptr;
        int iteration = 0;
    };

    iterator begin() { return iterator(&link, &link); }

    iterator end()
    {
        GwLink<T>* last = &link;
        while (last->Next() != nullptr) {
            if (last->NextLink() == &link) {
                return ++iterator(last, &link);
            }
            last = last->NextLink();
        }
        return iterator(last, &link);
    }

    iterator begin() const { return iterator(const_cast<GwLink<T>*>(&link), const_cast<GwLink<T>*>(&link)); }
    iterator end() const { return iterator(const_cast<GwLink<T>*>(&link), const_cast<GwLink<T>*>(&link)); }

    GwLink<T>* Get() { return &link; }

protected:
    size_t offset{};
    GwLink<T> link;
};

static_assert(sizeof(GwList<void*>) == 0xC, "GwList has incorrect size");

}  // namespace GW
