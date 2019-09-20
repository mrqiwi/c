#!/usr/bin/python3

import socket
from sys import argv

PORT = 5000

def send_msg(msg, host='127.0.0.1'):
    if msg != '+' and msg != '-' and msg != 'r' and msg != 's':
        print_help()

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.connect((host, PORT))
        s.sendall(msg.encode())
        exit(0)

def print_help():
    print("Usage: scrypt [+|-|r|s] ip");
    print("[+] - to turn up the volume");
    print("[-] - to turn down the volume");
    print("[r] - to restart the computer");
    print("[s] - to shut down the computer.");
    print("ip  - server IP address");

    exit(1)

def main(argv):
    if len(argv) == 3:
        send_msg(argv[1], argv[2])
    elif len(argv) == 2:
        send_msg(argv[1])
    else:
        print_help()

if __name__ == '__main__':
    main(argv)