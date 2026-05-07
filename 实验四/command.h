/*
 * command.h  --  串口命令解析
 *
 * 支持命令:
 *   start_tem
 *   stop_tem
 *   T=X            (1 < X < 9, 即 2..8)
 *   TEM_L=XX.X
 *   TEM_H=XX.X
 *   TIME=hh:mm:ss
 *
 * 解析规则:
 *   - 大小写敏感, 严格匹配 (与示例一致).
 *   - 未知命令 -> "ERROR:CMD"
 *   - 数值非法 -> "ERROR:VALUE"
 *
 * 解析后通过 g_* 全局变量影响主循环行为.
 */

#ifndef __COMMAND_H__
#define __COMMAND_H__

#include "config.h"

void Command_Parse(void);

#endif
