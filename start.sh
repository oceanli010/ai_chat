#!/bin/bash
set -e

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$PROJECT_DIR/build"
BUILD_TYPE="Release"
CONFIG_FILE="$PROJECT_DIR/config/config.json"
NO_BUILD=false

# 解析命令行参数
for arg in "$@"; do
    case "$arg" in
        -n|--no-build)
            NO_BUILD=true
            ;;
        *)
            CONFIG_FILE="$arg"
            ;;
    esac
done

echo "========================================"
echo "  AI Chat Server - 启动脚本"
echo "========================================"

STEP=1
TOTAL_STEPS=$([ "$NO_BUILD" = true ] && echo 2 || echo 4)

# 1. 停止正在运行的服务器
echo "[$STEP/$TOTAL_STEPS] 正在停止旧服务器..."
if pgrep -x ai_chat_server > /dev/null 2>&1; then
    pkill -x ai_chat_server 2>/dev/null || true
    sleep 1
    if pgrep -x ai_chat_server > /dev/null 2>&1; then
        pkill -9 -x ai_chat_server 2>/dev/null || true
        sleep 1
    fi
    echo "      旧服务器已停止"
else
    echo "      旧服务器未运行，跳过"
fi
STEP=$((STEP + 1))

if [ "$NO_BUILD" = false ]; then
    # 2. 清除旧的 build 文件
    echo "[$STEP/$TOTAL_STEPS] 清除旧的 build 文件..."
    rm -rf "$BUILD_DIR"
    mkdir -p "$BUILD_DIR"
    echo "      已清除"
    STEP=$((STEP + 1))

    # 3. 构建新的 build 文件
    echo "[$STEP/$TOTAL_STEPS] 正在编译项目..."
    cd "$BUILD_DIR"
    cmake "$PROJECT_DIR" \
        -DCMAKE_TOOLCHAIN_FILE=/home/oceanli/vcpkg/scripts/buildsystems/vcpkg.cmake \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
        -DCMAKE_INSTALL_PREFIX="$PROJECT_DIR" \
        2>&1 | tail -3

    cmake --build "$BUILD_DIR" -j$(nproc) 2>&1 | tail -3
    echo "      编译完成"
    STEP=$((STEP + 1))
fi

# 启动服务器
EXECUTABLE="$BUILD_DIR/ai_chat_server"
if [ ! -f "$EXECUTABLE" ]; then
    echo "      ❌ 可执行文件不存在: $EXECUTABLE"
    echo "      请先运行不带 --no-build 参数的启动脚本进行编译"
    exit 1
fi

echo "[$STEP/$TOTAL_STEPS] 正在启动服务器..."
cd "$PROJECT_DIR"
nohup "$EXECUTABLE" "$CONFIG_FILE" > /dev/null 2>&1 &
sleep 2

if pgrep -x ai_chat_server > /dev/null 2>&1; then
    echo "      服务器已启动 (PID: $(pgrep -x ai_chat_server))"
    echo ""
    echo "========================================"
    echo "  访问地址: http://localhost:8444"
    echo "  登录账号: 请查看配置文件获取账号信息"
    echo "========================================"
else
    echo "      ❌ 启动失败，请检查日志"
    exit 1
fi
