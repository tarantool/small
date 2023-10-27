#include <small/matras.h>
#include <set>
#include <vector>
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include "unit.h"

static void *
pta_alloc(void *ctx);
static void
pta_free(void *ctx, void *p);

#define PROV_BLOCK_SIZE 16
#define PROV_EXTENT_SIZE 64

static size_t AllocatedCount = 0;
static std::set<void*> AllocatedBlocks;
static std::set<void*> AllocatedItems;

bool alloc_err_inj_enabled = false;
unsigned int alloc_err_inj_countdown = 0;

#define MATRAS_VERSION_COUNT 8

static void *
pta_alloc(void *ctx)
{
	static_cast<void>(ctx);
	if (alloc_err_inj_enabled) {
		if (alloc_err_inj_countdown == 0)
			return 0;
		alloc_err_inj_countdown--;
	}
	void *p = new char[PROV_EXTENT_SIZE];
	AllocatedCount++;
	AllocatedBlocks.insert(p);
	return p;
}
static void
pta_free(void *ctx, void *p)
{
	static_cast<void>(ctx);
	fail_unless(AllocatedBlocks.find(p) != AllocatedBlocks.end());
	AllocatedBlocks.erase(p);
	delete [] static_cast<char *>(p);
	AllocatedCount--;
}

void matras_alloc_test()
{
	plan(1);
	header();

	unsigned int maxCapacity =  PROV_EXTENT_SIZE / PROV_BLOCK_SIZE;
	maxCapacity *= PROV_EXTENT_SIZE / sizeof(void *);
	maxCapacity *= PROV_EXTENT_SIZE / sizeof(void *);

	struct matras mat;

	alloc_err_inj_enabled = false;
	for (unsigned int i = 0; i <= maxCapacity; i++) {
		matras_create(&mat, PROV_EXTENT_SIZE, PROV_BLOCK_SIZE, pta_alloc, pta_free, NULL, NULL);
		fail_unless(mat.capacity == maxCapacity);
		AllocatedItems.clear();
		for (unsigned int j = 0; j < i; j++) {
			unsigned int res = 0;
			void *data = matras_alloc(&mat, &res);
			fail_unless(data != NULL);
			void *test_data = matras_get(&mat, res);
			fail_unless(data == test_data);
			size_t provConsumedMemory = (size_t)matras_extent_count(&mat) * PROV_EXTENT_SIZE;
			fail_unless(provConsumedMemory == AllocatedCount * PROV_EXTENT_SIZE);
			fail_unless(res == j);
			{
				fail_unless(!AllocatedBlocks.empty());
				std::set<void*>::iterator itr = AllocatedBlocks.lower_bound(data);
				if (itr == AllocatedBlocks.end() || *itr != data) {
					fail_unless(itr != AllocatedBlocks.begin());
					--itr;
				}
				fail_unless(itr != AllocatedBlocks.end());
				fail_unless(data <= (void*)( ((char*)(*itr)) + PROV_EXTENT_SIZE - PROV_BLOCK_SIZE));
			}
			{
				if (!AllocatedItems.empty()) {
					std::set<void*>::iterator itr = AllocatedItems.lower_bound(data);
					if (itr != AllocatedItems.end()) {
						fail_unless(*itr >= (void*)(((char*)data) + PROV_BLOCK_SIZE));
					}
					if (itr != AllocatedItems.begin()) {
						--itr;
						fail_unless(data >= (void*)(((char*)(*itr)) + PROV_BLOCK_SIZE));
					}
				}
			}
			AllocatedItems.insert(data);
		}
		size_t provConsumedMemory = (size_t)matras_extent_count(&mat) * PROV_EXTENT_SIZE;
		fail_unless(provConsumedMemory == AllocatedCount * PROV_EXTENT_SIZE);
		matras_destroy(&mat);
		fail_unless(AllocatedCount == 0);
	}

	for (unsigned int i = 0; i <= maxCapacity; i++) {
		matras_create(&mat, PROV_EXTENT_SIZE, PROV_BLOCK_SIZE, pta_alloc, pta_free, NULL, NULL);
		for (unsigned int j = 0; j < i; j++) {
			unsigned int res = 0;
			(void) matras_alloc(&mat, &res);
		}
		for (unsigned int j = 0; j < i; j++) {
			matras_dealloc(&mat);
			size_t provConsumedMemory = (size_t)matras_extent_count(&mat) * PROV_EXTENT_SIZE;
			fail_unless(provConsumedMemory == AllocatedCount * PROV_EXTENT_SIZE);
		}
		fail_unless(AllocatedCount == 0);
		matras_destroy(&mat);
	}

	alloc_err_inj_enabled = true;
	for (unsigned int i = 0; i <= maxCapacity; i++) {
		matras_create(&mat, PROV_EXTENT_SIZE, PROV_BLOCK_SIZE, pta_alloc, pta_free, NULL, NULL);

		alloc_err_inj_countdown = i;

		for (unsigned int j = 0; j < maxCapacity; j++) {
			unsigned int res = 0;
			unsigned int prev_block_count = mat.head.block_count;
			void *data = matras_alloc(&mat, &res);
			if (!data) {
				fail_unless(prev_block_count == mat.head.block_count);
				break;
			}
		}
		matras_destroy(&mat);
		fail_unless(AllocatedCount == 0);
	}
	ok(true);

	footer();
	check_plan();
}

