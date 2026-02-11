# 贡献指南

感谢你对 tempmail-sdk 项目的关注！我们欢迎所有形式的贡献，包括 Bug 报告、功能建议、文档改进和代码提交。

## 目录

- [行为准则](#行为准则)
- [如何贡献](#如何贡献)
- [开发环境](#开发环境)
- [项目结构](#项目结构)
- [添加新渠道](#添加新渠道)
- [代码风格](#代码风格)
- [提交规范](#提交规范)
- [Pull Request 流程](#pull-request-流程)

## 行为准则

- 尊重每一位参与者，保持友善和专业
- 对事不对人，聚焦于技术讨论
- 包容不同的观点和经验

## 如何贡献

### 报告 Bug

1. 先搜索 [已有 Issues](https://github.com/XxxXTeam/tempmail-sdk/issues) 确认是否已报告
2. 使用 **Bug Report** 模板创建新 Issue
3. 提供尽可能详细的复现步骤、SDK 版本和运行环境

### 建议功能

1. 使用 **Feature Request** 模板创建新 Issue
2. 描述你的使用场景和期望的行为
3. 如果可能，附上 API 设计示例

### 提交代码

1. Fork 本仓库
2. 创建功能分支：`git checkout -b feat/your-feature`
3. 开发并测试
4. 提交 Pull Request

## 开发环境

本项目包含 5 个 SDK，各有不同的工具链要求：

| SDK | 工具链 | 最低版本 |
|-----|--------|---------|
| Go | Go | 1.21+ |
| npm | Node.js + TypeScript | Node 18+, TS 5.0+ |
| Rust | Rust (cargo) | 1.75+ |
| Python | Python + pip | 3.8+ |
| C | CMake + GCC/Clang + libcurl | CMake 3.14+ |

### 快速验证

```bash
# Go
cd sdk/go && go build ./...

# npm
cd sdk/npm && npm install && npx tsc --noEmit

# Rust
cd sdk/rust && cargo check

# Python
cd sdk/python && python -c "import tempmail_sdk"

# C
cd sdk/c && cmake -B build -S . && cmake --build build
```

## 项目结构

```
tempmail-sdk/
├── sdk/
│   ├── go/                     # Go SDK
│   │   ├── config.go           # 全局配置
│   │   ├── client.go           # Client 实现
│   │   ├── types.go            # 类型定义
│   │   ├── normalize.go        # 邮件标准化
│   │   └── provider_*.go       # 各渠道实现
│   ├── npm/                    # npm SDK (TypeScript)
│   │   └── src/
│   │       ├── config.ts       # 全局配置
│   │       ├── index.ts        # 入口
│   │       ├── types.ts        # 类型定义
│   │       └── providers/      # 各渠道实现
│   ├── rust/                   # Rust SDK
│   │   └── src/
│   │       ├── config.rs       # 全局配置
│   │       ├── lib.rs          # 库入口
│   │       ├── client.rs       # Client 实现
│   │       └── providers/      # 各渠道实现
│   ├── python/                 # Python SDK
│   │   └── tempmail_sdk/
│   │       ├── config.py       # 全局配置
│   │       ├── http.py         # 共享 HTTP 客户端
│   │       ├── __init__.py     # 入口
│   │       └── providers/      # 各渠道实现
│   └── c/                      # C SDK
│       ├── include/            # 公共头文件
│       └── src/
│           ├── http.c          # HTTP 请求 + 全局配置
│           └── providers/      # 各渠道实现
├── .github/
│   ├── ISSUE_TEMPLATE/         # Issue 模板
│   └── workflows/              # CI/CD 工作流
├── CONTRIBUTING.md             # 本文件
└── README.md
```

## 添加新渠道

添加新的临时邮箱服务商需要在**全部 5 个 SDK** 中实现。每个渠道需要实现两个核心函数：

1. **`generateEmail()`** — 创建临时邮箱，返回邮箱地址和认证信息
2. **`getEmails()`** — 获取邮件列表，返回标准化邮件数组

### 步骤

1. **实现 Provider**

   在各 SDK 的 `providers/` 目录下新建文件（如 `newprovider.go`、`newprovider.ts` 等），实现上述两个函数。

2. **使用标准化函数**

   所有返回数据必须经过 `normalizeEmail()` 处理，确保字段一致：
   - `id` — 邮件唯一标识（无则生成 UUID/hash）
   - `from` — 发件人
   - `to` — 收件人（若缺失则用当前邮箱地址填充）
   - `subject` — 主题
   - `text` / `html` — 正文
   - `date` — ISO 8601 格式日期
   - `isRead` — 是否已读（默认 `false`）
   - `attachments` — 附件数组

3. **注册渠道**

   在各 SDK 的 Channel 类型/枚举中注册新渠道标识，并在入口文件的 `switch` / `match` / `if-else` 分发逻辑中添加分支。

4. **使用全局 HTTP 客户端**

   所有 HTTP 请求必须使用 SDK 的共享 HTTP 客户端（`HTTPClient()` / `http_client()` / `tm_http`），以支持代理和 TLS 配置。

5. **更新文档**

   - 在各 SDK 的 README 支持渠道表格中添加新渠道
   - 在根目录 `README.md` 中同步更新

### 示例：Go SDK

```go
// provider_newservice.go
package tempemail

import (
    "encoding/json"
    "fmt"
)

func generateEmailNewService() (*EmailInfo, error) {
    resp, err := HTTPClient().Get("https://api.newservice.com/email")
    if err != nil {
        return nil, fmt.Errorf("newservice request failed: %w", err)
    }
    defer resp.Body.Close()
    // ... 解析响应并返回 EmailInfo
}

func getEmailsNewService(email string) ([]Email, error) {
    resp, err := HTTPClient().Get(fmt.Sprintf("https://api.newservice.com/emails?addr=%s", email))
    if err != nil {
        return nil, fmt.Errorf("newservice request failed: %w", err)
    }
    defer resp.Body.Close()
    // ... 解析响应，使用 NormalizeEmail() 标准化，返回 []Email
}
```

## 代码风格

### 通用规则

- 注释和日志使用中文
- 长段注释使用 `/* */` 格式
- 功能模块注释使用注解类型，说明功能用途
- 不使用硬编码数据

### 各语言规范

| SDK | 格式化工具 | 检查工具 |
|-----|-----------|---------|
| Go | `gofmt` | `go vet` |
| npm | 项目 tsconfig | `tsc --noEmit` |
| Rust | `rustfmt` | `cargo clippy` |
| Python | PEP 8 | `flake8` / `ruff` |
| C | 项目风格 | GCC `-Wall -Wextra` |

## 提交规范

使用 [Conventional Commits](https://www.conventionalcommits.org/) 格式：

```
<type>(<scope>): <description>

[optional body]
```

### Type

| 类型 | 说明 |
|------|------|
| `feat` | 新功能 |
| `fix` | Bug 修复 |
| `docs` | 文档变更 |
| `refactor` | 代码重构（不影响功能） |
| `perf` | 性能优化 |
| `test` | 测试相关 |
| `ci` | CI/CD 配置 |
| `chore` | 构建过程或辅助工具变更 |

### Scope

`go` / `npm` / `rust` / `python` / `c` / `all`

### 示例

```
feat(go): 添加 newservice 渠道支持
fix(npm): 修复 temp-mail-io 超时处理
docs(all): 更新代理配置文档
perf(rust): 缓存 HTTP 客户端复用连接池
```

## Pull Request 流程

1. **确保 CI 通过** — PR 会自动触发所有 SDK 的构建检查
2. **一个 PR 只做一件事** — 不要混合功能开发和 Bug 修复
3. **保持向后兼容** — 公共 API 变更需要讨论
4. **更新文档** — 新功能和 API 变更必须同步更新文档
5. **跨 SDK 一致性** — 新功能尽量在全部 5 个 SDK 中同步实现

### Review 标准

- 代码清晰可读
- 使用全局 HTTP 客户端，支持代理/TLS 配置
- 邮件数据经过标准化处理
- 错误处理完整，有日志记录
- 无硬编码值

---

如有任何问题，欢迎在 [Discussions](https://github.com/XxxXTeam/tempmail-sdk/discussions) 中讨论！
