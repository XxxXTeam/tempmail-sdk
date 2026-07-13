/**
 * 匿名用量上报：事件入队，合并为 schema_version=2 后异步 POST
 */

#include "cJSON.h"
#include "tempmail_internal.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <sys/time.h>
#include <unistd.h>
#endif

#define TM_TELEMETRY_DEFAULT_URL "https://sdk-1.openel.top/v1/event"
#define TM_TELEMETRY_MAX_BATCH 32
#define TM_TELEMETRY_FLUSH_MS 2000

#ifdef _WIN32
#define TM_OS_NAME "windows"
#elif defined(__APPLE__)
#define TM_OS_NAME "darwin"
#elif defined(__linux__)
#define TM_OS_NAME "linux"
#else
#define TM_OS_NAME "unknown"
#endif

typedef struct {
  char *url;
  char *json;
} tm_telemetry_job_t;

static long long tm_now_ms(void) {
#ifdef _WIN32
  FILETIME ft;
  ULARGE_INTEGER u;
  GetSystemTimeAsFileTime(&ft);
  u.LowPart = ft.dwLowDateTime;
  u.HighPart = ft.dwHighDateTime;
  return (long long)(u.QuadPart / 10000ULL - 11644473600000ULL);
#else
  struct timeval tv;
  if (gettimeofday(&tv, NULL) != 0)
    return (long long)time(NULL) * 1000LL;
  return (long long)tv.tv_sec * 1000LL + (long long)tv.tv_usec / 1000LL;
#endif
}

static void truncate_error(char *s, size_t max_len) {
  if (!s)
    return;
  size_t n = strlen(s);
  if (n > max_len)
    s[max_len] = '\0';
}

#ifdef _WIN32
static CRITICAL_SECTION g_tel_cs;
static volatile LONG g_tel_cs_state = 0; /* 0 未初始化 1 正在初始化 2 完成 */

static void tm_tel_ensure_cs(void) {
  for (;;) {
    LONG s = g_tel_cs_state;
    if (s == 2)
      return;
    if (s == 0) {
      if (InterlockedCompareExchange(&g_tel_cs_state, 1, 0) == 0) {
        InitializeCriticalSection(&g_tel_cs);
        InterlockedExchange(&g_tel_cs_state, 2);
        return;
      }
      continue;
    }
    Sleep(1);
  }
}

static HANDLE g_flush_thread = NULL;
static volatile LONG g_tel_exit = 0;
static cJSON *g_batch = NULL;

static DWORD WINAPI tm_telemetry_post_thread(LPVOID param) {
  tm_telemetry_job_t *job = (tm_telemetry_job_t *)param;
  char ua_hdr[128];
  snprintf(ua_hdr, sizeof(ua_hdr), "User-Agent: tempmail-sdk-c/%s",
           TEMPMAIL_SDK_VERSION);
  const char *hdrs[] = {
      "Content-Type: application/json",
      ua_hdr,
      NULL,
  };
  tm_http_response_t *r =
      tm_http_request(TM_HTTP_POST, job->url, hdrs, job->json, 8);
  if (r)
    tm_http_response_free(r);
  free(job->url);
  free(job->json);
  free(job);
  return 0;
}

static void tm_telemetry_send_async(const char *url, char *json_body) {
  tm_telemetry_job_t *job =
      (tm_telemetry_job_t *)calloc(1, sizeof(tm_telemetry_job_t));
  if (!job) {
    free(json_body);
    return;
  }
  job->url = strdup(url);
  job->json = json_body;
  if (!job->url || !job->json) {
    free(job->url);
    free(job->json);
    free(job);
    return;
  }
  HANDLE h = CreateThread(NULL, 0, tm_telemetry_post_thread, job, 0, NULL);
  if (h)
    CloseHandle(h);
  else {
    free(job->url);
    free(job->json);
    free(job);
  }
}

static DWORD WINAPI tm_telemetry_flush_loop(LPVOID p) {
  (void)p;
  while (!InterlockedCompareExchange(&g_tel_exit, 0, 0)) {
    Sleep(TM_TELEMETRY_FLUSH_MS);
    tm_telemetry_flush_batch();
  }
  return 0;
}

static void tm_telemetry_ensure_flush_thread(void) {
  static volatile LONG started = 0;
  if (InterlockedCompareExchange(&started, 1, 0) != 0)
    return;
  g_flush_thread =
      CreateThread(NULL, 0, tm_telemetry_flush_loop, NULL, 0, NULL);
  if (g_flush_thread)
    CloseHandle(g_flush_thread);
}

#else /* POSIX */

static pthread_mutex_t g_tel_mu = PTHREAD_MUTEX_INITIALIZER;
static cJSON *g_batch = NULL;
static pthread_t g_flush_th;
static volatile int g_flush_started = 0;
static volatile int g_tel_exit_posix = 0;

static void *tm_telemetry_post_thread(void *param) {
  tm_telemetry_job_t *job = (tm_telemetry_job_t *)param;
  char ua_hdr[128];
  snprintf(ua_hdr, sizeof(ua_hdr), "User-Agent: tempmail-sdk-c/%s",
           TEMPMAIL_SDK_VERSION);
  const char *hdrs[] = {
      "Content-Type: application/json",
      ua_hdr,
      NULL,
  };
  tm_http_response_t *r =
      tm_http_request(TM_HTTP_POST, job->url, hdrs, job->json, 8);
  if (r)
    tm_http_response_free(r);
  free(job->url);
  free(job->json);
  free(job);
  return NULL;
}

