"""
渠道顺序回归保护测试

校验运行时经注册表派生的有序渠道 slug 列表，与仓库根 .baseline_channels.txt
基线文件逐行完全一致。基线来自重构前的 ALL_CHANNELS 顺序（硬约束，五端一致）。
任何改变渠道数量或顺序的改动都会使本测试失败，防止意外破坏跨端约束。
"""

import os

from tempmail_sdk import list_channels
from tempmail_sdk.registry import CHANNEL_REGISTRY, CHANNEL_REGISTRY_MAP

# 仓库根基线文件：本文件位于 sdk/python/tests/，向上三级到仓库根
_BASELINE_PATH = os.path.normpath(
    os.path.join(os.path.dirname(__file__), "..", "..", "..", ".baseline_channels.txt")
)


def _load_baseline():
    with open(_BASELINE_PATH, encoding="utf-8") as f:
        return [line.strip() for line in f if line.strip()]


def test_channel_order_matches_baseline():
    """运行时渠道顺序与基线逐行完全一致"""
    baseline = _load_baseline()
    infos = list_channels()
    assert len(infos) == len(baseline), (
        f"渠道数量不一致: 运行时 {len(infos)}, 基线 {len(baseline)}"
    )
    for i, want in enumerate(baseline):
        got = infos[i].channel
        assert got == want, f"第 {i + 1} 个渠道不一致: 运行时 {got!r}, 基线 {want!r}"


def test_registry_consistency():
    """注册表内部一致性：有序列表与映射数量相同，每个渠道可查且实现非空"""
    assert len(CHANNEL_REGISTRY) == len(CHANNEL_REGISTRY_MAP)
    for spec in CHANNEL_REGISTRY:
        mapped = CHANNEL_REGISTRY_MAP.get(spec.channel)
        assert mapped is not None, f"渠道 {spec.channel} 不在映射表中"
        assert mapped is spec, f"映射表指针错位: {spec.channel}"
        assert callable(spec.generate), f"渠道 {spec.channel} 缺少 generate 实现"
        assert callable(spec.get_emails), f"渠道 {spec.channel} 缺少 get_emails 实现"
