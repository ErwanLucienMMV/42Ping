#include "ft_ping.h"
#include <math.h>
#include <stdio.h>

void ping_record(t_ping *ping, double milliseconds)
{
	double delta;

	if (ping->samples == 0 || milliseconds < ping->minimum)
		ping->minimum = milliseconds;
	if (ping->samples == 0 || milliseconds > ping->maximum)
		ping->maximum = milliseconds;
	++ping->samples;
	/* Welford's update avoids cancellation in the RTT variance. */
	delta = milliseconds - ping->mean;
	ping->mean += delta / (double)ping->samples;
	ping->m2 += delta * (milliseconds - ping->mean);
}

void ping_statistics(const t_ping *ping)
{
	unsigned int loss;
	double variance;

	loss = 0;
	if (ping->transmitted)
		loss = (unsigned int)(100 * (ping->transmitted - ping->received)
			/ ping->transmitted);
	printf("--- %s ping statistics ---\n", ping->options.host);
	printf("%llu packets transmitted, %llu packets received, ",
		(unsigned long long)ping->transmitted,
		(unsigned long long)ping->received);
	if (ping->duplicates)
		printf("+%llu duplicates, ", (unsigned long long)ping->duplicates);
	printf("%u%% packet loss\n", loss);
	if (ping->samples)
	{
		variance = ping->m2 / (double)ping->samples;
		printf("round-trip min/avg/max/stddev = %.3f/%.3f/%.3f/%.3f ms\n",
			ping->minimum, ping->mean, ping->maximum,
			sqrt(variance < 0 ? 0 : variance));
	}
}
