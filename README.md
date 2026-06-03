# AI 聊天室

高性能、实时 AI 聊天服务器，基于现代 C++（Drogon 框架）、MySQL 和 Redis 构建。支持用户认证、AI 智能对话、管理员后台、WebSocket 实时推送通知。

---

## 功能特性

### 用户系统
- **邮箱注册**：通过邮箱验证码完成注册
- **登录认证**：支持用户名或邮箱登录，JWT Token 鉴权
- **密码管理**：支持修改密码、邮箱重置密码
- **账号注销**：24 小时冷静期机制，冷静期内可取消注销
- **个人资料**：查看/修改用户名等个人信息

### AI 聊天
- **智能对话**：接入 OpenAI / DeepSeek 等兼容 API，实现 AI 对话
- **聊天记录**：消息持久化存储，支持分页查询历史记录
- **内容限制**：消息长度限制 2000 字符，防止滥用

### 管理员系统
- **数据统计**：查看注册用户数、在线用户数、总对话数
- **用户管理**：搜索用户、封禁/解封（可设时长和原因）、强制注销
- **日志查看**：在线查看服务器运行日志，支持级别过滤

### 通知推送
- **WebSocket 实时通知**：封禁通知实时推送

### 安全防护
- **密码安全**：PBKDF2-HMAC-SHA1 加盐哈希，10 万次迭代
- **身份认证**：JWT Token + Redis 服务端会话验证（7 天有效期）
- **接口限流**：按 IP 维度限流，防止暴力破解（每 5 分钟 10 次）
- **SQL 注入防护**：全部使用参数化预编译语句
- **HTTPS 支持**：内置 TLS 加密传输
- **CSRF 防护**：Origin 请求来源校验
- **内容安全策略**：CSP 响应头配置

### 自动维护
- **聊天记录清理**：自动删除 30 天前的聊天记录（每 60 秒）
- **验证码清理**：自动清理过期的邮箱验证码
- **账号清理**：自动删除冷静期已过的注销账号
---

## 技术栈

