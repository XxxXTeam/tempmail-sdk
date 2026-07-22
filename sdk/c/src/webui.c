/**
 * WebUI 嵌入式 HTTP 服务器实现
 *
 * 使用 POSIX socket + pthread 实现一个极简 HTTP/1.1 服务器：
 *   - GET /                 返回内嵌前端 HTML
 *   - GET /api/info         返回 SDK 配置信息 JSON
 *   - GET /api/channels     返回渠道列表 JSON
 *   - GET /api/logs/stream  以 SSE 方式实时推送日志
 *
 * 日志推送通过接管全局日志处理器实现：启动时保存原处理器，替换为
 * webui_log_handler，后者在调用原处理器的同时把日志广播给所有 SSE 连接。
 */

#include "webui.h"
#include "tempmail_internal.h"
#include "webui_html.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

/* ========== 全局状态 ========== */

#define TM_WEBUI_MAX_SSE 64 /* 最大并发 SSE 连接数 */

/* SSE 连接列表：保存已建立 SSE 的客户端 socket fd */
static int g_sse_fds[TM_WEBUI_MAX_SSE];
static int g_sse_count = 0;
static pthread_mutex_t g_sse_mutex = PTHREAD_MUTEX_INITIALIZER;

/* 服务器运行状态 */
static pthread_mutex_t g_start_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool g_running = false;
static int g_listen_fd = -1;
static int g_port = 0;
static char g_host[64] = "127.0.0.1"; /* 实际绑定地址，供 /api/info 回显 */
static pthread_t g_accept_thread;
static tm_log_handler_t g_orig_handler = NULL;

/* ========== 前置声明 ========== */

static void *accept_loop(void *arg);
static void handle_client(int fd);
static void webui_log_handler(tm_log_level_t level, const char *msg);

/* ========== 辅助函数 ========== */

/* 完整写出缓冲区（处理部分写），失败返回 -1 */
static int send_all(int fd, const char *buf, size_t len) {
  size_t sent = 0;
  while (sent < len) {
    ssize_t n = send(fd, buf + sent, len - sent, MSG_NOSIGNAL);
    if (n <= 0) {
      if (n < 0 && (errno == EINTR))
        continue;
      return -1;
    }
    sent += (size_t)n;
  }
  return 0;
}

/* 日志级别转小写字符串（与前端 log-<level> 样式约定一致） */
static const char *level_lower(tm_log_level_t level) {
  switch (level) {
  case TM_LOG_ERROR:
    return "error";
  case TM_LOG_WARN:
    return "warn";
  case TM_LOG_INFO:
    return "info";
  case TM_LOG_DEBUG:
    return "debug";
  default:
    return "info";
  }
}

/* 发送一段固定内容的 HTTP 响应 */
static void send_response(int fd, const char *status, const char *content_type,
                          const char *body, size_t body_len) {
  char header[512];
  int hlen = snprintf(header, sizeof(header),
                      "HTTP/1.1 %s\r\n"
                      "Content-Type: %s\r\n"
                      "Content-Length: %zu\r\n"
                      "Access-Control-Allow-Origin: *\r\n"
                      "Connection: close\r\n"
                      "\r\n",
                      status, content_type, body_len);
  if (hlen <= 0)
    return;
  if (send_all(fd, header, (size_t)hlen) != 0)
    return;
  if (body_len > 0)
    send_all(fd, body, body_len);
}

/* 从 SSE 列表移除并关闭指定 fd（调用方不应持有 g_sse_mutex） */
static void sse_remove(int fd) {
  pthread_mutex_lock(&g_sse_mutex);
  for (int i = 0; i < g_sse_count; i++) {
    if (g_sse_fds[i] == fd) {
      g_sse_fds[i] = g_sse_fds[g_sse_count - 1];
      g_sse_count--;
      break;
    }
  }
  pthread_mutex_unlock(&g_sse_mutex);
  close(fd);
}

/* ========== 日志处理器（接管 + 广播） ========== */

/*
 * WebUI 日志处理器：先转发给原处理器（保持既有行为），再把日志组装为
 * JSON SSE 事件广播给所有 SSE 连接。写失败的连接会被移除并关闭。
 */
