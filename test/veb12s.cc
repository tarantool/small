#include <small/veb12s.h>

#include "unit.h"
#include <stdint.h>
#include <stdbool.h>
#include <set>

using namespace std;

const size_t test_count = 200000;

int
main(int n, char **a)
{
	(void)n;
	(void)a;

	plan(6);
	bool has_mismatch1 = false;
	bool min_mismatch1 = false;
	bool status_mismatch1 = false;
	bool has_mismatch2 = false;
	bool min_mismatch2 = false;
	bool status_mismatch2 = false;

	struct veb12static v;
	veb12static_init(&v);
	std::set<uint32_t> s;
	s.insert(4096);
	for (size_t i = 0; i < test_count; i++) {
		uint32_t val = rand() % 4096;
		bool has1 = veb12static_has(&v, val);
		bool has2 = s.find(val) != s.end();
		if (has1 != has2)
			has_mismatch1 = true;
		uint32_t min1 = veb12static_lower_bound(&v, val);
		std::set<uint32_t>::iterator itr = s.lower_bound(val);
		uint32_t min2 = *itr;
		if (min1 != min2)
			min_mismatch1 = true;
		if (has1) {
			veb12static_delete(&v, val);
			s.erase(val);
		} else {
			veb12static_insert(&v, val);
			s.insert(val);
		}
		int status = veb12static_check(&v);
		if (status)
			status_mismatch1 = true;
	}
	for (size_t i = 0; i < test_count; i++) {
		uint32_t val = rand() % 4096;
		bool toset = rand() % 2;
		bool has1 = veb12static_has(&v, val);
		bool has2 = s.find(val) != s.end();
		if (has1 != has2)
			has_mismatch2 = true;
		uint32_t min1 = veb12static_lower_bound(&v, val);
		std::set<uint32_t>::iterator itr = s.lower_bound(val);
		uint32_t min2 = *itr;
		if (min1 != min2)
			min_mismatch2 = true;
		veb12static_set(&v, val, toset);
		if (has1) {
			if (!toset)
				s.erase(val);
		} else {
			if (toset)
				s.insert(val);
		}
		int status = veb12static_check(&v);
		if (status)
			status_mismatch2 = true;
	}

	ok(!has_mismatch1, "content matches (1)");
	ok(!min_mismatch1, "find minimum is correct (1)");
	ok(!status_mismatch1, "status ok (1)");
	ok(!has_mismatch2, "content matches (2)");
	ok(!min_mismatch2, "find minimum is correct (2)");
	ok(!status_mismatch2, "status ok (2)");

	return check_plan();
}
