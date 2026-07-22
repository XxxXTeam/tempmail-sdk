package com.xxxxteam.tempmail;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.TreeMap;

/**
 * HTTP 响应封装，供 provider 读取状态码、正文与响应头。
 */
public final class HttpResult {

    /** HTTP 状态码。 */
    private final int statusCode;

    /** 响应正文（UTF-8 文本）。 */
    private final String body;

    /** 响应头（大小写不敏感）。 */
    private final Map<String, String> headers;

    /** 所有 Set-Cookie 原始值。 */
    private final List<String> setCookies;

    /**
     * 构造响应结果。
     *
     * @param statusCode 状态码
     * @param body       响应正文
     * @param headers    响应头
     * @param setCookies Set-Cookie 列表
     */
    public HttpResult(int statusCode, String body, Map<String, String> headers, List<String> setCookies) {
        this.statusCode = statusCode;
        this.body = body != null ? body : "";
        this.headers = headers != null ? headers : new TreeMap<>(String.CASE_INSENSITIVE_ORDER);
        this.setCookies = setCookies != null ? setCookies : new ArrayList<>();
    }

    /**
     * 获取状态码。
     *
     * @return HTTP 状态码
     */
    public int getStatusCode() {
        return statusCode;
    }

    /**
     * 获取响应正文。
     *
     * @return 正文文本
     */
    public String getBody() {
        return body;
    }

    /**
     * 获取响应头。
     *
     * @return 响应头映射
     */
    public Map<String, String> getHeaders() {
        return headers;
    }

    /**
     * 获取 Set-Cookie 列表。
     *
     * @return Set-Cookie 原始值列表
     */
    public List<String> getSetCookies() {
        return setCookies;
    }

    /**
     * 状态码是否在 2xx 范围。
     *
     * @return 2xx 返回 true
     */
    public boolean isOk() {
        return statusCode >= 200 && statusCode < 300;
    }

    /**
     * 状态码非 2xx 时抛出异常（消息含状态码，供 retry 判定）。
     */
    public void ensureSuccess() {
        if (!isOk()) {
            throw new RuntimeException("HTTP request failed: " + statusCode);
        }
    }
}
