#!/bin/bash
set -e

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$PROJECT_DIR/build"
BUILD_TYPE="Release"
CONFIG_FILE="$PROJECT_DIR/config/config.json"

echo "========================================"
echo "  AI Chat Server - 启动脚本"
echo "========================================"

# 1. 停止正在运行的服务器
if pgrep -x ai_chat_server > /dev/null 2>&1; then
    echo "[1/4] 正在停止旧服务器..."
    pkill -x ai_chat_server 2>/dev/null || true
    sleep 1
    if pgrep -x ai_chat_server > /dev/null 2>&1; then
        pkill -9 -x ai_chat_server 2>/dev/null || true
        sleep 1
    fi
    echo "      旧服务器已停止"
else
    echo "[1/4] 旧服务器未运行，跳过"
fi

# 2. 清除旧的 build 文件
echo "[2/4] 清除旧的 build 文件..."
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
echo "      已清除"

# 3. 构建新的 build 文件
echo "[3/4] 正在编译项目..."
cd "$BUILD_DIR"
cmake "$PROJECT_DIR" \
    -DCMAKE_TOOLCHAIN_FILE=/home/oceanli/vcpkg/scripts/buildsystems/vcpkg.cmake \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_INSTALL_PREFIX="$PROJECT_DIR" \
    2>&1 | tail -3

cmake --build "$BUILD_DIR" -j$(nproc) 2>&1 | tail -3
echo "      编译完成"

# 4. 启动新项目
echo "[4/4] 正在启动服务器..."
cd "$PROJECT_DIR"
nohup "$BUILD_DIR/ai_chat_server" "$CONFIG_FILE" > /dev/null 2>&1 &
sleep 2

if pgrep -x ai_chat_server > /dev/null 2>&1; then
    echo "      服务器已启动 (PID: $(pgrep -x ai_chat_server))"
    echo ""
    echo "========================================"
    echo "  访问地址: http://localhost:8443"
    echo "  登录账号: admin_oceanli / admim1005"
    echo "========================================"
else
    echo "      ❌ 启动失败，请检查日志"
    exit 1
fi
