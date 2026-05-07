import argparse
import queue
import threading
import tkinter as tk
from tkinter import messagebox, scrolledtext

import serial


class SerialGui:
    def __init__(self, root, port_name, baud):
        self.root = root
        self.port_name = port_name
        self.baud = baud
        self.rx_queue = queue.Queue()
        self.ser = None
        self.running = False

        root.title(f"Serial Monitor - {port_name} @ {baud}")
        root.geometry("920x560")

        top = tk.Frame(root)
        top.pack(fill=tk.X, padx=8, pady=8)

        self.status = tk.Label(top, text="Disconnected", anchor="w")
        self.status.pack(side=tk.LEFT, fill=tk.X, expand=True)

        tk.Button(top, text="Clear", command=self.clear_rx).pack(side=tk.RIGHT, padx=(6, 0))
        tk.Button(top, text="Connect", command=self.connect).pack(side=tk.RIGHT, padx=(6, 0))

        self.rx_text = scrolledtext.ScrolledText(root, height=24, font=("Consolas", 11))
        self.rx_text.pack(fill=tk.BOTH, expand=True, padx=8, pady=(0, 8))

        send_frame = tk.Frame(root)
        send_frame.pack(fill=tk.X, padx=8, pady=(0, 8))

        self.entry = tk.Entry(send_frame, font=("Consolas", 12))
        self.entry.pack(side=tk.LEFT, fill=tk.X, expand=True)
        self.entry.bind("<Return>", self.send_command)

        tk.Button(send_frame, text="Send", command=self.send_command).pack(side=tk.LEFT, padx=(6, 0))

        quick = tk.Frame(root)
        quick.pack(fill=tk.X, padx=8, pady=(0, 8))
        for cmd in ("stop_tem", "start_tem", "T=3", "TEM_L=18.5", "TEM_H=32.0", "TIME=12:30:00"):
            tk.Button(quick, text=cmd, command=lambda c=cmd: self.send_text(c)).pack(side=tk.LEFT, padx=(0, 6))

        root.protocol("WM_DELETE_WINDOW", self.close)
        self.connect()
        self.poll_rx()

    def connect(self):
        if self.ser and self.ser.is_open:
            return
        try:
            self.ser = serial.Serial(self.port_name, self.baud, bytesize=8, parity="N", stopbits=1, timeout=0.2)
        except serial.SerialException as exc:
            messagebox.showerror("Serial error", str(exc))
            self.status.config(text="Disconnected")
            return

        self.running = True
        self.status.config(text=f"Connected: {self.port_name} @ {self.baud} 8N1")
        threading.Thread(target=self.read_loop, daemon=True).start()

    def read_loop(self):
        while self.running and self.ser and self.ser.is_open:
            try:
                data = self.ser.readline()
            except serial.SerialException as exc:
                self.rx_queue.put(f"[serial error] {exc}\n")
                break
            if data:
                self.rx_queue.put(data.decode("ascii", errors="replace"))

    def poll_rx(self):
        while True:
            try:
                text = self.rx_queue.get_nowait()
            except queue.Empty:
                break
            self.rx_text.insert(tk.END, text)
            self.rx_text.see(tk.END)
        self.root.after(80, self.poll_rx)

    def send_command(self, event=None):
        cmd = self.entry.get().strip()
        if cmd:
            self.send_text(cmd)
            self.entry.delete(0, tk.END)

    def send_text(self, cmd):
        if not self.ser or not self.ser.is_open:
            self.connect()
        if self.ser and self.ser.is_open:
            self.ser.write((cmd + "\r\n").encode("ascii"))
            self.rx_text.insert(tk.END, f"> {cmd}\n")
            self.rx_text.see(tk.END)

    def clear_rx(self):
        self.rx_text.delete("1.0", tk.END)

    def close(self):
        self.running = False
        if self.ser and self.ser.is_open:
            self.ser.close()
        self.root.destroy()


def main():
    parser = argparse.ArgumentParser(description="GUI serial monitor for the STC89C52 project.")
    parser.add_argument("--port", default="COM7")
    parser.add_argument("--baud", type=int, default=9600)
    args = parser.parse_args()

    root = tk.Tk()
    SerialGui(root, args.port, args.baud)
    root.mainloop()


if __name__ == "__main__":
    main()
