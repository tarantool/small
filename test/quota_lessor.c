#include "small/quota_lessor.h"
#include "unit.h"

void
test_basic()
{
	plan(23);
	struct quota q;
	quota_init(&q, QUOTA_MAX);
	struct quota_lessor l;
	quota_lessor_create(&l, &q);
	is(100, quota_lease(&l, 100), "lease 100 bytes");
	is(100, quota_leased(&l), "leased 100 bytes");
	is(QUOTA_USE_MIN - 100, quota_available(&l),
	   "UNIT_SIZE - 100 available");
	is(QUOTA_USE_MIN, quota_used(&q), "source quota used");

	/* Lease without source qouta usage. */
	is(200, quota_lease(&l, 200), "lease 200 bytes");
	is(300, quota_leased(&l), "leased 300 bytes at all");
	is(QUOTA_USE_MIN - 300, quota_available(&l),
	   "UNIT_SIZE - 300 available");
	is(QUOTA_USE_MIN, quota_used(&q),
	   "source quota used did not change");

	/* Lease several LEASE_SIZEs. */
	is(QUOTA_USE_MIN * 3, quota_lease(&l, QUOTA_USE_MIN * 3), "lease big size");
	is(QUOTA_USE_MIN * 3 + 300, quota_leased(&l), "leased size");
	is(QUOTA_UNIT_SIZE - 300, quota_available(&l), "available size");
	is(QUOTA_USE_MIN * 3 + QUOTA_UNIT_SIZE, quota_used(&q),
	   "update source quota used");

	/* End lease. */
	quota_end_lease(&l, 300);
	is(QUOTA_UNIT_SIZE, quota_available(&l), "end small lease");
	is(QUOTA_USE_MIN * 3, quota_leased(&l), "decrease leased");
	is(QUOTA_USE_MIN * 3 + QUOTA_UNIT_SIZE, quota_used(&q),
	   "source quota did not change - too small size to free");

	quota_end_lease(&l, QUOTA_USE_MIN * 2 + 100);
	is(QUOTA_USE_MIN - 100, quota_leased(&l),
	   "decrease leased with big chunk");
	is(100 + QUOTA_USE_MIN, quota_available(&l),
	   "return big chunks into source quota");
	is(QUOTA_USE_MIN * 2, quota_used(&q), "release source quota");

	quota_end_lease(&l, QUOTA_USE_MIN - 100);
	is(0, quota_leased(&l), "lessor is empty");
	is(true, quota_available(&l) > 0, "lessor avoids oscillation");
	is(quota_available(&l), quota_used(&q), "source quota isn't empty");

	quota_lessor_destroy(&l);
	is(0, quota_available(&l), "lessor has no memory");
	is(0, quota_used(&q), "source quota is empty");

	check_plan();
}

void
test_hard_lease()
{
	plan(12);
	struct quota q;
	size_t quota_total = QUOTA_USE_MIN + QUOTA_USE_MIN / 8;
	quota_init(&q, quota_total);
	struct quota_lessor l;
	quota_lessor_create(&l, &q);
	is(QUOTA_USE_MIN, quota_lease(&l, QUOTA_USE_MIN), "lease 1Mb");
	is(0, quota_available(&l), "available 0");
	is(QUOTA_USE_MIN, quota_leased(&l), "leased 1Mb");
	is(QUOTA_USE_MIN, quota_used(&q), "source quota used");

	is(-1, quota_lease(&l, QUOTA_USE_MIN), "lease too big");

	is(QUOTA_UNIT_SIZE, quota_lease(&l, QUOTA_UNIT_SIZE), "hard lease");

	is(QUOTA_UNIT_SIZE + QUOTA_USE_MIN, quota_leased(&l), "leased changed");
	is(QUOTA_USE_MIN / 8 - QUOTA_UNIT_SIZE, quota_available(&l),
	   "available the part of 1MB");
	is(quota_total, quota_used(&q), "source quota fully used");

	quota_end_lease(&l, quota_leased(&l));

	quota_lessor_destroy(&l);
	is(0, quota_available(&l), "lessor is empty");
	is(0, quota_leased(&l), "lessor is empty");
	is(0, quota_used(&q), "sourcr quota is empty");

	check_plan();
}

int
main()
{
	plan(2);
	test_basic();
	test_hard_lease();
	return check_plan();
}
