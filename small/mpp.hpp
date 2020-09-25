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
#include <type_traits>

namespace mpp {


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
	template <class T>
	iterator add(T&& t) {
		using U = std::remove_reference_t<T>;
		if constexpr (std::is_integral_v<U> && std::is_unsigned_v<U>)
			return add_uint(t);
		else if constexpr (std::is_integral_v<U> && std::is_signed_v<U>)
			return add_int(t);
		else
			unreachable();

	}


private:
	BUFFER& m_Buf;
};



} // namespace mpp {
