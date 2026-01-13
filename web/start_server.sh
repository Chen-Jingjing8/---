#!/bin/bash

echo "========== 无人机管理与网络威胁预警系统 =========="
echo

echo "正在启动后端服务..."
echo

# 检查Python是否安装
if ! command -v python3 &> /dev/null; then
    echo "错误：未找到Python3，请先安装Python 3.6+"
    exit 1
fi

# 安装必要的Python包
echo "检查并安装必要的Python包..."
pip3 install flask flask-cors > /dev/null 2>&1

# 启动后端服务
echo "启动后端桥接服务..."
echo "访问地址: http://127.0.0.1:5000"
echo
echo "按 Ctrl+C 停止服务"
echo

python3 backend_bridge.py
