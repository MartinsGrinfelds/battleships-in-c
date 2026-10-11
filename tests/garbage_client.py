#!/usr/bin/env python3
"""Connects to Battleships server and misbehaves in many ways.
Server must survive all of this and keep the game running for others."""
import random
import socket
import struct
import sys
import time

HOST = "127.0.0.1"
PORT = int(sys.argv[1])
random.seed(int(sys.argv[2]) if len(sys.argv) > 2 else 1)


def connect():
    s = socket.create_connection((HOST, PORT), timeout=5)
    s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    return s


def packet(sequence, packet_type, content, break_checksum=False):
    header = struct.pack(">IIB", sequence, len(content), packet_type)
    checksum = 0
    for byte in header + content:
        checksum ^= byte
    if break_checksum:
        checksum ^= 0xFF
    return header + bytes([checksum]) + content


def hello(name, sequence=1):
    encoded = name.encode()
    return packet(sequence, 0, bytes([len(encoded)]) + encoded)


def quietly(action):
    try:
        action()
    except OSError:
        pass  # Server is allowed to close connection on us.


def random_garbage():
    s = connect()
    s.sendall(bytes(random.getrandbits(8) for _ in range(random.randint(1, 3000))))
    time.sleep(0.05)
    s.close()


def huge_size():
    s = connect()
    s.sendall(struct.pack(">IIBB", 1, 0xFFFFFFF0, 0, 0))
    time.sleep(0.05)
    s.close()


def half_packet_then_close():
    s = connect()
    s.sendall(hello("Half Packet")[:7])
    time.sleep(0.05)
    s.close()


def wrong_first_packet():
    s = connect()
    s.sendall(packet(1, 7, bytes(6)))  # IGo before HELLO
    time.sleep(0.05)
    s.close()


def bad_names():
    for name in ["", " Space", "Space ", "X" * 40, "Tab\tName", "Martins\x00Hidden"]:
        s = connect()
        encoded = name.encode()
        length = min(len(encoded), 255)
        s.sendall(packet(1, 0, bytes([length]) + encoded[:length]))
        time.sleep(0.02)
        s.close()


def truncated_hello():
    s = connect()
    s.sendall(packet(1, 0, bytes([200]) + b"short"))  # Says 200 characters but has 5.
    time.sleep(0.05)
    s.close()


def registered_then_nonsense():
    """Registers as observer/player and then sends every kind of broken packet."""
    s = connect()
    s.sendall(hello("Troublemaker %d" % random.randint(0, 999)))
    time.sleep(0.1)
    sequence = 2
    for _ in range(200):
        kind = random.randint(0, 6)
        if kind == 0:
            content = bytes(random.getrandbits(8) for _ in range(random.randint(0, 20)))
            s.sendall(packet(sequence, random.randint(0, 255), content))
        elif kind == 1:
            s.sendall(packet(sequence, 7, bytes([random.randint(0, 255) for _ in range(6)])))  # Random IGo
        elif kind == 2:
            s.sendall(packet(sequence, 5, bytes([random.randint(0, 255) for _ in range(7)])))  # Random IPlace
        elif kind == 3:
            s.sendall(packet(sequence, 2, bytes([1, 0, random.randint(0, 255), 255]) + b"x" * 10))  # Lies about length
        elif kind == 4:
            s.sendall(packet(sequence, 2, bytes([1, 0, 0, 5]) + b"\x01\x02\x1b[2J"))  # Control characters in chat
        elif kind == 5:
            s.sendall(packet(sequence, 2, b"x", break_checksum=True))
        else:
            s.sendall(packet(max(1, sequence - 50), 2, b"old"))  # Old sequence number
        sequence += 1
    time.sleep(0.1)
    s.close()


def never_reads():
    """Registers and never reads anything. Server must not block on it."""
    s = connect()
    s.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 4096)
    s.sendall(hello("Sleepy %d" % random.randint(0, 999)))
    time.sleep(3)
    s.close()


def connection_flood():
    sockets = []
    for _ in range(40):
        try:
            sockets.append(connect())
        except OSError:
            break
    time.sleep(0.2)
    for s in sockets:
        s.close()


ATTACKS = [random_garbage, huge_size, half_packet_then_close, wrong_first_packet, bad_names,
           truncated_hello, registered_then_nonsense, never_reads, connection_flood]

if __name__ == "__main__":
    rounds = int(sys.argv[3]) if len(sys.argv) > 3 else 1
    for _ in range(rounds):
        for attack in ATTACKS:
            quietly(attack)
    print("Garbage client finished %d rounds" % rounds)
