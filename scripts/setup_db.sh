#!/bin/bash
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

echo "=== AI Chat Database Setup ==="
echo ""

MYSQL_USER="${MYSQL_USER:-root}"
MYSQL_PASS="${MYSQL_PASS:-}"
MYSQL_HOST="${MYSQL_HOST:-127.0.0.1}"
MYSQL_PORT="${MYSQL_PORT:-3306}"

echo "MySQL Host: $MYSQL_HOST:$MYSQL_PORT"
echo "MySQL User: $MYSQL_USER"
echo ""

if [ -n "$MYSQL_PASS" ]; then
    mysql -h "$MYSQL_HOST" -P "$MYSQL_PORT" -u "$MYSQL_USER" -p"$MYSQL_PASS" < "$PROJECT_DIR/sql/init.sql"
else
    mysql -h "$MYSQL_HOST" -P "$MYSQL_PORT" -u "$MYSQL_USER" < "$PROJECT_DIR/sql/init.sql"
fi

if [ $? -eq 0 ]; then
    echo ""
    echo "=== Database setup completed successfully ==="
else
    echo ""
    echo "=== Database setup failed ==="
    exit 1
fi
