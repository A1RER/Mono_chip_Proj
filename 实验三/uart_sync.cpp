/*
 * uart_sync.cpp -- 向单片机发送实时时间 (任务五) C++ 版
 *
 * 编译:
 *   g++ uart_sync.cpp -o uart_sync.exe        (MinGW)
 *   cl  uart_sync.cpp                         (MSVC)
 *
 * 用法:
 *   uart_sync.exe              默认 COM7, 19200bps, 10s 校准一次
 *   uart_sync.exe COM5         指定串口
 *   uart_sync.exe COM5 1       指定串口 + 发送间隔(秒)
 *
 * 协议: 每次发送 "HH:MM:SS\n" (9字节), 单片机解析后写入 DS1302.
 *
 * 波特率: 19200bps. MCU 侧 SMOD=1 + TH1=0xFD, 11.0592MHz 下误差 0%.
 */

#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <ctime>

static HANDLE       g_hSerial = INVALID_HANDLE_VALUE;
static volatile int g_exit    = 0;

static BOOL WINAPI CtrlHandler(DWORD type)
{
    if (type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT || type == CTRL_CLOSE_EVENT)
    {
        g_exit = 1;
        return TRUE;
    }
    return FALSE;
}

int main(int argc, char* argv[])
{
    const char* port_short    = (argc > 1) ? argv[1]       : "COM7";
    int         sync_interval = (argc > 2) ? atoi(argv[2]) : 10;
    const DWORD baud          = 10000;

    if (sync_interval < 1) sync_interval = 1;

    /* COM10+ 必须用 \\.\COMxx 形式, 小端口号也兼容这种写法 */
    char port_full[64];
    snprintf(port_full, sizeof(port_full), "\\\\.\\%s", port_short);

    printf("[uart_sync] 连接 %s @ %lubps, 同步间隔 %ds\n", port_short, baud, sync_interval);
    printf("按 Ctrl+C 退出\n\n");

    SetConsoleCtrlHandler(CtrlHandler, TRUE);

    g_hSerial = CreateFileA(
        port_full,
        GENERIC_READ | GENERIC_WRITE,
        0,                  /* 独占访问 */
        NULL,
        OPEN_EXISTING,
        0,                  /* 同步 IO */
        NULL);

    if (g_hSerial == INVALID_HANDLE_VALUE)
    {
        fprintf(stderr, "[错误] 无法打开串口 %s, GetLastError=%lu\n",
                port_short, GetLastError());
        fprintf(stderr, "请检查: 1) COM 端口号  2) STC-ISP / 串口助手是否已关闭\n");
        return 1;
    }

    DCB dcb = {0};
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(g_hSerial, &dcb))
    {
        fprintf(stderr, "[错误] GetCommState 失败: %lu\n", GetLastError());
        CloseHandle(g_hSerial);
        return 1;
    }

    dcb.BaudRate        = baud;
    dcb.ByteSize        = 8;
    dcb.StopBits        = ONESTOPBIT;
    dcb.Parity          = NOPARITY;
    dcb.fBinary         = TRUE;
    /* 关键: DTR/RTS 保持禁用, 避免 CH340 把 DTR 拉低触发 STC 复位 */
    dcb.fDtrControl     = DTR_CONTROL_DISABLE;
    dcb.fRtsControl     = RTS_CONTROL_DISABLE;
    dcb.fOutxCtsFlow    = FALSE;
    dcb.fOutxDsrFlow    = FALSE;
    dcb.fDsrSensitivity = FALSE;
    dcb.fOutX           = FALSE;
    dcb.fInX            = FALSE;

    if (!SetCommState(g_hSerial, &dcb))
    {
        fprintf(stderr, "[错误] SetCommState 失败: %lu (驱动可能不支持 %lubps)\n",
                GetLastError(), baud);
        CloseHandle(g_hSerial);
        return 1;
    }

    COMMTIMEOUTS ct = {0};
    ct.ReadIntervalTimeout         = MAXDWORD;
    ct.ReadTotalTimeoutConstant    = 0;
    ct.ReadTotalTimeoutMultiplier  = 0;
    ct.WriteTotalTimeoutConstant   = 1000;
    ct.WriteTotalTimeoutMultiplier = 0;
    SetCommTimeouts(g_hSerial, &ct);

    /* 双保险: 显式再清一次 DTR/RTS */
    EscapeCommFunction(g_hSerial, CLRDTR);
    EscapeCommFunction(g_hSerial, CLRRTS);

    PurgeComm(g_hSerial, PURGE_RXCLEAR | PURGE_TXCLEAR);

    printf("串口已打开 (DTR/RTS=False, 单片机不会复位)\n");
    printf("请确认 LCD 已显示 'Serial+DS1302' (任务五), 否则按 K3 切换\n\n");

    while (!g_exit)
    {
        time_t     now = time(NULL);
        struct tm* lt  = localtime(&now);

        char buf[16];
        int  n = snprintf(buf, sizeof(buf), "%02d:%02d:%02d\n",
                          lt->tm_hour, lt->tm_min, lt->tm_sec);

        DWORD written = 0;
        if (!WriteFile(g_hSerial, buf, (DWORD)n, &written, NULL) || written != (DWORD)n)
        {
            fprintf(stderr, "[错误] WriteFile 失败: %lu\n", GetLastError());
            break;
        }

        buf[n - 1] = '\0';              /* 去掉 '\n' 便于打印 */
        printf("[发送] %s\n", buf);
        fflush(stdout);

        /* 粗粒度睡眠, 每 100ms 检查一次 Ctrl+C */
        for (int i = 0; i < sync_interval * 10 && !g_exit; i++)
            Sleep(100);
    }

    printf("\n[uart_sync] 已停止.\n");
    CloseHandle(g_hSerial);
    return 0;
}
