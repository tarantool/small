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
	is(0, quota_lease(&l, 100), "lease 100 bytes");
	is(100, quota_leased(&l), "leased 100 bytes");
	is(QUOTA_LEASE_SIZE - 100, quota_available(&l),
	   "UNIT_SIZE - 100 available");
	is(QUOTA_LEASE_SIZE, quota_used(&q), "source quota used");

	/* Lease without source qouta usage. */
	is(0, quota_lease(&l, 200), "lease 200 bytes");
	is(300, quota_leased(&l), "leased 300 bytes at all");
	is(QUOTA_LEASE_SIZE - 300, quota_available(&l),
	   "UNIT_SIZE - 300 available");
	is(QUOTA_LEASE_SIZE, quota_used(&q),
	   "source quota used did not change");

	/* Lease several LEASE_SIZEs. */
	is(0, quota_lease(&l, QUOTA_LEASE_SIZE * 3), "lease big size");
	is(QUOTA_LEASE_SIZE * 3 + 300, quota_leased(&l), "leased size");
	is(QUOTA_UNIT_SIZE - 300, quota_available(&l), "available size");
	is(QUOTA_LEASE_SIZE * 3 + QUOTA_UNIT_SIZE, quota_used(&q),
	   "update source quota used");

	/* End lease. */
	quota_end_lease(&l, 300);
	is(QUOTA_UNIT_SIZE, quota_available(&l), "end small lease");
	is(QUOTA_LEASE_SIZE * 3, quota_leased(&l), "decrease leased");
	is(QUOTA_LEASE_SIZE * 3 + QUOTA_UNIT_SIZE, quota_used(&q),
	   "source quota did not change - too small size to free");

	quota_end_lease(&l, QUOTA_LEASE_SIZE * 2 + 100);
	is(QUOTA_LEASE_SIZE - 100, quota_leased(&l),
	   "decrease leased with big chunk");
	is(100 + QUOTA_LEASE_SIZE, quota_available(&l),
	   "return big chunks into source quota");
	is(QUOTA_LEASE_SIZE * 2, quota_used(&q), "release source quota");

	quota_end_lease(&l, QUOTA_LEASE_SIZE - 100);
	is(0, quota_leased(&l), "lessor is empty");
	is(QUOTA_LEASE_SIZE, quota_available(&l), "lessor avoids oscillation");
	is(QUOTA_LEASE_SIZE, quota_used(&q), "source quota isn't empty");

	quota_end_total(&l);
	is(0, quota_available(&l), "lessor has no memory");
	is(0, quota_used(&q), "source quota is empty");

	check_plan();
}

void
test_hard_lease()
{
	plan(12);
	struct quota q;
	size_t quota_total = QUOTA_LEASE_SIZE + QUOTA_LEASE_SIZE / 8;
	quota_init(&q, quota_total);
	struct quota_lessor l;
	quota_lessor_create(&l, &q);
	is(0, quota_lease(&l, QUOTA_LEASE_SIZE), "lease 1Mb");
	is(0, quota_available(&l), "available 0");
	is(QUOTA_LEASE_SIZE, quota_leased(&l), "leased 1Mb");
	is(QUOTA_LEASE_SIZE, quota_used(&q), "source quota used");

	is(-1, quota_lease(&l, QUOTA_LEASE_SIZE), "lease too big");

	is(0, quota_lease(&l, QUOTA_UNIT_SIZE), "hard lease");

	is(QUOTA_UNIT_SIZE + QUOTA_LEASE_SIZE, quota_leased(&l), "leased changed");
	is(QUOTA_LEASE_SIZE / 8 - QUOTA_UNIT_SIZE, quota_available(&l),
	   "available the part of 1MB");
	is(quota_total, quota_used(&q), "source quota fully used");

	quota_end_lease(&l, quota_leased(&l));

	quota_end_total(&l);
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
