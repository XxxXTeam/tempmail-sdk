use rand::Rng;
use serde::Serialize;
use sha2::{Digest, Sha256};

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
pub struct LinshiScreenSer {
    pub width: i32,
    pub height: i32,
    pub avail_width: i32,
    pub avail_height: i32,
}

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
pub struct LinshiProfileSer {
    pub color_depth: i32,
    pub device_memory: i32,
    pub hardware_concurrency: i32,
    pub language: String,
    pub languages: String,
    pub pixel_ratio: f64,
    pub platform: String,
    pub screen: LinshiScreenSer,
    pub timezone: String,
    pub user_agent: String,
}

const UA: &[&str] = &[
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36",
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/121.0.0.0 Safari/537.36 Edg/121.0.0.0",
    "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36",
    "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/119.0.0.0 Safari/537.36",
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:122.0) Gecko/20100101 Firefox/122.0",
];
const PLATFORM: &[&str] = &["Win32", "MacIntel", "Linux x86_64"];
const LANG: &[&str] = &["zh-CN", "en-US", "en-GB", "ja-JP"];
const LANGS: &[&str] = &["zh-CN,zh,en", "en-US,en", "ja-JP,ja,en"];
const TZ: &[&str] = &["Asia/Shanghai", "America/New_York", "Europe/London", "UTC"];
const MEM: &[i32] = &[4, 8, 16];
const DEPTH: &[i32] = &[24, 30];
const RATIO: &[f64] = &[1.0, 1.25, 1.5, 2.0];
const W0: &[i32] = &[1920, 2560, 1680, 1536, 1366];
const H0: &[i32] = &[1080, 1440, 1050, 864, 768];

fn random_linshi_profile() -> LinshiProfileSer {
    let mut rng = rand::thread_rng();
    let w = rng.gen_range(0..W0.len());
    let h = rng.gen_range(0..H0.len());
    let w = W0[w] + rng.gen_range(0..81);
    let h = H0[h] + rng.gen_range(0..41);
    let mut avail_h = h - 24 - rng.gen_range(0..97);
    if avail_h < 0 {
        avail_h = h;
    }
    LinshiProfileSer {
        color_depth: DEPTH[rng.gen_range(0..DEPTH.len())],
        device_memory: MEM[rng.gen_range(0..MEM.len())],
        hardware_concurrency: rng.gen_range(4..33),
        language: LANG[rng.gen_range(0..LANG.len())].to_string(),
        languages: LANGS[rng.gen_range(0..LANGS.len())].to_string(),
        pixel_ratio: RATIO[rng.gen_range(0..RATIO.len())],
        platform: PLATFORM[rng.gen_range(0..PLATFORM.len())].to_string(),
        screen: LinshiScreenSer {
            width: w,
            height: h,
            avail_width: w,
            avail_height: avail_h,
        },
        timezone: TZ[rng.gen_range(0..TZ.len())].to_string(),
        user_agent: UA[rng.gen_range(0..UA.len())].to_string(),
    }
}

/// 与 linshi 前端 u(visitorId) 等价
pub fn derive_linshi_api_path_key(visitor_id: &str) -> String {
    if visitor_id.len() < 4 {
        panic!("visitorId too short");
    }
    let mut e: String = visitor_id[..visitor_id.len() - 2].to_string();
    let mut t: i32 = 0;
    for ch in e.chars() {
        t += (ch as u32 % 5) as i32;
    }
    if t < 10 {
        t = 10;
    }
    if t >= 100 {
        t = 99;
    }
    let ts = t.to_string();
    let ts0 = ts.chars().next().unwrap();
    let ts1 = ts.chars().nth(1).unwrap();
    e = format!("{}{}{}", &e[..3], ts0, &e[3..]);
    e = format!("{}{}{}", &e[..12], ts1, &e[12..]);
    e
}

fn synthetic_visitor_id(p: &LinshiProfileSer, salt: &[u8; 16]) -> String {
    let payload = serde_json::to_vec(p).expect("linshi profile json");
    let mut h = Sha256::new();
    h.update(&payload);
    h.update(salt);
    let out = h.finalize();
    hex::encode(out)[..32].to_string()
}

/// 随机 path key（与 npm randomSyntheticLinshiKey.apiPathKey 同逻辑）
pub fn random_synthetic_linshi_api_path_key() -> String {
    let p = random_linshi_profile();
    let salt: [u8; 16] = rand::thread_rng().gen();
    let vid = synthetic_visitor_id(&p, &salt);
    derive_linshi_api_path_key(&vid)
}
