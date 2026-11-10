#include "ft_ping.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

volatile sig_atomic_t g_stop = 0;

static void stop_ping(int signal_number)
{
	(void)signal_number;
	g_stop = 1;
}

static int install_signals(void)
{
	struct sigaction action;

	memset(&action, 0, sizeof(action));
	action.sa_handler = stop_ping;
	sigemptyset(&action.sa_mask);
	if (sigaction(SIGINT, &action, NULL) < 0
		|| sigaction(SIGTERM, &action, NULL) < 0)
	{
		fprintf(stderr, "ft_ping: sigaction: %s\n", strerror(errno));
		return (-1);
	}
	return (0);
}

int main(int argc, char **argv)
{
	t_ping ping;
	int status;

	memset(&ping, 0, sizeof(ping));
	ping.socket_fd = -1;
	status = ping_options(argc, argv, &ping.options);
	if (status != 0)
		return (status < 0 ? 2 : 0);
	if (install_signals() < 0 || ping_open(&ping) < 0)
	{
		ping_close(&ping);
		return (2);
	}
	printf("PING %s (%s): %zu data bytes", ping.options.host,
		ping.address, ping.options.data_size);
	if (ping.options.verbose)
		printf(", id 0x%04x = %u", ping.identifier, ping.identifier);
	putchar('\n');
	fflush(stdout);
	status = ping_run(&ping);
	ping_statistics(&ping);
	ping_close(&ping);
	if (status < 0)
		return (2);
	return (ping.received == 0 ? 1 : 0);
}
