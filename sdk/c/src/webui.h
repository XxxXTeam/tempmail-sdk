/**
 * WebUI 嵌入式 HTTP 服务器
 *
 * 通过极简 POSIX socket 实现一个内嵌的调试面板，展示 SDK 配置、
 * 渠道列表，并通过 SSE 实时推送日志。默认不启动，需显式调用
 * tm_webui_start() 或设置环境变量 TEMPMAIL_WEBUI=true。
 */

#ifndef TEMPMAIL_WEBUI_H
#define TEMPMAIL_WEBUI_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 启动 WebUI HTTP 服务器（在独立 pthread 中运行，不阻塞调用线程）。
 *
 * 绑定地址取值优先级：
 *   1. host 参数（非空时使用）
 *   2. 环境变量 TEMPMAIL_WEBUI_HOST
 *   3. 默认 127.0.0.1（仅本地回环）
 *
 * 端口取值优先级：
 *   1. port 参数（> 0 时使用）
 *   2. 环境变量 TEMPMAIL_WEBUI_PORT
 *   3. 随机端口（bind 到 0，由内核分配）
 *
 * 启动后会接管全局日志处理器（链式包装原处理器），把日志推送到
 * 所有 SSE 连接。重复调用无副作用（已启动时直接返回当前端口）。
 *
 * @param host 指定绑定地址（如 "127.0.0.1" / "0.0.0.0"），传 NULL 或空串
 *             表示走环境变量或默认 127.0.0.1
 * @param port 指定监听端口，传 0 表示走环境变量或随机端口
 * @return 实际监听的端口号；启动失败返回 0
 */
int tm_webui_start(const char *host, int port);

/**
 * 停止 WebUI HTTP 服务器，恢复原日志处理器并释放资源。
 * 未启动时调用无副作用。
 */
void tm_webui_stop(void);

/**
 * 根据环境变量决定是否自动启动 WebUI。
 * 当 TEMPMAIL_WEBUI 为 true/1/yes 时启动，否则不做任何事。
 * 供 tm_init() 调用。
 *
 * @return 已启动时返回监听端口，否则返回 0
 */
int tm_webui_maybe_start_from_env(void);

#ifdef __cplusplus
}
#endif

#endif /* TEMPMAIL_WEBUI_H */
