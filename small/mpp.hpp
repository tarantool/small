#pragma once
/*
 * Copyright 2010-2020, Tarantool AUTHORS, please see AUTHORS file.
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the
 *    following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY <COPYRIGHT HOLDER> ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * <COPYRIGHT HOLDER> OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */


#include <cstdint>
#include <iosfwd>
#include <tuple>
#include <type_traits>

namespace mpp {

// TODO: move to mpp_dec.hpp ?
enum Types : uint8_t {
	MP_NIL,
	MP_UINT,
	MP_INT,
	MP_STR,
	MP_BIN,
	MP_ARR,
	MP_MAP,
	MP_BOOL,
	MP_FLT,
	MP_DBL,
	MP_EXT,
	MP_END
};

inline const char *TypeNames[] = {
	"MP_NIL",
	"MP_UINT",
	"MP_INT",
	"MP_STR",
	"MP_BIN",
	"MP_ARR",
	"MP_MAP",
	"MP_BOOL",
	"MP_FLT",
	"MP_DBL",
	"MP_EXT",
	"MP_BAD" // note that "MP_BAD" stands for MP_END.
};

std::ostream& operator<<(std::ostream& strm, Types t) {
	if (t >= Types::MP_END)
		return strm << TypeNames[Types::MP_END]
			    << "(" << static_cast<uint32_t>(t) << ")";
	return strm << TypeNames[t];
}

// TODO: move to mpp_utils.hpp ?
uint8_t  bswap8(uint8_t x)   { return x; }
uint16_t bswap16(uint16_t x) { return __builtin_bswap16(x); }
uint32_t bswap32(uint32_t x) { return __builtin_bswap32(x); }
uint64_t bswap64(uint64_t x) { return __builtin_bswap64(x); }
uint32_t packf(float x) { uint32_t r; memcpy(&r, &x, sizeof(r)); return r; }
float unpackf(uint32_t x) { float r; memcpy(&r, &f, sizeof(r)); return r; }
uint64_t packd(double x) { uint64_t r; memcpy(&r, &x, sizeof(r)); return r; }
double unpackd(uint64_t x) { double r; memcpy(&r, &f, sizeof(r)); return r; }

uint8_t  bswap(uint8_t x)  { return bswap8(x); }
uint16_t bswap(uint16_t x) { return bswap16(x); }
uint32_t bswap(uint32_t x) { return bswap32(x); }
uint64_t bswap(uint64_t x) { return bswap64(x); }

[[noreturn]] void unreachable() { assert(false); }

// TODO: move to mpp_utils.hpp or mpp_traits?
template<typename T, typename _ = void>
struct is_iterable : std::false_type {};

template<typename T>
struct is_iterable<
	T,
	std::conditional_t<
		false,
		std::tuple<
			decltype(*std::cbegin(std::declval<T>())),
			decltype(*std::cend(std::declval<T>()))
		>,
		void
	>
> : public std::true_type {};

template<typename T>
constexpr bool is_iterable_v = is_iterable<T>::value;

template<typename T, typename _ = void>
struct is_kv_iterable : std::false_type {};

template<typename T>
struct is_kv_iterable<
	T,
	std::conditional_t<
		false,
		std::tuple<
			decltype(std::cbegin(std::declval<T>())->first),
			decltype(std::cbegin(std::declval<T>())->second),
			decltype(*std::cend(std::declval<T>()))
		>,
		void
	>
> : public std::true_type {};

template<typename T>
constexpr bool is_kv_iterable_v = is_kv_iterable<T>::value;

template<typename T>
struct is_tuple : std::false_type {};

template<class... Args>
struct is_tuple<std::tuple<Args...>> : std::true_type {};

template<typename T>
constexpr bool is_tuple_v = is_tuple::value;

#define DEFINE_WRAPPER(name) \
template <class T> \
struct name { \
	const T& value; \
	name(const T& arg) : value(arg) {} \
}; \
\
template <class T> \
struct name<T> as_##name(const T& t) { return name<T>{t}; } \
\
template<typename T> \
struct is_##name : std::false_type {}; \
\
template<class... Args> \
struct is_##name<name<Args...>> : std::true_type {}; \
\
template<typename T> \
constexpr bool is_##name##_v = is_##name::value

DEFINE_WRAPPER(arr);
DEFINE_WRAPPER(map);

#undef DEFINE_WRAPPER



// TODO: move to mpp_enc.hpp ?
template <class BUFFER>
class Enc
{
	using Buffer_t = BUFFER;
	using iterator_base_t = typename BUFFER::iterator;

public:
	struct iterator : public iterator_base_t
	{
		template <class T>
		void set_uint(const T&) const;
		template <class T>
		void set_int(const T&) const;
	};

