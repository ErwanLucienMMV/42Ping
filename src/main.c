#include "ft_ping.h"
#include <stdio.h>

void ping_usage(void)
{
	fputs("Usage: ft_ping [-hv] destination\n", stdout);
}

int main(void)
{
	ping_usage();
	return (0);
}
