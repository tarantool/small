#ifndef VEB12S_H_INCLUDED
#define VEB12S_H_INCLUDED

#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

struct veb12static {
	uint64_t upper;
	uint64_t lower[64];
};

inline void
veb12static_init(struct veb12static *v)
{
	v->upper = 0;
	for (int i = 0; i < 64; i++)
		v->lower[i] = 0;
}

inline bool
veb12static_has(const struct veb12static *v, uint32_t val)
{
	assert(val < 4096);
	uint32_t upper = val >> 6;
	uint32_t lower = val & 0x3F;
	return (bool)(v->lower[upper] & (1ull << lower));
}

inline void
veb12static_insert(struct veb12static *v, uint32_t val)
{
	assert(val < 4096);
	uint32_t upper = val >> 6;
	uint32_t lower = val & 0x3F;
	v->upper |= (1ull << upper);
	v->lower[upper] |= (1ull << lower);
}

inline void
veb12static_delete(struct veb12static *v, uint32_t val)
{
	assert(val < 4096);
	uint32_t upper = val >> 6;
	uint32_t lower = val & 0x3F;
	v->lower[upper] &= ~(1ull << lower);
	v->upper &= ~((v->lower[upper] ? 0ull : 1ull) << upper);
}

inline void
veb12static_cond_set(uint64_t *val, uint64_t mask, bool flag)
{
	*val = (*val & ~mask) | ((-(uint64_t)flag) & mask);
}

inline void
veb12static_set(struct veb12static *v, uint32_t val, bool present)
{
	assert(val < 4096);
	uint32_t upper = val >> 6;
	uint32_t lower = val & 0x3F;
	veb12static_cond_set(&v->lower[upper], 1ull << lower, present);
	veb12static_cond_set(&v->upper, 1ull << upper, v->lower[upper]);
}

inline uint32_t
veb12static_lower_bound(const struct veb12static *v, uint32_t val)
{
	assert(val < 4096);
	uint32_t upper = val >> 6;
	uint32_t lower = val & 0x3F;
	uint64_t lower_mask = UINT64_MAX << lower;
	lower_mask &= v->lower[upper];
	uint64_t upper_mask = UINT64_MAX << upper << (lower_mask ? 0 : 1);
	upper_mask &= v->upper;
	upper = upper_mask ? __builtin_ctzll(upper_mask) : (1u << 6);
	lower_mask = upper_mask ? lower_mask ? lower_mask : v->lower[upper] : 1;
	lower = __builtin_ctzll(lower_mask);
	return (upper << 6) | lower;
}

inline uint32_t
veb12static_calc_size(const struct veb12static *v)
{
	uint32_t res = 0;
	for (int i = 0; i < 64; i++)
		res += __builtin_popcountll(v->lower[i]);
	return res;
}

inline int
veb12static_check(const struct veb12static *v)
{
	int res = 0;
	for (int i = 0; i < 64; i++) {
		bool has_upper = (bool)(v->upper & (1ull << i));
		bool has_lower = (bool)(v->lower[i]);
		if (has_upper && !has_lower)
			res |= 1;
		else if (!has_upper && has_lower)
			res |= 2;
	}
	return res;
}

#endif /* #ifndef VEB12S_H_INCLUDED */
