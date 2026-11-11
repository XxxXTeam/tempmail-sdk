<?php

declare(strict_types=1);

/**
 * 校验脚本：验证 Registry 初始化无异常且渠道覆盖完整。
 *
 * 用法: php bin/check_registry.php <ChannelData.php>
 */

$channelDataPath = $argv[1] ?? null;
if ($channelDataPath === null) {
    fwrite(STDERR, "用法: php bin/check_registry.php <ChannelData.php>\n");
    exit(2);
}

require $channelDataPath;

$slugs = array_column(ChanhanzhanX\TempMail\ChannelData::CHANNELS, 0);
$n = count($slugs);
if ($n !== 304) {
    fwrite(STDERR, "FAIL: ChannelData 数量 $n，期望 304\n");
    exit(1);
}
if (count(array_unique($slugs)) !== $n) {
    fwrite(STDERR, "FAIL: ChannelData 存在重复 slug\n");
    exit(1);
}

$regFiles = glob('/home/clfchen/Desktop/file/tempmail-sdk/sdk/php/src/Registry.php');
if (empty($regFiles)) {
    fwrite(STDERR, "FAIL: 未找到 Registry.php\n");
    exit(1);
}

$regSrc = file_get_contents($regFiles[0]);
if ($regSrc === false) {
    fwrite(STDERR, "FAIL: 读取 Registry.php 失败\n");
    exit(1);
}

// 移除 use 与 use function / use const 声明，避免侧载真实 provider 类
$classLike = preg_replace('/^\s*use\s+[A-Za-z_\\\\]+;/m', '', $regSrc);
if (!is_string($classLike)) {
    fwrite(STDERR, "FAIL: 预处理 Registry.php 失败\n");
    exit(1);
}
$classLike = preg_replace('/^\s*use\s+(function|const)\s+[A-Za-z_\\\\]+;/m', '', $classLike);
if (!is_string($classLike)) {
    fwrite(STDERR, "FAIL: 预处理 Registry.php 失败\n");
    exit(1);
}

// 侧载替身自动加载：剥离 use 后，Registry 内的短名 provider 引用会解析到
// ChanhanzhanX\TempMail\X，用 __callStatic 万能替身承载 makeGenerate/makeGetEmails 调用。
spl_autoload_register(static function (string $class): void {
    if (!str_starts_with($class, 'ChanhanzhanX\\TempMail\\')) {
        return;
    }
    if (class_exists($class, false) || interface_exists($class, false) || trait_exists($class, false)) {
        return;
    }
    $short = substr($class, strlen('ChanhanzhanX\\TempMail\\'));
    if ($short === '' || str_contains($short, '\\') || !preg_match('/^[A-Za-z_][A-Za-z0-9_]*$/', $short)) {
        return;
    }
    eval('namespace ChanhanzhanX\\TempMail; final class ' . $short . ' { '
        . 'public static function __callStatic(string $n, array $a): mixed { '
        . 'return static function (): mixed { return null; }; } }');
});

// 在 namespace 声明后注入侧载基础类（Registry::ensureInitialized 需要 ChannelSpec）
$stub = <<<'PHP'

final class ChannelSpec
{
    public function __construct(
        public readonly string $channel,
        public readonly string $name,
        public readonly string $website,
        public readonly mixed $generate,
        public readonly mixed $getEmails,
    ) {
    }
}

PHP;

$innerNs = 'namespace ChanhanzhanX\\TempMail;' . "\n" . $stub;
$evalSrc = str_replace('namespace ChanhanzhanX\\TempMail;', $innerNs, $classLike, $count);
if ($count !== 1) {
    fwrite(STDERR, "FAIL: 未能定位 Registry.php 的 namespace 声明\n");
    exit(1);
}

try {
    eval(substr($evalSrc, strlen('<?php ')));
} catch (\Throwable $e) {
    fwrite(STDERR, "FAIL: Registry 初始化抛异常: " . $e->getMessage() . "\n");
    fwrite(STDERR, $e->getTraceAsString() . "\n");
    exit(1);
}

try {
    $all = ChanhanzhanX\TempMail\Registry::all();
    $names = ChanhanzhanX\TempMail\Registry::channelNames();
} catch (\Throwable $e) {
    fwrite(STDERR, "FAIL: Registry::all()/channelNames() 抛异常: " . $e->getMessage() . "\n");
    exit(1);
}

if (count($all) !== 304) {
    fwrite(STDERR, 'FAIL: Registry::all() 数量 ' . count($all) . "，期望 304\n");
    exit(1);
}
if ($names !== $slugs) {
    fwrite(STDERR, "FAIL: Registry::channelNames() 与 ChannelData 不一致\n");
    exit(1);
}

$actual = array_column($all, 'channel');
if ($actual !== $slugs) {
    fwrite(STDERR, "FAIL: Registry 注册顺序与 ChannelData 不一致\n");
    foreach ($actual as $i => $slug) {
        if (($slugs[$i] ?? null) !== $slug) {
            fwrite(STDERR, "  差异@{$i}: registry=$slug channelData=" . ($slugs[$i] ?? '<无>') . "\n");
        }
    }
    exit(1);
}

echo "OK: Registry 与 ChannelData 均为 304 个渠道且顺序一致\n";
exit(0);