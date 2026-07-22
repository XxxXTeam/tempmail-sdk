package com.xxxxteam.tempmail

import kotlinx.coroutines.delay

/**
 * HTTP 重试策略
 *
 * 使用指数退避，可配置最大重试次数和初始延迟。
 */
object Retry {
    /** 默认最大重试次数 */
    private const val DEFAULT_MAX_RETRIES = 2
    /** 默认初始退避延迟（毫秒） */
    private const val DEFAULT_INITIAL_DELAY_MS = 500L

    /**
     * 带重试的挂起操作
     *
     * @param maxRetries 最大重试次数
     * @param initialDelayMs 初始退避延迟
     * @param block 需要重试的挂起函数
     * @return 操作结果
     * @throws Exception 最终失败时抛出最后一次异常
     */
    suspend fun <T> withRetry(
        maxRetries: Int = DEFAULT_MAX_RETRIES,
        initialDelayMs: Long = DEFAULT_INITIAL_DELAY_MS,
        block: suspend () -> T,
    ): T {
        var lastException: Exception? = null
        var currentDelay = initialDelayMs

        repeat(maxRetries + 1) { attempt ->
            try {
                return block()
            } catch (e: Exception) {
                lastException = e
                if (attempt < maxRetries) {
                    delay(currentDelay)
                    currentDelay *= 2
                }
            }
        }
        throw lastException!!
    }
}
