#!/bin/bash

set -e

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$PROJECT_DIR/build2"
SERVER_BIN="$BUILD_DIR/server/ai_chat_server"
CONFIG_FILE="$PROJECT_DIR/config/server.conf"
PID_FILE="$PROJECT_DIR/.server.pid"
LOG_DIR="$PROJECT_DIR/logs"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

print_info()  { echo -e "${CYAN}[INFO]${NC}  $1"; }
print_ok()    { echo -e "${GREEN}[OK]${NC}    $1"; }
print_warn()  { echo -e "${YELLOW}[WARN]${NC}  $1"; }
print_err()   { echo -e "${RED}[ERROR]${NC} $1"; }

cleanup() {
    if [[ -f "$PID_FILE" ]]; then
        local old_pid
        old_pid=$(cat "$PID_FILE")
        if kill -0 "$old_pid" 2>/dev/null; then
            print_info "Stopping existing server (PID: $old_pid)..."
            kill "$old_pid" 2>/dev/null
            for i in {1..10}; do
                if ! kill -0 "$old_pid" 2>/dev/null; then break; fi
                sleep 0.5
            done
            if kill -0 "$old_pid" 2>/dev/null; then
                kill -9 "$old_pid" 2>/dev/null
                print_warn "Force killed server (PID: $old_pid)"
            else
                print_ok "Server stopped gracefully"
            fi
        fi
        rm -f "$PID_FILE"
    fi
}

check_dependencies() {
    local missing=false

    if ! command -v mysql &>/dev/null; then
        print_warn "mysql client not found (optional, needed for DB management)"
    fi

    if ! command -v redis-cli &>/dev/null; then
        print_warn "redis-cli not found (optional, needed for Redis check)"
    fi
}

check_prerequisites() {
    local ok=true

    if command -v mysqladmin &>/dev/null; then
        if mysqladmin ping -u "$DB_USER" -p"$DB_PASS" --silent 2>/dev/null; then
            print_ok "MySQL is running ($DB_USER@$DB_HOST:$DB_PORT)"
        else
            print_err "MySQL is not reachable ($DB_USER@$DB_HOST:$DB_PORT)"
            print_info "  Start MySQL: sudo systemctl start mysql  or  sudo service mysql start"
            ok=false
        fi
    else
        print_warn "mysqladmin not found, skipping MySQL check"
    fi

    if command -v redis-cli &>/dev/null; then
        if redis-cli -h "$REDIS_HOST" -p "$REDIS_PORT" ping 2>/dev/null | grep -q "PONG"; then
            print_ok "Redis is running ($REDIS_HOST:$REDIS_PORT)"
        else
            print_err "Redis is not reachable ($REDIS_HOST:$REDIS_PORT)"
            print_info "  Start Redis: sudo systemctl start redis-server  or  sudo service redis-server start"
            ok=false
        fi
    else
        print_warn "redis-cli not found, skipping Redis check"
    fi

    if [[ ! -f "$SERVER_BIN" ]]; then
        print_err "Server binary not found at: $SERVER_BIN"
        print_info "  Build the project first: cmake --build $BUILD_DIR -j\$(nproc)"
        ok=false
    fi

    if [[ ! -f "$CONFIG_FILE" ]]; then
        print_warn "Config file not found at: $CONFIG_FILE"
    fi

    $ok
}

build() {
    print_info "Building project..."
    if [[ ! -d "$BUILD_DIR" ]]; then
        cmake -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
    fi
    cmake --build "$BUILD_DIR" -j"$(nproc)"
    print_ok "Build complete"
}

init_db() {
    local sql_file="$PROJECT_DIR/scripts/init_db.sql"
    if [[ ! -f "$sql_file" ]]; then
        print_warn "Database init script not found: $sql_file"
        print_info "  Skipping database initialization"
        return
    fi

    if [[ -z "$DB_PASS" ]]; then
        read -rsp "Enter MySQL password: " DB_PASS
        echo
    fi

    print_info "Initializing database..."
    if mysql -h "$DB_HOST" -P "$DB_PORT" -u "$DB_USER" -p"$DB_PASS" < "$sql_file" 2>/dev/null; then
        print_ok "Database initialized successfully"
    else
        print_err "Database initialization failed"
        print_info "  Check: Is MySQL running? Is the password correct?"
        return 1
    fi
}

start_server() {
    local cmd="$SERVER_BIN"
    if [[ -n "$1" ]]; then
        cmd="$cmd $1"
    fi

    mkdir -p "$LOG_DIR"

    print_info "Starting ai_chat server..."
    nohup $cmd > "$LOG_DIR/output.log" 2>&1 &
    local pid=$!
    echo "$pid" > "$PID_FILE"

    sleep 1
    if kill -0 "$pid" 2>/dev/null; then
        print_ok "Server started (PID: $pid)"
        print_info "  HTTP API:  http://$SRV_ADDR:$SRV_HTTP_PORT"
        print_info "  WebSocket: ws://$SRV_ADDR:$SRV_WS_PORT"
        print_info "  Log file:  $LOG_DIR/output.log"
    else
        print_err "Server failed to start"
        print_info "  Check logs: cat $LOG_DIR/output.log"
        return 1
    fi
}

