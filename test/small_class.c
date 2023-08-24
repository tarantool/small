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
#include "small/small_class.h"
#include "unit.h"
#include <math.h>
#include <stdbool.h>

#ifndef lengthof
#define lengthof(array) (sizeof (array) / sizeof ((array)[0]))
#endif

static void
test_class(void)
{
	plan(1);
	header();

	struct small_class sc;
	float actual_factor;
	small_class_create(&sc, 2, 1.2, 12, &actual_factor);
	unsigned class = small_class_calc_offset_by_size(&sc, 0);
	unsigned class_size = small_class_calc_size_by_offset(&sc, class);

	for (unsigned size = 1; size <= 100; size++) {
		unsigned cls = small_class_calc_offset_by_size(&sc, size);
		if (size <= class_size)
			fail_unless(cls == class);
		if (size == class_size + 1) {
			fail_unless(cls == class + 1);
			class = cls;
			class_size = small_class_calc_size_by_offset(&sc, class);
		}
	}
	ok(true);

	footer();
	check_plan();
}

static void
check_expectation()
{
	plan(4);
	header();

	const unsigned granularity_arr[] = {1, 2, 4, 8};
	const unsigned test_sizes = 1024;
	const unsigned test_classes = 1024;
	/*
	 * We expect 4 effective bits with factor = 1.05,
	 * see small_class_create
	 */
	float factor = 1.05;
	/*
	 * 1 << 4 (effective bits mentioned above)
	 */
	const unsigned eff_size = 16;
	unsigned test_class_size[test_classes + eff_size];

	for (unsigned variant = 0; variant < lengthof(granularity_arr); variant++) {
		unsigned granularity = granularity_arr[variant];
		unsigned min_alloc = granularity + (rand () % 16);

		{
			unsigned class_size = min_alloc - granularity;
			/* incremental growth */
			for (unsigned i = 0; i < eff_size; i++) {
				class_size += granularity;
				test_class_size[i] = class_size;
			}
			/* exponential growth */
			unsigned growth = granularity;
			for (unsigned i = eff_size; i < test_classes; i += eff_size) {
				for (unsigned j = 0; j < eff_size; j++) {
					class_size += growth;
					test_class_size[i + j] = class_size;
				}
				growth *= 2;
			}
		}

		struct small_class sc;
		float actual_factor;
		small_class_create(&sc, granularity, factor, min_alloc, &actual_factor);
		ok(sc.effective_size == eff_size);

		for (unsigned s = 0; s <= test_sizes; s++) {
			unsigned expect_class = 0;
			while (expect_class < test_classes && s > test_class_size[expect_class])
				expect_class++;
			unsigned expect_class_size = test_class_size[expect_class];
			unsigned got_class = small_class_calc_offset_by_size(&sc, s);
			unsigned got_class_size = small_class_calc_size_by_offset(&sc, got_class);
			fail_unless(got_class == expect_class);
			fail_unless(got_class_size == expect_class_size);
		}
	}

	footer();
	check_plan();
}

static void
check_factor()
{
	plan(1);
	header();

	for (unsigned granularity = 1; granularity <= 4; granularity *= 4) {
		for(float factor = 1.01; factor < 1.995; factor += 0.01) {
			struct small_class sc;
			float actual_factor;
			small_class_create(&sc, granularity, factor, granularity, &actual_factor);
			float k = powf(factor, 0.5f);
			fail_unless(sc.actual_factor >= factor / k && sc.actual_factor <= factor * k);

			float min_deviation = 1.f;
			float max_deviation = 1.f;
			/* Skip incremental growth. */
			for (unsigned i = sc.effective_size; i < sc.effective_size * 3; i++) {
				unsigned cl_sz1 = small_class_calc_size_by_offset(&sc, i);
				unsigned cl_sz2 = small_class_calc_size_by_offset(&sc, i + 1);
				float real_growth = 1.f * cl_sz2 / cl_sz1;
				float deviation = sc.actual_factor / real_growth;
				if (deviation < min_deviation)
					min_deviation = deviation;
				if (deviation > max_deviation)
					max_deviation = deviation;
			}
			float ln2 = logf(2);
			fail_unless(min_deviation > ln2 && max_deviation < 2 * ln2);
		}
	}
	ok(true);

	footer();
	check_plan();
}


int
main(void)
{
	plan(3);
	header();

	test_class();
	check_expectation();
	check_factor();

	footer();
	return check_plan();
}
