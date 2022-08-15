#pragma once

#include <cstddef>
#include <new>
#include <utility>

namespace tnt {

template <size_t ExtentSize>
struct MatrasDefaultAllocator {
	static constexpr size_t EXTENT_SIZE = ExtentSize;
	void *alloc()
	{
		return ::operator new(EXTENT_SIZE);
	}
	void free(void* ptr)
	{
		::operator delete(ptr);
	}
};

template <class T, size_t ExtentSize = 16 * 1024, size_t NumLvl = 3,
	  class Allocator = MatrasDefaultAllocator<ExtentSize>>
class Matras {
public:
	// Convenient constants and types.
	using id_t = uint32_t;
	static constexpr size_t BLOCK_SIZE = sizeof(T);
	static constexpr size_t EXTENT_SIZE = ExtentSize;
	static constexpr size_t NUM_LVL = NumLvl;

	// General API.
	const T &get(id_t id) const;
	T &wget(id_t id);

	template <size_t Count>
	T *add(id_t &id);
	template <size_t Count>
	void remove();
	T &add(id_t &id) { return add<1>(id); }
	void remove() { return remove<1>(); }

	// Check template arguments.
	static_assert((EXTENT_SIZE & (EXTENT_SIZE - 1)) == 0, "must be 2^n");
	static_assert(EXTENT_SIZE > BLOCK_SIZE, "must be greater");
	static_assert(EXTENT_SIZE % BLOCK_SIZE == 0, "must be multiple");
	static_assert(NUM_LVL > 0, "must be positive");


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


struct Data {
	Data() { exit(0); }
	int i;
};

struct Test {
	using T = char[4];
	union {
		char c = 'a';
		Data d;
	};
	Test() : c(char{}) {}
};

void g(void *ptr)
{
	free(ptr);
}

void f()
{
	Test t;
	g(&t);

}




} // namespace tnt