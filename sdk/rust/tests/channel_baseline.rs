/*!
 * 渠道注册表回归测试
 *
 * 校验由注册表派生的有序渠道列表与仓库根 `.baseline_channels.txt`
 * 逐行完全一致（数量与顺序均为硬约束，五端一致）。
 * 任何改变渠道数量或顺序的改动都会使本测试失败，防止意外破坏跨端约束。
 */

use tempmail_sdk::{all_channels, get_channel_info, list_channels, Channel};

/// 读取基线文件（相对 crate 根目录）的非空行列表。
fn load_baseline() -> Vec<String> {
    let path = concat!(env!("CARGO_MANIFEST_DIR"), "/../../.baseline_channels.txt");
    let content =
        std::fs::read_to_string(path).unwrap_or_else(|e| panic!("无法读取基线文件 {path}: {e}"));
    content
        .lines()
        .map(|l| l.trim())
        .filter(|l| !l.is_empty())
        .map(|l| l.to_string())
        .collect()
}

/// list_channels() 的 slug 顺序与基线逐行完全一致。
#[test]
fn test_channel_order_matches_baseline() {
    let baseline = load_baseline();
    let infos = list_channels();
    assert_eq!(
        infos.len(),
        baseline.len(),
        "渠道数量不一致: 运行时 {}, 基线 {}",
        infos.len(),
        baseline.len()
    );
    for (i, want) in baseline.iter().enumerate() {
        let got = infos[i].channel.to_string();
        assert_eq!(
            &got,
            want,
            "第 {} 个渠道不一致: 运行时 {:?}, 基线 {:?}",
            i + 1,
            got,
            want
        );
    }
}

/// all_channels() 与 list_channels() 顺序一致，且每个渠道都能查到信息。
#[test]
fn test_registry_consistency() {
    let channels = all_channels();
    let infos = list_channels();
    assert_eq!(
        channels.len(),
        infos.len(),
        "all_channels 与 list_channels 数量不一致"
    );
    for (i, ch) in channels.iter().enumerate() {
        assert_eq!(ch, &infos[i].channel, "第 {} 项渠道错位", i + 1);
        let info = get_channel_info(ch).unwrap_or_else(|| panic!("渠道 {ch} 缺少信息"));
        assert_eq!(&info.channel, ch);
        assert!(!info.name.is_empty(), "渠道 {ch} 显示名为空");
    }
}

/// 抽查 6 个代表性渠道的 slug、名称与网站，覆盖固定域名/镜像/带 token 等分支。
#[test]
fn test_representative_channels() {
    let cases = [
        (Channel::Tempmail, "tempmail", "TempMail", "tempmail.ing"),
        (
            Channel::XghffCom,
            "xghff-com",
            "xghff.com",
            "10minutemail.one",
        ),
        (Channel::MailTm, "mail-tm", "Mail.tm", "mail.tm"),
        (
            Channel::Mailinator,
            "mailinator",
            "Mailinator",
            "mailinator.com",
        ),
        (
            Channel::TempmailPlus,
            "tempmail-plus",
            "TempMail Plus",
            "tempmail.plus",
        ),
        (
            Channel::TenMinuteMailNet,
            "ten-minute-mail-net",
            "10MinuteMail.net",
            "10minutemail.net",
        ),
    ];
    for (ch, slug, name, website) in cases {
        assert_eq!(ch.to_string(), slug, "{ch} slug 不符");
        let info = get_channel_info(&ch).expect("应存在");
        assert_eq!(info.name, name, "{ch} name 不符");
        assert_eq!(info.website, website, "{ch} website 不符");
    }
}