| 组件 | 技术 | 用途 |
|------|------|------|
| 后端框架 | [Drogon](https://github.com/drogonframework/drogon) 1.9+ | 高性能异步 HTTP/HTTPS + WebSocket 服务器 |
| 数据库 | MySQL 8.0+ | 用户、聊天记录等持久化存储 |
| 缓存 | Redis 6.0+ | 会话 Token、在线状态、验证码、限流计数 |
| 日志 | [spdlog](https://github.com/gabime/spdlog) | 多级别文件 + 控制台日志，支持轮转 |
| 认证 | JWT（自定义实现，HS256） | 无状态 Token 认证（7 天过期） |
| AI 集成 | REST API | OpenAI / DeepSeek 兼容的聊天补全接口 |
| 邮件 | SMTP（via libcurl） | 发送验证码、账号通知 |
| 前端 | 原生 HTML/CSS/JS | Drogon 直接托管静态文件，无需构建工具 |

---

## 环境要求

### 系统依赖

| 依赖项 | 最低版本 | 说明 |
|--------|----------|------|
| 编译器 | GCC 8+ / Clang 10+ | 需支持 C++17 |
| CMake | 3.20+ | 构建系统 |
| MySQL | 8.0+ | 数据库服务 |
| Redis | 6.0+ | 缓存服务 |
| OpenSSL | 1.1+ | HTTPS 和密码哈希 |
| vcpkg | 最新版 | 推荐用于管理 C++ 依赖 |

### 第三方库

| 库名 | 安装方式 |
|------|----------|
| Drogon（含 MySQL + Redis） | `vcpkg install drogon[mysql,redis]` |
| spdlog | `apt install libspdlog-dev` 或 `vcpkg install spdlog` |
| nlohmann-json | `apt install nlohmann-json3-dev` 或 `vcpkg install nlohmann-json` |
| hiredis | `apt install libhiredis-dev` 或 `vcpkg install hiredis` |
| MySQL Connector/C++ | `apt install libmysqlcppconn-dev` 或 `vcpkg install mysql-connector-cpp` |
| libcurl | `apt install libcurl4-openssl-dev` |

---

## 快速开始

### 1. 安装系统依赖

```bash
# Ubuntu / Debian
sudo apt update
sudo apt install -y build-essential cmake git libssl-dev \
    libmysqlcppconn-dev mysql-server mysql-client \
    redis-server libhiredis-dev nlohmann-json3-dev \
    libspdlog-dev libcurl4-openssl-dev

# 安装 vcpkg（如果尚未安装）
git clone https://github.com/Microsoft/vcpkg.git ~/vcpkg
~/vcpkg/bootstrap-vcpkg.sh

# 安装 Drogon（核心依赖）
~/vcpkg/vcpkg install drogon[mysql,redis]

# 启动数据库服务
sudo systemctl start mysql
sudo systemctl start redis
```

### 2. 初始化数据库

```bash
# 方式一：使用脚本
bash scripts/setup_db.sh

# 方式二：手动导入
mysql -u root -p < sql/init.sql
```

### 3. 生成 HTTPS 证书（可选）

本地开发可使用自签名证书：

```bash
bash scripts/gen_cert.sh
```

生产环境请替换 `config/server.crt` 和 `config/server.key` 为 CA 签名证书。

### 4. 配置文件

复制配置模板并编辑：

```bash
cp config/config.example.json config/config.json
```

编辑 `config/config.json`，至少配置以下项：

```json
{
  "server": {
    "jwt_secret": "请设置一个随机字符串作为 JWT 密钥"
  },
  "database": {
    "mysql": {
      "password": "你的 MySQL 密码"
    }
  },
  "admin": {
    "username": "admin",
    "password": "请设置管理员密码"
  },
  "ai": {
    "api_key": "你的 OpenAI 或 DeepSeek API 密钥",
    "api_url": "https://api.deepseek.com",
    "model": "deepseek-chat"
  }
}
```

> **提示**：
> - 邮箱为可选项。未配置时，验证码会输出到服务器日志中
> - AI API 密钥为可选项。未配置时，AI 会回复"服务未配置"提示

### 5. 编译并运行

```bash
# 一键构建并启动
bash start.sh

# 或手动构建
mkdir -p build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=$HOME/vcpkg/scripts/buildsystems/vcpkg.cmake \
         -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
cd ..
./build/ai_chat_server config/config.json
```

### 6. 访问

打开浏览器访问：

```
https://localhost:8443
```

管理员账号在首次启动时自动创建，请查看配置文件中的 `admin` 字段。

---

## 页面导航

| 页面 | URL | 说明 |
|------|-----|------|
| 登录 | `/login.html` | 用户登录 |
| 注册 | `/register.html` | 邮箱注册 |
| 重置密码 | `/reset_password.html` | 通过邮箱重置密码 |
| 主应用 | `/index.html` | AI 聊天 + 个人中心（需登录） |
| 管理后台 | `/admin.html` | 系统管理（需管理员权限） |

---

## API 参考

所有 API 响应遵循统一格式：

```json
{
  "code": 200,
  "message": "操作成功",
  "data": { ... }
}
```

### 认证接口（公开）

| 方法 | 路径 | 说明 |
|------|------|------|
| POST | `/api/auth/send-code` | 发送邮箱验证码 |
| POST | `/api/auth/send-delete-code` | 发送注销验证码 |
| POST | `/api/auth/register` | 注册新用户 |
| POST | `/api/auth/login` | 登录（返回 JWT Token） |
| POST | `/api/auth/reset-password` | 重置密码（需验证码） |

### 用户接口（需登录）

| 方法 | 路径 | 说明 |
|------|------|------|
| GET | `/api/user/profile` | 获取个人信息 |
| PUT | `/api/user/profile` | 修改用户名/昵称 |
| PUT | `/api/user/password` | 修改密码（需旧密码验证，踢下线其他会话） |
| DELETE | `/api/user/account` | 注销账号（需邮箱验证码，24 小时冷静期） |
| POST | `/api/user/cancel-delete` | 取消注销申请（Token 或 用户名+密码验证） |

### 聊天接口（需登录）

| 方法 | 路径 | 说明 |
|------|------|------|
| GET | `/api/chat/history` | 获取聊天记录（支持分页） |
| POST | `/api/chat/send` | 发送消息并获取 AI 回复 |
| DELETE | `/api/chat/clear` | 清除全部聊天记录 |

### 管理员接口（需管理员权限）

| 方法 | 路径 | 说明 |
|------|------|------|
| GET | `/api/admin/stats` | 获取系统统计数据 |
| GET | `/api/admin/users` | 获取用户列表（分页，含在线状态） |
| POST | `/api/admin/search` | 搜索用户（支持用户名/邮箱/ID） |
| POST | `/api/admin/ban` | 封禁/解封用户 |
| POST | `/api/admin/delete-user` | 强制注销用户 |
| GET | `/api/admin/logs` | 查看服务器日志（分页，支持级别过滤） |

### 通知接口（需登录）

| 方法 | 路径 | 说明 |
|------|------|------|
| WebSocket | `/ws/notification?token={jwt}` | 实时通知连接 |

---

## 配置说明

完整配置文件结构（`config/config.json`）：

```json
{
  "server": {
    "port": 8443,
    "enable_https": true,
    "https_cert": "config/server.crt",
    "https_key": "config/server.key",
    "thread_num": 4,
    "document_root": "views",
    "home_page": "login.html",
    "jwt_secret": ""
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
      "db": 0
    }
  },
  "email": {
    "smtp_host": "smtp.example.com",
    "smtp_port": 465,
    "username": "",
    "password": "",
    "from_address": ""
  },
  "ai": {
    "api_url": "https://api.openai.com",
    "api_key": "",
    "model": "gpt-3.5-turbo"
  },
  "admin": {
    "username": "admin",
    "password": ""
  },
  "log": {
    "level": "info",
    "file": "logs/server.log",
    "max_size": 10485760,
    "max_files": 7
  }
}
```

### 配置项说明

| 配置项 | 类型 | 说明 |
|--------|------|------|
| `server.port` | int | 监听端口 |
| `server.enable_https` | bool | 是否启用 HTTPS |
| `server.https_cert` | string | HTTPS 证书路径 |
| `server.https_key` | string | HTTPS 私钥路径 |
| `server.thread_num` | int | 事件循环线程数 |
| `server.document_root` | string | 静态文件根目录 |
| `server.home_page` | string | 首页文件名 |
| `server.jwt_secret` | string | JWT 签名密钥（建议设置随机字符串） |
| `database.mysql.*` | - | MySQL 连接配置（主机/端口/用户/密码/库名/池大小） |
| `database.redis.*` | - | Redis 连接配置（主机/端口/密码/数据库编号） |
| `email.*` | - | SMTP 邮件配置（主机/端口/用户名/密码/发件地址） |
| `ai.api_url` | string | AI API 基础地址 |
| `ai.api_key` | string | AI API 密钥 |
| `ai.model` | string | AI 模型名称 |
| `admin.*` | - | 初始管理员账号配置 |
| `log.level` | string | 日志级别（trace/debug/info/warn/error） |
| `log.file` | string | 日志文件路径 |
| `log.max_size` | int | 单个日志文件最大字节数（默认 10MB） |
| `log.max_files` | int | 保留的轮转文件数 |

---

## 数据库设计

### MySQL 表结构

| 表名 | 用途 |
|------|------|
| `users` | 用户账号（用户名、密码哈希、角色、状态、封禁信息） |
| `chat_messages` | 聊天记录（用户 ID、角色、内容、Token 数） |
| `email_verifications` | 邮箱验证码（类型：register/reset_password） |


### Redis 数据结构

| Key 模式 | 类型 | 用途 | TTL |
|----------|------|------|-----|
| `session:{token}` | String (JSON) | 用户会话信息 | 7 天 |
| `verify_code:{email}:{type}` | String | 邮箱验证码 | 5-10 分钟 |
| `online_users` | Set | 在线用户 ID 集合 | 无 |
| `banned_users` | Set | 被封禁用户 ID 集合 | 无 |
| `banned_info:{user_id}` | String (JSON) | 封禁详细信息 | 封禁时长 |
| `user_sessions:{user_id}` | Set | 用户的所有活跃会话 | 无 |
| `rate_limit:{func}:{ip}` | String (计数) | 接口限流计数 | 5 分钟 |

完整建表语句见 [`sql/init.sql`](sql/init.sql)。

---

## 项目结构

```
ai_chat/
├── CMakeLists.txt                    # CMake 构建配置
├── config/
│   ├── config.example.json           # 配置模板（不含敏感信息）
│   ├── server.crt                    # HTTPS 证书
│   └── server.key                    # HTTPS 私钥
├── sql/
│   └── init.sql                      # 数据库初始化脚本
├── scripts/
│   ├── setup_db.sh                   # 数据库初始化脚本
│   └── gen_cert.sh                   # 自签名证书生成脚本
├── src/
│   ├── main.cpp                      # 程序入口，加载配置、初始化组件
│   ├── controllers/
│   │   ├── AuthController.h/cpp      # 认证（注册、登录、重置密码）
│   │   ├── ChatController.h/cpp      # AI 聊天
│   │   ├── UserController.h/cpp      # 用户资料、账号注销
│   │   ├── AdminController.h/cpp     # 管理操作（统计、用户管理、封禁、日志）
│   │   └── NotificationController.h/cpp  # WebSocket 实时通知
│   ├── database/
│   │   ├── MySQLClient.h/cpp         # MySQL 连接池
│   │   └── RedisClient.h/cpp         # Redis 客户端
│   ├── middleware/
│   │   └── AuthMiddleware.h/cpp      # JWT 认证中间件
│   ├── models/
│   │   ├── User.h                    # 用户数据模型
│   │   └── ChatMessage.h             # 聊天消息模型
│   └── utils/
│       ├── Logger.h                  # spdlog 日志封装
│       ├── JWTUtils.h/cpp            # JWT Token 工具
│       ├── PasswordHasher.h/cpp      # PBKDF2-HMAC-SHA1 密码哈希
│       ├── IDGenerator.h/cpp         # 雪花算法 ID 生成器
│       ├── EmailSender.h/cpp         # SMTP 邮件发送
│       └── Validator.h               # 输入验证工具
├── views/
│   ├── login.html                    # 登录页
│   ├── register.html                 # 注册页
│   ├── reset_password.html           # 重置密码页
│   ├── index.html                    # 主应用（聊天 + 个人中心）
│   ├── admin.html                    # 管理后台
│   ├── css/style.css                 # 全局样式
│   └── js/app.js                     # 公共工具（提示、API、WebSocket）
├── start.sh                          # 一键构建启动脚本
├── .gitignore                        # Git 忽略规则
├── LICENSE                           # MIT 开源许可证
└── README.md                         # 本文件
```

---

## 核心业务流程

### 注册流程

```
填写邮箱 → 点击发送验证码 → 后端生成6位随机码 →
存入Redis（5分钟过期）→ 发送邮件 →
用户填写验证码 + 用户名 + 密码 →
后端验证验证码 → 检查邮箱/用户名是否重复 →
生成唯一ID → PBKDF2 加盐哈希密码 → 写入 MySQL → 返回成功
```

### 登录流程

```
输入用户名/邮箱 + 密码 → 查询用户 → 验证密码哈希 →
检查账号状态（是否封禁/注销中）→ 生成 JWT Token →
存入 Redis 会话 → 返回 Token + 用户信息
```

### AI 聊天流程

```
发送消息 → 验证 Token → 保存用户消息到 MySQL →
异步调用 AI 接口获取回复 → 保存 AI 回复到 MySQL →
更新用户累计对话数 → 返回 AI 回复
```

### 账号注销流程

1. 用户请求注销 → 发送邮箱验证码
2. 用户输入验证码确认 → 账号标记为 `pending_deletion`
3. 24 小时冷静期开始
4. 冷静期内登录会收到提示
5. 用户可随时取消注销（取消后 72 小时内不可再次申请）
6. 24 小时后自动清理任务永久删除账号
7. 管理员也可强制注销任意用户

### 封禁流程

1. 管理员设置封禁时长和原因
2. 数据库更新用户状态 + Redis 记录封禁信息
3. WebSocket 实时通知被封禁用户
4. 被封禁用户的后续 API 请求被中间件拦截
5. 所有活跃会话被清除

---

## 常见问题

### 服务器启动失败，端口被占用

```bash
# 查看端口占用
sudo lsof -i :8443

# 终止进程
sudo fuser -k 8443/tcp

# 或修改配置文件中的端口号
```

### MySQL 连接错误

```
[error] MySQL connection failed: Unknown database 'ai_chat'
```

执行数据库初始化：

```bash
mysql -u root -p < sql/init.sql
```

### 收不到验证码邮件

验证码会作为备用方案输出到服务器日志中：

```bash
grep "验证码" logs/server.log
```

如需要邮件正常发送，请确保：
- SMTP 配置正确
- QQ 邮箱需使用**授权码**而非登录密码
- 检查 SMTP 端口（465 为隐式 TLS，587 为 STARTTLS）

### Redis 连接失败

```bash
# 检查 Redis 是否运行
sudo systemctl status redis

# 启动 Redis
sudo systemctl start redis
```

### 编译错误：找不到 Drogon

确保通过 vcpkg 正确安装：

```bash
~/vcpkg/vcpkg install drogon[mysql,redis]
cmake .. -DCMAKE_TOOLCHAIN_FILE=$HOME/vcpkg/scripts/buildsystems/vcpkg.cmake
```

---

## 贡献指南

欢迎提交 Issue 和 Pull Request 参与项目贡献。

### 开发流程

1. Fork 本仓库
2. 创建特性分支：`git checkout -b feature/your-feature`
3. 提交变更：`git commit -m 'feat: add some feature'`
4. 推送分支：`git push origin feature/your-feature`
5. 创建 Pull Request

### 编码规范

- 遵循 C++17 标准
- 头文件（`.h`）声明与实现文件（`.cpp`）分离
- 使用 Drogon 框架的 Controller 分层架构
- 所有 SQL 查询使用参数化预编译语句
- 添加适当的错误处理和日志记录

---

## 许可证

本项目基于 MIT 许可证开源。详见 [LICENSE](LICENSE) 文件。
