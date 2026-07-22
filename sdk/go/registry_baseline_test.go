package tempemail

import (
	"bufio"
	"os"
	"testing"
)

/*
 * TestChannelOrderMatchesBaseline 回归保护：
 * 校验运行时经注册表派生的有序渠道 slug 列表，与仓库根 .baseline_channels.txt
 * 基线文件逐行完全一致。基线来自重构前的 allChannels 顺序（硬约束，五端一致）。
 * 任何改变渠道数量或顺序的改动都会使本测试失败，防止意外破坏跨端约束。
 */
func TestChannelOrderMatchesBaseline(t *testing.T) {
	const baselinePath = "../../.baseline_channels.txt"
	f, err := os.Open(baselinePath)
	if err != nil {
		t.Fatalf("无法打开基线文件 %s: %v", baselinePath, err)
	}
	defer f.Close()

	var baseline []string
	scanner := bufio.NewScanner(f)
	for scanner.Scan() {
		line := scanner.Text()
		if line == "" {
			continue
		}
		baseline = append(baseline, line)
	}
	if err := scanner.Err(); err != nil {
		t.Fatalf("读取基线文件失败: %v", err)
	}

	infos := ListChannels()
	if len(infos) != len(baseline) {
		t.Fatalf("渠道数量不一致: 运行时 %d, 基线 %d", len(infos), len(baseline))
	}

	for i, want := range baseline {
		got := string(infos[i].Channel)
		if got != want {
			t.Fatalf("第 %d 个渠道不一致: 运行时 %q, 基线 %q", i+1, got, want)
		}
	}
}

/*
 * TestRegistryConsistency 校验注册表内部一致性：
 * 有序切片与映射表数量相同，且每个 Channel 都能在映射中查到、Generate 非空。
 */
func TestRegistryConsistency(t *testing.T) {
	if len(channelRegistry) != len(channelRegistryMap) {
		t.Fatalf("注册表切片与映射数量不一致: %d vs %d", len(channelRegistry), len(channelRegistryMap))
	}
	for i := range channelRegistry {
		ch := channelRegistry[i].Channel
		spec, ok := channelRegistryMap[ch]
		if !ok {
			t.Fatalf("渠道 %s 不在映射表中", ch)
		}
		if spec.Channel != ch {
			t.Fatalf("映射表指针错位: %s -> %s", ch, spec.Channel)
		}
		if spec.Generate == nil {
			t.Fatalf("渠道 %s 缺少 Generate 实现", ch)
		}
	}
}