typedef uint64_t type_t;
const size_t VER_EXTENT_SIZE = 512;

void *all(void *ctx)
{
	long *extents_in_use = static_cast<long *>(ctx);
	++*extents_in_use;
	return malloc(VER_EXTENT_SIZE);
}

void dea(void *ctx, void *p)
{
	long *extents_in_use = static_cast<long *>(ctx);
	--*extents_in_use;
	free(p);
}

struct matras_view views[MATRAS_VERSION_COUNT];
int vermask = 1;

int reg_view_id()
{
	int id = __builtin_ctz(~vermask);
	vermask |= 1 << id;
	return id;
}

void unreg_view_id(int id)
{
	vermask &=~ (1 << id);
}

void
matras_vers_test()
{
	plan(1);
	header();

	std::vector<type_t> comps[MATRAS_VERSION_COUNT];
	int use_mask = 1;
	int cur_num_or_ver = 1;
	struct matras local;
	long extents_in_use = 0;
	matras_create(&local, VER_EXTENT_SIZE, sizeof(type_t), all, dea, &extents_in_use, NULL);
	type_t val = 0;
	for (int s = 10; s < 8000; s = int(s * 1.5)) {
		for (int k = 0; k < 800; k++) {
			if (rand() % 16 == 0) {
				bool add_ver;
				if (cur_num_or_ver == 1)
					add_ver = true;
				else if (cur_num_or_ver == MATRAS_VERSION_COUNT)
					add_ver = false;
				else
					add_ver = rand() % 2 == 0;
				if (add_ver) {
					cur_num_or_ver++;
					matras_id_t new_ver = reg_view_id();
					matras_create_read_view(&local, views + new_ver);
					fail_unless(new_ver > 0);
					use_mask |= (1 << new_ver);
					comps[new_ver] = comps[0];
				} else {
					cur_num_or_ver--;
					int del_ver;
					do {
						del_ver = 1 + rand() % (MATRAS_VERSION_COUNT - 1);
					} while ((use_mask & (1 << del_ver)) == 0);
					matras_destroy_read_view(&local, views + del_ver);
					unreg_view_id(del_ver);
					comps[del_ver].clear();
					use_mask &= ~(1 << del_ver);
				}
			} else {
				if (rand() % 8 == 0 && comps[0].size() > 0) {
					matras_dealloc(&local);
					comps[0].pop_back();
				}
				size_t p = rand() % s;
				type_t mod = 0;
				while (p >= comps[0].size()) {
					comps[0].push_back(val * 10000 + mod);
					matras_id_t tmp;
					type_t *ptrval = (type_t *)matras_alloc(&local, &tmp);
					*ptrval = val * 10000 + mod;
					mod++;
				}
				val++;
				comps[0][p] = val;
				matras_touch(&local, p);
				*(type_t *)matras_get(&local, p) = val;
			}
			views[0] = local.head;

			for (int i = 0; i < MATRAS_VERSION_COUNT; i++) {
				if ((use_mask & (1 << i)) == 0)
					continue;
				fail_unless(comps[i].size() == views[i].block_count);
				for (size_t j = 0; j < comps[i].size(); j++) {
					type_t val1 = comps[i][j];
					type_t val2 = *(type_t *)matras_view_get(&local, views + i, j);
					fail_unless(val1 == val2);
				}
			}
		}
	}
	matras_destroy(&local);
	ok(extents_in_use == 0);

	footer();
	check_plan();
}

