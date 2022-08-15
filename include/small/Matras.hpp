#pragma once

#include <cstddef>
#include <new>
#include <utility>

namespace tnt {

struct VariadicUnionNothing {};

template <class T, class ...MORE>
struct VariadicUnionMore;

template <class ...T>
struct VariadicUnion {
	union {
		VariadicUnionNothing nothing;
		VariadicUnionMore<T...> more;
	};
	VariadicUnion() : nothing() {}
	template <class V, class ...Args> V &construct(Args &&...args);
	template <class V> void destroy();
	template <class V> V &get();
	template <class V> V &&get() &&;
	template <class V> const V &get() const;
};

//////////////////////////////////////////////////////////////////////
//////////////////////////  Implementation  //////////////////////////
//////////////////////////////////////////////////////////////////////

template <class T, class ...MORE>
struct VariadicUnionMore {
	union {
		T value;
		VariadicUnionMore<MORE...> more;
	};
};

template <class T>
struct VariadicUnionMore<T> {
	T value;
};

template <class V, class T, class ...MORE>
struct VariadicUnionExtractor {
	static V &get(VariadicUnionMore<T, MORE...>& u)
	{
		return VariadicUnionExtractor<V, MORE...>::get(u.more);
	}
	static const V &get(const VariadicUnionMore<T, MORE...>& u)
	{
		return VariadicUnionExtractor<V, MORE...>::get(u.more);
	}
	static V &&get(const VariadicUnionMore<T, MORE...>&& u)
	{
		return VariadicUnionExtractor<V, MORE...>::get(u.more);
	}
};

template <class T, class ...MORE>
struct VariadicUnionExtractor<T, T, MORE...> {
	static T &get(VariadicUnionMore<T, MORE...>& u)
	{
		return u.value;
	}
	static const T &get(const VariadicUnionMore<T, MORE...>& u)
	{
		return u.value;
	}
	static T &&get(const VariadicUnionMore<T, MORE...>&& u)
	{
		return u.value;
	}
};

template <class ...T>
template <class V, class ...Args>
V &VariadicUnion<T...>::construct(Args &&...args)
{
	nothing.~VariadicUnionNothing();
	V &value = VariadicUnionExtractor<V, T...>::get(more);
	return *::new (&value) V(std::forward<Args>(args)...);
}

template <class ...T>
template <class V>
void VariadicUnion<T...>::destroy()
{
	V &value = VariadicUnionExtractor<V, T...>::get(more);
	value.~V();
	new (&nothing) VariadicUnionNothing;
}

template <class ...T>
template <class V>
V &VariadicUnion<T...>::get()
{
	VariadicUnionExtractor<V, T...>::get(more);
}

template <class ...T>
template <class V>
V &&VariadicUnion<T...>::get() &&
{
	VariadicUnionExtractor<V, T...>::get(more);
}

template <class ...T>
template <class V>
const V &VariadicUnion<T...>::get() const
{
	VariadicUnionExtractor<V, T...>::get(more);
}




/**
 * Helper that checks that all types in list has the same size.
 */
template <class ...T>
struct MatrasTypesChecker;

template <class T, class U, class ...M>
struct MatrasTypesChecker<T, U, M...>
{
	static constexpr bool next = MatrasTypesChecker<T, M...>::are_same_size;
	static constexpr bool are_same_size = sizeof(T) == sizeof(U) && next;
};

template <class T>
struct MatrasTypesChecker<T>
{
	static const bool are_same_size = true;
};

/**
 * Type list for class Matras class.
 */
template <class T, class ...MORE>
struct MatrasTypes {
	using first_t = T;
	using union_t = VariadicUnion<T, MORE...>;
	constexpr static bool is_one_type = sizeof...(MORE) == 0;
	constexpr static bool are_same_size =
		MatrasTypesChecker<T, MORE...>::are_same_size;
};

/**
 * Helper that normalizes type list for Matras class.
 */
template <class T>
struct MatrasTypesNormalizer {
	using types = MatrasTypes<T>;
};

template <class ...T>
struct MatrasTypesNormalizer<MatrasTypes<T...>> {
	using types = MatrasTypes<T...>;
};


template <size_t ExtentSize>
struct MatrasDefaultAllocator {
	static constexpr size_t EXTENT_SIZE = ExtentSize;
	void *alloc();
	void free(void *ptr);
};

template <class Types, size_t ExtentSize = 16 * 1024, size_t NumLvl = 3,
	  class IdType = uint32_t,
	  class Allocator = MatrasDefaultAllocator<ExtentSize> >
class Matras {
public:
	// Convenient constants and types.
	using id_t = IdType;
	using types_list_t = typename MatrasTypesNormalizer<Types>::types;
	using first_t = typename types_list_t::first_t;
	using union_t = typename types_list_t::union_t;
	static constexpr size_t OBJECT_SIZE = sizeof(first_t);
	static constexpr size_t EXTENT_SIZE = ExtentSize;
	static constexpr size_t NUM_LVL = NumLvl;

