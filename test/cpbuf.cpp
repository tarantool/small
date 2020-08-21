#include <small/cpbuf.hpp>
#include <time.h>
#include <sys/uio.h> /* struct iovec */

#include "unit.h"

constexpr static size_t SMALL_BLOCK_SZ = 24;
constexpr static size_t LARGE_BLOCK_SZ = 104;

constexpr static char samples[] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09
};

constexpr static int samples_cnt = sizeof(samples);

constexpr static char marker = '#';

template<size_t N>
static void
fillBuffer(tnt::Buffer<N> &buffer, size_t size)
{
	for (size_t i = 0; i < size - 1; ++i)
		buffer.template addBack<char>(samples[i % samples_cnt]);
}

template<size_t N>
static void
eraseBuffer(tnt::Buffer<N> &buffer)
{
	int IOVEC_MAX = 1024;
	struct iovec vec[IOVEC_MAX];
	do {
		size_t vec_size = buffer.getIOV(buffer.begin(), vec, IOVEC_MAX);
		buffer.dropFront(vec_size);
	} while (!buffer.empty());

}

/**
 * AddBack() + dropBack()/dropFront() combinations.
 */
template<size_t N>
void
buffer_simple()
{
	header();
	tnt::Buffer<N> buf;
	fail_unless(buf.empty());
	size_t sz = buf.template addBack<int>(666);
	fail_unless(! buf.empty());
	fail_unless(sz == sizeof(int));
	auto itr = buf.begin();
	int res = -1;
	buf.template get<int>(itr, res);
	fail_unless(res == 666);
	itr.~iterator();
	buf.dropBack(sz);
	fail_unless(buf.empty());
	/* Test non-template ::addBack() method. */
	buf.addBack((const char *)&samples, sizeof(samples));
	fail_unless(! buf.empty());
	char samples_from_buf[sizeof(samples)];
	itr = buf.begin();
	buf.get(itr, (char *)&samples_from_buf, sizeof(samples));
	for (int i = 0; i < (int)sizeof(samples); ++i)
		fail_unless(samples[i] == samples_from_buf[i]);
	itr.~iterator();
	buf.dropFront(sizeof(samples));
	fail_unless(buf.empty());
	itr = buf.appendBack(sizeof(double));
	itr.~iterator();
	buf.dropFront(sizeof(double));
	fail_unless(buf.empty());
	footer();
}

template<size_t N>
void
buffer_iterator()
{
	header();
	tnt::Buffer<N> buf;
	fillBuffer(buf, samples_cnt);
	buf.addBack(marker);
	auto itr = buf.begin();
	while (itr != buf.end())
		itr++;
	char res = 'x';
	buf.get(itr, &res);
	fail_unless(res != marker);
}

template <size_t N>
void
buffer_insert_simple()
{
	header();
	tnt::Buffer<N> buf;

	for (int i = 0; i < 25; ++i) {
		size_t sz = buf.template addBack<char>(i);
		fail_unless(sz == sizeof(int));
	}
	auto begin_itr = buf.end();
	auto mid_itr = buf.end();
	auto mid_itr_cp = buf.end();
	for (int i = 25; i < 50; ++i) {
		size_t sz = buf.template addBack<char>(i);
		fail_unless(sz == sizeof(int));
	}
	auto end_itr = buf.end();
	/* Insert chunk with size less than block size. */
	buf.insert(mid_itr, SMALL_BLOCK_SZ / 2);
	int res = 0;
	buf.get(mid_itr, res);
	fail_unless(res != 24);
	buf.get(mid_itr_cp, res);
	fail_unless(res != 24);
	footer();
}

/**
 * Complex test emulating IPROTO interaction.
 */
template<size_t N>
void
buffer_out()
{
	header();
	tnt::Buffer<N> buf;
	buf.template addBack<char>(0xce); // uin32 tag
	auto save = buf.appendBack(4); // uint32, will be set later
	size_t total = buf.template addBack<char>(0x82); // map(2) - header
	total += buf.template addBack<char>(0x00); // IPROTO_REQUEST_TYPE
	total += buf.template addBack<char>(0x01); // IPROTO_SELECT
	total += buf.template addBack<char>(0x01); // IPROTO_SYNC
	total += buf.template addBack<char>(0x00); // sync = 0
	total += buf.template addBack<char>(0x82); // map(2) - body
	total += buf.template addBack<char>(0x10); // IPROTO_SPACE_ID
	total += buf.template addBack<char>(0xcd); // uint16 tag
	total += buf.template addBack(__builtin_bswap16(512)); // space_id = 512
	total += buf.template addBack<char>(0x20); // IPROTO_KEY
	total += buf.template addBack<char>(0x90); // empty array key
	buf.set(save, __builtin_bswap32(total)); // set calculated size
	//char request_res = {0x82, 0x00, 0x01, 0x01, 0x00, 0x82, 0x10, 0xcd };
	save.~iterator();
	do {
		int IOVEC_MAX = 1024;
		struct iovec vec[IOVEC_MAX];
		size_t vec_size = buf.getIOV(buf.begin(), vec, IOVEC_MAX);
		buf.dropFront(vec_size);
	} while (!buf.empty());
	footer();
}

int main()
{
	buffer_simple<SMALL_BLOCK_SZ>();
	buffer_simple<LARGE_BLOCK_SZ>();
	buffer_iterator<SMALL_BLOCK_SZ>();
	buffer_iterator<LARGE_BLOCK_SZ>();
	//buffer_insert_simple<SMALL_BLOCK_SZ>();
	buffer_out<SMALL_BLOCK_SZ>();
	buffer_out<LARGE_BLOCK_SZ>();
}
