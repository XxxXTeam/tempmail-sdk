<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail;

/**
 * 全局配置容器（单例）
 *
 * 维护当前 SDK 全局配置及其版本号，配置变更时递增版本号，
 * 供共享 HTTP 客户端判断缓存是否失效。
 */
final class ConfigStore
{
    private static ?Config $config = null;
    private static int $version = 0;

    /**
     * 获取当前全局配置（首次调用时从环境变量初始化）。
     */
    public static function get(): Config
    {
        if (self::$config === null) {
            self::$config = Config::fromEnv();
        }
        return self::$config;
    }

    /**
     * 设置全局配置，并使缓存的 HTTP 客户端失效。
     */
    public static function set(Config $config): void
    {
        self::$config = $config;
        self::$version++;
    }

    /**
     * 获取当前配置版本号，用于缓存失效判断。
     */
    public static function version(): int
    {
        return self::$version;
    }
}
