#!/usr/bin/env python3
"""Host tests for src/hex_parser.c. Run: python3 tests/test_hex_parser.py"""
import os
import random
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
PAGE = 64
FBL = 0x1800


def build():
    exe = os.path.join(tempfile.mkdtemp(), "hex_parser_host")
    subprocess.check_call(["gcc", "-std=gnu99", "-Wall", "-Werror", "-DSPM_PAGESIZE=%d" % PAGE,
                           "-o", exe, os.path.join(HERE, "hex_parser_host.c"),
                           os.path.join(HERE, "..", "src", "hex_parser.c")])
    return exe


def record(addr, rtype, data):
    b = bytes([len(data), addr >> 8, addr & 0xFF, rtype]) + bytes(data)
    return ":" + (b + bytes([(-sum(b)) & 0xFF])).hex().upper()


def to_hex(segments, rec_len=16, eol="\r\n"):
    lines = []
    for start, data in segments:
        for i in range(0, len(data), rec_len):
            lines.append(record(start + i, 0, data[i:i + rec_len]))
    lines.append(record(0, 1, b""))
    return eol.join(lines) + eol


def run(exe, text):
    out = subprocess.run([exe], input=text.encode(), stdout=subprocess.PIPE, check=True).stdout.decode()
    res = {"pages": []}
    for line in out.splitlines():
        k, v = line.split(" ", 1)
        if k == "RESULT":
            res["result"] = v
        elif k == "IMAGE":
            res["image"] = bytes.fromhex(v)
        else:
            res["pages"].append((k, int(v, 16)))
    return res


def expected_image(segments):
    img = bytearray(b"\xFF" * 0x2000)
    for start, data in segments:
        img[start:start + len(data)] = data
    return bytes(img)


failures = 0


def check(name, cond):
    global failures
    print("%-55s %s" % (name, "ok" if cond else "FAIL"))
    if not cond:
        failures += 1


def main():
    exe = build()
    rnd = random.Random(1)

    for size in (1, 63, 64, 65, 128, 6000, 6144, FBL):
        seg = [(0, bytes(rnd.randrange(256) for _ in range(size)))]
        r = run(exe, to_hex(seg))
        check("image of %d bytes" % size,
              r["result"] == "finished" and r["image"] == expected_image(seg)
              and all(k == "PAGE" for k, _ in r["pages"])
              and r["pages"][0] == ("PAGE", 0) and r["pages"][-1] == ("PAGE", 0)
              and len(r["pages"]) == (size + PAGE - 1) // PAGE + 1)

    seg = [(0, bytes(rnd.randrange(256) for _ in range(3000)))]
    r = run(exe, to_hex(seg, rec_len=32, eol="\n").lower())
    check("32 byte records, LF, lower case", r["result"] == "finished" and r["image"] == expected_image(seg))
    r = run(exe, to_hex(seg).replace("\r\n", ""))
    check("no line breaks", r["result"] == "finished" and r["image"] == expected_image(seg))
    r = run(exe, "\0\0\r\n" + to_hex(seg))
    check("leading garbage outside records ignored", r["result"] == "finished")

    seg = [(0, b"\x01" * 100), (0x205, b"\x02" * 37), (0x1000, b"\x03" * 7)]
    r = run(exe, to_hex(seg, rec_len=13))
    check("gaps / unaligned records", r["result"] == "finished" and r["image"] == expected_image(seg))

    seg = [(0, b"\x11" * 64)]
    r = run(exe, to_hex(seg))
    check("page-multiple image: page 0 erased first, written last",
          r["result"] == "finished" and r["pages"] == [("PAGE", 0), ("PAGE", 0)]
          and r["image"] == expected_image(seg))

    seg = [(0, bytes(rnd.randrange(256) for _ in range(1000)))]
    r = run(exe, to_hex(seg)[:-30])
    check("interrupted transfer leaves page 0 erased (no app)",
          r["result"] == "busy" and r["image"][:PAGE] == b"\xFF" * PAGE)

    good = to_hex([(0, bytes(range(128)))])
    lines = good.split("\r\n")
    bad = list(lines)
    bad[3] = bad[3][:-2] + "%02X" % ((int(bad[3][-2:], 16) + 1) & 0xFF)
    r = run(exe, "\r\n".join(bad))
    check("checksum error detected, app invalidated",
          r["result"] == "error" and r["image"][:PAGE] == b"\xFF" * PAGE)

    bad = list(lines)
    bad[2] = bad[2][:20]
    r = run(exe, "\r\n".join(bad))
    check("truncated record detected", r["result"] == "error")

    bad = list(lines)
    bad[2] = bad[2][:20] + "G" + bad[2][21:]
    r = run(exe, "\r\n".join(bad))
    check("invalid character detected", r["result"] == "error")

    r = run(exe, to_hex([(0, b"\x00" * 16), (FBL - 8, b"\xAA" * 16)]))
    check("write into FBL rejected", r["result"] == "error" and r["image"][FBL - 64:FBL] == b"\xFF" * 64)

    r = run(exe, to_hex([(0x100, b"\x00" * 16), (0x0, b"\x00" * 16)]))
    check("descending addresses rejected", r["result"] == "error")

    r = run(exe, record(0, 4, b"\x00\x01") + record(0, 0, b"\x00") + record(0, 1, b""))
    check("address above 64K rejected", r["result"] == "error")

    r = run(exe, record(0, 4, b"\x00\x00") + record(0, 3, b"\x00\x00\x00\x00") + record(0, 0, b"\x42")
            + record(0, 1, b""))
    check("extended address 0 / start address records accepted",
          r["result"] == "finished" and r["image"][0] == 0x42)

    r = run(exe, record(0, 0, b"\x00" * 33) + record(0, 1, b""))
    check("over-long record rejected", r["result"] == "error")

    print("\n%d failure(s)" % failures)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
