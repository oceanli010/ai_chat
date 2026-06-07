# AI Chat 连接管理与通信机制详解

> 本文档基于 AI Chat 项目的源代码，深入解析其网络连接管理与通信流程的核心实现。

---

## 目录

- [1. 连接建立](#1-连接建立)
  - [1.1 系统启动整体流程](#11-系统启动整体流程)
  - [1.2 HTTP/HTTPS 服务器初始化](#12-httphttps-服务器初始化)
  - [1.3 MySQL 连接池初始化](#13-mysql-连接池初始化)
  - [1.4 Redis 连接初始化](#14-redis-连接初始化)
  - [1.5 WebSocket 连接握手](#15-websocket-连接握手)
  - [1.6 JWT 会话建立](#16-jwt-会话建立)
- [2. 连接销毁](#2-连接销毁)
  - [2.1 HTTP 连接关闭](#21-http-连接关闭)
  - [2.2 MySQL 连接归还与销毁](#22-mysql-连接归还与销毁)
  - [2.3 Redis 连接释放](#23-redis-连接释放)
  - [2.4 WebSocket 连接断开](#24-websocket-连接断开)
  - [2.5 用户登出与会话销毁](#25-用户登出与会话销毁)
  - [2.6 封禁用户强制断连](#26-封禁用户强制断连)
  - [2.7 定时清理任务](#27-定时清理任务)
- [3. 连接管理](#3-连接管理)
  - [3.1 MySQL 连接池架构](#31-mysql-连接池架构)
  - [3.2 连接获取策略](#32-连接获取策略)
  - [3.3 连接归还策略](#33-连接归还策略)
  - [3.4 健康检查机制](#34-健康检查机制)
  - [3.5 RAII 作用域管理](#35-raii-作用域管理)
  - [3.6 Redis 单连接模型](#36-redis-单连接模型)
  - [3.7 事件循环线程并发模型](#37-事件循环线程并发模型)
  - [3.8 连接池状态监控](#38-连接池状态监控)
- [4. 通信机制](#4-通信机制)
  - [4.1 HTTP 请求/响应处理流程](#41-http-请求响应处理流程)
  - [4.2 中间件认证链路](#42-中间件认证链路)
  - [4.3 数据封装格式](#43-数据封装格式)
  - [4.4 AI API 异步通信流程](#44-ai-api-异步通信流程)
  - [4.5 WebSocket 实时推送](#45-websocket-实时推送)
  - [4.6 异常处理与重试策略](#46-异常处理与重试策略)

---

## 1. 连接建立

### 1.1 系统启动整体流程

项目入口在 `src/main.cpp`，其启动流程严格按以下顺序执行：

```cpp
// src/main.cpp:130-253
int main(int argc, char* argv[]) {
    // ① 加载配置文件
    auto config = loadConfig(config_path);

    // ② 初始化日志系统
    Logger::init(log_cfg["file"].asString(), log_cfg["level"].asString(),
                 log_cfg["max_size"].asUInt64(), log_cfg["max_files"].asUInt());

    // ③ 初始化 MySQL 连接池
    MySQLClient::instance().init(host, port, user, password, database, pool_size);

    // ④ 初始化 Redis 连接
    RedisClient::instance().init(host, port, password, db);

    // ⑤ 创建管理员账号
    initAdminAccount(config);

    // ⑥ 初始化 JWT 密钥
    JWTUtils::init(jwt_secret);

    // ⑦ 配置邮件发送器和 AI 接口
    // ...

    // ⑧ 注册定时清理任务（每 60 秒）
    scheduleCleanupTask();

    // ⑨ 配置 HTTP/HTTPS 监听
    app().addListener("0.0.0.0", port, use_https, cert, key);

    // ⑩ 设置线程数、文档根目录、安全策略
    app().setThreadNum(4);
    app().setDocumentRoot("views");
    app().registerPostHandlingAdvice(...);

    // ⑪ 启动 Drogon 事件循环
    app().run();
}
```

**启动依赖顺序图**：每一步依赖前一步的结果，配置加载是所有操作的先决条件。

---

### 1.2 HTTP/HTTPS 服务器初始化

#### 配置参数

```json
{
  "server": {
    "port": 8888,
    "thread_num": 4,
    "enable_https": false,
    "https_cert": "config/server.crt",
    "https_key": "config/server.key",
    "document_root": "views",
    "jwt_secret": ""
  }
}
```

| 参数 | 类型 | 说明 | 默认值 |
|------|------|------|--------|
| `port` | int | 监听端口 | 8888 |
| `thread_num` | int | 事件循环线程数 | 4 |
| `enable_https` | bool | 是否启用 HTTPS | false |
| `https_cert` | string | TLS 证书路径 | - |
| `https_key` | string | TLS 私钥路径 | - |
| `document_root` | string | 静态文件根目录 | views |

#### 监听器创建逻辑

```cpp
// src/main.cpp:209-216
bool use_https = server_cfg.isMember("enable_https") && server_cfg["enable_https"].asBool();
if (use_https) {
    // HTTPS 模式：Drogon 内部使用 OpenSSL 进行 TLS 握手
    app().addListener("0.0.0.0", server_cfg["port"].asInt(), true, cert, key);
} else {
    // HTTP 模式：纯 TCP 监听
    app().addListener("0.0.0.0", server_cfg["port"].asInt());
}
```

`addListener("0.0.0.0", port)` 绑定到所有网卡接口。Drogon 底层调用 trantor 的 `TcpServer` 完成：

1. `socket()` — 创建 TCP socket
2. `bind()` — 绑定到 `0.0.0.0:8888`
3. `listen()` — 开始监听，设置 backlog 队列
4. `epoll_ctl(EPOLL_CTL_ADD)` — 将 socket 加入 epoll 监听集合

对于 HTTPS 模式，额外加载 OpenSSL 证书和私钥，在 TCP 连接建立后自动执行 TLS 握手协议。

#### 事件循环线程启动

```cpp
// src/main.cpp:218
app().setThreadNum(server_cfg["thread_num"].asInt());  // 4 个 I/O 线程
```

Drogon 创建 4 个线程，每个线程运行独立的 `epoll` 事件循环（One Loop Per Thread 模式）。每个线程内部结构：

```
EventLoop Thread
├── epoll_create1()           # 创建 epoll 实例
├── epoll_wait() 循环          # 阻塞等待事件
├── 事件分发（回调）             # 调用 Controller
└── 定时器队列（runEvery/runAfter）
```

---

### 1.3 MySQL 连接池初始化

#### 配置参数

```json
{
  "database": {
    "mysql": {
      "host": "127.0.0.1",
      "port": 3306,
      "user": "root",
      "password": "123456",
      "database": "ai_chat",
      "pool_size": 10
    }
  }
}
```

#### 初始化流程

```cpp
// src/database/MySQLClient.cpp:25-52
void MySQLClient::init(const std::string& host, int port,
                        const std::string& user, const std::string& password,
                        const std::string& database, int pool_size) {
    host_ = host;           // 保存连接参数
    port_ = port;
    user_ = user;
    password_ = password;
    database_ = database;
    pool_size_ = pool_size;

    driver_ = get_driver_instance();  // 获取 MySQL Connector/C++ 驱动

    // 预创建 pool_size 个连接
    for (int i = 0; i < pool_size_; ++i) {
        auto conn = create_connection();
        if (conn) {
            pool_.push(std::move(conn));
        }
    }

    APP_LOG_INFO("MySQL connection pool initialized with {}/{} connections",
                 pool_.size(), pool_size_);
    if (pool_.empty()) {
        APP_LOG_WARN("MySQL pool is empty — database may not exist");
    }
}
```

#### 单条连接创建

```cpp
// src/database/MySQLClient.cpp:8-19
std::unique_ptr<sql::Connection> MySQLClient::create_connection() {
    try {
        // ① TCP 连接到 MySQL 服务器
        auto conn = std::unique_ptr<sql::Connection>(
            driver_->connect(host_ + ":" + std::to_string(port_), user_, password_));
        // ② 选择数据库
        conn->setSchema(database_);
        return conn;
    } catch (const sql::SQLException& e) {
        APP_LOG_ERROR("MySQL connection failed: {}", e.what());
        return nullptr;
    }
}
```

MySQL 连接建立涉及：
1. **TCP 三次握手** — `driver_->connect()` 内部完成
2. **MySQL 协议握手** — 服务端发送初始握手包，客户端响应认证信息
3. **认证** — 用户名/密码验证（默认 `caching_sha2_password` 或 `mysql_native_password`）
4. **schema 选择** — `setSchema()` 发送 `USE ai_chat` 命令

---

### 1.4 Redis 连接初始化

#### 配置参数

```json
{
  "database": {
    "redis": {
      "host": "127.0.0.1",
      "port": 6379,
      "password": "",
      "db": 0
    }
  }
}
```

#### 初始化流程

```cpp
// src/database/RedisClient.cpp:15-50
void RedisClient::init(const std::string& host, int port,
                        const std::string& password, int db) {
    host_ = host;
    port_ = port;
    password_ = password;
    db_ = db;

    // ① TCP 连接到 Redis 服务器（hiredis）
    context_ = redisConnect(host.c_str(), port);
    if (!context_ || context_->err) {
        APP_LOG_ERROR("Redis connection failed: {}",
                  context_ ? context_->errstr : "null context");
        if (context_) {
            redisFree(context_);
            context_ = nullptr;
        }
        return;
    }

    // ② 密码认证（可选）
    if (!password_.empty()) {
        auto* reply = (redisReply*)redisCommand(context_, "AUTH %s", password_.c_str());
        if (!reply || reply->type == REDIS_REPLY_ERROR) {
            APP_LOG_ERROR("Redis auth failed");
        }
        freeReplyObject(reply);
    }

    // ③ 选择数据库（编号 > 0 时）
    if (db_ > 0) {
        auto* reply = (redisReply*)redisCommand(context_, "SELECT %d", db_);
        freeReplyObject(reply);
    }

    APP_LOG_INFO("Redis connected to {}:{}", host_, port_);
}
```

Redis 连接采用 hiredis 的**同步阻塞** API，创建后全局唯一，所有 Redis 操作共享这一条连接，通过 `std::mutex` 保证线程安全。

---

### 1.5 WebSocket 连接握手

#### 路由注册

```cpp
// src/controllers/NotificationController.h:29-31
WS_PATH_LIST_BEGIN
WS_PATH_ADD("/ws/notification");
WS_PATH_LIST_END
```

#### 握手认证流程

WebSocket 连接由 HTTP Upgrade 请求发起，携带 JWT Token 进行身份认证：

```cpp
// src/controllers/NotificationController.cpp:13-51
void NotificationController::handleNewConnection(
        const HttpRequestPtr& req,
        const WebSocketConnectionPtr& conn) {

    // ① 从 URL 参数或 Authorization 头提取 Token
    auto token = req->getParameter("token");
    if (token.empty()) {
        auto auth = req->getHeader("Authorization");
        if (auth.size() > 7 && auth.substr(0, 7) == "Bearer ") {
            token = auth.substr(7);
        }
    }

    // ② 没有 Token 则直接关闭连接
    if (token.empty()) {
        conn->shutdown();
        return;
    }

    try {
        // ③ JWT 签名验证
        auto data = JWTUtils::verify_token(token);
        auto user_id = data.user_id;

        // ④ Redis 会话有效性验证
        auto session_key = "session:" + token;
        if (!RedisClient::instance().exists(session_key)) {
            conn->shutdown();
            return;
        }

        // ⑤ 绑定用户上下文
        conn->setContext(std::make_shared<UserContext>(user_id, data.username));

        // ⑥ 注册到连接映射表
        {
            std::lock_guard<std::mutex> lock(connections_mutex_);
            connections_[user_id].push_back(conn);
        }

        APP_LOG_INFO("WebSocket connected for user {} ({})", user_id, data.username);
    } catch (const std::exception& e) {
        APP_LOG_WARN("WebSocket auth failed: {}", e.what());
        conn->shutdown();
    }
}
```

**WebSocket 握手协议层面**：

```
客户端                                        服务端
  │                                              │
  │── HTTP GET /ws/notification?token=xxx ──────→│
  │   Upgrade: websocket                          │  ← HTTP 升级请求
  │   Connection: Upgrade                         │
  │   Sec-WebSocket-Key: dGhlIHNhbXBsZQ==         │
  │                                              │
  │←── HTTP 101 Switching Protocols ──────────── │  ← 协议切换响应
  │   Upgrade: websocket                          │
  │   Connection: Upgrade                         │
  │   Sec-WebSocket-Accept: s3pPLMBiTxaQ9k...    │
  │                                              │
  │←──────── WebSocket 帧双向通信 ────────────→  │  ← 全双工数据传输
```

**认证失败处理**：
- Token 缺失 → `conn->shutdown()` 直接关闭
- Token 无效/过期 → 捕获异常后 `conn->shutdown()`
- 会话不存在 → Redis 中 session 过期后拒绝连接

---

### 1.6 JWT 会话建立

用户登录成功后，系统创建完整的会话体系：

```cpp
// src/controllers/AuthController.cpp:336-358
// ① 生成 JWT Token（HS256，有效期 7 天）
std::string token = JWTUtils::generate_token(user_id, username, role);

// ② 将会话信息写入 Redis
Json::Value session_data;
session_data["user_id"] = user_id;
session_data["username"] = username;
session_data["role"] = role;
RedisClient::instance().setex("session:" + token, 86400 * 7,
                               session_data.toStyledString());

// ③ 更新在线用户和会话集合
RedisClient::instance().sadd("online_users", std::to_string(user_id));
RedisClient::instance().sadd("user_sessions:" + std::to_string(user_id), token);
```

JWT Token 结构：

```
Header.Payload.Signature
│       │           │
│       │           └── HMAC-SHA256(Header.Payload, secret)
│       └── {"user_id": 123, "username": "oceanli",
│            "role": "user", "exp": 1749168000}
└── {"alg": "HS256", "typ": "JWT"}
```

会话数据在 Redis 中的分布：

| Redis Key | 类型 | 内容 | TTL |
|-----------|------|------|-----|
| `session:{token}` | String (JSON) | `{"user_id":...,"username":...,"role":...}` | 7 天 |
| `online_users` | Set | 所有在线用户的 ID 集合 | 无 |
| `user_sessions:{user_id}` | Set | 该用户的所有活跃 Token | 无 |

---

## 2. 连接销毁

### 2.1 HTTP 连接关闭

HTTP/1.1 连接的关闭分为以下情况：

**正常关闭**：
- **短连接**：响应发送完毕后，服务端调用 `close(fd)` 关闭 TCP 连接
- **Keep-Alive**：连接保持打开，等待下一个请求，直到超时或达到请求数上限

**异常关闭**：
- **客户端断开**：epoll 检测到 `EPOLLRDHUP` / `EPOLLHUP` 事件
- **超时**：客户端在 Keep-Alive 超时（默认 60 秒）内未发送新请求
- **RST 报文**：客户端异常退出导致 TCP RST，Drogon 框架自动处理

**全局安全响应头**：

```cpp
// src/main.cpp:231-243
app().registerPostHandlingAdvice([](const HttpRequestPtr& req, const HttpResponsePtr& resp) {
    resp->addHeader("Content-Security-Policy",
        "default-src 'self'; "
        "script-src 'self' 'unsafe-inline'; "
        "style-src 'self' 'unsafe-inline'; "
        "img-src 'self' data:; "
        "font-src 'self'; "
        "connect-src 'self'; "
        "frame-ancestors 'none'; "
        "form-action 'self'"
    );
});
```

---

### 2.2 MySQL 连接归还与销毁

#### 归还到连接池

```cpp
// src/database/MySQLClient.cpp:101-123
void MySQLClient::release(std::unique_ptr<sql::Connection> conn) {
    if (!conn) return;

    // ① 健康检查：验证连接是否仍然有效
    bool valid = false;
    try {
        valid = !conn->isClosed();
    } catch (...) {
        valid = false;
    }

    if (valid) {
        // ② 有效连接：入队归还
        std::lock_guard<std::mutex> lock(mutex_);
        pool_.push(std::move(conn));
        cond_.notify_one();                     // 唤醒等待连接的线程
    } else {
        // ③ 无效连接：丢弃并补充新连接
        APP_LOG_WARN("Release: discarding stale MySQL connection");
        auto new_conn = create_connection();
        if (new_conn) {
            std::lock_guard<std::mutex> lock(mutex_);
            pool_.push(std::move(new_conn));
            cond_.notify_one();
        }
    }
}
```

**连接的生命周期状态**：

```
[创建] → [入队(空闲)] → [出队(使用中)] → [归还(检查)]
                                              ├── 有效 → [入队(空闲)]
                                              └── 无效 → [丢弃] → [补建新连接 → 入队]
```

---

### 2.3 Redis 连接释放

Redis 连接在进程结束时由析构函数清理：

```cpp
// src/database/RedisClient.cpp:5-9
RedisClient::~RedisClient() {
    if (context_) {
        redisFree(context_);   // hiredis 内部关闭 TCP 连接并释放内存
    }
}
```

由于 Redis 连接是单例的静态对象，其在程序退出时自动析构。`redisFree()` 内部执行：
1. 发送 `QUIT` 命令（如有必要）
2. `close(fd)` 关闭 TCP 连接
3. 释放 `redisContext` 内存

---

### 2.4 WebSocket 连接断开

#### 主动关闭

服务端通过 `conn->shutdown()` 主动拒绝或关闭连接，发生在以下场景：
- Token 缺失或无效
- JWT 签名验证失败
- Redis 会话不存在

#### 被动关闭处理

客户端主动关闭或网络中断时触发：

```cpp
// src/controllers/NotificationController.cpp:63-79
void NotificationController::handleConnectionClosed(
        const WebSocketConnectionPtr& conn) {
    auto ctx = conn->getContext<UserContext>();
    if (ctx) {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        auto it = connections_.find(ctx->user_id);
        if (it != connections_.end()) {
            auto& vec = it->second;

            // ① 从该用户的连接列表中移除当前连接
            vec.erase(std::remove(vec.begin(), vec.end(), conn), vec.end());

            // ② 如果用户没有其他连接，删除整个映射条目
            if (vec.empty()) {
                connections_.erase(it);
            }
        }
        APP_LOG_INFO("WebSocket disconnected for user {}", ctx->user_id);
    }
}
```

**资源清理步骤**：
1. 从 `connections_` 映射中移除该连接对象
2. 如果用户没有其他活跃连接，删除整个用户条目
3. `WebSocketConnectionPtr` 的 shared_ptr 引用计数归零后自动析构
4. 底层 Drogon 框架关闭 TCP socket

---

### 2.5 用户登出与会话销毁

用户主动登出时（由 UserController 处理），执行以下清理：

```
用户登出操作
├── ① Redis del("session:{token}")          —— 删除会话数据
├── ② Redis srem("online_users", user_id)    —— 从在线用户集合移除
├── ③ Redis srem("user_sessions:{user_id}", token) —— 从用户会话集合移除
└── ④ 通知客户端清除本地 Token
```

### 2.6 封禁用户强制断连

管理员封禁用户时，系统执行多层次的连接清理：

```cpp
// src/middleware/AuthMiddleware.cpp:93-134
// 检测到被封禁后：
// ① 从 Redis 获取封禁详情
Json::Value ban_data;
ban_data["banned"] = true;
std::string ban_info_str = RedisClient::instance().get("banned_info:" + user_id);

// ② 删除当前会话
RedisClient::instance().del("session:" + token);

// ③ 从在线用户集合移除
RedisClient::instance().srem("online_users", user_id);

// ④ 返回 403 + 封禁详情
resp->setStatusCode(drogon::k403Forbidden);
resp->setBody(result.toStyledString());
```

同时通过 WebSocket 实时推送封禁通知：

```cpp
// src/controllers/NotificationController.cpp:84-104
void NotificationController::sendBanNotification(uint64_t user_id,
        const std::string& ban_reason, int duration_hours) {
    Json::Value msg;
    msg["type"] = "ban";
    msg["ban_reason"] = ban_reason;
    msg["duration_hours"] = duration_hours;
    msg["banned_at"] = /* 当前时间戳 */;

    std::string payload = msg.toStyledString();

    std::lock_guard<std::mutex> lock(connections_mutex_);
    auto it = connections_.find(user_id);
    if (it != connections_.end()) {
        for (const auto& conn : it->second) {
            conn->send(payload);          // 推送封禁消息
        }
    }
}
```

---

### 2.7 定时清理任务

系统每 60 秒执行一次自动化资源清理：

```cpp
// src/main.cpp:83-121
void scheduleCleanupTask() {
    app().getLoop()->runEvery(60.0, []() {
        auto conn = MySQLClient::instance().acquire();

        // ① 清理 30 天前的聊天记录
        "DELETE FROM chat_messages WHERE created_at < DATE_SUB(NOW(), INTERVAL 30 DAY)"

        // ② 清理已过期的邮箱验证码
        "DELETE FROM email_verifications WHERE expires_at < NOW()"

        // ③ 清理冷却期已过的待注销账号
        "SELECT id FROM users WHERE status = 'pending_deletion' AND deleted_at <= NOW()"
        // → 删除用户记录

        MySQLClient::instance().release(std::move(conn));
    });
}
```

---

## 3. 连接管理

### 3.1 MySQL 连接池架构

项目实现了一个完整的**生产者-消费者连接池**，核心数据结构如下：

```cpp
// src/database/MySQLClient.h:78-80
std::mutex mutex_;                                    // 互斥锁
std::condition_variable cond_;                        // 条件变量
std::queue<std::unique_ptr<sql::Connection>> pool_;   // 连接队列（FIFO）
```

**架构图**：

```
┌─────────────────────────────────────────────────────────────────┐
│                     MySQL 连接池（pool_size = 10）               │
│                                                                 │
│  Controller Thread 1  ── acquire() ──→ ┌──────┐ ←── release()  │
│  Controller Thread 2  ── acquire() ──→ │ 连接 │ ←── release()  │
│  Controller Thread 3  ── acquire() ──→ │ 队列 │ ←── release()  │
│  Controller Thread 4  ── acquire() ──→ │ FIFO │ ←── release()  │
│                                 │      └──┬───┘                │
│                   cond_.notify_one()      │                     │
│                   cond_.wait_for(10s) ────┘                     │
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │ MySQL Server (127.0.0.1:3306)                          │    │
│  │ 最大连接数限制: max_connections = 151 (MySQL 默认)     │    │
│  └─────────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────────┘
```

**关键设计决策**：

| 设计点 | 选择 | 原因 |
|--------|------|------|
| 数据结构 | `std::queue` (FIFO) | 保证连接公平使用，避免某条连接被过度使用 |
| 存储方式 | `std::unique_ptr` | 独占所有权，防止连接被多处引用 |
| 通知机制 | `std::condition_variable` | 池空时阻塞等待，有连接归还时精准唤醒 |
| 锁粒度 | 全局 `std::mutex` | 简单可靠，池操作本身极短，锁竞争不显著 |

---

### 3.2 连接获取策略

```cpp
// src/database/MySQLClient.cpp:59-95
std::unique_ptr<sql::Connection> MySQLClient::acquire() {
    std::unique_lock<std::mutex> lock(mutex_);

    // ① 池为空时的等待/创建策略
    while (pool_.empty()) {
        if (cond_.wait_for(lock, std::chrono::seconds(10)) == std::cv_status::timeout) {
            // 超时 10 秒：主动尝试创建新连接
            auto new_conn = create_connection();
            if (new_conn) {
                return new_conn;       // 新建成功，直接返回（不经过池）
            }
            // 新建失败则继续等待
        }
    }

    // ② 从池中弹出连接
    auto conn = std::move(pool_.front());
    pool_.pop();
    lock.unlock();

    // ③ 使用前健康检查
    try {
        if (conn->isClosed()) {
            conn = create_connection();     // 无效则重建
        }
    } catch (const std::exception& e) {
        APP_LOG_WARN("Acquire: connection check failed, creating new");
        conn = create_connection();
    }

    // ④ 重试机制：最多 3 次重建尝试
    int retries = 3;
    while (!conn && retries-- > 0) {
        APP_LOG_WARN("Acquire: retrying... ({} retries left)", retries);
        conn = create_connection();
    }
    if (!conn) {
        throw std::runtime_error("Failed to acquire connection after multiple retries");
    }

    return conn;
}
```

**获取策略流程图**：

```
acquire() 被调用
    │
    ├── 加锁，检查 pool_
    │
    ├── pool_ 非空？
    │   ├── 是 → pop() 取出连接 → 解锁 → 执行健康检查
    │   │                                    ├── 有效 → 返回
    │   │                                    └── 无效 → create_connection()
    │   │                                                ├── 成功 → 返回
    │   │                                                └── 失败 → 重试（最多 3 次）
    │   │
    │   └── 否 → cond_.wait_for(10s)
    │            ├── 被 notify_one() 唤醒 → 回到 pool_ 检查
    │            └── 超时 → create_connection()
    │                      ├── 成功 → 直接返回（不经过池）
    │                      └── 失败 → 回到 wait_for 等待
    │
    └── 3 次重试后仍失败 → throw runtime_error
```

---

### 3.3 连接归还策略

```cpp
// src/database/MySQLClient.cpp:101-123
void MySQLClient::release(std::unique_ptr<sql::Connection> conn) {
    if (!conn) return;

    // ① 健康检查
    bool valid = false;
    try {
        valid = !conn->isClosed();
    } catch (...) {
        valid = false;
    }

    if (valid) {
        // ② 有效：入队 + 唤醒一个等待线程
        std::lock_guard<std::mutex> lock(mutex_);
        pool_.push(std::move(conn));
        cond_.notify_one();
    } else {
        // ③ 无效：丢弃 + 补充新连接
        APP_LOG_WARN("Release: discarding stale MySQL connection");
        auto new_conn = create_connection();
        if (new_conn) {
            std::lock_guard<std::mutex> lock(mutex_);
            pool_.push(std::move(new_conn));
            cond_.notify_one();
        }
    }
}
```

**归还策略保证**：无论连接有效与否，归还操作后都会调用 `cond_.notify_one()`，确保等待中的 `acquire()` 能被唤醒。

---

### 3.4 健康检查机制

项目在两处执行连接健康检查：

| 检查点 | 位置 | 触发时机 | 处理方式 |
|--------|------|----------|----------|
| **获取时检查** | `acquire()` | 从池中取出连接后 | `isClosed()` → 重建 |
| **归还时检查** | `release()` | 归还连接前 | `isClosed()` → 丢弃 + 补建 |

`isClosed()` 底层原理：
- MySQL Connector/C++ 通过 `mysql_ping()` 检测连接是否存活
- 如果连接已断开（如 MySQL 重启、网络中断、超时断开），`isClosed()` 返回 `true`

MySQL 默认的 `wait_timeout` 为 8 小时，连接池中的空闲连接可能因超时被 MySQL 服务端断开。健康检查机制确保这种情况被及时处理。

---

### 3.5 RAII 作用域管理

`ScopedConnection` 类提供 RAII 风格的连接管理，确保连接在离开作用域时自动归还：

```cpp
// src/database/MySQLClient.h:85-121
class ScopedConnection {
public:
    explicit ScopedConnection(std::unique_ptr<sql::Connection> conn)
        : conn_(std::move(conn)) {}

    ~ScopedConnection() {
        if (conn_) {
            MySQLClient::instance().release(std::move(conn_));  // 自动归还
        }
    }

    sql::Connection* operator->() const { return conn_.get(); }  // 指针代理
    sql::Connection& operator*() const { return *conn_; }
    sql::Connection* get() const { return conn_.get(); }

    bool valid() const {                                   // 有效性检查
        try { return conn_ != nullptr && !conn_->isClosed(); }
        catch (...) { return false; }
    }

    // 禁止拷贝，允许移动
    ScopedConnection(const ScopedConnection&) = delete;
    ScopedConnection& operator=(const ScopedConnection&) = delete;
    ScopedConnection(ScopedConnection&&) noexcept = default;
    ScopedConnection& operator=(ScopedConnection&&) noexcept = default;

private:
    std::unique_ptr<sql::Connection> conn_;
};

// 便捷工厂方法
inline ScopedConnection MySQLClient::acquireScoped() {
    return ScopedConnection(instance().acquire());
}
```

**两种使用方式对比**：

```cpp
// 方式一：手动管理（需要显式 release）
auto conn = MySQLClient::instance().acquire();
// ... 使用连接 ...
MySQLClient::instance().release(std::move(conn));

// 方式二：RAII 自动管理（推荐）
auto scoped_conn = MySQLClient::acquireScoped();
// ... 使用连接 ...
// 离开作用域时自动归还，不会遗漏
```

---

### 3.6 Redis 单连接模型

```cpp
// src/database/RedisClient.h:118-124
redisContext* context_;    // 单一 Redis 连接
std::string host_;
int port_;
std::string password_;
int db_;
std::mutex mutex_;         // 全局互斥锁
```

每个 Redis 操作都通过 `std::lock_guard<std::mutex>` 保护：

```cpp
// 典型操作模式
bool RedisClient::set(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto* reply = (redisReply*)redisCommand(context_, "SET %s %s",
                                             key.c_str(), value.c_str());
    bool ok = reply && reply->type == REDIS_REPLY_STATUS &&
              std::string(reply->str) == "OK";
    freeReplyObject(reply);
    return ok;
}
```

**单连接模型的优缺点**：

| 优点 | 缺点 |
|------|------|
| 实现简单 | 所有 Redis 操作串行化，可能成为瓶颈 |
| 无连接池复杂度 | 单点故障：连接断开后所有操作失败 |
| 内存占用低 | 高并发下吞吐量受限 |

**改进方向**：可改为连接池模型，类似于 MySQL 的实现，或使用 hiredis 的异步 API。

---

### 3.7 事件循环线程并发模型

```cpp
// src/main.cpp:218
app().setThreadNum(4);
```

**One Loop Per Thread 架构**：

```
                  ┌──────────────────────┐
                  │     主线程 (main)     │
                  │  初始化 + app().run() │
                  └──────────┬───────────┘
                             │ 创建 4 个 I/O 线程
          ┌──────────────────┼──────────────────┐
          │                  │                  │
   ┌──────▼──────┐   ┌──────▼──────┐   ┌──────▼──────┐   ┌──────▼──────┐
   │ EventLoop 0 │   │ EventLoop 1 │   │ EventLoop 2 │   │ EventLoop 3 │
   │             │   │             │   │             │   │             │
   │ epoll_wait  │   │ epoll_wait  │   │ epoll_wait  │   │ epoll_wait  │
   │     ↓       │   │     ↓       │   │     ↓       │   │     ↓       │
   │ 事件分发    │   │ 事件分发    │   │ 事件分发    │   │ 事件分发    │
   │     ↓       │   │     ↓       │   │     ↓       │   │     ↓       │
   │ Controller  │   │ Controller  │   │ Controller  │   │ Controller  │
   │ 回调执行    │   │ 回调执行    │   │ 回调执行    │   │ 回调执行    │
   │     ↓       │   │     ↓       │   │     ↓       │   │     ↓       │
   │ Database    │   │ Database    │   │ Database    │   │ Database    │
   └─────────────┘   └─────────────┘   └─────────────┘   └─────────────┘
          │                  │                  │                  │
          └──────────────────┴──────────────────┴──────────────────┘
                             │
                   共享资源（需要同步）：
                   ├── MySQL 连接池（mutex + condition_variable）
                   └── Redis 连接（mutex）
```

**并发安全保证**：
- **HTTP 请求处理**：由 Drogon 框架保证，每个连接的事件在同一个 EventLoop 线程上处理
- **MySQL 操作**：连接池自身的 `std::mutex` 保证线程安全
- **Redis 操作**：每次调用都持有 `std::lock_guard<std::mutex>`
- **WebSocket 连接表**：`connections_mutex_` 保护 `connections_` 映射

---

### 3.8 连接池状态监控

通过日志输出监控连接池健康状态：

```cpp
// 初始化时
APP_LOG_INFO("MySQL connection pool initialized with {}/{} connections",
             pool_.size(), pool_size_);

// 连接复用成功
// （隐式：acquire 从池中取出，release 归还入队，无额外日志）

// 连接被丢弃
APP_LOG_WARN("Release: discarding stale MySQL connection");

// 重建连接
APP_LOG_WARN("Acquire: connection check failed ({}), creating new", e.what());
APP_LOG_WARN("Acquire: failed to create new connection, retrying... ({} retries left)", retries);
```

**监控指标建议**：

| 指标 | 获取方式 | 正常范围 |
|------|----------|----------|
| 池大小 | `pool_.size()` | 接近 `pool_size_` (10) |
| 获取超时次数 | 计数器累加 | 0 |
| 连接重建次数 | 日志计数 | 极少 |
| 等待队列长度 | 推断（调用方 block 数） | 0~3 |

---

## 4. 通信机制

### 4.1 HTTP 请求/响应处理流程

**完整请求生命周期**：

```
┌─ 客户端 ─┐                                  ┌─ 服务端 ─┐
│           │── TCP 三次握手 ──────────────────→│           │
│           │←── SYN-ACK ──────────────────────│           │
│           │── ACK ──────────────────────────→│           │
│           │                                  │           │
│           │  (HTTPS: TLS 握手)               │           │
│           │── ClientHello ───────────────────→│           │
│           │←── ServerHello + 证书 ───────────│           │
│           │── 密钥交换 ──────────────────────→│           │
│           │←── Finished ─────────────────────│           │
│           │                                  │           │
│           │── HTTP Request ──────────────────→│ epoll 检测到 EPOLLIN
│           │  POST /api/chat/send HTTP/1.1     │   → 读取并解析 HTTP
│           │  Host: localhost:8888             │   → 路由匹配 → ChatController
│           │  Authorization: Bearer <token>    │   → AuthMiddleware::invoke()
│           │  Content-Type: application/json   │       ├── 公共路径检查
│           │  {"content": "你好"}              │       ├── CSRF Origin 校验
│           │                                  │       ├── JWT 验证
│           │                                  │       ├── 封禁检查
│           │                                  │       ├── 会话检查
│           │                                  │       └── 注入用户信息
│           │                                  │   → ChatController::sendMessage()
│           │                                  │       ├── 参数校验
│           │                                  │       ├── MySQL: 保存用户消息
│           │                                  │       ├── 异步 HttpClient → AI API
│           │                                  │       │   (不阻塞事件循环)
│           │                                  │       └── 回调: 保存 AI 回复
│           │                                  │
│           │←── HTTP Response ────────────────│ epoll 检测到 EPOLLOUT
│           │  HTTP/1.1 200 OK                 │   → 写入响应
│           │  Content-Type: application/json  │
│           │  {"code":200,"data":{"reply":"你好！...}}
│           │                                  │
│           │  (Keep-Alive: 等待下一个请求)      │
│           │── FIN ──────────────────────────→│ 或超时关闭
└───────────┘                                  └───────────┘
```

**路由注册**：

```cpp
// src/controllers/ChatController.h:18-22
METHOD_LIST_BEGIN
ADD_METHOD_TO(ChatController::getHistory,   "/api/chat/history", Get);
ADD_METHOD_TO(ChatController::sendMessage,  "/api/chat/send",    Post);
ADD_METHOD_TO(ChatController::clearHistory, "/api/chat/clear",   Delete);
METHOD_LIST_END
```

---

### 4.2 中间件认证链路

`AuthMiddleware` 作为 Drogon 框架的中间件，在请求到达 Controller 之前执行：

```cpp
// src/middleware/AuthMiddleware.cpp:9-169
void AuthMiddleware::invoke(const HttpRequestPtr& req,
                             MiddlewareNextCallback&& nextCb,
                             MiddlewareCallback&& mcb) {
    auto path = req->path();

    // 第 1 关：公共路径白名单
    static const std::vector<std::string> public_paths = {
        "/api/auth/send-code", "/api/auth/register", "/api/auth/login",
        "/api/auth/reset-password", "/login.html", "/register.html",
        "/css/", "/js/", "/ws/", "/favicon.ico", /* ... */
    };
    for (const auto& pp : public_paths) {
        if (path.find(pp) == 0) { nextCb(std::move(mcb)); return; }
    }

    // 第 2 关：CSRF 来源校验（POST/PUT/DELETE）
    if (method == drogon::Post || method == drogon::Put || method == drogon::Delete) {
        auto origin = req->getHeader("Origin");
        auto host = req->getHeader("Host");
        // 提取 Origin 的 hostname，与 Host 头比较
        if (origin_hostname != host_hostname) {
            // 403: 无效的请求来源
            mcb(resp); return;
        }
    }

    // 第 3 关：Token 提取
    auto auth_header = req->getHeader("Authorization");
    if (auth_header.empty()) {
        // 401: 未登录
        mcb(resp); return;
    }

    // 第 4 关：JWT 验证 + 封禁检查 + 会话检查
    try {
        auto data = JWTUtils::verify_token(token);     // 签名 + 过期验证

        // 检查封禁状态
        if (RedisClient::instance().sismember("banned_users", user_id)) {
            // 403: 账号已被封禁 + 封禁详情
            mcb(resp); return;
        }

        // 检查会话有效性
        if (!RedisClient::instance().exists("session:" + token)) {
            // 401: 会话已过期
            mcb(resp); return;
        }

        // 第 5 关：角色鉴权
        if (path.find("/api/admin/") == 0 && role != "admin") {
            // 403: 仅管理员可访问
            mcb(resp); return;
        }

        // 全部通过：注入用户信息到请求头
        req->addHeader("X-Token", token);
        req->addHeader("X-User-Id", user_id);
        req->addHeader("X-Username", username);
        req->addHeader("X-User-Role", role);

        nextCb(std::move(mcb));     // 放行到 Controller

    } catch (...) {
        // 401: Token无效
        mcb(resp);
    }
}
```

**认证链路示意图**：

```
请求到达
    │
    ├── [关 1] 公共路径？──→ 是 → 直接放行
    │         否
    ├── [关 2] CSRF 校验 ──→ 失败 → 403 Forbidden
    │         通过
    ├── [关 3] Token 提取 ──→ 缺失 → 401 Unauthorized
    │         存在
    ├── [关 4] JWT 验证 ────→ 失败 → 401 Unauthorized
    │         通过
    ├── [关 5] 封禁检查 ────→ 被封禁 → 403 Forbidden（含封禁详情）
    │         未封禁
    ├── [关 6] 会话检查 ────→ 过期 → 401 Unauthorized
    │         有效
    ├── [关 7] 角色鉴权 ────→ 无权限 → 403 Forbidden
    │         通过
    └── 注入用户信息 → 放行到 Controller
```

---

### 4.3 数据封装格式

#### HTTP 协议

- **协议版本**：HTTP/1.1
- **传输编码**：`Content-Type: application/json`
- **认证方式**：`Authorization: Bearer <JWT Token>`

#### 响应 JSON 统一格式

**成功响应**：

```json
{
  "code": 200,
  "message": "操作成功",
  "data": {
    "messages": [
      {
        "id": 123456789,
        "role": "user",
        "content": "你好",
        "token_count": 2,
        "created_at": "2026-06-04 17:00:00"
      }
    ],
    "total": 42,
    "page": 1,
    "page_size": 200
  }
}
```

**错误响应**：

```json
{
  "code": 401,
  "message": "未登录，请先登录"
}
```

**封禁响应**：

```json
{
  "code": 403,
  "message": "账号已被封禁",
  "data": {
    "banned": true,
    "ban_reason": "违反平台规则",
    "remaining_seconds": 3600
  }
}
```

**状态码规范**：

| HTTP 状态码 | `code` 字段 | 含义 |
|------------|------------|------|
| 200 | 200 | 操作成功 |
| 400 | 400 | 请求参数无效 |
| 401 | 401 | 未登录 / Token 无效 / 会话过期 |
| 403 | 403 | 被封禁 / CSRF 校验失败 / 权限不足 |
| 404 | 404 | 资源不存在 |
| 409 | 409 | 资源冲突（邮箱/用户名已注册） |
| 429 | 429 | 操作过于频繁 |
| 490 | 490 | 账号处于注销冷静期 |
| 500 | 500 | 服务器内部错误 |
| 502 | 502 | AI 服务请求失败 |

#### WebSocket 消息格式

```json
{
  "type": "ban",
  "ban_reason": "违规操作",
  "duration_hours": 24,
  "banned_at": "1749052800"
}
```

---

### 4.4 AI API 异步通信流程

这是项目中最重要的通信模式——外部 API 的异步调用与回调处理。

```cpp
// src/controllers/ChatController.cpp:94-212
void ChatController::sendMessage(const HttpRequestPtr& req,
                                  std::function<void(const HttpResponsePtr&)>&& callback) {
    // ① 参数校验
    auto json = req->getJsonObject();
    std::string content = (*json)["content"].asString();
    if (content.empty()) { /* 400 */ return; }
    if (content.length() > 2000) { /* 400 */ return; }

    // ② 将 callback 包装为 shared_ptr，确保异步回调中安全使用
    auto cb_ptr = std::make_shared<std::function<void(const HttpResponsePtr&)>>(
        std::move(callback));

    try {
        auto conn = MySQLClient::instance().acquire();

        // ③ 保存用户消息到 MySQL（同步操作）
        auto stmt = conn->prepareStatement(
            "INSERT INTO chat_messages (user_id, role, content) VALUES (?, 'user', ?)");
        stmt->setUInt64(1, user_id);
        stmt->setString(2, content);
        stmt->executeUpdate();

        // ④ 将数据库连接包装为 shared_ptr，传递给异步回调
        auto conn_ptr = std::make_shared<std::unique_ptr<sql::Connection>>(
            std::move(conn));

        // ⑤ 构建 AI API 请求
        auto client = HttpClient::newHttpClient(ai_api_url_);  // 异步 HTTP 客户端
        auto ai_req = HttpRequest::newHttpRequest();
        ai_req->setPath("/v1/chat/completions");
        ai_req->setMethod(HttpMethod::Post);
        ai_req->setContentTypeCode(ContentType::CT_APPLICATION_JSON);
        ai_req->addHeader("Authorization", "Bearer " + ai_api_key_);

        Json::Value body;
        body["model"] = ai_model_;
        body["messages"][0]["role"] = "user";
        body["messages"][0]["content"] = content;
        ai_req->setBody(body.toStyledString());

        // ⑥ 发送异步请求，设置 30 秒超时
        client->sendRequest(ai_req, [cb_ptr, conn_ptr, user_id]
                (ReqResult result, const HttpResponsePtr& response) {
            // ===== 异步回调：在 AI API 返回后执行 =====

            if (result != ReqResult::Ok || !response) {
                // ⑦ 请求失败：归还连接 + 返回错误
                MySQLClient::instance().release(std::move(*conn_ptr));
                (*cb_ptr)(502_error_response);
                return;
            }

            // ⑧ 解析 AI 回复
            auto resp_json = response->getJsonObject();
            std::string ai_reply = (*resp_json)["choices"][0]["message"]["content"];

            try {
                auto& conn = *conn_ptr;
                // ⑨ 保存 AI 回复到数据库
                conn->prepareStatement(
                    "INSERT INTO chat_messages (user_id, role, content) "
                    "VALUES (?, 'assistant', ?)") -> executeUpdate();

                // ⑩ 更新用户聊天计数
                conn->prepareStatement(
                    "UPDATE users SET total_chats = total_chats + 1 "
                    "WHERE id = ?") -> executeUpdate();

                // ⑪ 归还连接
                MySQLClient::instance().release(std::move(*conn_ptr));
            } catch (...) {
                MySQLClient::instance().release(std::move(*conn_ptr));
            }

            // ⑫ 返回 AI 回复给客户端
            (*cb_ptr)(200_success_response);
        }, 30.0);  // ← 超时时间：30 秒

    } catch (const std::exception& e) {
        // 同步阶段异常：直接返回 500
        (*cb_ptr)(500_error_response);
    }
}
```

**异步通信时序图**：

```
时间轴 →

EventLoop 线程:
  │
  ├─ 收到 POST /api/chat/send
  ├─ 参数校验
  ├─ MySQL: INSERT user message ─────────┐
  ├─ HttpClient::sendRequest(ai_req) ──┐ │
  │  (不阻塞，立即返回)                  │ │
  ├─ 继续处理下一个事件...               │ │
  │                                     │ │
  │  [等待 AI API 响应...]              │ │
  │                                     │ │
  ├─ AI API 返回 ── 触发回调 ──────────┘ │
  │  ├─ 解析 JSON                       │
  │  ├─ MySQL: INSERT assistant msg ────┘
  │  ├─ MySQL: UPDATE total_chats
  │  ├─ release(conn)
  │  └─ callback(client_resp) ───→ 客户端收到回复
  │
  └─ 继续处理下一个事件...
```

**异步设计的关键点**：

1. **`shared_ptr` 生命周期管理**：`cb_ptr` 和 `conn_ptr` 被 lambda 捕获，确保在异步回调执行时这些资源仍然有效
2. **不阻塞事件循环**：`sendRequest` 立即返回，AI API 的等待不影响其他连接的处理
3. **超时控制**：`30.0` 秒超时参数，超时后自动触发失败回调
4. **连接始终归还**：无论成功、失败还是异常，`conn_ptr` 最终都会调用 `release()`

---

### 4.5 WebSocket 实时推送

#### 连接管理架构

```cpp
// src/controllers/NotificationController.h:43-44
static inline std::mutex connections_mutex_;
static inline std::unordered_map<uint64_t, std::vector<WebSocketConnectionPtr>> connections_;
```

**数据结构**：`user_id → [WebSocket 连接 1, 连接 2, ...]`

一个用户可以同时从多个设备/标签页建立 WebSocket 连接，推送时遍历所有连接发送。

#### 消息推送流程

```
管理员调用 banUser(user_id, reason, duration)
    │
    ├── ① MySQL: UPDATE users SET status='banned' WHERE id=?
    │
    ├── ② Redis: SADD banned_users {user_id}
    ├──    Redis: SETEX banned_info:{user_id} {ban_json} (duration * 3600)
    │
    ├── ③ NotificationController::sendBanNotification(user_id, reason, duration)
    │       ├── 构造 ban JSON 消息
    │       ├── lock(connections_mutex_)
    │       ├── 查找 connections_[user_id]
    │       ├── for each conn: conn->send(payload)    ← 推送
    │       └── unlock(connections_mutex_)
    │
    └── ④ 返回成功响应给管理员
```

**WebSocket 提供的功能**：

| 事件 | 处理函数 | 用途 |
|------|----------|------|
| 新连接建立 | `handleNewConnection()` | JWT 认证 + 注册到映射表 |
| 收到客户端消息 | `handleNewMessage()` | 当前为空实现（预留） |
| 连接关闭 | `handleConnectionClosed()` | 从映射表移除 |

---

### 4.6 异常处理与重试策略

#### IP 频率限制（防刷）

```cpp
// src/controllers/AuthController.cpp:19-22
std::string rate_key = "rate_limit:" + std::string(__func__) + ":" + req->getPeerAddr().toIp();
auto attempts = RedisClient::instance().incr(rate_key);
if (attempts == 1) RedisClient::instance().expire(rate_key, 300);  // 首次设置 5 分钟过期
if (attempts > 10) { /* 429 频率限制 */ }
```

**策略**：每个接口 + 每个 IP 独立计数，5 分钟内最多 10 次。使用 Redis `INCR` 原子操作保证并发安全。

#### MySQL 连接重试

```cpp
// src/database/MySQLClient.cpp:85-92
int retries = 3;
while (!conn && retries-- > 0) {
    APP_LOG_WARN("Acquire: retrying... ({} retries left)", retries);
    conn = create_connection();
}
if (!conn) {
    throw std::runtime_error("Failed to acquire connection after multiple retries");
}
```

**策略**：最多 3 次重建尝试，全部失败则抛出异常，由上层 Controller 的 try-catch 捕获后返回 500 错误。

#### AI API 调用失败处理

```cpp
// src/controllers/ChatController.cpp:158-164
if (result != ReqResult::Ok || !response) {
    MySQLClient::instance().release(std::move(*conn_ptr));
    auto resp = HttpResponse::newHttpResponse();
    resp->setBody(generateError(502, "AI服务请求失败，请稍后重试。"));
    (*cb_ptr)(resp);
    return;
}
```

**策略**：不重试，直接返回 502 错误给客户端，由用户决定是否重新发送。这样可以避免：
- 重复扣费（API 调用通常按次计费）
- 重复保存消息

#### Controller 层统一异常处理

所有 Controller 方法都遵循以下模式：

```cpp
try {
    auto conn = MySQLClient::instance().acquire();
    // ... 业务逻辑 ...
    MySQLClient::instance().release(std::move(conn));
    // 返回成功响应
} catch (const std::exception& e) {
    APP_LOG_ERROR("... error: {}", e.what());
    // 返回 500 错误
    auto resp = HttpResponse::newHttpResponse();
    resp->setBody(generateError(500, "服务器错误"));
    callback(resp);
}
```

#### 验证码防重复使用

```cpp
// src/controllers/AuthController.cpp:164-165
RedisClient::instance().del(redis_key);  // 使用后立即删除
```

**策略**：验证码验证通过后立即从 Redis 删除，确保一次性使用。

---

## 附录

### A. 配置文件完整结构

```json
{
  "server": {
    "port": 8888,
    "enable_https": false,
    "https_cert": "config/server.crt",
    "https_key": "config/server.key",
    "thread_num": 4,
    "document_root": "views",
    "jwt_secret": "",
    "home_page": "login.html"
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
  "ai": {
    "api_url": "https://api.deepseek.com",
    "api_key": "",
    "model": "deepseek-v4-flash"
  },
  "log": {
    "level": "info",
    "file": "logs/server.log",
    "max_size": 10485760,
    "max_files": 7
  }
}
```

### B. 依赖关系总览

```
ai_chat_server
├── Drogon             HTTP/HTTPS/WebSocket 框架（基于 trantor）
│   ├── trantor        底层网络库（epoll + 非阻塞 I/O）
│   ├── OpenSSL        TLS 加密 / HMAC 签名
│   └── jsoncpp        内置 JSON 解析
├── spdlog             高性能日志库
├── nlohmann_json      JSON 解析（配置和 API 响应）
├── hiredis            Redis 客户端
├── mysqlcppconn       MySQL Connector/C++
├── libcurl            HTTP 客户端（SMTP 邮件 + AI API 调用备选）
└── OpenSSL            密码哈希 / HMAC-SHA256
```

### C. Redis 数据键空间

| Key Pattern | 类型 | 用途 | TTL |
|-------------|------|------|-----|
| `session:{token}` | String (JSON) | 用户会话信息 | 7 天 |
| `verify_code:{email}:{type}` | String | 邮箱验证码 | 5~10 分钟 |
| `online_users` | Set | 在线用户 ID 集合 | 无 |
| `banned_users` | Set | 封禁用户 ID 集合 | 无 |
| `banned_info:{user_id}` | String (JSON) | 封禁详细信息 | 封禁时长 |
| `user_sessions:{user_id}` | Set | 用户所有活跃会话 Token | 无 |
| `rate_limit:{func}:{ip}` | String (计数器) | 接口频率限制 | 5 分钟 |