static void webui_log_handler(tm_log_level_t level, const char *msg) {
  /* 链式调用原处理器；若无原处理器则退回默认 stderr 行为 */
  if (g_orig_handler) {
    g_orig_handler(level, msg);
  } else {
    fprintf(stderr, "%s %s\n", level_lower(level), msg ? msg : "");
  }

  pthread_mutex_lock(&g_sse_mutex);
  if (g_sse_count == 0) {
    pthread_mutex_unlock(&g_sse_mutex);
    return;
  }

  /* 组装本地时间 HH:MM:SS */
  char timestr[16] = "";
  time_t now = time(NULL);
  struct tm tmv;
  if (localtime_r(&now, &tmv))
    strftime(timestr, sizeof(timestr), "%H:%M:%S", &tmv);

  /* 用 cJSON 生成合法转义的日志对象 */
  cJSON *obj = cJSON_CreateObject();
  cJSON_AddStringToObject(obj, "time", timestr);
  cJSON_AddStringToObject(obj, "level", level_lower(level));
  cJSON_AddStringToObject(obj, "msg", msg ? msg : "");
  char *json = cJSON_PrintUnformatted(obj);
  cJSON_Delete(obj);
  if (!json) {
    pthread_mutex_unlock(&g_sse_mutex);
    return;
  }

  /* 组装 SSE 帧：data: <json>\n\n */
  size_t jlen = strlen(json);
  char *frame = (char *)malloc(jlen + 10);
  int flen = 0;
  if (frame)
    flen = snprintf(frame, jlen + 10, "data: %s\n\n", json);
  free(json);

  /* 复制一份当前 fd 列表，写操作在锁内进行以保证一致性 */
  if (frame && flen > 0) {
    int dead[TM_WEBUI_MAX_SSE];
    int dead_n = 0;
    for (int i = 0; i < g_sse_count; i++) {
      if (send_all(g_sse_fds[i], frame, (size_t)flen) != 0)
        dead[dead_n++] = g_sse_fds[i];
    }
    /* 移除写失败的连接 */
    for (int d = 0; d < dead_n; d++) {
      for (int i = 0; i < g_sse_count; i++) {
        if (g_sse_fds[i] == dead[d]) {
          g_sse_fds[i] = g_sse_fds[g_sse_count - 1];
          g_sse_count--;
          break;
        }
      }
      close(dead[d]);
    }
  }
  free(frame);
  pthread_mutex_unlock(&g_sse_mutex);
}

/* ========== API JSON 构建 ========== */

/* 构建 /api/info 响应体，返回 malloc 字符串（调用方 free），失败返回 NULL */
static char *build_info_json(void) {
  const tm_config_t *cfg = tm_get_config();
  int timeout = cfg->timeout_secs > 0 ? cfg->timeout_secs : 15;

  cJSON *root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "language", "C");
  cJSON_AddStringToObject(root, "version", TEMPMAIL_SDK_VERSION);
  cJSON_AddStringToObject(root, "host", g_host);
  cJSON_AddNumberToObject(root, "port", g_port);
  if (cfg->proxy)
    cJSON_AddStringToObject(root, "proxy", cfg->proxy);
  else
    cJSON_AddStringToObject(root, "proxy", "");
  cJSON_AddNumberToObject(root, "timeout", timeout);

  cJSON *config = cJSON_CreateObject();
  cJSON_AddBoolToObject(config, "telemetry", cfg->telemetry_enabled);
  cJSON_AddBoolToObject(config, "insecure", cfg->insecure);
  cJSON_AddItemToObject(root, "config", config);

  char *out = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  return out;
}

/* 构建 /api/channels 响应体，返回 malloc 字符串（调用方 free），失败返回 NULL */
static char *build_channels_json(void) {
  int count = 0;
  const tm_channel_info_t *list = tm_list_channels(&count);
  cJSON *arr = cJSON_CreateArray();
  for (int i = 0; i < count && list; i++) {
    cJSON *item = cJSON_CreateObject();
    cJSON_AddStringToObject(item, "channel",
                            tm_channel_name(list[i].channel));
    cJSON_AddStringToObject(item, "name", list[i].name ? list[i].name : "");
    cJSON_AddStringToObject(item, "website",
                            list[i].website ? list[i].website : "");
    cJSON_AddItemToArray(arr, item);
  }
  char *out = cJSON_PrintUnformatted(arr);
  cJSON_Delete(arr);
  return out;
}

/* ========== 请求处理 ========== */

