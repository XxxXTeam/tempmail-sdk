package com.xxxxteam.tempmail;

/**
 * 通用重试工具：网络错误 / 超时 / 429 / HTTP 4xx-5xx 自动重试，指数退避。
 *
 * <p>参数校验类错误（{@link IllegalArgumentException} 等）不重试，直接返回失败。</p>
 */
public final class Retry {

    private Retry() {
    }

    /**
     * 无参操作接口，允许抛出异常。
     *
     * @param <T> 返回类型
     */
    @FunctionalInterface
    public interface ThrowingSupplier<T> {
        /**
         * 执行操作。
         *
         * @return 结果
         * @throws Exception 执行失败
         */
        T get() throws Exception;
    }

    /**
     * 重试结果：ok 时 value 有效，否则 error 有效。
     *
     * @param <T> 返回类型
     */
    public static final class Result<T> {

        /** 是否成功。 */
        private final boolean ok;

        /** 实际尝试次数。 */
        private final int attempts;

        /** 成功值，失败时为 null。 */
        private final T value;

        /** 失败异常，成功时为 null。 */
        private final Exception error;

        private Result(boolean ok, int attempts, T value, Exception error) {
            this.ok = ok;
            this.attempts = attempts;
            this.value = value;
            this.error = error;
        }

        /**
         * 是否成功。
         *
         * @return 成功返回 true
         */
        public boolean isOk() {
            return ok;
        }

        /**
         * 获取尝试次数。
         *
         * @return 尝试次数
         */
        public int getAttempts() {
            return attempts;
        }

        /**
         * 获取成功值。
         *
         * @return 成功值，可能为 null
         */
        public T getValue() {
            return value;
        }

        /**
         * 获取失败异常。
         *
         * @return 异常，可能为 null
         */
        public Exception getError() {
            return error;
        }
    }

    /**
     * 判断错误是否应重试。
     *
     * @param error 异常
     * @return 应重试返回 true
     */
    private static boolean shouldRetry(Exception error) {
        if (error instanceof IllegalArgumentException) {
            return false;
        }
        String msg = error.getMessage() != null ? error.getMessage().toLowerCase() : "";
        String[] networkKeywords = {
            "connection", "timeout", "timed out", "dns", "eof",
            "broken pipe", "network is unreachable", "refused", "reset",
        };
        for (String kw : networkKeywords) {
            if (msg.contains(kw)) {
                return true;
            }
        }
        if (msg.contains("429") || msg.contains("too many requests") || msg.contains("rate limit")) {
            return true;
        }
        return msg.contains(": 4") || msg.contains(": 5");
    }

    /**
     * 带尝试次数的重试执行器，失败时不抛异常（见 error 字段）。
     *
     * @param fn     操作
     * @param config 重试配置，可为 null
     * @param <T>    返回类型
     * @return 重试结果
     */
    public static <T> Result<T> withAttempts(ThrowingSupplier<T> fn, RetryConfig config) {
        RetryConfig cfg = config != null ? config : new RetryConfig();
        Exception lastError = null;

        for (int attempt = 0; attempt <= cfg.getMaxRetries(); attempt++) {
            int attempts = attempt + 1;
            try {
                T value = fn.get();
                return new Result<>(true, attempts, value, null);
            } catch (Exception e) {
                lastError = e;
                if (attempt >= cfg.getMaxRetries() || !shouldRetry(e)) {
                    return new Result<>(false, attempts, null, e);
                }
                long delay = Math.min(
                        (long) (cfg.getInitialDelayMillis() * Math.pow(2, attempt)),
                        cfg.getMaxDelayMillis());
                try {
                    Thread.sleep(delay);
                } catch (InterruptedException ie) {
                    Thread.currentThread().interrupt();
                    return new Result<>(false, attempts, null, e);
                }
            }
        }

        return new Result<>(false, cfg.getMaxRetries() + 1, null, lastError);
    }
}
