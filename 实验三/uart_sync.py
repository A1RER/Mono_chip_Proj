"""
uart_sync.py -- 向单片机发送实时时间 (任务五)

用法:
    pip install pyserial
    python uart_sync.py              # 默认 COM7, 9600bps, 10s 校准一次
    python uart_sync.py COM5         # 指定串口
    python uart_sync.py COM5 1       # 指定串口 + 发送间隔(秒)

协议: 每次发送 "HH:MM:SS\n" (9字节), 单片机解析后写入 DS1302.
策略: 初次发送立即校准, 之后每隔 SYNC_INTERVAL 秒再校准一次;
      两次校准之间由 DS1302 自行计时, 兼顾效率与准确性.
"""

import sys
import time
import datetime
import serial

PORT          = sys.argv[1] if len(sys.argv) > 1 else "COM7"
BAUD          = 9600
SYNC_INTERVAL = int(sys.argv[2]) if len(sys.argv) > 2 else 10   # 秒


def main():
    print(f"[uart_sync] 连接 {PORT} @ {BAUD}bps, 同步间隔 {SYNC_INTERVAL}s")
    print("按 Ctrl+C 退出\n")

    # 必须先建对象、设 dtr=False, 再 open()
    # 否则 pyserial 在 open() 瞬间拉低 DTR, 通过 CH340 触发单片机复位
    ser = serial.Serial()
    ser.port     = PORT
    ser.baudrate = BAUD
    ser.timeout  = 1
    ser.dtr      = False
    ser.rts      = False
    try:
        ser.open()
    except serial.SerialException as e:
        print(f"[错误] 无法打开串口: {e}")
        print("请检查: 1) COM 端口号是否正确  2) EIDE/STC-ISP 串口监视器是否已关闭")
        sys.exit(1)

    print("串口已打开 (DTR 保持 False, 单片机不会复位)")
    print("请确认 LCD 已显示 'Serial+DS1302' (任务五), 否则按 K3 切换\n")

    try:
        while True:
            now = datetime.datetime.now()
            time_str = now.strftime("%H:%M:%S\n")
            ser.write(time_str.encode("ascii"))
            print(f"[发送] {time_str.strip()}", flush=True)
            time.sleep(SYNC_INTERVAL)
    except KeyboardInterrupt:
        print("\n[uart_sync] 已停止.")
    finally:
        ser.close()


if __name__ == "__main__":
    main()
