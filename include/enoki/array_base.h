/*
    enoki/array_base.h -- Base class of all Enoki arrays

    Enoki is a C++ template library for efficient vectorization and
    differentiation of numerical kernels on modern processor architectures.

    Copyright (c) 2020 Wenzel Jakob <wenzel.jakob@epfl.ch>

    All rights reserved. Use of this source code is governed by a BSD-style
    license that can be found in the LICENSE file.
*/

#pragma once

#include <enoki/array_router.h>
#include <enoki/array_constants.h>

NAMESPACE_BEGIN(enoki)

#define ENOKI_ARRAY_DEFAULTS(Name)                                             \
    Name(const Name &) = default;                                              \
    Name(Name &&) = default;                                                   \
    Name &operator=(const Name &) = default;                                   \
    Name &operator=(Name &&) = default;

#define ENOKI_ARRAY_IMPORT(Name, Base)                                         \
    Name() = default;                                                          \
    ENOKI_ARRAY_DEFAULTS(Name)                                                 \
    using Base::Base;

#define ENOKI_ARRAY_FALLBACK_CONSTRUCTORS(Name)                                \
    template <typename Value2, typename D2, typename D = Derived_,             \
              enable_if_t<D::Size == D2::Size && D::Depth == D2::Depth> = 0>   \
    Name(const ArrayBaseT<Value2, false, D2> &v) {                             \
        ENOKI_CHKSCALAR("Copy constructor (conversion)");                      \
        for (size_t i = 0; i < derived().size(); ++i)                          \
            derived().entry(i) = (Value) v.derived().entry(i);                 \
    }                                                                          \
    template <typename Value2, typename D2, typename D = Derived_,             \
              enable_if_t<D::Size == D2::Size && D::Depth == D2::Depth> = 0>   \
    Name(const ArrayBaseT<Value2, IsMask_, D2> &v, detail::reinterpret_flag) { \
        ENOKI_CHKSCALAR("Copy constructor (reinterpret_cast)");                \
        for (size_t i = 0; i < derived().size(); ++i)                          \
            derived().entry(i) = reinterpret_array<Value>(v[i]);               \
    }


/// Generic array base class
struct ArrayBase {
    // -----------------------------------------------------------------------
    //! @{ \name Basic declarations (may be overridden in subclasses)
    // -----------------------------------------------------------------------

    /// Is this an Enoki array?
    static constexpr bool IsEnoki = true;

    /// Does this array map operations onto packed vector instructions?
    static constexpr bool IsPacked = false;

    /// Is this an AVX512-style 'k' mask register?
    static constexpr bool IsKMask = false;

    /// Is the storage representation of this array implemented recursively?
    static constexpr bool IsRecursive = false;

    /// Always prefer broadcasting to the outer dimensions of a N-D array
    static constexpr bool BroadcastOuter = true;

    /// Does this array represent a fixed size vector?
    static constexpr bool IsVector = false;

    /// Does this array represent a complex number?
    static constexpr bool IsComplex = false;

    /// Does this array represent a quaternion?
    static constexpr bool IsQuaternion = false;

    /// Does this array represent a matrix?
    static constexpr bool IsMatrix = false;

    /// Does this array represent the result of a 'masked(...)' epxpression?
    static constexpr bool IsMaskedArray = false;

    //! @}
    // -----------------------------------------------------------------------
};

