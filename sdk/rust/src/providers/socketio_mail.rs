/*!
 * Socket.IO 临时邮箱共享实现
 * 用于 mjj.cm、mail.xiuvi.cn、linshi.co 等使用相同 Socket.IO 协议的站点
 */

use std::collections::{HashMap, HashSet};
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::Duration;

use serde_json::{json, Value};
use tungstenite::http::Request;
use tungstenite::{connect, Message};

use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};

const CONNECT_TIMEOUT_MS: u64 = 15000;
const HANDSHAKE_WAIT_MS: u64 = 1000;
const INITIAL_SYNC_WAIT_MS: u64 = 80;
const SOCKET_IO_VERSIONS: &[u32] = &[4, 3];

fn socket_url(host: &str, eio: u32) -> String {
    format!("wss://{}/socket.io/?EIO={}&transport=websocket", host, eio)
}

fn parse_event_packet(packet: &str) -> Option<(String, Value)> {
    if !packet.starts_with("42") {
        return None;
    }
    let arr: Vec<Value> = serde_json::from_str(&packet[2..]).ok()?;
    if arr.len() < 2 {
        return None;
    }
    let event = arr[0].as_str()?.to_string();
    Some((event, arr[1].clone()))
}

fn send_event(
    ws: &mut tungstenite::WebSocket<tungstenite::stream::MaybeTlsStream<std::net::TcpStream>>,
    event: &str,
    payload: &Value,
) -> Result<(), String> {
    let arr = json!([event, payload]);
    let msg = format!("42{}", arr);
    ws.send(Message::Text(msg)).map_err(|e| e.to_string())
}

fn flatten_mail(raw: &Value, recipient_email: &str) -> Value {
    let headers = raw.get("headers").and_then(|v| v.as_object());
    let empty_map = serde_json::Map::new();
    let hdrs = headers.unwrap_or(&empty_map);

    let id = raw
        .get("id")
        .and_then(|v| v.as_str())
        .or_else(|| raw.get("messageId").and_then(|v| v.as_str()))
        .or_else(|| hdrs.get("message-id").and_then(|v| v.as_str()))
        .or_else(|| hdrs.get("messageId").and_then(|v| v.as_str()))
        .map(|s| s.to_string())
        .unwrap_or_else(|| {
            let from = hdrs
                .get("from")
                .or_else(|| raw.get("from"))
                .and_then(|v| v.as_str())
                .unwrap_or("");
            let subj = hdrs
                .get("subject")
                .or_else(|| raw.get("subject"))
                .and_then(|v| v.as_str())
                .unwrap_or("");
            let dt = hdrs
                .get("date")
                .or_else(|| raw.get("date"))
                .and_then(|v| v.as_str())
                .unwrap_or("");
            format!("{}\n{}\n{}\n{}", from, subj, dt, recipient_email)
        });

    let from = hdrs
        .get("from")
        .or_else(|| raw.get("from"))
        .and_then(|v| v.as_str())
        .unwrap_or("");
    let subject = hdrs
        .get("subject")
        .or_else(|| raw.get("subject"))
        .and_then(|v| v.as_str())
        .unwrap_or("");
    let text = raw
        .get("text")
        .or_else(|| raw.get("body"))
        .and_then(|v| v.as_str())
        .unwrap_or("");
    let html = raw.get("html").and_then(|v| v.as_str()).unwrap_or("");
    let date = hdrs
        .get("date")
        .or_else(|| raw.get("date"))
        .and_then(|v| v.as_str())
        .unwrap_or("");

    json!({
        "id": id,
        "from": from,
        "to": recipient_email,
        "subject": subject,
        "text": text,
        "html": html,
        "date": date,
        "isRead": false,
    })
}

struct BoxState {
    emails: Vec<Email>,
    seen_ids: HashSet<String>,
    connected: bool,
}

/// Socket.IO 临时邮箱 provider
pub struct SocketIoMailProvider {
    channel: Channel,
    channel_str: String,
    default_host: String,
    mailboxes: Mutex<HashMap<String, Arc<Mutex<BoxState>>>>,
}

impl SocketIoMailProvider {
    pub fn new(channel: Channel, default_host: &str) -> Self {
        Self {
            channel_str: channel.to_string(),
            channel,
            default_host: default_host.to_string(),
            mailboxes: Mutex::new(HashMap::new()),
        }
    }

    fn connect_socket(
        &self,
        host: &str,
    ) -> Result<
        tungstenite::WebSocket<tungstenite::stream::MaybeTlsStream<std::net::TcpStream>>,
        String,
    > {
        let mut last_err = None;
        for &version in SOCKET_IO_VERSIONS {
            let url = socket_url(host, version);
            let request = Request::builder()
                .uri(&url)
                .header("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0")
                .header("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6")
                .header("Cache-Control", "no-cache")
                .header("DNT", "1")
                .header("Pragma", "no-cache")
                .header("Origin", format!("https://{}", host))
                .body(())
                .map_err(|e| e.to_string());

            let request = match request {
                Ok(r) => r,
                Err(e) => {
                    last_err = Some(e);
                    continue;
                }
            };

            match connect(request) {
                Ok((mut ws, _)) => {
                    let mut sent_connect = false;
                    for _ in 0..10 {
                        match ws.read() {
                            Ok(Message::Text(packet)) => {
                                if packet == "2" {
                                    let _ = ws.send(Message::Text("3".into()));
                                    continue;
                                }
                                if !sent_connect {
                                    if !packet.starts_with('0') {
                                        last_err = Some(format!(
                                            "{}: unexpected open packet for EIO={}",
                                            self.channel_str, version
                                        ));
                                        let _ = ws.close(None);
                                        break;
                                    }
                                    sent_connect = true;
                                    let _ = ws.send(Message::Text("40".into()));
                                    thread::sleep(Duration::from_millis(HANDSHAKE_WAIT_MS));
                                    return Ok(ws);
                                }
                                if packet.starts_with("40") {
                                    return Ok(ws);
                                }
                            }
                            Ok(_) => continue,
                            Err(e) => {
                                last_err = Some(e.to_string());
                                break;
                            }
                        }
                    }
                    let _ = ws.close(None);
                }
                Err(e) => {
                    last_err = Some(e.to_string());
                }
            }
        }
        Err(last_err.unwrap_or_else(|| format!("{}: websocket handshake failed", self.channel_str)))
    }