show_status() {
    if [[ -f "$PID_FILE" ]]; then
        local pid
        pid=$(cat "$PID_FILE")
        if kill -0 "$pid" 2>/dev/null; then
            print_ok "Server is running (PID: $pid)"
            return 0
        else
            print_warn "Server is not running (stale PID file)"
            rm -f "$PID_FILE"
        fi
    fi

    if pgrep -f "$SERVER_BIN" > /dev/null 2>&1; then
        local pid
        pid=$(pgrep -f "$SERVER_BIN" | head -1)
        print_ok "Server is running (PID: $pid)"
    else
        print_warn "Server is not running"
    fi
}

load_config() {
    if command -v python3 &>/dev/null; then
        local json
        json=$(python3 -c "
import json
with open('$CONFIG_FILE') as f:
    c = json.load(f)
db = c.get('database', {})
redis = c.get('redis', {})
srv = c.get('server', {})
print(f'{db.get(\"host\",\"127.0.0.1\")}|{db.get(\"port\",3306)}|{db.get(\"user\",\"root\")}|{db.get(\"password\",\"\")}')
print(f'{redis.get(\"host\",\"127.0.0.1\")}|{redis.get(\"port\",6379)}')
print(f'{srv.get(\"http_addr\",\"0.0.0.0\")}|{srv.get(\"http_port\",8080)}|{srv.get(\"ws_port\",8081)}')
" 2>/dev/null) || json=""
        if [[ -n "$json" ]]; then
            local db_line redis_line srv_line
            db_line=$(echo "$json" | sed -n '1p')
            redis_line=$(echo "$json" | sed -n '2p')
            srv_line=$(echo "$json" | sed -n '3p')

            DB_HOST=$(echo "$db_line" | cut -d'|' -f1)
            DB_PORT=$(echo "$db_line" | cut -d'|' -f2)
            DB_USER=$(echo "$db_line" | cut -d'|' -f3)
            DB_PASS=$(echo "$db_line" | cut -d'|' -f4)
            REDIS_HOST=$(echo "$redis_line" | cut -d'|' -f1)
            REDIS_PORT=$(echo "$redis_line" | cut -d'|' -f2)
            SRV_ADDR=$(echo "$srv_line" | cut -d'|' -f1)
            SRV_HTTP_PORT=$(echo "$srv_line" | cut -d'|' -f2)
            SRV_WS_PORT=$(echo "$srv_line" | cut -d'|' -f3)
        fi
    fi

    : "${DB_HOST:=127.0.0.1}"
    : "${DB_PORT:=3306}"
    : "${DB_USER:=root}"
    : "${DB_PASS:=}"
    : "${REDIS_HOST:=127.0.0.1}"
    : "${REDIS_PORT:=6379}"
    : "${SRV_ADDR:=0.0.0.0}"
    : "${SRV_HTTP_PORT:=8080}"
    : "${SRV_WS_PORT:=8081}"
}

usage() {
    echo ""
    echo -e "${CYAN}ai_chat server startup script${NC}"
    echo ""
    echo "Usage: $0 {start|stop|restart|status|build|init-db} [config_path]"
    echo ""
    echo "Commands:"
    echo "  start       Build (if needed), check deps, and start server"
    echo "  stop        Stop running server"
    echo "  restart     Restart server"
    echo "  status      Check server status"
    echo "  build       Build project only"
    echo "  init-db     Initialize MySQL database only"
    echo ""
    echo "Options:"
    echo "  config_path  Optional path to config file"
    echo ""
    echo "Examples:"
    echo "  $0 start                 Start with default config"
    echo "  $0 start ./my.conf       Start with custom config"
    echo "  $0 restart               Restart server"
    echo "  $0 status                Check if server is running"
    echo "  $0 init-db               Initialize database"
    echo ""
}

case "${1:-help}" in
    start)
        load_config
        check_dependencies
        build
        check_prerequisites || { print_err "Prerequisites not met"; exit 1; }
        cleanup
        start_server "$2"
        ;;
    stop)
        cleanup
        print_ok "Server stopped"
        ;;
    restart)
        load_config
        cleanup
        sleep 1
        build
        start_server "$2"
        ;;
    status)
        show_status
        ;;
    build)
        build
        ;;
    init-db)
        load_config
        init_db
        ;;
    help|--help|-h)
        usage
        ;;
    *)
        print_err "Unknown command: $1"
        usage
        exit 1
        ;;
esac