/// Array base class templated via the curiously recurring template pattern
template <typename Value_, bool IsMask_, typename Derived_> struct ArrayBaseT : ArrayBase {
    using Base = ArrayBase;

    // -----------------------------------------------------------------------
    //! @{ \name Basic declarations (may be overridden in subclasses)
    // -----------------------------------------------------------------------

    /// Type underlying the array
    using Value = Value_;

    /// Scalar data type all the way at the lowest level
    using Scalar = scalar_t<Value_>;

    /// Helper structure for dispatching vectorized method calls
    using CallSupport =
        call_support<std::decay_t<std::remove_pointer_t<Value_>>, Derived_>;

    /// Specifies how deeply nested this array is
    static constexpr size_t Depth = 1 + array_depth_v<Value>;

    /// Is this a mask array?
    static constexpr bool IsMask = IsMask_;

    /// Is this an array of values that can be added, multiplied, etc.?
    static constexpr bool IsArithmetic = std::is_arithmetic_v<Scalar> && !IsMask;

    /// Is this an array of signed or unsigned integer values?
    static constexpr bool IsIntegral = std::is_integral_v<Scalar> && !IsMask;

    /// Is this an array of floating point values?
    static constexpr bool IsFloat = std::is_floating_point_v<Scalar> && !IsMask;

    /// Does this array compute derivatives using automatic differentation?
    static constexpr bool IsDiff = is_diff_array_v<Value_>;

    /// Are elements of this array implemented using the LLVM backend?
    static constexpr bool IsLLVM = is_llvm_array_v<Value_>;

    /// Are elements of this array implemented using the CUDA backend?
    static constexpr bool IsCUDA = is_cuda_array_v<Value_>;

    /// Are elements of this array implemented using a JIT-compiled backend?
    static constexpr bool IsJIT = IsLLVM || IsCUDA;

    /// Are elements of this array dynamic?
    static constexpr bool IsDynamic = is_dynamic_array_v<Value_>;

    //! @}
    // -----------------------------------------------------------------------

    // -----------------------------------------------------------------------
    //! @{ \name Curiously Recurring Template design pattern
    // -----------------------------------------------------------------------

    /// Alias to the derived type
    using Derived = Derived_;

    /// Cast to derived type
    ENOKI_INLINE Derived &derived()             { return (Derived &) *this; }

    /// Cast to derived type (const version)
    ENOKI_INLINE const Derived &derived() const { return (Derived &) *this; }

    //! @}
    // -----------------------------------------------------------------------

    // -----------------------------------------------------------------------
    //! @{ \name Iterators
    // -----------------------------------------------------------------------

    ENOKI_INLINE auto begin() const { return derived().data(); }
    ENOKI_INLINE auto begin()       { return derived().data(); }
    ENOKI_INLINE auto end()   const { return derived().data() + derived().size(); }
    ENOKI_INLINE auto end()         { return derived().data() + derived().size(); }

    //! @}
    // -----------------------------------------------------------------------

    // -----------------------------------------------------------------------
    //! @{ \name Element access
    // -----------------------------------------------------------------------

    /// Recursive array indexing operator
    template <typename... Indices, enable_if_t<(sizeof...(Indices) >= 1)> = 0>
    ENOKI_INLINE decltype(auto) entry(size_t i0, Indices... indices) {
        return derived().entry(i0).entry(indices...);
    }

    /// Recursive array indexing operator (const)
    template <typename... Indices, enable_if_t<(sizeof...(Indices) >= 1)> = 0>
    ENOKI_INLINE decltype(auto) entry(size_t i0, Indices... indices) const {
        return derived().entry(i0).entry(indices...);
    }

    /// Array indexing operator with bounds checks in debug mode
    ENOKI_INLINE decltype(auto) operator[](size_t i) {
        #if !defined(NDEBUG) && !defined(ENOKI_DISABLE_RANGE_CHECK)
        if (i >= derived().size())
            enoki_raise("ArrayBase: out of range access (tried to "
                        "access index %zu in an array of size %zu)",
                        i, derived().size());
        #endif
        return derived().entry(i);
    }

    /// Array indexing operator with bounds checks in debug mode, const version
    ENOKI_INLINE decltype(auto) operator[](size_t i) const {
        #if !defined(NDEBUG) && !defined(ENOKI_DISABLE_RANGE_CHECK)
        if (i >= derived().size())
            enoki_raise("ArrayBase: out of range access (tried to "
                        "access index %zu in an array of size %zu)",
                        i, derived().size());
        #endif
        return derived().entry(i);
    }

    template <typename T>
    ENOKI_INLINE void set_entry(size_t i, const T &value) {
        derived().entry(i) = value;
    }

    template <typename Mask, enable_if_mask_t<Mask> = 0>
    ENOKI_INLINE auto operator[](const Mask &m) {
        return detail::MaskedArray<Derived>{ derived(),
                                             (const mask_t<Derived> &) m };
    }

    ENOKI_INLINE bool empty() const { return derived().size() == 0; }

    const CallSupport operator->() const {
        return CallSupport(derived());
    }

    //! @}
    // -----------------------------------------------------------------------

    // -----------------------------------------------------------------------
    //! @{ \name Fallback implementations of vertical operations
    // -----------------------------------------------------------------------

    #define ENOKI_IMPLEMENT_UNARY(name, op, cond)                            \
        Derived name##_() const {                                            \
            ENOKI_CHKSCALAR(#name "_");                                      \
            Derived result;                                                  \
                                                                             \
            if constexpr (cond) {                                            \
                size_t sa = derived().size();                                \
                                                                             \
                if constexpr (Derived::Size == Dynamic)                      \
                    result = enoki::empty<Derived>(sa);                      \
                                                                             \
                for (size_t i = 0; i < sa; ++i) {                            \
                    const Value &a = derived().entry(i);                     \
                    result.entry(i) = op;                                    \
                }                                                            \
            } else {                                                         \
                enoki_raise(#name "_(): invalid operand type!");             \
            }                                                                \
                                                                             \
            return result;                                                   \
        }

    #define ENOKI_IMPLEMENT_UNARY_TEMPLATE(name, arg, op, cond)              \
        template <arg> Derived name##_() const {                             \
            ENOKI_CHKSCALAR(#name "_");                                      \
            Derived result;                                                  \
                                                                             \
            if constexpr (cond) {                                            \
                size_t sa = derived().size();                                \
                                                                             \
                if constexpr (Derived::Size == Dynamic)                      \
                    result = enoki::empty<Derived>(sa);                      \
                                                                             \
                for (size_t i = 0; i < sa; ++i) {                            \
                    const Value &a = derived().entry(i);                     \
                    result.entry(i) = op;                                    \
                }                                                            \
            } else {                                                         \
                enoki_raise(#name "_(): invalid operand type!");             \
            }                                                                \
                                                                             \
            return result;                                                   \
        }

    #define ENOKI_IMPLEMENT_UNARY_PAIR(name, op, cond)                       \
        std::pair<Derived, Derived> name##_() const {                        \
            Derived result_1, result_2;                                      \
                                                                             \
            if constexpr (cond) {                                            \
                size_t sa = derived().size();                                \
                                                                             \
                if constexpr (Derived::Size == Dynamic) {                    \
                    result_1 = enoki::empty<Derived>(sa);                    \
                    result_2 = enoki::empty<Derived>(sa);                    \
                }                                                            \
                                                                             \
                for (size_t i = 0; i < sa; ++i) {                            \
                    const Value &a = derived().entry(i);                     \
                    auto result = op;                                        \
                    result_1.entry(i) = std::move(result.first);             \
                    result_2.entry(i) = std::move(result.second);            \
                }                                                            \
            } else {                                                         \
                enoki_raise(#name "_(): invalid operand type!");             \
            }                                                                \
                                                                             \
            return std::pair<Derived, Derived>(std::move(result_1),          \
                                               std::move(result_2));         \
        }

    #define ENOKI_IMPLEMENT_ROUND2INT(name)                                  \
        template <typename T> T name##2int_() const {                        \
            ENOKI_CHKSCALAR(#name "_");                                      \
            T result;                                                        \
                                                                             \
            if constexpr (!IsFloat) {                                        \
                enoki_raise(#name "_(): invalid operand type!");             \
            } else if constexpr (!std::is_scalar_v<Value>) {                 \
                size_t sa = derived().size();                                \
                                                                             \
                if constexpr (T::Size == Dynamic)                            \
                    result = enoki::empty<T>(sa);                            \
                                                                             \
                for (size_t i = 0; i < sa; ++i)                              \
                    result.entry(i) =                                        \
                        enoki::name##2int<value_t<T>> (derived().entry(i));  \
            } else {                                                         \
                result = T(enoki::name(derived()));                          \
            }                                                                \
                                                                             \
            return result;                                                   \
        }

    #define ENOKI_IMPLEMENT_BINARY(name, op, cond)                           \
        Derived name##_(const Derived &v) const {                            \
            ENOKI_CHKSCALAR(#name "_");                                      \
            Derived result;                                                  \
                                                                             \
            if constexpr (cond) {                                            \
                size_t sa = derived().size(), sb = v.size(),                 \
                       sr = sa > sb ? sa : sb;                               \
                                                                             \
                if constexpr (Derived::Size == Dynamic) {                    \
                    result = enoki::empty<Derived>(sr);                      \
                    if ((sa != sr && sa != 1) || (sb != sr && sb != 1))      \
                        enoki_raise(#name "_() : mismatched input sizes "    \
                                   "(%zu and %zu)", sa, sb);                 \
                }                                                            \
                                                                             \
                for (size_t i = 0; i < sr; ++i) {                            \
                    const Value &a = derived().entry(i);                     \
                    const Value &b = v.entry(i);                             \
                    result.entry(i) = op;                                    \
                }                                                            \
            } else {                                                         \
                enoki_raise(#name "_(): invalid operand type!");             \
            }                                                                \
                                                                             \
            return result;                                                   \
        }

    #define ENOKI_IMPLEMENT_BINARY_BITOP(name, op, cond)                     \
        template <typename Mask> Derived name##_(const Mask &v) const {      \
            ENOKI_CHKSCALAR(#name "_");                                      \
            Derived result;                                                  \
                                                                             \
            if constexpr (cond) {                                            \
                size_t sa = derived().size(), sb = v.size(),                 \
                       sr = sa > sb ? sa : sb;                               \
                                                                             \
                if constexpr (Derived::Size == Dynamic) {                    \
                    result = enoki::empty<Derived>(sr);                      \
                    if ((sa != sr && sa != 1) || (sb != sr && sb != 1))      \
                        enoki_raise(#name "_() : mismatched input sizes "    \
                                   "(%zu and %zu)", sa, sb);                 \
                }                                                            \
                                                                             \
                for (size_t i = 0; i < sr; ++i) {                            \
                    const Value &a = derived().entry(i);                     \
                    const auto &b = v.entry(i);                              \
                    result.entry(i) = op;                                    \
                }                                                            \
            } else {                                                         \
                enoki_raise(#name "_(): invalid operand type!");             \
            }                                                                \
                                                                             \
            return result;                                                   \
        }

    #define ENOKI_IMPLEMENT_BINARY_MASK(name, op, cond)                      \
        ENOKI_INLINE auto name##_(const Derived &v) const {                  \
            ENOKI_CHKSCALAR(#name "_");                                      \
            mask_t<Derived> result;                                          \
                                                                             \
            if constexpr (cond) {                                            \
                size_t sa = derived().size(), sb = v.size(),                 \
                       sr = sa > sb ? sa : sb;                               \
                                                                             \
                if constexpr (Derived::Size == Dynamic) {                    \
                    result = enoki::empty<mask_t<Derived>>(sr);              \
                    if ((sa != sr && sa != 1) || (sb != sr && sb != 1))      \
                        enoki_raise(#name "_() : mismatched input sizes "    \
                                   "(%zu and %zu)", sa, sb);                 \
                }                                                            \
                                                                             \
                for (size_t i = 0; i < sr; ++i) {                            \
                    const Value &a = derived().entry(i);                     \
                    const Value &b = v.entry(i);                             \
                    result.entry(i) = op;                                    \
                }                                                            \
            } else {                                                         \
                enoki_raise(#name "_(): invalid operand type!");             \
            }                                                                \
                                                                             \
            return result;                                                   \
        }

    #define ENOKI_IMPLEMENT_TERNARY_ALT(name, op, alt, cond)                 \
        Derived name##_(const Derived &v1, const Derived &v2) const {        \
            ENOKI_CHKSCALAR(#name "_");                                      \
            Derived result;                                                  \
                                                                             \
            if constexpr (!cond) {                                           \
                enoki_raise(#name "_(): invalid operand type!");             \
            } else if constexpr (!std::is_scalar_v<Value>) {                 \
                size_t sa = derived().size(), sb = v1.size(), sc = v2.size(),\
                       sd = sa > sb ? sa : sb, sr = sc > sd ? sc : sd;       \
                                                                             \
                if constexpr (Derived::Size == Dynamic) {                    \
                    result = enoki::empty<Derived>(sr);                      \
                    if ((sa != sr && sa != 1) || (sb != sr && sb != 1) ||    \
                        (sc != sr && sc != 1))                               \
                        enoki_raise(#name "_() : mismatched input sizes "    \
                                   "(%zu, %zu, and %zu)", sa, sb, sc);       \
                }                                                            \
                                                                             \
                for (size_t i = 0; i < sr; ++i) {                            \
                    const Value &a = derived().entry(i);                     \
                    const Value &b = v1.entry(i);                            \
                    const Value &c = v2.entry(i);                            \
                    result.entry(i) = op;                                    \
                }                                                            \
            } else {                                                         \
                return alt;                                                  \
            }                                                                \
                                                                             \
            return result;                                                   \
        }

    ENOKI_IMPLEMENT_BINARY(add,   a + b,       IsArithmetic)
    ENOKI_IMPLEMENT_BINARY(sub,   a - b,       IsArithmetic)
    ENOKI_IMPLEMENT_BINARY(mul,   a * b,       IsArithmetic)
    ENOKI_IMPLEMENT_BINARY(mulhi, mulhi(a, b), IsIntegral)
    ENOKI_IMPLEMENT_BINARY(div,   a / b,       IsArithmetic)
    ENOKI_IMPLEMENT_BINARY(mod,   a % b,       IsIntegral)

    ENOKI_IMPLEMENT_BINARY_BITOP(or,     detail::or_(a, b),     true)
    ENOKI_IMPLEMENT_BINARY_BITOP(and,    detail::and_(a, b),    true)
    ENOKI_IMPLEMENT_BINARY_BITOP(andnot, detail::andnot_(a, b), true)
    ENOKI_IMPLEMENT_BINARY_BITOP(xor,    detail::xor_(a, b),    true)

    ENOKI_IMPLEMENT_BINARY(sl, a << b, IsIntegral)
    ENOKI_IMPLEMENT_BINARY(sr, a >> b, IsIntegral)

    ENOKI_IMPLEMENT_UNARY_TEMPLATE(sl, int Imm, a << Imm, IsIntegral)
    ENOKI_IMPLEMENT_UNARY_TEMPLATE(sr, int Imm, a >> Imm, IsIntegral)

    ENOKI_IMPLEMENT_BINARY_MASK(eq,  enoki::eq(a, b), true)
    ENOKI_IMPLEMENT_BINARY_MASK(neq, enoki::neq(a, b), true)
    ENOKI_IMPLEMENT_BINARY_MASK(lt, a < b,  IsArithmetic)
    ENOKI_IMPLEMENT_BINARY_MASK(le, a <= b, IsArithmetic)
    ENOKI_IMPLEMENT_BINARY_MASK(gt, a > b,  IsArithmetic)
    ENOKI_IMPLEMENT_BINARY_MASK(ge, a >= b, IsArithmetic)

    ENOKI_IMPLEMENT_UNARY(neg, -a, IsArithmetic)
    ENOKI_IMPLEMENT_UNARY(not, detail::not_(a), !IsFloat)

    ENOKI_IMPLEMENT_UNARY(sqrt,  enoki::sqrt(a), IsFloat)
    ENOKI_IMPLEMENT_UNARY(abs,   enoki::abs(a), IsArithmetic)

    ENOKI_IMPLEMENT_UNARY(floor, enoki::floor(a), IsFloat)
    ENOKI_IMPLEMENT_UNARY(ceil,  enoki::ceil(a),  IsFloat)
    ENOKI_IMPLEMENT_UNARY(trunc, enoki::trunc(a), IsFloat)
    ENOKI_IMPLEMENT_UNARY(round, enoki::round(a), IsFloat)

    ENOKI_IMPLEMENT_ROUND2INT(trunc)
    ENOKI_IMPLEMENT_ROUND2INT(ceil)
    ENOKI_IMPLEMENT_ROUND2INT(floor)
    ENOKI_IMPLEMENT_ROUND2INT(round)

    ENOKI_IMPLEMENT_BINARY(min, enoki::min(a, b), IsArithmetic)
    ENOKI_IMPLEMENT_BINARY(max, enoki::max(a, b), IsArithmetic)

    ENOKI_IMPLEMENT_UNARY(rcp, enoki::rcp(a), IsFloat)
    ENOKI_IMPLEMENT_UNARY(rsqrt, enoki::rsqrt(a), IsFloat)

    ENOKI_IMPLEMENT_TERNARY_ALT(fmadd,  enoki::fmadd(a, b, c),   derived()*v1+v2, IsFloat)
    ENOKI_IMPLEMENT_TERNARY_ALT(fmsub,  enoki::fmsub(a, b, c),   derived()*v1-v2, IsFloat)
    ENOKI_IMPLEMENT_TERNARY_ALT(fnmadd, enoki::fnmadd(a, b, c), -derived()*v1+v2, IsFloat)
    ENOKI_IMPLEMENT_TERNARY_ALT(fnmsub, enoki::fnmsub(a, b, c), -derived()*v1-v2, IsFloat)

    template <typename T = Value, enable_if_array_t<T> = 0>
    ENOKI_IMPLEMENT_UNARY(sin, enoki::sin(a), IsFloat)
    template <typename T = Value, enable_if_array_t<T> = 0>
    ENOKI_IMPLEMENT_UNARY(cos, enoki::cos(a), IsFloat)

    template <typename T = Value, enable_if_array_t<T> = 0>
    ENOKI_IMPLEMENT_UNARY(csc, enoki::csc(a), IsFloat)
    template <typename T = Value, enable_if_array_t<T> = 0>
    ENOKI_IMPLEMENT_UNARY(sec, enoki::sec(a), IsFloat)
    template <typename T = Value, enable_if_array_t<T> = 0>
    ENOKI_IMPLEMENT_UNARY(tan, enoki::tan(a), IsFloat)
    template <typename T = Value, enable_if_array_t<T> = 0>
    ENOKI_IMPLEMENT_UNARY(cot, enoki::cot(a), IsFloat)
    template <typename T = Value, enable_if_array_t<T> = 0>
    ENOKI_IMPLEMENT_UNARY(asin, enoki::asin(a), IsFloat)
    template <typename T = Value, enable_if_array_t<T> = 0>
    ENOKI_IMPLEMENT_UNARY(acos, enoki::acos(a), IsFloat)
    template <typename T = Value, enable_if_array_t<T> = 0>
    ENOKI_IMPLEMENT_UNARY(atan, enoki::atan(a), IsFloat)
    template <typename T = Value, enable_if_array_t<T> = 0>
    ENOKI_IMPLEMENT_BINARY(atan2, enoki::atan2(a, b), IsFloat)
    template <typename T = Value, enable_if_array_t<T> = 0>
    ENOKI_IMPLEMENT_BINARY(ldexp, enoki::ldexp(a, b), IsFloat)

    template <typename T = Value, enable_if_array_t<T> = 0>
    ENOKI_IMPLEMENT_UNARY(exp2, enoki::exp2(a), IsFloat)
    template <typename T = Value, enable_if_array_t<T> = 0>
    ENOKI_IMPLEMENT_UNARY(exp, enoki::exp(a), IsFloat)
    template <typename T = Value, enable_if_array_t<T> = 0>
    ENOKI_IMPLEMENT_UNARY(log2, enoki::log2(a), IsFloat)
    template <typename T = Value, enable_if_array_t<T> = 0>
    ENOKI_IMPLEMENT_UNARY(log, enoki::log(a), IsFloat)
    template <typename T = Value, enable_if_array_t<T> = 0>
    ENOKI_IMPLEMENT_BINARY(pow, enoki::pow(a, b), IsFloat)

    template <typename T = Value, enable_if_array_t<T> = 0>
    ENOKI_IMPLEMENT_UNARY_PAIR(sincos, enoki::sincos(a), IsFloat)
    template <typename T = Value, enable_if_array_t<T> = 0>
    ENOKI_IMPLEMENT_UNARY_PAIR(frexp, enoki::frexp(a), IsFloat)

    #undef ENOKI_IMPLEMENT_UNARY
    #undef ENOKI_IMPLEMENT_UNARY_TEMPLATE
    #undef ENOKI_IMPLEMENT_ROUND2INT
    #undef ENOKI_IMPLEMENT_BINARY
    #undef ENOKI_IMPLEMENT_BINARY_BITOP
    #undef ENOKI_IMPLEMENT_BINARY_MASK
    #undef ENOKI_IMPLEMENT_TERNARY_ALT

    template <typename Mask>
    static ENOKI_INLINE auto select_(const Mask &m, const Derived &t, const Derived &f) {
        ENOKI_CHKSCALAR("select_");
        size_t sm = m.size(), st = t.size(), sf = f.size(),
               sd = sm > st ? sm : st, sr = sf > sd ? sf : sd;
        Derived result;

        if constexpr (Derived::Size == Dynamic) {
            result = enoki::empty<Derived>(sr);
            if ((sm != sr && sm != 1) || (st != sr && st != 1) ||
                (sf != sr && sf != 1))
                enoki_raise("select_() : mismatched input sizes "
                           "(%zu, %zu, and %zu)", sm, st, sf);
        }

        for (size_t i = 0; i < sr; ++i) {
            const auto &v_m = m.entry(sm > 1 ? i : 0);
            const Value &v_t = t.entry(st > 1 ? i : 0);
            const Value &v_f = f.entry(sf > 1 ? i : 0);
            result.entry(i) = enoki::select(v_m, v_t, v_f);
        }

        return result;
    }

    //! @}
    // -----------------------------------------------------------------------

    // -----------------------------------------------------------------------
    //! @{ \name Fallback implementations of horizontal operations
    // -----------------------------------------------------------------------

    template <size_t... Indices> ENOKI_INLINE Derived shuffle_() const {
        static_assert(sizeof...(Indices) == Derived::Size, "shuffle(): Invalid size!");
        ENOKI_CHKSCALAR("shuffle_");
        Derived out;
        size_t idx = 0;
        bool result[] = { (out.entry(idx++) = derived().entry(Indices % Derived::Size), false)... };
        (void) idx; (void) result;
        return out;
    }

    Value dot_(const Derived &a) const {
        Value result;
        if constexpr (IsArithmetic) {
			if constexpr (is_array_v<Value>) {
                size_t sa = derived().size(), sb = a.size(),
                       sr = sa > sb ? sa : sb;

                if constexpr (Derived::Size == Dynamic) {
                    if ((sa != sr && sa != 1) || (sb != sr && sb != 1))
                        enoki_raise("dot_() : mismatched input sizes "
                                    "(%zu and %zu)", sa, sb);
                    else if (sr == 0)
                        enoki_raise("dot_(): zero-sized array!");
                }

				result = derived().entry(0) * a.entry(0);
				if constexpr (std::is_floating_point_v<Scalar>) {
                    for (size_t i = 1; i < sr; ++i)
                        result = enoki::fmadd(derived().entry(i),
                                              a.entry(i), result);
                } else {
                    for (size_t i = 1; i < sr; ++i)
                        result += derived().entry(i) * a.entry(i);
                }
            } else {
				result = hsum(derived() * a);
			}
		}
        return result;
    }

    Derived dot_async_(const Derived &a) const { return dot_(a); }
    Derived hsum_async_()  const { return hsum_(); }
    Derived hprod_async_() const { return hprod_(); }
    Derived hmax_async_() const  { return hmax_(); }
    Derived hmin_async_() const  { return hmin_(); }

    Value hsum_() const {
        Value value;

        if constexpr (IsArithmetic) {
            if constexpr (Derived::Size == Dynamic) {
                if (empty())
                    enoki_raise("hsum_(): zero-sized array!");
            }

            value = derived().entry(0);
            for (size_t i = 1; i < derived().size(); ++i)
                value += derived().entry(i);
        } else {
            enoki_raise("hsum_(): invalid operand type!");
        }

        return value;
    }

    Value hprod_() const {
        Value value;

        if constexpr (IsArithmetic) {
            if constexpr (Derived::Size == Dynamic) {
                if (empty())
                    enoki_raise("hprod_(): zero-sized array!");
            }

            value = derived().entry(0);
            for (size_t i = 1; i < derived().size(); ++i)
                value *= derived().entry(i);
        } else {
            enoki_raise("hprod_(): invalid operand type!");
        }

        return value;
    }

    Value hmin_() const {
        Value value;

        if constexpr (IsArithmetic) {
            if constexpr (Derived::Size == Dynamic) {
                if (empty())
                    enoki_raise("hmin_(): zero-sized array!");
            }

            value = derived().entry(0);
            for (size_t i = 1; i < derived().size(); ++i)
                value = enoki::min(value, derived().entry(i));
        } else {
            enoki_raise("hmin_(): invalid operand type!");
        }

        return value;
    }

    Value hmax_() const {
        Value value;

        if constexpr (IsArithmetic) {
            if constexpr (Derived::Size == Dynamic) {
                if (empty())
                    enoki_raise("hmax_(): zero-sized array!");
            }

            value = derived().entry(0);
            for (size_t i = 1; i < derived().size(); ++i)
                value = enoki::max(value, derived().entry(i));
        } else {
            enoki_raise("hmax_(): invalid operand type!");
        }

        return value;
    }

    mask_t<Value> all_() const {
        mask_t<Value> value;

        if constexpr (IsMask) {
            if constexpr (Derived::Size == Dynamic) {
                if (empty())
                    enoki_raise("all_(): zero-sized array!");
            }

            value = derived().entry(0);
            for (size_t i = 1; i < derived().size(); ++i)
                value = value && derived().entry(i);
        } else {
            enoki_raise("all_(): invalid operand type!");
        }

        return value;
    }

    mask_t<Value> any_() const {
        mask_t<Value> value;

        if constexpr (IsMask) {
            if constexpr (Derived::Size == Dynamic) {
                if (empty())
                    enoki_raise("any_(): zero-sized array!");
            }

            value = derived().entry(0);
            for (size_t i = 1; i < derived().size(); ++i)
                value = value || derived().entry(i);
        } else {
            enoki_raise("any_(): invalid operand type!");
        }

        return value;
    }

    uint32_array_t<Value> count_() const {
        uint32_array_t<Value> value;

        if constexpr (IsMask) {
            if constexpr (Derived::Size == Dynamic) {
                if (empty())
                    enoki_raise("count_(): zero-sized array!");
            }
            value = select(derived().entry(0), 1, 0);
            for (size_t i = 1; i < derived().size(); ++i)
                value += select(derived().entry(i), 1, 0);
        } else {
            enoki_raise("count_(): invalid operand type!");
        }

        return value;
    }

    template <typename Mask, enable_if_t<Mask::Depth == 1> = 0>
    ENOKI_INLINE Value extract_(const Mask &mask) const {
        size_t sa = derived().size(), sb = mask.size(),
               sr = sa > sb ? sa : sb;

        for (size_t i = 0; i < sr; ++i) {
            bool m = mask.entry(i);
            if (m)
                return derived().entry(i);
        }

        return zero<Value>();
    }

    //! @}
    // -----------------------------------------------------------------------

    // -----------------------------------------------------------------------
    //! @{ \name Fallback implementations of scatter/gather/load/store ops
    // -----------------------------------------------------------------------

    template <bool Permute, typename Source, typename Index, typename Mask>
    static Derived gather_(Source &&source, const Index &index, const Mask &mask) {
        ENOKI_CHKSCALAR("gather_");
        Derived result;

        size_t sa = index.size(), sb = mask.size(),
               sr = sa > sb ? sa : sb;

        if constexpr (Derived::Size == Dynamic) {
            result = enoki::empty<Derived>(sr);
            if ((sa != sr && sa != 1) || (sb != sr && sb != 1))
                enoki_raise("gather_() : mismatched input sizes "
                            "(%zu and %zu)", sa, sb);
        }

        for (size_t i = 0; i < sr; ++i)
            result.entry(i) = enoki::gather<Value, Permute>(
                source, index.entry(i),
                mask.entry(i));

        return result;
    }

    template <bool Permute, typename Target, typename Index, typename Mask>
    void scatter_(Target &&target, const Index &index, const Mask &mask) const {
        ENOKI_CHKSCALAR("scatter_");

        size_t sa = derived().size(), sb = index.size(), sc = mask.size(),
               sd = sa > sb ? sa : sb, sr = sc > sd ? sc : sd;

        for (size_t i = 0; i < sr; ++i)
            enoki::scatter<Permute>(target, derived().entry(i),
                                    index.entry(i),
                                    mask.entry(i));
    }

    template <typename Target, typename Index, typename Mask>
    void scatter_add_(Target &&target, const Index &index, const Mask &mask) const {
        ENOKI_CHKSCALAR("scatter_add_");

        size_t sa = derived().size(), sb = index.size(), sc = mask.size(),
               sd = sa > sb ? sa : sb, sr = sc > sd ? sc : sd;

        for (size_t i = 0; i < sr; ++i)
            enoki::scatter_add(target, derived().entry(i), index.entry(i),
                               mask.entry(i));
    }

    static Derived load_(const void *mem, size_t size) {
        return Derived::load_unaligned_(mem, size);
    }

    void store_(void *mem, size_t size) const {
        return derived().store_unaligned_(mem, size);
    }

    //! @}
    // -----------------------------------------------------------------------
};

NAMESPACE_END(enoki)
