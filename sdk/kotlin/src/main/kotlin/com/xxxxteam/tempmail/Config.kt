package com.xxxxteam.tempmail

/**
 * SDK 全局配置
 *
 * 支持环境变量和代码配置两种方式，代码配置优先级高于环境变量。
 * 使用 DSL 风格配置：
 * ```kotlin
 * TempMail.configure {
 *     proxy = "http://127.0.0.1:7890"
 *     timeout = 30
 *     insecure = false
 * }
 * ```
 */
data class SdkConfig(
    /** HTTP/SOCKS5 代理 URL */
    var proxy: String? = null,
    /** 超时秒数 */
    var timeout: Int = 15,
    /** 是否跳过 TLS 验证 */
    var insecure: Boolean = false,
    /** 遥测是否启用（null 表示默认开启） */
    var telemetryEnabled: Boolean? = null,
    /** 自定义遥测端点 */
    var telemetryUrl: String? = null,
    /** apihz 渠道 ID */
    var apihzId: String? = null,
    /** apihz 渠道 Key */
    var apihzKey: String? = null,
    /** DropMail auth token */
    var dropmailAuthToken: String? = null,
)

/**
 * 全局配置管理
 */
object Config {
    @Volatile
    private var config: SdkConfig = loadFromEnv()

    /** 获取当前配置（只读快照，防止外部修改内部状态） */
    fun get(): SdkConfig = config.copy()

    /**
     * 获取当前配置的内部直接引用（零拷贝，仅供 SDK 内部只读使用）
     *
     * 避免热路径（遥测上报、HTTP 客户端构建等）每次调用 [get] 产生的对象分配。
     * 调用方不得修改返回对象的任何字段。
     */
    internal fun current(): SdkConfig = config

    /** DSL 风格配置 */
    fun configure(block: SdkConfig.() -> Unit) {
        val base = loadFromEnv()
        block(base)
        config = base
        // 重建 HTTP 客户端
        Http.rebuildClient()
    }

    /** 从环境变量加载配置 */
    private fun loadFromEnv(): SdkConfig {
        val cfg = SdkConfig()
        System.getenv("TEMPMAIL_PROXY")?.takeIf { it.isNotBlank() }?.let { cfg.proxy = it }
        System.getenv("TEMPMAIL_TIMEOUT")?.toIntOrNull()?.let { cfg.timeout = it }
        System.getenv("TEMPMAIL_INSECURE")?.lowercase()?.let {
            cfg.insecure = it in listOf("1", "true", "yes")
        }
        System.getenv("TEMPMAIL_TELEMETRY_ENABLED")?.lowercase()?.let {
            cfg.telemetryEnabled = it !in listOf("false", "0", "no")
        }
        System.getenv("TEMPMAIL_TELEMETRY_URL")?.takeIf { it.isNotBlank() }?.let { cfg.telemetryUrl = it }
        System.getenv("DROPMAIL_AUTH_TOKEN")?.takeIf { it.isNotBlank() }?.let { cfg.dropmailAuthToken = it }
        System.getenv("APIHZ_ID")?.takeIf { it.isNotBlank() }?.let { cfg.apihzId = it }
        System.getenv("APIHZ_KEY")?.takeIf { it.isNotBlank() }?.let { cfg.apihzKey = it }
        return cfg
    }
}
