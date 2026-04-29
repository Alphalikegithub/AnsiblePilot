#!/bin/bash

########################################
# 基本信息
########################################

# 脚本所在目录
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# 程序信息
MS_APP_NAME="AnsiblePilot"
MS_APP_DIR="${SCRIPT_DIR}"
MS_APP_PATH="${MS_APP_DIR}/${MS_APP_NAME}"

# 日志目录
LOG_DIR="${SCRIPT_DIR}/logs"
mkdir -p "${LOG_DIR}"

# PID文件
PID_FILE="${LOG_DIR}/${MS_APP_NAME}.pid"

# 日志文件（按时间生成）
LOG_FILE="${LOG_DIR}/${MS_APP_NAME}_$(date +%Y%m%d_%H%M%S).log"

########################################
# 自动删除7天前日志
########################################

find "${LOG_DIR}" -type f -name "*.log" -mtime +7 -delete

########################################
# 防止重复启动
########################################

if [ -f "${PID_FILE}" ]; then
    OLD_PID=$(cat "${PID_FILE}")

    if ps -p ${OLD_PID} > /dev/null 2>&1; then
        echo "程序已经在运行！PID=${OLD_PID}"
        exit 1
    else
        echo "发现旧PID文件但进程不存在，删除旧PID文件"
        rm -f "${PID_FILE}"
    fi
fi

########################################
# 检查程序是否存在
########################################

if [ ! -f "${MS_APP_PATH}" ]; then
    echo "错误：程序 ${MS_APP_PATH} 不存在！"
    exit 1
fi

########################################
# 环境变量配置
########################################

export QT_ROOT="/home/user/QtCode/Qt5.12.5"
#export QT_ROOT="/root/work/QtCode/Qt5.12.5"

export LD_LIBRARY_PATH="${SCRIPT_DIR}/bin:${QT_ROOT}/lib:${LD_LIBRARY_PATH}"
export QT_PLUGIN_PATH="${QT_ROOT}/plugins"

########################################
# 启动信息
########################################

echo "========================================"
echo "程序启动时间：$(date)"
echo "程序路径：${MS_APP_PATH}"
echo "日志文件：${LOG_FILE}"
echo "========================================"

cd "${MS_APP_DIR}" || exit 1

########################################
# 后台启动程序
########################################

nohup "${MS_APP_PATH}" "$@" >> "${LOG_FILE}" 2>&1 &

APP_PID=$!

echo ${APP_PID} > "${PID_FILE}"

echo "========================================"
echo "程序已后台启动"
echo "PID: ${APP_PID}"
echo "日志文件: ${LOG_FILE}"
echo "========================================"

exit 0