void
matras_gh_1145_test()
{
	header();
	plan(1);

	struct matras local;
	long extents_in_use = 0;
	matras_create(&local, VER_EXTENT_SIZE, sizeof(type_t), all, dea, &extents_in_use, NULL);
	struct matras_view view;
	matras_create_read_view(&local, &view);
	matras_id_t id;
	fail_unless(matras_alloc(&local, &id) != NULL);
	fail_unless(matras_touch(&local, id) != NULL);
	matras_destroy_read_view(&local, &view);
	matras_destroy(&local);
	ok(extents_in_use == 0);

	footer();
	check_plan();
}

void
matras_stats_test()
{
	header();
	plan(12);

	struct matras_stats stats;
	matras_stats_create(&stats);
	ok(stats.extent_count == 0);
	ok(stats.read_view_extent_count == 0);

	long extents_in_use = 0;
	struct matras matras;
	matras_create(&matras, VER_EXTENT_SIZE, sizeof(type_t), all, dea, &extents_in_use, &stats);
	ok(stats.extent_count == 0);
	ok(stats.read_view_extent_count == 0);

	matras_id_t id;
	fail_unless(matras_alloc(&matras, &id) != NULL);
	ok(stats.extent_count == 3);
	ok(stats.read_view_extent_count == 0);

	struct matras_view view;
	matras_create_read_view(&matras, &view);
	fail_unless(matras_touch(&matras, id) != NULL);
	ok(stats.extent_count == 6);
	ok(stats.read_view_extent_count == 3);

	matras_destroy_read_view(&matras, &view);
	ok(stats.extent_count == 3);
	ok(stats.read_view_extent_count == 0);

	matras_destroy(&matras);
	ok(stats.extent_count == 0);
	ok(stats.read_view_extent_count == 0);

	footer();
	check_plan();
}

void
matras_alloc_overflow_test()
{
	header();
	plan(2);

	size_t max_capacity = PROV_EXTENT_SIZE / PROV_BLOCK_SIZE;
	max_capacity *= PROV_EXTENT_SIZE / sizeof(void *);
	max_capacity *= PROV_EXTENT_SIZE / sizeof(void *);

	matras_id_t id;
	struct matras mat;
	matras_create(&mat, PROV_EXTENT_SIZE, PROV_BLOCK_SIZE, pta_alloc,
		      pta_free, NULL, NULL);
	for (size_t i = 0; i < max_capacity; i++) {
		void *data = matras_alloc(&mat, &id);
		fail_unless(data != NULL);
		fail_unless(id == i);
	}
	/* Try to exceed the maximum capacity. */
	ok(matras_alloc(&mat, &id) == NULL);
	ok(id == max_capacity - 1);
	matras_destroy(&mat);

	footer();
	check_plan();
}

void
matras_alloc_range_overflow_test()
{
	header();
	plan(2);

	const size_t range_count = 4;
	size_t max_capacity = PROV_EXTENT_SIZE / PROV_BLOCK_SIZE;
	max_capacity *= PROV_EXTENT_SIZE / sizeof(void *);
	max_capacity *= PROV_EXTENT_SIZE / sizeof(void *);

	matras_id_t id;
	struct matras mat;
	matras_create(&mat, PROV_EXTENT_SIZE, PROV_BLOCK_SIZE, pta_alloc,
		      pta_free, NULL, NULL);
	for (size_t i = 0; i < max_capacity; i += range_count) {
		void *data = matras_alloc_range(&mat, &id, range_count);
		fail_unless(data != NULL);
		fail_unless(id == i);
	}
	/* Try to exceed the maximum capacity. */
	ok(matras_alloc_range(&mat, &id, range_count) == NULL);
	ok(id == max_capacity - range_count);
	matras_destroy(&mat);

	footer();
	check_plan();
}

int
main(int, const char **)
{
	plan(6);
	header();

	matras_alloc_test();
	matras_vers_test();
	matras_gh_1145_test();
	matras_stats_test();
	matras_alloc_overflow_test();
	matras_alloc_range_overflow_test();

	footer();
	return check_plan();
}
