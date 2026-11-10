#!/usr/bin/env python3
"""Exercise the executable; optional GNU reference is only a test oracle."""
import argparse
import os
import re
import signal
import subprocess
import time


def run(binary, args, code=0, timeout=5, **kwargs):
    result = subprocess.run(
        [binary] + args, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        universal_newlines=True, timeout=timeout, **kwargs
    )
    assert result.returncode == code, (args, result.returncode, result.stdout, result.stderr)
    return result.stdout, result.stderr


def normalized(output):
    lines = []
    for line in output.splitlines():
        if line.startswith("round-trip "):
            continue
        line = re.sub(r"time=\d+\.\d+ ms", "time=<RTT> ms", line)
        line = re.sub(r"id 0x[0-9a-f]+ = \d+", "id <ID>", line)
        lines.append(line)
    return lines


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("binary", nargs="?", default="./ft_ping")
    parser.add_argument("--reference", help="path to an independently installed GNU inetutils 2.0 ping")
    args = parser.parse_args()
    binary = os.path.abspath(args.binary)
    checks = 0
    for option in ["-?", "-h", "--help", "-v?", "-?v", "-vh"]:
        out, _ = run(binary, [option])
        assert "Usage:" in out
        checks += 1
    invalid = [
        [], [""], ["127.0.0.1", "127.0.0.2"], ["-z"], ["--unknown"],
        ["--unknown", "-?"], ["--unknown", "-h"], ["-z?"],
        ["-c"], ["--ttl"], ["-c0"], ["-c-1"], ["-c1.5"],
        ["-c4294967296"], ["-s65508"], ["-s-1"], ["--ttl=0"],
        ["--ttl=256"], ["-i0"], ["-i-1"], ["-inan"], ["-iinf"],
        ["-i1e3"], ["-i0.0001"], ["-i86401"], ["-w0"], ["-W0"],
        ["-W1x"], ["-s+1"], ["--ttl=hello"], ["-s", "--help"],
        ["-c1", "999.999.999.999"], ["-c1", "::1"],
    ]
    for options in invalid:
        _, err = run(binary, options, code=2)
        assert "ft_ping:" in err, options
        checks += 1
    for host in ["127.0.0.1", "localhost", "0.0.0.0"]:
        out, _ = run(binary, ["-n", "-c3", "-i.03", host])
        assert "3 packets transmitted, 3 packets received, 0% packet loss" in out
        assert [int(x) for x in re.findall(r"icmp_seq=(\d+)", out)] == [0, 1, 2]
        assert "round-trip min/avg/max/stddev = " in out
        checks += 1
    for size in [0, 1, 7, 8, 15, 16, 17, 57, 1501, 65507]:
        out, _ = run(binary, ["-c1", "-s", str(size), "127.0.0.1"])
        assert "{} bytes from 127.0.0.1".format(size + 8) in out
        assert "1 packets transmitted, 1 packets received, 0% packet loss" in out
        assert ("time=" in out) == (size >= 16)
        checks += 1
    out, _ = run(binary, ["-q", "-c2", "-i.01", "127.0.0.1"])
    assert "bytes from" not in out and "2 packets received" in out
    checks += 1
    start = time.monotonic()
    out, _ = run(binary, ["-c3", "-i.1", "127.0.0.1"])
    assert 0.17 <= time.monotonic() - start < 2
    checks += 1
    start = time.monotonic()
    out, _ = run(binary, ["-w.25", "-i.1", "127.0.0.1"])
    assert 0.22 <= time.monotonic() - start < 2
    assert "3 packets transmitted, 3 packets received" in out
    checks += 1
    for stop_signal in [signal.SIGINT, signal.SIGTERM]:
        process = subprocess.Popen(
            [binary, "-i3600", "127.0.0.1"], stdout=subprocess.PIPE,
            stderr=subprocess.PIPE, universal_newlines=True
        )
        try:
            header = process.stdout.readline()
            reply = process.stdout.readline()
            assert header.startswith("PING ") and "icmp_seq=0" in reply
            process.send_signal(stop_signal)
            out, err = process.communicate(timeout=2)
            assert process.returncode == 0, (out, err)
            assert out.count("ping statistics") == 1
            assert "1 packets transmitted, 1 packets received" in out
        finally:
            if process.poll() is None:
                process.kill()
                process.wait()
        checks += 1
    # A non-root process without capabilities must fail clearly, even if the
    # kernel allows unprivileged SOCK_DGRAM ping sockets (we use SOCK_RAW).
    if os.geteuid() == 0:
        def unprivileged():
            os.setgroups([])
            os.setgid(65534)
            os.setuid(65534)
        _, err = run(binary, ["-c1", "127.0.0.1"], code=2,
                     preexec_fn=unprivileged)
        assert "CAP_NET_RAW" in err
        checks += 1
    if args.reference:
        version, _ = run(args.reference, ["-V"])
        assert "(GNU inetutils) 2.0" in version, version
        for options in [
            ["-n", "-c3", "-i.03", "127.0.0.1"],
            ["-n", "-v", "-c1", "localhost"],
            ["-n", "-c1", "-s0", "127.0.0.1"],
            ["-n", "-c1", "-s57", "127.0.0.1"],
            ["-n", "-q", "-c1", "127.0.0.1"],
            ["-n", "-c1", "0.0.0.0"],
        ]:
            actual, _ = run(binary, options)
            expected, _ = run(args.reference, options)
            assert normalized(actual) == normalized(expected), (actual, expected)
            checks += 1
    print("PASS: {} integration checks".format(checks))


if __name__ == "__main__":
    main()