    fn request_shortid(&self, host: &str) -> Result<String, String> {
        let mut ws = self.connect_socket(host)?;
        send_event(&mut ws, "request shortid", &json!(true))?;

        for _ in 0..50 {
            match ws.read() {
                Ok(Message::Text(packet)) => {
                    if packet == "2" {
                        let _ = ws.send(Message::Text("3".into()));
                        continue;
                    }
                    if let Some((event, payload)) = parse_event_packet(&packet) {
                        if event == "shortid" {
                            if let Some(s) = payload.as_str() {
                                let _ = ws.close(None);
                                return Ok(s.to_string());
                            }
                        }
                    }
                }
                Ok(_) => continue,
                Err(e) => {
                    let _ = ws.close(None);
                    return Err(format!(
                        "{}: 等待 shortid 时连接断开: {}",
                        self.channel_str, e
                    ));
                }
            }
        }
        let _ = ws.close(None);
        Err(format!("{}: 等待 shortid 超时", self.channel_str))
    }

    pub fn generate_email(&self) -> Result<EmailInfo, String> {
        let host = &self.default_host;
        let shortid = self.request_shortid(host)?;
        let email = format!("{}@{}", shortid, host);
        self.ensure_mailbox(&email)?;
        Ok(EmailInfo {
            channel: self.channel.clone(),
            email,
            token: None,
            expires_at: None,
            created_at: None,
        })
    }

    fn get_state(&self, email: &str) -> Arc<Mutex<BoxState>> {
        let key = email.trim().to_lowercase();
        let mut map = self.mailboxes.lock().unwrap();
        map.entry(key)
            .or_insert_with(|| {
                Arc::new(Mutex::new(BoxState {
                    emails: Vec::new(),
                    seen_ids: HashSet::new(),
                    connected: false,
                }))
            })
            .clone()
    }

    fn ensure_mailbox(&self, email: &str) -> Result<(), String> {
        let state = self.get_state(email);
        {
            let st = state.lock().unwrap();
            if st.connected {
                return Ok(());
            }
        }

        let at_idx = email
            .find('@')
            .ok_or_else(|| format!("{}: invalid email address", self.channel_str))?;
        if at_idx == 0 {
            return Err(format!("{}: invalid email address", self.channel_str));
        }
        let local = &email[..at_idx];
        let host = &email[at_idx + 1..];
        let host = if host.is_empty() {
            &self.default_host
        } else {
            host
        };

        let mut ws = self.connect_socket(host)?;
        send_event(&mut ws, "set shortid", &json!(local))?;

        {
            let mut st = state.lock().unwrap();
            st.connected = true;
        }

        let state_clone = state.clone();
        let email_clone = email.to_string();
        thread::spawn(move || loop {
            match ws.read() {
                Ok(Message::Text(packet)) => {
                    if packet == "2" {
                        let _ = ws.send(Message::Text("3".into()));
                        continue;
                    }
                    if let Some((event, payload)) = parse_event_packet(&packet) {
                        if event == "mail" {
                            let flat = flatten_mail(&payload, &email_clone);
                            let normalized = normalize_email(&flat, &email_clone);
                            let mut st = state_clone.lock().unwrap();
                            if !normalized.id.is_empty() && !st.seen_ids.contains(&normalized.id) {
                                st.seen_ids.insert(normalized.id.clone());
                                st.emails.push(normalized);
                            }
                        }
                    }
                }
                Ok(_) => continue,
                Err(_) => {
                    let mut st = state_clone.lock().unwrap();
                    st.connected = false;
                    break;
                }
            }
        });

        thread::sleep(Duration::from_millis(INITIAL_SYNC_WAIT_MS));
        Ok(())
    }

    pub fn get_emails(&self, email: &str) -> Result<Vec<Email>, String> {
        self.ensure_mailbox(email)?;
        let state = self.get_state(email);
        let st = state.lock().unwrap();
        Ok(st.emails.clone())
    }
}

// 三个渠道的模块化封装

pub mod mjj_cm {
    use super::*;
    use std::sync::OnceLock;

    fn provider() -> &'static SocketIoMailProvider {
        static INSTANCE: OnceLock<SocketIoMailProvider> = OnceLock::new();
        INSTANCE.get_or_init(|| SocketIoMailProvider::new(Channel::MjjCm, "mjj.cm"))
    }

    pub fn generate_email() -> Result<EmailInfo, String> {
        provider().generate_email()
    }

    pub fn get_emails(email: &str) -> Result<Vec<Email>, String> {
        provider().get_emails(email)
    }
}

pub mod linshi_co {
    use super::*;
    use std::sync::OnceLock;

    fn provider() -> &'static SocketIoMailProvider {
        static INSTANCE: OnceLock<SocketIoMailProvider> = OnceLock::new();
        INSTANCE.get_or_init(|| SocketIoMailProvider::new(Channel::LinshiCo, "linshi.co"))
    }

    pub fn generate_email() -> Result<EmailInfo, String> {
        provider().generate_email()
    }

    pub fn get_emails(email: &str) -> Result<Vec<Email>, String> {
        provider().get_emails(email)
    }
}
