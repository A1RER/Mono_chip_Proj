import argparse
import threading
import time

import serial


def read_loop(port):
    while True:
        data = port.readline()
        if data:
            text = data.decode("ascii", errors="replace").rstrip()
            print(text)


def main():
    parser = argparse.ArgumentParser(description="Serial monitor for the STC89C52 project.")
    parser.add_argument("--port", default="COM7", help="Serial port, for example COM7.")
    parser.add_argument("--baud", type=int, default=9600, help="Baud rate.")
    args = parser.parse_args()

    ser = serial.Serial(args.port, args.baud, bytesize=8, parity="N", stopbits=1, timeout=1)
    time.sleep(0.2)
    print(f"Opened {args.port} at {args.baud} 8N1")
    print("Type commands such as start_tem, stop_tem, T=3, TEM_L=18.5, TEM_H=32.0, TIME=12:30:00")
    print("Press Ctrl+C to exit.")

    threading.Thread(target=read_loop, args=(ser,), daemon=True).start()

    try:
        while True:
            cmd = input("> ").strip()
            if cmd:
                ser.write((cmd + "\r\n").encode("ascii"))
    except KeyboardInterrupt:
        print("\nClosed.")
    finally:
        ser.close()


if __name__ == "__main__":
    main()
