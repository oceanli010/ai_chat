# AI 聊天室

一个基于 C++ (Drogon) + MySQL + Redis 的高并发 AI 聊天服务器。支持用户注册登录、AI 对话、个人中心、管理员后台等功能。

## 技术栈w

| 组件   | 技术             | 用途                   |
| ---- | -------------- | -------------------- |
| 后端框架 | Drogon         | 高性能异步 HTTP/HTTPS 服务器 |
| 数据库  | MySQL          | 用户信息、聊天记录持久化存储       |
| 缓存   | Redis          | 验证码、会话 Token、在线状态    |
| 日志   | spdlog         | 文件+控制台双重日志输出         |
| 认证   | JWT            | 用户身份认证 Token         |
| 前端   | 原生 HTML/CSS/JS | 由 Drogon 直接提供服务      |

## 项目结构

```
ai_chat/
├── CMakeLists.txt              # 项目构建配置
├── config/
│   ├── config.json             # 服务器配置文件
│   ├── server.crt              # HTTPS 证书（需生成）
│   └── server.key              # HTTPS 私钥（需生成）
├── sql/
│   └── init.sql                # 数据库初始化脚本
├── scripts/
│   ├── setup_db.sh             # 数据库安装脚本
│   └── gen_cert.sh             # 自签名证书生成脚本
├── src/
│   ├── main.cpp                # 程序入口
│   ├── controllers/            # HTTP 请求控制器
│   │   ├── AuthController.h    # 注册/登录/重置密码
│   │   ├── ChatController.h    # AI 聊天
│   │   ├── UserController.h    # 个人中心
│   │   └── AdminController.h   # 管理员功能
│   ├── database/               # 数据库访问层
│   │   ├── MySQLClient.h       # MySQL 连接池
│   │   └── RedisClient.h       # Redis 客户端
│   ├── middleware/
│   │   └── AuthMiddleware.h    # JWT 认证中间件
│   ├── models/
│   │   ├── User.h              # 用户模型
│   │   └── ChatMessage.h       # 聊天消息模型
│   └── utils/
│       ├── Logger.h            # spdlog 日志封装
│       ├── JWTUtils.h          # JWT 工具
│       ├── PasswordHasher.h    # 加盐哈希
│       ├── IDGenerator.h       # 雪花算法 ID 生成器
│       ├── EmailSender.h       # SMTP 邮件发送
│       └── Validator.h         # 输入验证
├── views/                      # 前端页面
│   ├── login.html              # 登录页
│   ├── register.html           # 注册页
│   ├── reset_password.html     # 重置密码
│   ├── index.html              # 主页面（聊天+个人中心）
│   ├── admin.html              # 管理员后台
│   ├── css/style.css           # 全局样式
│   └── js/app.js               # 全局工具函数
├── start.sh                    # 一键启动脚本
└── README.md
```

## 环境要求

- **编译器**: GCC 8+ 或 Clang 10+（需支持 C++17）
- **CMake**: 3.20+
- **MySQL**: 8.0+
- **Redis**: 6.0+
- **OpenSSL**: 1.1+

## 快速开始

### 第一步：安装系统依赖

```bash
# Ubuntu/Debian
sudo apt update
sudo apt install -y build-essential cmake git libssl-dev \
    libmysqlcppconn-dev mysql-server mysql-client \
    redis-server libhiredis-dev nlohmann-json3-dev

# 启动 MySQL 和 Redis
sudo systemctl start mysql
sudo systemctl start redis
```

### 第二步：初始化数据库

```bash
# 方式一：使用脚本（默认 root 用户，无密码）
bash scripts/setup_db.sh

# 方式二：手动导入
mysql -u root -p < sql/init.sql
```

### 第三步：生成 HTTPS 自签名证书

```bash
bash scripts/gen_cert.sh
```

此命令会在 `config/` 目录下生成 `server.crt` 和 `server.key`。

生产环境请替换为正规 CA 签发的证书。

### 第四步：修改配置文件

编辑 `config/config.json`，至少配置以下项：

```json
{
  "email": {
    "username": "your_email@qq.com",
    "password": "your_smtp_authorization_code",
    "from_address": "your_email@qq.com"
  },
  "ai": {
    "api_key": "your_openai_api_key"
  }
}
```

> 若不配置邮箱，验证码会打印在服务器日志中，仍可正常使用。

