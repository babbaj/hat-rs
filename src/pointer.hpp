#pragma once

#include <cstdint>
#include <cstddef>
#include <cassert>
#include <type_traits>
#include <exception>
//#include <span>

#include "../vmread/hlapi/hlapi.h"

using std::size_t;

template<typename T, typename Owner>
inline std::ptrdiff_t offset_of(T Owner::*member) {
    const char storage[sizeof(Owner)]{0};
    auto* obj = reinterpret_cast<const Owner*>(storage);
    return intptr_t(&(obj->*member)) - intptr_t(obj);
}

template<typename T, typename Owner>
std::type_identity<T> member_type_fn(T Owner::*);

template<typename M>
using member_type = typename decltype(member_type_fn(std::declval<M>()))::type;

template<typename T, typename U>
concept equal_comparable = std::is_convertible_v<T, U> || std::is_convertible_v<U, T>;

// true if 2 types have the same memory footprint
template<typename T, typename U>
constexpr bool similar = sizeof(T) == sizeof(U) && alignof(T) == alignof(U);
template<typename T, typename U> requires (std::same_as<T, void> || std::same_as<U, void>)
constexpr bool similar<T, U> = true;


template<typename>
struct pointer;

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

    template<typename U> requires similar<T, U>
    pointer<U> bit_cast() {
        return pointer<U>{this->address};
    }

    template<typename U>
    pointer<U> unsafe_cast() {
        return pointer<U>{this->address};
    }

    template<typename U>
    pointer<U> cast() const {
        static_assert(std::is_base_of_v<U, T> || std::is_base_of_v<T, U>);
        return pointer<U>{this->address};
    }

    explicit operator bool() const {
        return address != 0;
    }
};

template<typename T>
struct pointer : pointer_base<T> {
    using pointer_base<T>::pointer_base;

    [[nodiscard]] pointer<T> index(size_t idx) const {
        return pointer<T>{this->address + (idx * sizeof(T))};
    }

    T read(WinProcess& proc, size_t idx = 0) const {
        nullCheck();
        return proc.Read<T>(this->address + (idx * sizeof(T)));
    }

    std::vector<T> readArray(WinProcess& proc, size_t len) const {
        nullCheck();
        std::vector<T> vec(len);
        proc.Read(this->address, vec.data(), len * sizeof(T));
        return vec;
    }

    void write(WinProcess& proc, const T& value, size_t idx = 0) {
        nullCheck();
        proc.Write(this->address + (idx * sizeof(T)), value);
    }

    /*void writeArray(WinProcess& proc, std::span<const T> array) {
        nullCheck();
        proc.Write(this->address, array.data(), sizeof(T) * array.size());
    }*/

    // TODO: implicit conversion operator/constructor for super types

    template<typename U> requires equal_comparable<U, T>
    bool operator==(pointer<U> rhs) const {
        return this->address == rhs.address;
    }

    template<typename U> requires equal_comparable<U, T>
    bool operator!=(pointer<U> rhs) const {
        return !(*this == rhs);
    }

    constexpr bool operator==(std::nullptr_t) const {
        return this->address == 0;
    }
    constexpr bool operator!=(std::nullptr_t) const {
        return this->address != 0;
    }

    pointer<T>& operator=(const pointer<T>&) = default;
    pointer<T>& operator=(std::nullptr_t) {
        this->address = 0;
        return *this;
    };

    template<typename Fn>
    auto memberImpl(Fn f) const {
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

    pointer<T> decay() {
        return pointer<T>{this->address};
    }

    pointer<T> index(size_t idx) {
        return pointer<T>{this->address + sizeof(T) * idx};
    }
};

template<typename R, typename... Args>
struct pointer<R(Args...)> : pointer_base<R(Args...)> {
    using pointer_base<R(Args...)>::pointer_base;
};

// TODO: use concepts instead of specializing every combo of const
template<>
struct pointer<void> : pointer_base<void> {
    using pointer_base<void>::pointer_base;
};
template<>
struct pointer<const void> : pointer_base<const void> {
    using pointer_base<const void>::pointer_base;
};

static_assert(sizeof(pointer<void>) == sizeof(void*));
static_assert(sizeof(pointer<int>) == sizeof(int*));


#define member(m) memberImpl([](auto ptr) { return &decltype(ptr)::type::m; })