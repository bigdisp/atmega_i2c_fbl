#!/usr/bin/env python3
"""
Flash an Intel HEX file to an atmega8 / atmega88pa running atmega_i2c_fbl.

Uses the Linux i2c-dev interface directly (no external Python modules),
e.g. on a Raspberry Pi with `dtparam=i2c_arm=on`.

Protocol (see src/main.c, run_flasher()):
  * After reset, the FBL waits 1 s for 'w' 'f' (flash mode), then starts the app.
    'w' 'f' is answered with 'f' when read back.
  * The hex file is then sent in chunks of up to CHUNK_SIZE characters.
    Each chunk write is followed by a single byte read. The read completes the
    exchange and returns the last character of the previous chunk (echo).
  * After the end of file record, the FBL starts the new application by itself.
    On an error, it stays in the FBL and answers 'E' 'R' 'R'.
  * While the FBL parses a chunk / writes a flash page, it does not ACK its
    address, so writes are simply retried until they are ACKed.
"""

import argparse
import errno
import fcntl
import os
import sys
import time

I2C_SLAVE = 0x0703

# Must be <= I2C_BUFFER_SIZE - 2 of the firmware (src/lib/i2c.h)
CHUNK_SIZE = 32

# Errors the i2c-dev driver reports for a NACK / busy slave
RETRY_ERRNOS = (errno.EREMOTEIO, errno.EIO, errno.ENXIO, errno.EAGAIN, errno.ETIMEDOUT)


class I2CDevice:
    def __init__(self, bus, address, timeout):
        self.fd = os.open("/dev/i2c-%d" % bus, os.O_RDWR)
        fcntl.ioctl(self.fd, I2C_SLAVE, address)
        self.timeout = timeout

    def close(self):
        os.close(self.fd)

    def _retry(self, func):
        deadline = time.monotonic() + self.timeout
        while True:
            try:
                return func()
            except OSError as e:
                if e.errno not in RETRY_ERRNOS or time.monotonic() > deadline:
                    raise
                # Slave is busy (e.g. writing a flash page, ~10 ms)
                time.sleep(0.0005)

    def write(self, data):
        data = bytes(data)

        def do_write():
            n = os.write(self.fd, data)
            if n != len(data):
                raise OSError(errno.EIO, "short write")

        self._retry(do_write)

    def read_byte(self):
        def do_read():
            d = os.read(self.fd, 1)
            if len(d) != 1:
                raise OSError(errno.EIO, "short read")
            return d[0]

        return self._retry(do_read)


def load_hex(path):
    """Return the records of the hex file up to and including the EOF record,
    without line breaks (the FBL ignores them anyway)."""
    records = []
    with open(path, "rb") as f:
        for n, line in enumerate(f, 1):
            line = line.strip()
            if not line:
                continue
            try:
                if not line.startswith(b":"):
                    raise ValueError
                rec = bytes.fromhex(line[1:].decode("ascii"))
                if len(rec) < 5 or len(rec) != rec[0] + 5 or sum(rec) & 0xFF:
                    raise ValueError
            except ValueError:
                raise ValueError("%s:%d: invalid Intel HEX record" % (path, n))
            if rec[0] > 32:
                raise ValueError("%s:%d: records longer than 32 bytes are not supported" % (path, n))
            records.append(line.upper())
            if rec[3] == 0x01:
                return b"".join(records)
    raise ValueError("%s: end of file record missing" % path)


def enter_fbl(dev, wait):
    """Send 'wf' until the FBL answers with 'f'. Returns True on success."""
    deadline = time.monotonic() + wait
    while True:
        try:
            dev.write(b"wf")
            time.sleep(0.005)
            if dev.read_byte() == ord("f"):
                return True
        except OSError:
            pass
        if time.monotonic() > deadline:
            return False
        time.sleep(0.02)


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("address", type=lambda x: int(x, 0), help="i2c address, e.g. 0x14")
    parser.add_argument("hexfile", help="Intel HEX file to flash")
    parser.add_argument("-b", "--bus", type=int, default=1, help="i2c bus number (default: 1)")
    parser.add_argument("--no-stepdown", action="store_true",
                        help="don't send the 'go to FBL' command to the application first")
    parser.add_argument("--wait", type=float, default=10.0,
                        help="seconds to wait for the FBL to show up (default: 10). Reset "
                             "the device during this time if the app can't enter the FBL itself.")
    parser.add_argument("--timeout", type=float, default=1.0,
                        help="max. seconds to retry a NACKed transfer (default: 1.0)")
    args = parser.parse_args()

    data = load_hex(args.hexfile)
    dev = I2CDevice(args.bus, args.address, args.timeout)
    dev_quick = I2CDevice(args.bus, args.address, 0)

    try:
        if not args.no_stepdown:
            print("Stepdown from application...")
            try:
                # Same as: i2cset -y 1 <addr> 0x10 0xFB4C w
                dev.write([0x10, 0x4C, 0xFB])
            except OSError:
                pass  # Already in FBL or app not answering; continue anyway
            time.sleep(0.05)

        print("Set to flash mode (reset the device now if it doesn't enter the FBL by itself)...")
        if not enter_fbl(dev_quick, args.wait):
            print("Error: FBL did not answer within %.0f s." % args.wait, file=sys.stderr)
            return 2

        print("Writing %d hex characters..." % len(data))
        start = time.monotonic()
        prev_last = None
        total = len(data)
        for pos in range(0, total, CHUNK_SIZE):
            chunk = data[pos:pos + CHUNK_SIZE]
            dev.write(chunk)
            echo = dev.read_byte()
            # The echo is the last character of the previous chunk
            if prev_last is not None and echo != prev_last:
                print("\nError: FBL echoed 0x%02X, expected 0x%02X near offset %d. "
                      "The FBL probably detected a checksum error." % (echo, prev_last, pos),
                      file=sys.stderr)
                return 3
            prev_last = chunk[-1]
            done = pos + len(chunk)
            if done == total or (pos // CHUNK_SIZE) % 16 == 0:
                sys.stdout.write("\r%3d%% (%d/%d)" % (done * 100 // total, done, total))
                sys.stdout.flush()

        print("\nTransferred in %.1f s." % (time.monotonic() - start))

        # The FBL processes the last chunk now and starts the application if
        # everything was fine. If not, it stays in the FBL and answers 'E'.
        time.sleep(0.1)
        try:
            if dev.read_byte() == ord("E"):
                print("Error: FBL reported an error (checksum or address).", file=sys.stderr)
                return 3
        except OSError:
            pass  # Application doesn't answer reads (yet)
        print("Done, application started.")
    finally:
        dev.close()
        dev_quick.close()

    return 0


if __name__ == "__main__":
    sys.exit(main())
