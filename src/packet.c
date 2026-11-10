#include "ft_ping.h"
#include <string.h>

uint16_t ping_checksum(const unsigned char *bytes, size_t length)
{
	uint32_t sum;

	sum = 0;
	while (length >= 2)
	{
		sum += ping_read16(bytes);
		bytes += 2;
		length -= 2;
	}
	if (length)
		sum += (uint32_t)*bytes << 8;
	while (sum >> 16)
		sum = (sum & 0xffff) + (sum >> 16);
	return ((uint16_t)~sum);
}

static unsigned char payload_byte(size_t index, int64_t stamp)
{
	if (index < 8)
		return ((unsigned char)((uint64_t)stamp >> ((7 - index) * 8)));
	return ((unsigned char)index);
}

size_t ping_packet(unsigned char *bytes, size_t data_size, uint16_t id,
	uint16_t sequence, int64_t stamp)
{
	size_t index;

	memset(bytes, 0, 8);
	bytes[0] = 8;
	ping_write16(bytes + 4, id);
	ping_write16(bytes + 6, sequence);
	index = 0;
	while (index < data_size)
	{
		bytes[index + 8] = payload_byte(index, stamp);
		++index;
	}
	ping_write16(bytes + 2, ping_checksum(bytes, data_size + 8));
	return (data_size + 8);
}

int ping_payload(const unsigned char *bytes, size_t data_size, int64_t stamp)
{
	size_t index;

	index = 0;
	while (index < data_size)
	{
		if (bytes[index] != payload_byte(index, stamp))
			return (0);
		++index;
	}
	return (1);
}
