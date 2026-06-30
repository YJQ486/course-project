/**
 * @file    protocol.c
 * @brief   蓝牙文本行协议实现
 */
#include "protocol.h"
#include <stdio.h>
#include <string.h>

#define RX_LINE_MAX  48

static char    s_rx[RX_LINE_MAX];
static uint8_t s_rx_len;
static char    s_line[RX_LINE_MAX];     /* 最近完成的一行 */

int Protocol_BuildStatus(char *buf, int size, const UiData_t *ui)
{
    return snprintf(buf, size,
        "T:%02d:%02d:%02d S:%lu/%lu A:%d,%d,%d\n",
        ui->time.hour, ui->time.min, ui->time.sec,
        (unsigned long)ui->total_steps, (unsigned long)ui->today_steps,
        ui->sensor.ax, ui->sensor.ay, ui->sensor.az);
}

uint8_t Protocol_FeedRxByte(char c)
{
    if (c == '\r') return 0;
    if (c == '\n') {
        s_rx[s_rx_len] = '\0';
        memcpy(s_line, s_rx, s_rx_len + 1);
        s_rx_len = 0;
        return 1;
    }
    if (s_rx_len < RX_LINE_MAX - 1) {
        s_rx[s_rx_len++] = c;
    } else {
        s_rx_len = 0;        /* 溢出丢弃 */
    }
    return 0;
}

ProtoCmd_t Protocol_ParseLine(uint8_t *h, uint8_t *m, uint8_t *s)
{
    int hh, mm, ss;
    if (strncmp(s_line, "SET", 3) == 0) {
        if (sscanf(s_line, "SET %d %d %d", &hh, &mm, &ss) == 3) {
            *h = (uint8_t)hh; *m = (uint8_t)mm; *s = (uint8_t)ss;
            return CMD_SET_TIME;
        }
    } else if (strncmp(s_line, "SYNC", 4) == 0) {
        return CMD_SYNC;
    }
    return CMD_NONE;
}
