<?php

declare(strict_types=1);

/**
 * 验证脚本：导出 listChannels() 的有序 slug，与 baseline 逐行比对。
 *
 * 用法: php bin/list_channels.php
 * 退出码 0 表示 279 且顺序完全一致。
 */

require __DIR__ . '/../vendor/autoload.php';

use ChanhanzhanX\TempMail\TempMail;

$channels = TempMail::listChannels();
$slugs = array_map(static fn ($c): string => $c->channel, $channels);

$baselinePath = __DIR__ . '/../../../.baseline_channels.txt';
$baseline = array_values(array_filter(
    array_map('trim', file($baselinePath)),
    static fn ($x): bool => $x !== '',
));

fwrite(STDERR, 'listChannels 数量: ' . count($slugs) . "\n");
fwrite(STDERR, 'baseline 数量:     ' . count($baseline) . "\n");

if ($slugs === $baseline) {
    fwrite(STDERR, "结果: 顺序完全一致\n");
    // 正常输出有序 slug
    echo implode("\n", $slugs) . "\n";
    exit(0);
}

fwrite(STDERR, "结果: 不一致\n");
$n = max(count($slugs), count($baseline));
for ($i = 0; $i < $n; $i++) {
    $a = $slugs[$i] ?? '<无>';
    $b = $baseline[$i] ?? '<无>';
    if ($a !== $b) {
        fwrite(STDERR, "差异 @行" . ($i + 1) . ": listChannels=$a baseline=$b\n");
    }
}
exit(1);
