# AI Chat Room

A high-performance, real-time AI chat server built with modern C++ (Drogon framework), MySQL, and Redis. Features user authentication, AI-powered conversations, admin management, WebSocket-based push notifications, and a built-in ban/kick system.

## Features

- **User System**: Email-based registration, login, password reset, account deletion with 24-hour cooling-off period
- **AI Chat**: Integration with OpenAI / DeepSeek / compatible APIs for intelligent conversations
- **Admin Dashboard**: User management (search, ban/unban, force delete), server statistics, log viewer
- **Notification Push**: WebSocket-based real-time ban notifications, push to connected clients
- **Security**: JWT authentication, salted password hashing (SHA-256 + 32-byte salt), parameterized SQL queries, HTTPS support
- **Rate Limiting**: Input validation on both client and server sides
- **Auto Cleanup**: Scheduled cleanup of old chat messages (30 days) and expired verification codes (every 60s)

## Tech Stack

| Component | Technology | Purpose |
|-----------|-----------|---------|
| Backend Framework | [Drogon](https://github.com/drogonframework/drogon) 1.9+ | High-performance async HTTP/HTTPS + WebSocket server |
| Database | MySQL 8.0+ | Persistent storage for users, messages, logs |
| Cache | Redis 6.0+ | Session tokens, online status, rate limiting |
| Logging | [spdlog](https://github.com/gabime/spdlog) | Multi-level file + console logging with rotation |
| Authentication | JWT (custom, HS256) | Stateless token-based auth (7-day expiry) |
| AI Integration | REST API | OpenAI / DeepSeek compatible chat completions |
| Email | SMTP (via curl) | Verification codes, account notifications |
| Frontend | Vanilla HTML/CSS/JS | Served directly by Drogon, no build tools needed |

## Requirements

- **Compiler**: GCC 8+ or Clang 10+ (C++17 support required)
- **Build System**: CMake 3.20+
- **Databases**: MySQL 8.0+, Redis 6.0+
- **SSL**: OpenSSL 1.1+
- **Package Manager**: vcpkg (recommended for dependency management)

### Dependencies

| Dependency | Installation |
|-----------|-------------|
| Drogon (with MySQL + Redis) | `vcpkg install drogon[mysql,redis]` |
| spdlog | `apt install libspdlog-dev` or `vcpkg install spdlog` |
| nlohmann-json | `apt install nlohmann-json3-dev` or `vcpkg install nlohmann-json` |
| hiredis | `apt install libhiredis-dev` or `vcpkg install hiredis` |
| MySQL Connector/C++ | `apt install libmysqlcppconn-dev` or `vcpkg install mysql-connector-cpp` |
| OpenSSL | `apt install libssl-dev` |

## Project Structure

```
ai_chat/
├── CMakeLists.txt                   # Build configuration
├── config/
│   ├── config.json                  # Server configuration (with credentials)
│   ├── config.example.json          # Template configuration
│   ├── server.crt                   # HTTPS certificate
│   └── server.key                   # HTTPS private key
├── sql/
│   └── init.sql                     # Database schema initialization
├── scripts/
│   ├── setup_db.sh                  # Database setup script
│   └── gen_cert.sh                  # Self-signed certificate generator
├── src/
│   ├── main.cpp                     # Entry point
│   ├── controllers/
│   │   ├── AuthController.h         # Authentication (register, login, reset password)
│   │   ├── ChatController.h         # AI chat messaging
│   │   ├── UserController.h         # User profile, account deletion
│   │   ├── AdminController.h        # Admin operations (stats, users, bans, logs)
│   │   └── NotificationController.h # WebSocket push notifications
│   ├── database/
│   │   ├── MySQLClient.h            # MySQL connection pool
│   │   └── RedisClient.h            # Redis client
│   ├── middleware/
│   │   └── AuthMiddleware.h         # JWT authentication middleware
│   ├── models/
│   │   ├── User.h                   # User data model
│   │   └── ChatMessage.h            # Chat message model
│   └── utils/
│       ├── Logger.h                 # spdlog wrapper
│       ├── JWTUtils.h               # JWT token utilities
│       ├── PasswordHasher.h         # Password hashing (SHA-256 + salt)
│       ├── IDGenerator.h            # Snowflake-style ID generator
│       ├── EmailSender.h            # SMTP email sender
│       └── Validator.h              # Input validation helpers
├── views/
│   ├── login.html                   # Login page
│   ├── register.html                # Registration page
│   ├── reset_password.html          # Password reset page
│   ├── index.html                   # Main app (chat + profile)
│   ├── admin.html                   # Admin dashboard
│   ├── css/style.css                # Global stylesheet
│   └── js/app.js                    # Shared utilities (toast, API, WebSocket)
├── start.sh                         # One-click build & start script
├── .gitignore                       # Git ignore rules
├── LICENSE                          # MIT License
└── README.md                        # This file
```

## Quick Start

### 1. Install System Dependencies

```bash
# Ubuntu / Debian
sudo apt update
sudo apt install -y build-essential cmake git libssl-dev \
    libmysqlcppconn-dev mysql-server mysql-client \
    redis-server libhiredis-dev nlohmann-json3-dev libspdlog-dev \
    curl

# Install vcpkg (if not already installed)
git clone https://github.com/Microsoft/vcpkg.git ~/vcpkg
~/vcpkg/bootstrap-vcpkg.sh
~/vcpkg/vcpkg install drogon[mysql,redis] spdlog nlohmann-json hiredis

# Start services
sudo systemctl start mysql
sudo systemctl start redis
```

### 2. Initialize Database

```bash
# Option A: Use setup script
bash scripts/setup_db.sh

# Option B: Manual import
mysql -u root -p < sql/init.sql
```

### 3. Generate HTTPS Certificate (Optional)

For local development, you can use the self-signed certificate generator:

```bash
bash scripts/gen_cert.sh
```

For production, replace `config/server.crt` and `config/server.key` with CA-signed certificates.

### 4. Configure

Edit `config/config.json` with your settings:

```json
{
  "email": {
    "username": "your_email@qq.com",
    "password": "your_smtp_authorization_code",
    "from_address": "your_email@qq.com"
  },
  "ai": {
    "api_key": "your_openai_or_deepseek_api_key",
    "api_url": "https://api.deepseek.com",
    "model": "deepseek-chat"
  },
  "admin": {
    "username": "admin_oceanli",
    "password": "admim1005"
  }
}
```

> **Note**: Email is optional. Without it, verification codes are printed to the server log.
> **Note**: AI API key is optional. Without it, the AI will respond with a "not configured" message.

### 5. Build & Run

```bash
# One-click build and start
bash start.sh

# Or manually:
mkdir -p build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=$HOME/vcpkg/scripts/buildsystems/vcpkg.cmake \
         -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
cd ..
./build/ai_chat_server config/config.json
```

### 6. Access

Open your browser to:

```
https://localhost:8443
```

## Pages

| Page | URL | Description |
|------|-----|-------------|
| Login | `/login.html` | User login |
| Register | `/register.html` | Email-based registration |
| Reset Password | `/reset_password.html` | Password reset via email |
| Main App | `/index.html` | AI Chat + Profile (requires login) |
| Admin | `/admin.html` | Admin dashboard (requires admin role) |

## Administration

The admin account is automatically created on first server startup.

- **Username**: `admin_oceanli`
- **Password**: `admim1005`

Admin features available at `/admin.html`:

- **Dashboard**: View system statistics (total users, online users, total chats)
- **User Management**: Search users, view details, ban/unban with configurable duration and reason, force delete accounts
- **Server Logs**: View and filter application logs

### Ban/Unban Flow

1. Admin bans a user → record stored in MySQL + Redis
2. WebSocket notification pushed to the banned user in real-time
3. Affected user sees a modal with ban reason and expiry time
4. All subsequent API requests from the banned user are rejected by AuthMiddleware
5. Admin can unban at any time

## API Reference

All API responses follow the format:

```json
{
  "code": 200,
  "message": "操作成功",
  "data": { ... }
}
```

### Authentication (Public)

| Method | Endpoint | Description |
|--------|----------|-------------|
| POST | `/api/auth/send-code` | Send email verification code |
| POST | `/api/auth/verify-code` | Verify email code |
| POST | `/api/auth/check-email` | Check email availability |
| POST | `/api/auth/register` | Register new account |
| POST | `/api/auth/login` | Login (returns JWT token) |
| POST | `/api/auth/reset-password` | Reset password (requires verified code) |

### User (Authenticated)

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/user/profile` | Get user profile |
| PUT | `/api/user/profile` | Update username/display name |
| PUT | `/api/user/password` | Change password |
| DELETE | `/api/user/account` | Delete account (requires verification code, 24h cooling-off) |
| POST | `/api/user/cancel-delete` | Cancel pending deletion |

### Chat (Authenticated)

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/chat/history` | Get chat history (paginated) |
| POST | `/api/chat/send` | Send a message and get AI response |
| DELETE | `/api/chat/clear` | Clear all chat history |

### Admin (Requires admin role)

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/admin/stats` | Get system statistics |
| GET | `/api/admin/users` | List users (paginated) |
| POST | `/api/admin/search` | Search users by ID or username |
| POST | `/api/admin/ban` | Ban or unban a user |
| POST | `/api/admin/delete-user` | Force delete a user account |
| GET | `/api/admin/logs` | View server logs (paginated) |

### Notifications (Authenticated)

| Method | Endpoint | Description |
|--------|----------|-------------|
| WebSocket | `/ws/notification?token={jwt}` | Real-time notification connection |

## Configuration Reference

Full `config/config.json` structure:

```jsonc
{
  "server": {
    "port": 8443,              // HTTPS listen port
    "thread_num": 4,           // Event loop thread count
    "document_root": "views",  // Static file root
    "home_page": "login.html"  // Default redirect
  },
  "database": {
    "mysql": {
      "host": "127.0.0.1",
      "port": 3306,
      "user": "root",
      "password": "",
      "database": "ai_chat",
      "pool_size": 10
    },
    "redis": {
      "host": "127.0.0.1",
      "port": 6379,
      "password": "",
      "db": 0,
      "pool_size": 5
    }
  },
  "email": {
    "smtp_host": "smtp.qq.com",
    "smtp_port": 465,
    "username": "",
    "password": "",
    "from_address": ""
  },
  "ai": {
    "api_url": "https://api.openai.com/v1/chat/completions",
    "api_key": "",
    "model": "gpt-3.5-turbo"
  },
  "admin": {
    "username": "admin_oceanli",
    "password": "admim1005"
  },
  "log": {
    "level": "info",           // Log level: trace/debug/info/warn/error
    "file": "logs/server.log", // Log file path
    "max_size": 10485760,      // 10MB max per file
    "max_files": 7             // Keep 7 rotated files
  }
}
```

> A template file `config/config.example.json` is provided with placeholder values.

## Database Schema

The database `ai_chat` contains the following tables:

| Table | Purpose |
|-------|---------|
| `users` | User accounts with hashed passwords, roles (user/admin), ban status |
| `chat_messages` | Chat message history (user_id, role, content, token_count) |
| `email_verifications` | Email verification codes (supports register + reset_password types) |

Schema file: `sql/init.sql`

## Security Features

- **Password Storage**: SHA-256 hash with 32-byte random salt per user
- **Transport Security**: HTTPS with TLS encryption
- **Authentication**: JWT tokens (7-day expiry, server-side session validation)
- **Account Protection**: Ban system with configurable duration and reason, instant enforcement via API middleware
- **Input Validation**: Length limits, format checks on both frontend and backend
- **SQL Injection Prevention**: All queries use parameterized prepared statements
- **XSS Prevention**: User content is HTML-escaped before rendering
- **Automatic Cleanup**: Old chat messages (30 days), expired verification codes, and pending deletion accounts cleaned automatically every 60 seconds

## Account Deletion Flow

1. User requests account deletion → verification code sent to email
2. User enters code to confirm → account marked as `pending_deletion`
3. 24-hour cooling-off period begins
4. During cooling-off: login triggers prompt asking to cancel or proceed
5. User can cancel deletion (then locked out for 3 days)
6. After 24 hours, account is permanently deleted by cleanup task
7. Admin can also force-delete any user account

## Development

### Building from Source

```bash
# Clean build
rm -rf build && mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=$HOME/vcpkg/scripts/buildsystems/vcpkg.cmake \
         -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
```

### Logging

Logs are written to both console and file (`logs/server.log`). Log level can be configured in `config.json`. File rotation is configured with max size and file count.

## Troubleshooting

### Server won't start, port already in use

```bash
# Check what's using the port
sudo lsof -i :8443

# Kill the process
sudo fuser -k 8443/tcp

# Or use a different port
sed -i 's/8443/9443/' config/config.json
```

### MySQL connection errors

```
[error] MySQL connection failed: Unknown database 'ai_chat'
```

Run the database initialization:

```bash
mysql -u root -p < sql/init.sql
```

### Verification codes not arriving

Codes are logged to the server log as a fallback:

```bash
grep "验" logs/server.log
```

For email delivery, ensure SMTP settings are correct. QQ email requires an **authorization code** (not the account password).

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.

## Contact

For issues and feature requests, please open an issue on the project repository.