/* 建立 SSE 连接：发送 SSE 响应头并把 fd 登记到列表（不关闭 fd） */
static void start_sse(int fd) {
  const char *hdr = "HTTP/1.1 200 OK\r\n"
                    "Content-Type: text/event-stream\r\n"
                    "Cache-Control: no-cache\r\n"
                    "Access-Control-Allow-Origin: *\r\n"
                    "Connection: keep-alive\r\n"
                    "\r\n"
                    "retry: 3000\n\n";
  if (send_all(fd, hdr, strlen(hdr)) != 0) {
    close(fd);
    return;
  }
  pthread_mutex_lock(&g_sse_mutex);
  if (g_sse_count >= TM_WEBUI_MAX_SSE) {
    pthread_mutex_unlock(&g_sse_mutex);
    close(fd); /* 连接数超限，拒绝 */
    return;
  }
  g_sse_fds[g_sse_count++] = fd;
  pthread_mutex_unlock(&g_sse_mutex);
  /* fd 交由日志广播线程持有，本处理线程返回即可（不关闭） */
}

/* 从请求首行提取方法与路径，解析 HTTP 请求并路由 */
static void handle_client(int fd) {
  char buf[2048];
  ssize_t n = recv(fd, buf, sizeof(buf) - 1, 0);
  if (n <= 0) {
    close(fd);
    return;
  }
  buf[n] = '\0';

  /* 仅解析请求行首行：METHOD SP PATH SP VERSION */
  char method[8] = "";
  char path[512] = "";
  if (sscanf(buf, "%7s %511s", method, path) != 2) {
    send_response(fd, "400 Bad Request", "text/plain", "bad request", 11);
    close(fd);
    return;
  }
  /* 去掉查询串 */
  char *q = strchr(path, '?');
  if (q)
    *q = '\0';

  if (strcmp(path, "/") == 0 || strcmp(path, "/index.html") == 0) {
    send_response(fd, "200 OK", "text/html; charset=utf-8", TM_WEBUI_HTML,
                  strlen(TM_WEBUI_HTML));
    close(fd);
    return;
  }
  if (strcmp(path, "/api/info") == 0) {
    char *json = build_info_json();
    if (json) {
      send_response(fd, "200 OK", "application/json", json, strlen(json));
      free(json);
    } else {
      send_response(fd, "500 Internal Server Error", "text/plain", "err", 3);
    }
    close(fd);
    return;
  }
  if (strcmp(path, "/api/channels") == 0) {
    char *json = build_channels_json();
    if (json) {
      send_response(fd, "200 OK", "application/json", json, strlen(json));
      free(json);
    } else {
      send_response(fd, "500 Internal Server Error", "text/plain", "err", 3);
    }
    close(fd);
    return;
  }
  if (strcmp(path, "/api/logs/stream") == 0) {
    start_sse(fd); /* 成功时保留 fd，不关闭 */
    return;
  }

  send_response(fd, "404 Not Found", "text/plain", "not found", 9);
  close(fd);
}

/* 每个连接的处理线程入口 */
static void *client_thread(void *arg) {
  int fd = (int)(intptr_t)arg;
  handle_client(fd);
  return NULL;
}

/* ========== accept 主循环 ========== */

static void *accept_loop(void *arg) {
  (void)arg;
  while (1) {
    struct sockaddr_in cli;
    socklen_t clilen = sizeof(cli);
    int cfd = accept(g_listen_fd, (struct sockaddr *)&cli, &clilen);
    if (cfd < 0) {
      if (errno == EINTR)
        continue;
      break; /* listen_fd 已关闭（停止服务器）或出错，退出循环 */
    }
    pthread_t t;
    if (pthread_create(&t, NULL, client_thread,
                       (void *)(intptr_t)cfd) == 0) {
      pthread_detach(t);
    } else {
      close(cfd);
    }
  }
  return NULL;
}

/* ========== 公共 API ========== */

