package com.xxxxteam.tempmail;

import org.junit.jupiter.api.Test;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.List;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertNotNull;
import static org.junit.jupiter.api.Assertions.assertTrue;

/**
 * 渠道 baseline 回归测试：确保 {@code TempMail.listChannels()} 的渠道顺序与数量
 * 与仓库根目录 {@code .baseline_channels.txt} 逐行一致（五端同步的硬约束）。
 */
class ChannelBaselineTest {

    /**
     * 从当前工作目录逐级向上查找 .baseline_channels.txt。
     *
     * @return baseline 文件路径
     */
    private static Path locateBaseline() {
        Path dir = Paths.get("").toAbsolutePath();
        for (int i = 0; i < 8 && dir != null; i++) {
            Path candidate = dir.resolve(".baseline_channels.txt");
            if (Files.exists(candidate)) {
                return candidate;
            }
            dir = dir.getParent();
        }
        return null;
    }

    /**
     * 校验渠道数量与顺序完全对齐 baseline。
     *
     * @throws IOException 读取 baseline 失败
     */
    @Test
    void listChannelsMatchesBaseline() throws IOException {
        Path baseline = locateBaseline();
        assertNotNull(baseline, "未找到 .baseline_channels.txt");

        List<String> expected = new ArrayList<>();
        for (String line : Files.readAllLines(baseline, StandardCharsets.UTF_8)) {
            String s = line.trim();
            if (!s.isEmpty()) {
                expected.add(s);
            }
        }

        List<String> actual = new ArrayList<>();
        for (ChannelInfo info : TempMail.listChannels()) {
            actual.add(info.getChannel());
        }

        assertEquals(279, expected.size(), "baseline 行数应为 279");
        assertEquals(expected.size(), actual.size(), "渠道数量应与 baseline 一致");
        for (int i = 0; i < expected.size(); i++) {
            assertEquals(expected.get(i), actual.get(i),
                    "第 " + (i + 1) + " 个渠道顺序不一致");
        }
    }

    /**
     * 校验渠道标识唯一且非空。
     */
    @Test
    void channelsAreUniqueAndNonEmpty() {
        List<ChannelInfo> channels = TempMail.listChannels();
        assertEquals(279, channels.size());
        java.util.Set<String> seen = new java.util.HashSet<>();
        for (ChannelInfo c : channels) {
            assertTrue(c.getChannel() != null && !c.getChannel().isEmpty(), "渠道标识不应为空");
            assertTrue(seen.add(c.getChannel()), "渠道标识重复: " + c.getChannel());
        }
    }
}
