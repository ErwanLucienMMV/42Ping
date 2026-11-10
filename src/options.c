#include "ft_ping.h"
#include <errno.h>
#include <getopt.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void ping_usage(void)
{
	puts("Usage: ft_ping [OPTION...] HOST\n"
		"Send IPv4 ICMP Echo requests to an address, hostname or FQDN.\n\n"
		"  -?, -h, --help          display this help and exit\n"
		"  -v, --verbose          report ICMP errors and packet diagnostics\n"
		"  -c, --count=N          stop after N requests (default: unlimited)\n"
		"  -i, --interval=SEC     seconds between requests (default: 1)\n"
		"  -s, --size=N           payload bytes, 0..65507 (default: 56)\n"
		"      --ttl=N            outgoing IPv4 TTL, 1..255\n"
		"  -w, --timeout=SEC      total run time, excluding DNS lookup\n"
		"  -W, --linger=SEC       final reply wait (default: 10 seconds)\n"
		"  -n, --numeric          numeric reply addresses (always enabled)\n"
		"  -q, --quiet            print only header and statistics\n\n"
		"Stop with Ctrl+C. Raw sockets require root or CAP_NET_RAW.\n"
		"Exit status: 0 = a reply received, 1 = no reply, 2 = local error.");
}

static int integer(const char *text, uint64_t maximum, uint64_t *result)
{
	char *end;
	unsigned long long value;
	const char *cursor;

	if (*text == '\0')
		return (-1);
	cursor = text;
	while (*cursor >= '0' && *cursor <= '9')
		++cursor;
	if (*cursor != '\0')
		return (-1);
	errno = 0;
	value = strtoull(text, &end, 10);
	if (errno || *end || value > maximum)
		return (-1);
	*result = (uint64_t)value;
	return (0);
}

static int seconds(const char *text, int64_t *result)
{
	char *end;
	double value;
	const char *cursor;
	int digits;
	int dots;

	cursor = text;
	digits = 0;
	dots = 0;
	while ((*cursor >= '0' && *cursor <= '9') || *cursor == '.')
	{
		digits += (*cursor >= '0' && *cursor <= '9');
		dots += (*cursor == '.');
		++cursor;
	}
	if (*cursor || !digits || dots > 1)
		return (-1);
	errno = 0;
	value = strtod(text, &end);
	if (errno || *end || !isfinite(value) || value < 0.001 || value > 86400)
		return (-1);
	*result = (int64_t)(value * (double)PING_SECOND);
	return (0);
}

static int set_number(int option, const char *value, t_options *options)
{
	uint64_t number;

	if (option == 'i')
		return (seconds(value, &options->interval));
	if (option == 'w')
		return (seconds(value, &options->deadline));
	if (option == 'W')
		return (seconds(value, &options->linger));
	if (integer(value, option == 'c' ? UINT32_MAX : PING_MAX_DATA,
			&number) < 0)
		return (-1);
	if ((option == 'c' || option == 't') && number == 0)
		return (-1);
	if (option == 't' && number > 255)
		return (-1);
	if (option == 'c')
		options->count = number;
	else if (option == 's')
		options->data_size = (size_t)number;
	else
		options->ttl = (int)number;
	return (0);
}

int ping_options(int argc, char **argv, t_options *options)
{
	const struct option long_options[] = {
		{"help", no_argument, NULL, 'h'},
		{"verbose", no_argument, NULL, 'v'},
		{"count", required_argument, NULL, 'c'},
		{"interval", required_argument, NULL, 'i'},
		{"size", required_argument, NULL, 's'},
		{"ttl", required_argument, NULL, 't'},
		{"timeout", required_argument, NULL, 'w'},
		{"linger", required_argument, NULL, 'W'},
		{"numeric", no_argument, NULL, 'n'},
		{"quiet", no_argument, NULL, 'q'},
		{NULL, 0, NULL, 0}
	};
	int option;

	options->data_size = 56;
	options->interval = PING_SECOND;
	options->linger = 10 * PING_SECOND;
	opterr = 0;
	/* Deliberately let getopt report -? as unknown: optopt then identifies
	 * it without confusing it with an unrecognized long option. */
	while ((option = getopt_long(argc, argv, ":hvnqc:i:s:w:W:",
			long_options, NULL)) != -1)
	{
		if (option == 'h' || (option == '?' && optopt == '?'))
			return (ping_usage(), 1);
		if (option == '?' || option == ':')
		{
			fputs("ft_ping: invalid option or missing option argument\n"
				"Try 'ft_ping --help' for more information.\n", stderr);
			return (-1);
		}
		if (option == 'v')
			options->verbose = 1;
		else if (option == 'q')
			options->quiet = 1;
		else if (option != 'n' && set_number(option, optarg, options) < 0)
		{
			fprintf(stderr, "ft_ping: invalid value: '%s'\n", optarg);
			return (-1);
		}
	}
	if (optind + 1 != argc || argv[optind][0] == '\0')
	{
		fputs("ft_ping: exactly one IPv4 destination is required\n"
			"Try 'ft_ping --help' for more information.\n", stderr);
		return (-1);
	}
	options->host = argv[optind];
	return (0);
}
