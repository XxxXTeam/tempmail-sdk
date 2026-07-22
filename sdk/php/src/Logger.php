<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail;

/**
 * 简易日志器
 *
 * 提供分级日志输出（默认静默），供 SDK 内部记录尝试与错误信息。
 */
final class Logger
{
    public const SILENT = 0;
    public const ERROR = 1;
    public const WARNING = 2;
    public const INFO = 3;
    public const DEBUG = 4;

    private static int $level = self::SILENT;

    /** @var string|null WebUI 日志汇聚文件路径；非空时所有级别日志都追加写入（不受 $level 限制） */
    private static ?string $webuiSink = null;

    /** @var array<int,string> 日志级别到前端约定字符串的映射 */
    private const LEVEL_NAMES = [
        self::ERROR => 'error',
        self::WARNING => 'warn',
        self::INFO => 'info',
        self::DEBUG => 'debug',
    ];

    /**
     * 设置 WebUI 日志汇聚文件路径。
     *
     * 传入非空路径后，无论控制台日志级别如何，log() 都会把结构化日志追加写入该文件，
     * 供 WebUI 服务器进程 tail 后经 SSE 推送。传入 null 关闭。
     */
    public static function setWebUISink(?string $path): void
    {
        self::$webuiSink = $path;
    }

    /**
     * 设置日志级别。
     */
    public static function setLevel(int $level): void
    {
        self::$level = $level;
    }

    /**
     * 获取当前日志级别。
     */
    public static function getLevel(): int
    {
        return self::$level;
    }

    private static function log(int $level, string $tag, string $msg): void
    {
        if ($level <= self::$level) {
            fwrite(STDERR, "[$tag] $msg\n");
        }
        // 写入 WebUI sink（不受控制台级别限制）
        if (self::$webuiSink !== null) {
            $entry = json_encode([
                'time'    => date('H:i:s'),
                'level'   => self::LEVEL_NAMES[$level] ?? 'debug',
                'channel' => '',
                'msg'     => $msg,
            ], JSON_UNESCAPED_UNICODE);
            @file_put_contents(self::$webuiSink, $entry . "\n", FILE_APPEND | LOCK_EX);
        }
    }

    public static function error(string $msg): void
    {
        self::log(self::ERROR, 'ERROR', $msg);
    }

    public static function warning(string $msg): void
    {
        self::log(self::WARNING, 'WARN', $msg);
    }

    public static function info(string $msg): void
    {
        self::log(self::INFO, 'INFO', $msg);
    }

    public static function debug(string $msg): void
    {
        self::log(self::DEBUG, 'DEBUG', $msg);
    }
}
