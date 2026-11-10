#!/usr/bin/env python3
"""Inject RFC-minimum ICMP error quotes in a disposable Linux container.

Requires NET_RAW and NET_ADMIN. An optional GNU 2.0 oracle checks exact output.
"""
import argparse
import os
import re
import socket
import struct
import subprocess

from integration import normalized


def checksum(data):
    if len(data) % 2:
        data += b"\0"
    total = sum(struct.unpack("!{}H".format(len(data) // 2), data))
    while total >> 16:
        total = (total & 65535) + (total >> 16)
    return (~total) & 65535


def inject(binary, kind, code):
    sniffer = socket.socket(socket.AF_INET, socket.SOCK_RAW, socket.IPPROTO_ICMP)
    sniffer.settimeout(2)
    process = subprocess.Popen(
        [binary, "-n", "-v", "-c1", "-W1", "127.0.0.5"],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True
    )
    try:
        header = process.stdout.readline()
        identifier = int(re.search(r"id 0x([0-9a-f]+)", header).group(1), 16)
        while True:
            packet = sniffer.recv(65535)
            ip_length = (packet[0] & 15) * 4
            if (packet[9] == 1 and packet[16:20] == socket.inet_aton("127.0.0.5")
                    and packet[ip_length] == 8
                    and struct.unpack("!H", packet[ip_length + 4:ip_length + 6])[0] == identifier):
                break
        # Quote only IP + eight ICMP bytes. Keep the quoted IP ID and flags
        # deterministic so oracle output can be compared across separate runs.
        quote = bytearray(packet[:ip_length + 8])
        quote[4:8] = b"\x12\x34\0\0"
        quote[10:12] = b"\0\0"
        quote[10:12] = struct.pack("!H", checksum(bytes(quote[:ip_length])))
        error = struct.pack("!BBHI", kind, code, 0, 0) + bytes(quote)
        error = error[:2] + struct.pack("!H", checksum(error)) + error[4:]
        sniffer.sendto(error, ("127.0.0.1", 0))
        out, err = process.communicate(timeout=3)
        assert process.returncode == 1, (kind, code, out, err)
        assert "36 bytes from 127.0.0.1:" in out
        assert "IP Hdr Dump:" in out
        assert "ICMP: type 8, code 0, size 64," in out
        assert "1 packets transmitted, 0 packets received, 100% packet loss" in out
        lines = normalized(header + out)
        return [re.sub(r"id 0x[0-9a-f]+", "id <ID>", line) for line in lines]
    finally:
        sniffer.close()
        if process.poll() is None:
            process.kill()
            process.wait()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("binary", nargs="?", default="./ft_ping")
    parser.add_argument("--reference")
    args = parser.parse_args()
    binary = os.path.abspath(args.binary)
    chain = "FT_PING_ERROR_{}".format(os.getpid())

    def firewall(*options):
        subprocess.check_call(["iptables", "-w", "2"] + list(options))

    firewall("-N", chain)
    attached = False
    try:
        firewall("-A", chain, "-p", "icmp", "--icmp-type", "echo-reply",
                 "-s", "127.0.0.5", "-j", "DROP")
        firewall("-I", "INPUT", "-i", "lo", "-p", "icmp", "-j", chain)
        attached = True
        cases = ([(3, code) for code in range(16)]
                 + [(11, code) for code in range(3)]
                 + [(5, code) for code in range(5)] + [(12, 0), (4, 0)])
        for kind, code in cases:
            actual = inject(binary, kind, code)
            if args.reference:
                expected = inject(args.reference, kind, code)
                assert actual == expected, (kind, code, actual, expected)
        print("PASS: {} minimally quoted ICMP error cases{}".format(
            len(cases), " matched GNU 2.0" if args.reference else ""))
    finally:
        if attached:
            firewall("-D", "INPUT", "-i", "lo", "-p", "icmp", "-j", chain)
        firewall("-F", chain)
        firewall("-X", chain)


if __name__ == "__main__":
    main()