int tm_webui_start(const char *host, int port) {
  pthread_mutex_lock(&g_start_mutex);
  if (g_running) {
    int p = g_port;
    pthread_mutex_unlock(&g_start_mutex);
    return p; /* 已启动，幂等返回当前端口 */
  }

  /* host 取值：参数 > 环境变量 > 默认 127.0.0.1 */
  const char *bind_host = "127.0.0.1";
  if (host && host[0]) {
    bind_host = host;
  } else {
    const char *env_host = getenv("TEMPMAIL_WEBUI_HOST");
    if (env_host && env_host[0])
      bind_host = env_host;
  }

  /* 端口取值：参数 > 环境变量 > 随机 */
  int bind_port = 0;
  if (port > 0) {
    bind_port = port;
  } else {
    const char *env = getenv("TEMPMAIL_WEBUI_PORT");
    if (env && env[0]) {
      int p = atoi(env);
      if (p > 0 && p < 65536)
        bind_port = p;
    }
  }

  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    TM_LOG_ERR("WebUI: 创建 socket 失败: %s", strerror(errno));
    pthread_mutex_unlock(&g_start_mutex);
    return 0;
  }
  int opt = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  if (inet_aton(bind_host, &addr.sin_addr) == 0) {
    TM_LOG_ERR("WebUI: 无效的 host 地址: %s", bind_host);
    close(fd);
    pthread_mutex_unlock(&g_start_mutex);
    return 0;
  }
  addr.sin_port = htons((uint16_t)bind_port);

  if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
    TM_LOG_ERR("WebUI: bind 端口 %d 失败: %s", bind_port, strerror(errno));
    close(fd);
    pthread_mutex_unlock(&g_start_mutex);
    return 0;
  }
  if (listen(fd, 16) != 0) {
    TM_LOG_ERR("WebUI: listen 失败: %s", strerror(errno));
    close(fd);
    pthread_mutex_unlock(&g_start_mutex);
    return 0;
  }

  /* 回读实际端口（随机端口时由内核分配） */
  struct sockaddr_in bound;
  socklen_t blen = sizeof(bound);
  if (getsockname(fd, (struct sockaddr *)&bound, &blen) == 0)
    g_port = ntohs(bound.sin_port);
  else
    g_port = bind_port;

  /* 保存实际绑定的 host 供 /api/info 回显 */
  strncpy(g_host, bind_host, sizeof(g_host) - 1);
  g_host[sizeof(g_host) - 1] = '\0';
  g_listen_fd = fd;
  g_running = true;

  /* 接管日志处理器 */
  g_orig_handler = tm_get_log_handler();
  tm_set_log_handler(webui_log_handler);

  if (pthread_create(&g_accept_thread, NULL, accept_loop, NULL) != 0) {
    TM_LOG_ERR("WebUI: 创建 accept 线程失败");
    tm_set_log_handler(g_orig_handler);
    g_orig_handler = NULL;
    close(fd);
    g_listen_fd = -1;
    g_running = false;
    g_port = 0;
    pthread_mutex_unlock(&g_start_mutex);
    return 0;
  }

  int p = g_port;
  pthread_mutex_unlock(&g_start_mutex);
  TM_LOG_INF("WebUI 已启动: http://%s:%d", g_host, p);
  return p;
}

void tm_webui_stop(void) {
  pthread_mutex_lock(&g_start_mutex);
  if (!g_running) {
    pthread_mutex_unlock(&g_start_mutex);
    return;
  }
  g_running = false;

  /* 恢复原日志处理器，避免停止后仍向已释放的 SSE 写入 */
  tm_set_log_handler(g_orig_handler);
  g_orig_handler = NULL;

  /* 关闭监听 socket，令 accept_loop 从 accept() 返回并退出 */
  if (g_listen_fd >= 0) {
    shutdown(g_listen_fd, SHUT_RDWR);
    close(g_listen_fd);
    g_listen_fd = -1;
  }
  pthread_mutex_unlock(&g_start_mutex);

  pthread_join(g_accept_thread, NULL);

  /* 关闭所有 SSE 连接 */
  pthread_mutex_lock(&g_sse_mutex);
  for (int i = 0; i < g_sse_count; i++)
    close(g_sse_fds[i]);
  g_sse_count = 0;
  pthread_mutex_unlock(&g_sse_mutex);

  g_port = 0;
  TM_LOG_INF("WebUI 已停止");
}

int tm_webui_maybe_start_from_env(void) {
  const char *env = getenv("TEMPMAIL_WEBUI");
  if (!env || !env[0])
    return 0;
  if (strcmp(env, "true") == 0 || strcmp(env, "1") == 0 ||
      strcmp(env, "yes") == 0 || strcmp(env, "TRUE") == 0)
    return tm_webui_start(NULL, 0); /* host/port 由内部走环境变量 */
  return 0;
}