	// General API.
	template <class T>
	const T &get(id_t id) const;
	template <class T>
	T &wget(id_t id);

	// Single-type versions.
	const first_t &get(id_t id) const
	{
		static_assert(types_list_t::is_one_type,
			      "The type must be specified for multitype matras");
		return get<first_t>(id);
	}

	first_t& wget(id_t id)
	{
		static_assert(types_list_t::is_one_type,
			      "The type must be specified for multitype matras");
		return wget<first_t>(id);
	}

	template <size_t Count, class T = first_t>
	T *add(id_t &id);
	template <size_t Count, class T = first_t>
	void remove();
	T &add(id_t &id) { return add<1>(id); }
	void remove() { return remove<1>(); }

	// Check template arguments.
	static_assert(types_list_t::are_same_size,
		      "objects must be of the same size");
	static_assert((OBJECT_SIZE & (OBJECT_SIZE - 1)) == 0,
		      "size of object(s) must be 2^n");
	static_assert((EXTENT_SIZE & (EXTENT_SIZE - 1)) == 0,
		      "ExtentSize must be 2^n");
	static_assert(EXTENT_SIZE > OBJECT_SIZE,
		      "ExtentSize must be greater that object size");
	static_assert(EXTENT_SIZE % OBJECT_SIZE == 0,
		      "ExtentSize must be multiple of object size");
	static_assert(NUM_LVL > 0,
		      "NumLvl must be positive");
	static_assert(EXTENT_SIZE == Allocator::EXTENT_SIZE,
		      "Allocator has wrong size of allocation");

	// More check for a better sleep.
	static_assert(sizeof(first_t) == sizeof(union_t), "cant't be");

private:
	struct Nothing {};
	struct Optional {
		union {
			Nothing nothing;
			T value;
		};
		Optional() : nothing(Nothing{}) {}
		~Optional() { nothing.~Nothing(); }
		template <class ...Args>
		T &construct(Args &&...args)
		{
			nothing.~Nothing();
			return *new (&value) T(std::forward<Args>(args)...);
		}
		void destroy()
		{
			value.~T();
			new (&nothing) Nothing;
		}
	};
	static_assert(sizeof(Optional) == sizeof(T)

	class LastLevelBlock {
	public:
		T &operator[](size_t i) { return data[i]; }
		const T &operator[](size_t i) const { return data[i]; }
		template <class ...Args>
		T &construct(size_t i, Args &&...args)
		{
			return *new (&data[i]) T(std::forward<Args>(args)...);
		}
		void destroy(size_t i)
		{
			data[i].~T();
		}

		union {
			char raw[BLOCK_SIZE]{};
			T data[BLOCK_SIZE / sizeof(T)];
		};
	};


	template <size_t Level>
	class Block {
	public:
		using Next_t = Block<Level - 1>;
		Next_t &operator[](size_t i) { return data[i]; }
		const Next_t &operator[](size_t i) const { return data[i]; }
		Next_t &construct(size_t i, Next_t &a) {data[i] = &a; return a;}
		void destroy(size_t i) {}
	private:
		Next_t *data[BLOCK_SIZE / sizeof(Next_t *)];
	};

};

//////////////////////////////////////////////////////////////////////
//////////////////////////  Implementation  //////////////////////////
//////////////////////////////////////////////////////////////////////

template <size_t ExtentSize>
void *MatrasDefaultAllocator<ExtentSize>::alloc()
{
	return ::operator new(EXTENT_SIZE);
}
template <size_t ExtentSize>
void MatrasDefaultAllocator<ExtentSize>::free(void *ptr)
{
	::operator delete(ptr);
}


} // namespace tnt