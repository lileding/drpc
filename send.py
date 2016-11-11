#!/usr/bin/env python
#
import sys
import socket
import random
import struct
import time
import multiprocessing.dummy


def main():
    p = multiprocessing.dummy.Pool(30)
    l = multiprocessing.dummy.Lock()
    for i in range(100):
        p.apply_async(do_send, (l, i))
    p.close()
    p.join()


def do_send(lock, idx):
    time.sleep(random.randint(0, 3))
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM, socket.IPPROTO_TCP)
    s.connect(('127.0.0.1', 8001))
    for _ in range(random.randint(0, 10)):
        sz = random.randint(10, 100)
        data = struct.pack('!L%ds' % sz, sz, 'a' * sz)
        with lock:
            print '[%02d] send %d %s' % (idx, sz, len(data))
            s.send(data)
        time.sleep(1)


if __name__ == '__main__':
    main()