static void tm_telemetry_send_async(const char *url, char *json_body) {
  tm_telemetry_job_t *job =
      (tm_telemetry_job_t *)calloc(1, sizeof(tm_telemetry_job_t));
  if (!job) {
    free(json_body);
    return;
  }
  job->url = strdup(url);
  job->json = json_body;
  if (!job->url || !job->json) {
    free(job->url);
    free(job->json);
    free(job);
    return;
  }
  pthread_t t;
  if (pthread_create(&t, NULL, tm_telemetry_post_thread, job) != 0) {
    free(job->url);
    free(job->json);
    free(job);
  } else {
    pthread_detach(t);
  }
}

static void *tm_telemetry_flush_loop_posix(void *p) {
  (void)p;
  while (!g_tel_exit_posix) {
    usleep((unsigned)(TM_TELEMETRY_FLUSH_MS * 1000));
    tm_telemetry_flush_batch();
  }
  return NULL;
}

static void tm_telemetry_ensure_flush_thread(void) {
  if (g_flush_started)
    return;
  pthread_mutex_lock(&g_tel_mu);
  if (!g_flush_started) {
    g_flush_started = 1;
    pthread_create(&g_flush_th, NULL, tm_telemetry_flush_loop_posix, NULL);
    pthread_detach(g_flush_th);
  }
  pthread_mutex_unlock(&g_tel_mu);
}

#endif /* _WIN32 vs POSIX */

static const char *tm_resolve_telemetry_url(void) {
  const tm_config_t *c = tm_get_config();
  if (c->telemetry_url && c->telemetry_url[0])
    return c->telemetry_url;
  const char *ev = getenv("TEMPMAIL_TELEMETRY_URL");
  if (ev && ev[0])
    return ev;
  return TM_TELEMETRY_DEFAULT_URL;
}

/* 取出当前批次并 POST */
void tm_telemetry_flush_batch(void) {
  const tm_config_t *c = tm_get_config();
  if (!c->telemetry_enabled) {
#ifdef _WIN32
    tm_tel_ensure_cs();
    EnterCriticalSection(&g_tel_cs);
    if (g_batch) {
      cJSON_Delete(g_batch);
      g_batch = NULL;
    }
    LeaveCriticalSection(&g_tel_cs);
#else
    pthread_mutex_lock(&g_tel_mu);
    if (g_batch) {
      cJSON_Delete(g_batch);
      g_batch = NULL;
    }
    pthread_mutex_unlock(&g_tel_mu);
#endif
    return;
  }

  const char *url = tm_resolve_telemetry_url();
  if (!url || !url[0])
    return;

  cJSON *events = NULL;
#ifdef _WIN32
  tm_tel_ensure_cs();
  EnterCriticalSection(&g_tel_cs);
  if (g_batch && cJSON_GetArraySize(g_batch) > 0) {
    events = g_batch;
    g_batch = cJSON_CreateArray();
  }
  LeaveCriticalSection(&g_tel_cs);
#else
  pthread_mutex_lock(&g_tel_mu);
  if (g_batch && cJSON_GetArraySize(g_batch) > 0) {
    events = g_batch;
    g_batch = cJSON_CreateArray();
  }
  pthread_mutex_unlock(&g_tel_mu);
#endif

  if (!events)
    return;

  cJSON *root = cJSON_CreateObject();
  if (!root) {
    cJSON_Delete(events);
    return;
  }
  cJSON_AddNumberToObject(root, "schema_version", 2);
  cJSON_AddStringToObject(root, "sdk_language", "c");
  cJSON_AddStringToObject(root, "sdk_version", TEMPMAIL_SDK_VERSION);
  cJSON_AddStringToObject(root, "os", TM_OS_NAME);
  cJSON_AddStringToObject(root, "arch", "unknown");
  cJSON_AddItemToObject(root, "events", events);

  char *json = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  if (!json)
    return;

  tm_telemetry_send_async(url, json);
}

void tm_telemetry_report(const char *operation, const char *channel,
                         bool success, int attempt_count, int channels_tried,
                         const char *error_msg) {
  const tm_config_t *c = tm_get_config();
  if (!c->telemetry_enabled)
    return;

#ifdef _WIN32
  tm_telemetry_ensure_flush_thread();
  tm_tel_ensure_cs();
  EnterCriticalSection(&g_tel_cs);
  if (!g_batch)
    g_batch = cJSON_CreateArray();
  if (!g_batch) {
    LeaveCriticalSection(&g_tel_cs);
    return;
  }
#else
  tm_telemetry_ensure_flush_thread();
  pthread_mutex_lock(&g_tel_mu);
  if (!g_batch)
    g_batch = cJSON_CreateArray();
  if (!g_batch) {
    pthread_mutex_unlock(&g_tel_mu);
    return;
  }
#endif

  cJSON *ev = cJSON_CreateObject();
  if (ev) {
    cJSON_AddStringToObject(ev, "operation", operation ? operation : "");
    cJSON_AddStringToObject(ev, "channel", channel ? channel : "");
    cJSON_AddBoolToObject(ev, "success", success);
    cJSON_AddNumberToObject(ev, "attempt_count", attempt_count);
    if (channels_tried > 0)
      cJSON_AddNumberToObject(ev, "channels_tried", channels_tried);
    if (error_msg && error_msg[0]) {
      char *err_copy = strdup(error_msg);
      if (err_copy) {
        truncate_error(err_copy, 400);
        cJSON_AddStringToObject(ev, "error", err_copy);
        free(err_copy);
      }
    }
    cJSON_AddNumberToObject(ev, "ts_ms", (double)tm_now_ms());
    cJSON_AddItemToArray(g_batch, ev);
  }

  int n = cJSON_GetArraySize(g_batch);
#ifdef _WIN32
  LeaveCriticalSection(&g_tel_cs);
#else
  pthread_mutex_unlock(&g_tel_mu);
#endif

  if (n >= TM_TELEMETRY_MAX_BATCH)
    tm_telemetry_flush_batch();
}
