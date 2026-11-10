#!/usr/bin/env python3
"""Optional, isolated-container-only firewall tests; requires CAP_NET_ADMIN.

Adds one temporary INPUT chain, removes it in finally, and uses loopback only.
"""
import argparse
import os
import re
import signal
import subprocess
import time

from integration import normalized, run


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("binary", nargs="?", default="./ft_ping")
    parser.add_argument("--reference")
    args = parser.parse_args()
    binary = os.path.abspath(args.binary)
    chain = "FT_PING_TEST_{}".format(os.getpid())

    def firewall(*options):
        subprocess.check_call(["iptables", "-w", "2"] + list(options))

    firewall("-N", chain)
    attached = False
    hosts_file = "/etc/hosts"
    with open(hosts_file) as stream:
        old_hosts = stream.read()
    try:
        firewall("-A", chain, "-p", "icmp", "--icmp-type", "echo-reply",
                 "-s", "127.0.0.2", "-j", "DROP")
        firewall("-A", chain, "-p", "icmp", "--icmp-type", "echo-request",
                 "-d", "127.0.0.3", "-j", "REJECT", "--reject-with", "icmp-host-unreachable")
        firewall("-I", "INPUT", "-i", "lo", "-p", "icmp", "-j", chain)
        attached = True
        with open(hosts_file, "a") as stream:
            stream.write("\n127.0.0.1 alive.ping.test alive.ping.test.\n127.0.0.2 silent.ping.test\n")
        checks = 0
        for host in ["alive.ping.test", "alive.ping.test."]:
            out, _ = run(binary, ["-c1", host])
            assert "PING {} (127.0.0.1)".format(host) in out
            assert "64 bytes from 127.0.0.1" in out
            checks += 1
        for host in ["127.0.0.2", "silent.ping.test"]:
            start = time.monotonic()
            out, _ = run(binary, ["-c2", "-i.03", "-W.15", host], code=1)
            assert 0.15 <= time.monotonic() - start < 2
            assert "2 packets transmitted, 0 packets received, 100% packet loss" in out
            assert "bytes from" not in out
            checks += 1
        start = time.monotonic()
        out, _ = run(binary, ["-w.15", "127.0.0.2"], code=1)
        assert 0.12 <= time.monotonic() - start < 2
        assert "1 packets transmitted, 0 packets received" in out
        checks += 1
        for verbose in [False, True]:
            options = ["-c2", "-i.03", "-W.1", "127.0.0.3"]
            if verbose:
                options.insert(0, "-v")
            out, _ = run(binary, options, code=1)
            assert "2 packets transmitted, 0 packets received" in out
            assert ("Destination Host Unreachable" in out) == verbose
            assert ("IP Hdr Dump:" in out) == verbose
            checks += 1
        start = time.monotonic()
        out, _ = run(binary, ["-v", "-w.15", "-i.03", "127.0.0.3"], code=1)
        assert 0.12 <= time.monotonic() - start < 2
        assert out.count("Destination Host Unreachable") >= 2
        checks += 1
        # Partial loss: receive the first answer, then discard the remaining
        # replies before the second request is sent.
        outputs = []
        for executable in [binary] + ([args.reference] if args.reference else []):
            process = subprocess.Popen(
                [executable, "-n", "-c3", "-i.2", "-W1", "127.0.0.4"],
                stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True
            )
            dropped = False
            try:
                header = process.stdout.readline()
                first_reply = process.stdout.readline()
                assert "icmp_seq=0" in first_reply
                firewall("-I", chain, "-p", "icmp", "--icmp-type", "echo-reply",
                         "-s", "127.0.0.4", "-j", "DROP")
                dropped = True
                out, err = process.communicate(timeout=4)
                assert process.returncode == 0, (out, err)
                assert "3 packets transmitted, 1 packets received" in out
                outputs.append(header + first_reply + out)
            finally:
                if process.poll() is None:
                    process.kill()
                    process.wait()
                if dropped:
                    firewall("-D", chain, "-p", "icmp", "--icmp-type", "echo-reply",
                             "-s", "127.0.0.4", "-j", "DROP")
            checks += 1
        if len(outputs) == 2:
            assert normalized(outputs[0]) == normalized(outputs[1]), outputs
        print("PASS: {} network checks (FQDN, loss, deadlines, ICMP errors)".format(checks))
    finally:
        with open(hosts_file, "w") as stream:
            stream.write(old_hosts)
        if attached:
            firewall("-D", "INPUT", "-i", "lo", "-p", "icmp", "-j", chain)
        firewall("-F", chain)
        firewall("-X", chain)


if __name__ == "__main__":
    main()