> 若不配置 AI API Key，AI 回复将提示未配置。

### 第五步：编译与运行

```bash
# 方式一：使用启动脚本（自动编译 + 运行）
bash start.sh

# 方式二：手动编译
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
cd ..

# 运行
./build/ai_chat_server config/config.json
```

### 第六步：访问

打开浏览器访问：

```
https://localhost
```

## 页面导航

| 页面   | 路径                     | 说明                |
| ---- | ---------------------- | ----------------- |
| 登录页  | `/login.html`          | 用户登录              |
| 注册页  | `/register.html`       | 邮箱注册              |
| 重置密码 | `/reset_password.html` | 邮箱验证重置密码          |
| 主页面  | `/index.html`          | 聊天 + 个人中心（需登录）    |
| 管理后台 | `/admin.html`          | 管理员管理（需 admin 登录） |

## 管理员账号

- **用户名**: `admin_oceanli`
- **密码**: `admim1005`

首次启动服务器时会自动创建该管理员账号。

管理员登录后自动跳转到管理后台，可查看统计、管理用户、查看日志。

## API 接口

所有 API 返回 JSON 格式：`{"code": 200, "message": "success", "data": {...}}`

### 认证接口（无需登录）

| 方法   | 路径                         | 说明      |
| ---- | -------------------------- | ------- |
| POST | `/api/auth/send-code`      | 发送邮箱验证码 |
| POST | `/api/auth/register`       | 注册账号    |
| POST | `/api/auth/login`          | 登录      |
| POST | `/api/auth/reset-password` | 重置密码    |

### 用户接口（需登录）

| 方法     | 路径                   | 说明       |
| ------ | -------------------- | -------- |
| GET    | `/api/user/profile`  | 获取个人信息   |
| PUT    | `/api/user/profile`  | 修改用户名/昵称 |
| PUT    | `/api/user/password` | 修改密码     |
| DELETE | `/api/user/account`  | 注销账号     |

### 聊天接口（需登录）

| 方法     | 路径                  | 说明     |
| ------ | ------------------- | ------ |
| GET    | `/api/chat/history` | 获取聊天记录 |
| POST   | `/api/chat/send`    | 发送消息   |
| DELETE | `/api/chat/clear`   | 清除聊天记录 |

### 管理员接口（需 admin 角色）

| 方法   | 路径                  | 说明     |
| ---- | ------------------- | ------ |
| GET  | `/api/admin/stats`  | 获取统计信息 |
| GET  | `/api/admin/users`  | 获取用户列表 |
| POST | `/api/admin/search` | 搜索用户   |
| POST | `/api/admin/ban`    | 封禁/解封  |
| GET  | `/api/admin/logs`   | 查看日志   |

## 配置说明

`config/config.json` 完整配置项：

```jsonc
{
  "server": {
    "port": 443,           // HTTPS 监听端口
    "thread_num": 4,       // 工作线程数
    "document_root": "views"  // 静态文件根目录
  },
  "database": {
    "mysql": {
      "host": "127.0.0.1",
      "port": 3306,
      "user": "root",
      "password": "",
      "database": "ai_chat",
      "pool_size": 10      // 连接池大小
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
    "level": "info",
    "file": "logs/server.log",
    "max_size": 10485760,    // 10MB 日志轮转
    "max_files": 7
  }
}
```

## 安全特性

- **密码加密**: SHA-256 + 随机 32 字节盐值
- **通信安全**: HTTPS (TLS)
- **身份认证**: JWT Token（7天过期）
- **账号防护**: 封禁状态检查
- **输入验证**: 前后端双重校验
- **SQL 防护**: 参数化查询防注入
- **自动清理**: 30天聊天记录自动清除
- **验证码安全**: 6位数字、5分钟过期、一次性使用

## 常见问题

### 1. 编译时找不到依赖

确保已安装所有系统依赖。Drogon、spdlog、jwt-cpp 等库会自动通过 `FetchContent` 下载。

### 2. MySQL 连接失败

检查 MySQL 服务是否运行，以及 `config.json` 中的数据库配置是否正确。

### 3. 验证码收不到邮件

验证码同时会输出到服务器日志文件中。配置邮箱时注意 SMTP 授权码不是邮箱登录密码。

### 4. AI 回复提示"未配置"

在 `config.json` 的 `ai.api_key` 中填入你的 OpenAI API Key。