	Enc(Buffer_t& buf) : m_Buf(buf) {}

	template <class T>
	iterator add_uint(T t)
	{
		static_assert(std::is_integral_v<T> && std::is_unsigned_v<T>);
		if constexpr (sizeof(T) >= 8) {
			if (t > UINT32_MAX)
			{
				uint8_t tag = 0xcf;
				iterator res = m_Buf.appendBack(1);
				m_Buf.set(res, tag);
				m_Buf.appendBack(bswap64(t));
				return res;
			}
		}
		if constexpr (sizeof(T) >= 4) {
			if (t > UINT16_MAX)
			{
				uint8_t tag = 0xce;
				iterator res = m_Buf.appendBack(1);
				m_Buf.set(res, tag);
				m_Buf.appendBack(bswap32(t));
				return res;
			}
		}
		if constexpr (sizeof(T) >= 2) {
			if (t > UINT8_MAX)
			{
				uint8_t tag = 0xcd;
				iterator res = m_Buf.appendBack(1);
				m_Buf.set(res, tag);
				m_Buf.appendBack(bswap16(t));
				return res;
			}
		}
		if (t > 0x7f)
		{
			uint8_t tag = 0xcc;
			iterator res = m_Buf.appendBack(1);
			m_Buf.set(res, tag);
			m_Buf.appendBack(bswap8(t));
			return res;
		} else {
			uint8_t tag = t;
			iterator res = m_Buf.appendBack(1);
			m_Buf.set(res, tag);
			return res;
		}
	}
	template <class T>
	iterator add_int(T t)
	{
		static_assert(std::is_integral_v<T> && std::is_signed_v<T>);
		if (t >= 0) {
			make_unsigned_t<T> x = t;
			return add_uint(t);
		}
		if constexpr (sizeof(T) >= 8) {
			if (t < INT32_MIN)
			{
				uint8_t tag = 0xd3;
				iterator res = m_Buf.appendBack(1);
				m_Buf.set(res, tag);
				m_Buf.appendBack(bswap64(t));
				return res;
			}
		}
		if constexpr (sizeof(T) >= 4) {
			if (t < INT16_MAX)
			{
				uint8_t tag = 0xd2;
				iterator res = m_Buf.appendBack(1);
				m_Buf.set(res, tag);
				m_Buf.appendBack(bswap32(t));
				return res;
			}
		}
		if constexpr (sizeof(T) >= 2) {
			if (t < INT8_MAX)
			{
				uint8_t tag = 0xd1;
				iterator res = m_Buf.appendBack(1);
				m_Buf.set(res, tag);
				m_Buf.appendBack(bswap16(t));
				return res;
			}
		}
		if (t < -31)
		{
			uint8_t tag = 0xd0;
			iterator res = m_Buf.appendBack(1);
			m_Buf.set(res, tag);
			m_Buf.appendBack(bswap8(t));
			return res;
		} else {
			uint8_t tag = t;
			iterator res = m_Buf.appendBack(1);
			m_Buf.set(res, tag);
			return res;
		}
	}
	iterator add_bool(bool b)
	{
		iterator res = m_Buf.appendBack(1);
		uint8_t tag = b ? 0xc3 : 0xc2;
		m_Buf.set(res, tag);
		return res;
	}
	iterator add_arr_tag(uint32_t size)
	{
		iterator res = m_Buf.appendBack(1);
		if (size < 16) {
			uint8_t tag = 0x80 + size;
			m_Buf.set(res, tag);
		} else if (size <= UINT16_MAX) {
			uint8_t tag = 0xdc;
			m_Buf.set(res, tag);
			m_Buf.appendBack(bswap16(size));
		} else {
			uint8_t tag = 0xdd;
			m_Buf.set(res, tag);
			m_Buf.appendBack(bswap32(size));
		}
		return res;
	}
	iterator add_map_tag(uint32_t size)
	{
		iterator res = m_Buf.appendBack(1);
		if (size < 16) {
			uint8_t tag = 0x80 + size;
			m_Buf.set(res, tag);
		} else if (size <= UINT16_MAX) {
			uint8_t tag = 0xdc;
			m_Buf.set(res, tag);
			m_Buf.appendBack(bswap16(size));
		} else {
			uint8_t tag = 0xdd;
			m_Buf.set(res, tag);
			m_Buf.appendBack(bswap32(size));
		}
		return res;
	}

