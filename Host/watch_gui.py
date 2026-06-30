#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
智能手表蓝牙上位机 (JDY-31 经典蓝牙 SPP / USB 串口)

下行帧 (手表 -> PC):  T:hh:mm:ss S:<total>/<today> A:<ax>,<ay>,<az>
上行帧 (PC -> 手表):  SET hh mm ss   (校时)
                      SYNC           (请求一次状态)

依赖:  pip install pyserial pyqt5
运行:  python watch_gui.py
说明:  PC 先在系统蓝牙设置里与 JDY-31 配对，会生成一个串口(COMx / /dev/rfcommX)，
       在下拉框选择该串口并连接即可。波特率默认 9600（JDY-31 出厂值）。
"""
import sys
import re
import datetime

import serial
import serial.tools.list_ports
from PyQt5 import QtWidgets, QtCore

FRAME_RE = re.compile(
    r"T:(\d+):(\d+):(\d+)\s+S:(\d+)/(\d+)\s+A:(-?\d+),(-?\d+),(-?\d+)")


class WatchGUI(QtWidgets.QWidget):
    def __init__(self):
        super().__init__()
        self.ser = None
        self.rx_buf = ""
        self._build_ui()

        self.timer = QtCore.QTimer(self)
        self.timer.timeout.connect(self.poll_serial)
        self.timer.start(100)

    def _build_ui(self):
        self.setWindowTitle("智能手表上位机 · SmartWatch Host")
        self.resize(420, 360)
        v = QtWidgets.QVBoxLayout(self)

        # 串口选择行
        h = QtWidgets.QHBoxLayout()
        self.port_box = QtWidgets.QComboBox()
        self.refresh_ports()
        self.baud_box = QtWidgets.QComboBox()
        self.baud_box.addItems(["9600", "115200", "38400"])
        self.btn_connect = QtWidgets.QPushButton("连接")
        self.btn_connect.clicked.connect(self.toggle_connect)
        btn_refresh = QtWidgets.QPushButton("刷新")
        btn_refresh.clicked.connect(self.refresh_ports)
        h.addWidget(self.port_box, 3)
        h.addWidget(self.baud_box, 1)
        h.addWidget(btn_refresh)
        h.addWidget(self.btn_connect)
        v.addLayout(h)

        # 数据显示
        self.lbl_time = QtWidgets.QLabel("--:--:--")
        self.lbl_time.setStyleSheet("font-size:48px; font-weight:bold;")
        self.lbl_time.setAlignment(QtCore.Qt.AlignCenter)
        v.addWidget(self.lbl_time)

        grid = QtWidgets.QGridLayout()
        self.lbl_today = QtWidgets.QLabel("0")
        self.lbl_total = QtWidgets.QLabel("0")
        self.lbl_acc = QtWidgets.QLabel("0, 0, 0")
        grid.addWidget(QtWidgets.QLabel("今日步数:"), 0, 0)
        grid.addWidget(self.lbl_today, 0, 1)
        grid.addWidget(QtWidgets.QLabel("累计步数:"), 1, 0)
        grid.addWidget(self.lbl_total, 1, 1)
        grid.addWidget(QtWidgets.QLabel("加速度 XYZ:"), 2, 0)
        grid.addWidget(self.lbl_acc, 2, 1)
        v.addLayout(grid)

        # 操作按钮
        h2 = QtWidgets.QHBoxLayout()
        btn_sync = QtWidgets.QPushButton("请求同步 (SYNC)")
        btn_sync.clicked.connect(lambda: self.send("SYNC\n"))
        btn_settime = QtWidgets.QPushButton("用本机时间校时")
        btn_settime.clicked.connect(self.sync_time)
        h2.addWidget(btn_sync)
        h2.addWidget(btn_settime)
        v.addLayout(h2)

        self.status = QtWidgets.QLabel("未连接")
        v.addWidget(self.status)

    def refresh_ports(self):
        self.port_box.clear()
        for p in serial.tools.list_ports.comports():
            self.port_box.addItem(p.device)

    def toggle_connect(self):
        if self.ser and self.ser.is_open:
            self.ser.close()
            self.ser = None
            self.btn_connect.setText("连接")
            self.status.setText("已断开")
            return
        port = self.port_box.currentText()
        baud = int(self.baud_box.currentText())
        try:
            self.ser = serial.Serial(port, baud, timeout=0)
            self.btn_connect.setText("断开")
            self.status.setText(f"已连接 {port} @ {baud}")
        except Exception as e:
            self.status.setText(f"打开失败: {e}")

    def send(self, text):
        if self.ser and self.ser.is_open:
            self.ser.write(text.encode("ascii"))
            self.status.setText(f"已发送: {text.strip()}")

    def sync_time(self):
        now = datetime.datetime.now()
        self.send(f"SET {now.hour} {now.minute} {now.second}\n")

    def poll_serial(self):
        if not (self.ser and self.ser.is_open):
            return
        try:
            data = self.ser.read(256)
        except Exception:
            return
        if not data:
            return
        self.rx_buf += data.decode("ascii", errors="ignore")
        while "\n" in self.rx_buf:
            line, self.rx_buf = self.rx_buf.split("\n", 1)
            self.parse_line(line.strip())

    def parse_line(self, line):
        m = FRAME_RE.search(line)
        if not m:
            return
        hh, mm, ss, total, today, ax, ay, az = m.groups()
        self.lbl_time.setText(f"{int(hh):02d}:{int(mm):02d}:{int(ss):02d}")
        self.lbl_today.setText(today)
        self.lbl_total.setText(total)
        self.lbl_acc.setText(f"{ax}, {ay}, {az}")


def main():
    app = QtWidgets.QApplication(sys.argv)
    w = WatchGUI()
    w.show()
    sys.exit(app.exec_())


if __name__ == "__main__":
    main()
