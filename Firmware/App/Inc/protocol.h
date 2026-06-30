/**
 * @file    protocol.h
 * @brief   蓝牙文本行协议（便于 Python 上位机调试与对接）
 *
 * 下行（手表 -> 上位机）：  "T:hh:mm:ss S:<total>/<today> A:<ax>,<ay>,<az>\n"
 * 上行（上位机 -> 手表）：  "SET hh mm ss\n"   校时
 *                          "SYNC\n"            请求一次状态
 */
#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include "menu.h"

typedef enum {
    CMD_NONE = 0,
    CMD_SET_TIME,
    CMD_SYNC
} ProtoCmd_t;

/* 构造一行状态文本，返回写入长度 */
int  Protocol_BuildStatus(char *buf, int size, const UiData_t *ui);

/* 逐字节喂入接收数据；收到完整一行返回 1，并把行存入内部缓冲 */
uint8_t Protocol_FeedRxByte(char c);

/* 解析最近完成的一行；CMD_SET_TIME 时通过 h/m/s 输出 */
ProtoCmd_t Protocol_ParseLine(uint8_t *h, uint8_t *m, uint8_t *s);

#endif /* PROTOCOL_H */
