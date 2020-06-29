#pragma once

#include "vmread/hlapi/hlapi.h"
#include <cstdint>
#include <cassert>
#include <type_traits>
#include <exception>

template<typename T, typename Owner>
inline constexpr std::ptrdiff_t offset_of(T Owner::*member) {
    const char storage[sizeof(Owner)]{0};
    auto* obj = reinterpret_cast<const Owner*>(storage);
    return intptr_t(&(obj->*member)) - intptr_t(obj);
}

template<typename T, typename Owner>
constexpr T member_type_fn(T Owner::*);

template<typename M>
using member_type = decltype(member_type_fn(std::declval<M>()));

template<typename T>
struct pointer_base {
    using type = T;

    uint64_t address{};

    explicit pointer_base(T* ptr): address(reinterpret_cast<uintptr_t>(ptr)) {}
    explicit pointer_base(uint64_t addr): address(addr) {}
    pointer_base() = default;

    T* as_raw() {
        return reinterpret_cast<T*>(address);
    }

    explicit operator bool() const {
        return address != 0;
    }
};

// TODO: add function to get pointer to member
template<typename T>
struct pointer : pointer_base<T> {
    using pointer_base<T>::pointer_base;

    [[nodiscard]] pointer<T> index(size_t idx) const {
        return pointer<T>{this->address + (idx * sizeof(T))};
    }

    T read(WinProcess& proc, size_t idx = 0) const {
        //assert(this->address);
        nullCheck();
        return proc.Read<T>(this->address + (idx * sizeof(T)));
    }

    void write(WinProcess& proc, const T& value, size_t idx = 0) {
        //assert(this->address);
        nullCheck();
        proc.Write(this->address + (idx * sizeof(T)), value);
    }

    template<typename U>
    pointer<U> cast() {
        static_assert(sizeof(U) >= sizeof(T));
        static_assert(alignof(U) >= alignof(T));
        return pointer<U>{this->address};
    }

    template<typename U>
    bool operator==(pointer<U> rhs) {
        return this->address == rhs.address;
    }

    template<typename U>
    bool operator!=(pointer<U> rhs) {
        return !(*this == rhs);
    }

    template<typename Fn>
    auto memberImpl(Fn f) {
        auto mem = f(*this);
        return pointer<member_type<decltype(mem)>>{this->address + offset_of(mem)};
    }

private:
    void nullCheck() const {
        if (!this->address) throw std::runtime_error{"null pointer!"};
    }
};


template<typename T, size_t N>
struct pointer<T[N]> : pointer_base<T[N]> {
    using pointer_base<T[N]>::pointer_base;

    // TODO: inherit from pointer<T> and replace these with bulk operations
    T read(WinProcess& proc, size_t idx) {
        assert(this->address);
        return proc.Read<T>(this->address + (idx * sizeof(T)));
    }

    void write(WinProcess& proc, const T& value, size_t idx) {
        assert(this->address);
        proc.Write(this->address + (idx * sizeof(T)), value);
    }
};

template<>
struct pointer<void> : pointer_base<void> {
    using pointer_base<void>::pointer_base;
};

template<typename T, typename M>
M readMember(WinProcess& proc, const pointer<T> ptr, M T::* member) {
    return proc.Read<M>(ptr.address + offset_of(member));
}

template<typename T, typename M>
void writeMember(WinProcess& proc, const pointer<T> ptr, M T::* member, const M& value) {
    proc.Write(ptr.address + offset_of(member), value);
}

template<typename T, typename M>
[[deprecated]] pointer<M> member0(pointer<T> ptr, M T::* member) {
    return pointer<M>{ptr.address + offset_of(member)};
}

static_assert(sizeof(pointer<void>) == sizeof(void*));
static_assert(sizeof(pointer<int>) == sizeof(int*));


#define member(m) memberImpl([](auto ptr) { return &decltype(ptr)::type::m; })