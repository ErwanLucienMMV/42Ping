#include "ft_ping.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <time.h>

static int send_request(t_ping *ping, int64_t now)
{
	uint16_t sequence;
	size_t length;
	ssize_t sent;
	t_probe *probe;

	sequence = (uint16_t)ping->transmitted;
	probe = &ping->probes[sequence];
	probe->active = 0;
	probe->replied = 0;
	probe->completed = 0;
	probe->sent_at = now;
	length = ping_packet(ping->outgoing, ping->options.data_size,
		ping->identifier, sequence, now);
	sent = sendto(ping->socket_fd, ping->outgoing, length, MSG_DONTWAIT,
		(const struct sockaddr *)&ping->destination,
		sizeof(ping->destination));
	++ping->transmitted;
	if (sent < 0 || (size_t)sent != length)
	{
		fprintf(stderr, "ft_ping: sendto: %s\n",
			sent < 0 ? strerror(errno) : "incomplete datagram");
		++ping->errors;
		++ping->completed;
		return (0);
	}
	probe->active = 1;
	return (0);
}

static int wait_for_reply(t_ping *ping, int64_t delay, const sigset_t *mask)
{
	fd_set readers;
	struct timespec timeout;
	int ready;

	FD_ZERO(&readers);
	FD_SET(ping->socket_fd, &readers);
	if (delay < 0)
		delay = 0;
	timeout.tv_sec = (time_t)(delay / PING_SECOND);
	timeout.tv_nsec = (long)(delay % PING_SECOND);
	/* Atomically unblock stop signals while sleeping: no lost Ctrl+C race. */
	ready = pselect(ping->socket_fd + 1, &readers, NULL, NULL, &timeout, mask);
	if (ready < 0 && errno != EINTR)
	{
		fprintf(stderr, "ft_ping: pselect: %s\n", strerror(errno));
		return (-1);
	}
	if (ready > 0 && !g_stop)
		return (ping_receive(ping));
	return (0);
}

static int run_loop(t_ping *ping, const sigset_t *mask)
{
	int64_t started;
	int64_t next;
	int64_t finish;
	int64_t now;
	int64_t wake;

	started = ping_now();
	if (started < 0)
		return (-1);
	next = started;
	finish = 0;
	while (!g_stop)
	{
		now = ping_now();
		if (now < 0)
			return (-1);
		if ((ping->options.deadline && now - started >= ping->options.deadline)
			|| (finish && (now >= finish
					|| ping->completed == ping->transmitted)))
			break ;
		if (!finish && now >= next)
		{
			send_request(ping, now);
			next = now + ping->options.interval;
			if (ping->options.count && ping->transmitted >= ping->options.count)
				finish = now + ping->options.linger;
		}
		if (finish && ping->completed == ping->transmitted)
			break ;
		wake = finish ? finish : next;
		if (ping->options.deadline && started + ping->options.deadline < wake)
			wake = started + ping->options.deadline;
		if (wait_for_reply(ping, wake - now, mask) < 0)
			return (-1);
	}
	return (0);
}

int ping_run(t_ping *ping)
{
	sigset_t blocked;
	sigset_t previous;
	int result;

	if (ping->socket_fd >= FD_SETSIZE)
	{
		fputs("ft_ping: socket descriptor exceeds pselect capacity\n", stderr);
		return (-1);
	}
	sigemptyset(&blocked);
	sigaddset(&blocked, SIGINT);
	sigaddset(&blocked, SIGTERM);
	if (sigprocmask(SIG_BLOCK, &blocked, &previous) < 0)
	{
		fprintf(stderr, "ft_ping: sigprocmask: %s\n", strerror(errno));
		return (-1);
	}
	result = run_loop(ping, &previous);
	if (sigprocmask(SIG_SETMASK, &previous, NULL) < 0)
		return (-1);
	return (result);
}
