#include "ft_ping.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

int64_t ping_now(void)
{
	struct timespec now;

	if (clock_gettime(CLOCK_MONOTONIC, &now) < 0)
	{
		fprintf(stderr, "ft_ping: clock_gettime: %s\n", strerror(errno));
		return (-1);
	}
	return ((int64_t)now.tv_sec * PING_SECOND + now.tv_nsec);
}

uint16_t ping_read16(const unsigned char *bytes)
{
	return ((uint16_t)((uint16_t)bytes[0] << 8 | bytes[1]));
}

void ping_write16(unsigned char *bytes, uint16_t value)
{
	bytes[0] = (unsigned char)(value >> 8);
	bytes[1] = (unsigned char)value;
}