	iterator add(const T& t) {
		if constexpr (std::is_integral_v<T> && std::is_unsigned_v<T>) {
			return add_uint(t);
		} else if constexpr (std::is_integral_v<T> && std::is_signed_v<T>) {
			return add_int(t);
		} else if constexpr (is_arr_v<T>) {
			if constexpr (is_iterable_v<T>) {
				iterator itr = add_arr_tag(t.size());
				for (const auto& x : t)
					add(x);
				return itr;
			} else if constexpr (is_tuple_v<T>) {
				iterator itr = add_arr_tag(std::tuple_size<T>::value);
				std::visit([](const auto& a){ add(a); }, t);
				return itr;
			} else {
				static_assert(fasle, "Wrong tuple was passed as array");
			}
		} else if constexpr (is_map_v<T>) {
			if constexpr (is_kv_iterable_v<T>) {
				iterator itr = add_map_tag(t.size());
				for (const auto& x : t) {
					add(x.first);
					add(x.second);
				}
				return itr;
			} else if constexpr (is_iterable_v<T>) {
				assert(t.size() % 2 == 0);
				iterator itr = add_map_tag(t.size() / 2);
				for (const auto& x : t)
					add(x);
				return itr;
			} else if constexpr (is_tuple_v<T>) {
				static_assert(std::tuple_size<T>::value % 2 == 0,
					      "Map expects even number of elements");
				iterator itr = add_map_tag(std::tuple_size<T>::value / 2);
				std::visit([](const auto& a){ add(a); }, t);
				return itr;
			} else {
				static_assert(fasle, "Wrong tuple was passed as map");
			}
		} else if constexpr (is_kv_iterable_v<T>::value) {
			return add(as_map(t));
		} else if constexpr (is_iterable_v<T>::value) {
			return add(as_arr(t));
		} else if constexpr (is_tuple_v<T>::value) {
			return add(as_arr(t));
		} else {
			static_assert(false, "Unknown type!");
		}

	}

private:
	BUFFER& m_Buf;
};

// TODO: move to mpp_enc.hpp ?
template <class BUFFER>
class Dec
{
	using Buffer_t = BUFFER;
	using iterator_base_t = typename BUFFER::iterator;

public:
	Dec(Buffer_t& buf) : m_Buf(buf) {}

	struct Item {
		Types type;
		uint8_t flags;
		union {
			int64_t uint_value;
			uint64_t int_value;
			uint32_t str_size;
			uint32_t bin_size;
			uint32_t arr_size;
			uint32_t map_size;
			bool bool_value;
			float flt_value;
			double dbl_value;
			// TODO: MP_EXT
		};
		Item *child;
		Item *next1;
		Item *next2;

	};

private:
	BUFFER& m_Buf;
};


} // namespace mpp {
