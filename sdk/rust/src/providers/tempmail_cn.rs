/*!
 * tempmail.cn：按 public 前端的 Socket.IO 事件协议工作
 * - `request shortid` -> `shortid`
 * - `set shortid` 持续订阅 `mail`
 */

use std::collections::HashMap;
use std::sync::{Arc, Mutex, OnceLock};
use std::thread;
use std::time::Duration;

use serde_json::{json, Value};
use tungstenite::protocol::Message;

use crate::config::get_current_ua;
use crate::normalize::normalize_email;
use crate::types::{Channel, Email, EmailInfo};

const DEFAULT_HOST: &str = "tempmail.cn";
const SOCKET_IO_VERSIONS: &[u8] = &[4, 3];

type WsSocket = tungstenite::WebSocket<tungstenite::stream::MaybeTlsStream<std::net::TcpStream>>;

#[derive(Default)]
struct TempmailCnBox {
    emails: Vec<Email>,
    seen: HashMap<String, ()>,
    started: bool,
}

fn registry() -> &'static Mutex<HashMap<String, Arc<Mutex<TempmailCnBox>>>> {
    static REG: OnceLock<Mutex<HashMap<String, Arc<Mutex<TempmailCnBox>>>>> = OnceLock::new();
    REG.get_or_init(|| Mutex::new(HashMap::new()))
}

fn get_box(email: &str) -> Arc<Mutex<TempmailCnBox>> {
    let key = email.trim().to_lowercase();
    let mut g = registry().lock().unwrap();
    g.entry(key)
        .or_insert_with(|| Arc::new(Mutex::new(TempmailCnBox::default())))
        .clone()
}

fn normalize_host(domain: Option<&str>) -> String {
    let raw = domain.unwrap_or("").trim();
    if raw.is_empty() {
        return DEFAULT_HOST.to_string();
    }

    let mut host = raw.to_string();
    if host.starts_with("http://") || host.starts_with("https://") {
        if let Some(after) = host.split_once("://").map(|(_, rest)| rest) {
            host = after.to_string();
        }
    }
    if let Some((_, after_auth)) = host.rsplit_once('@') {
        host = after_auth.to_string();
    }
    if let Some((before_path, _)) = host.split_once('/') {
        host = before_path.to_string();
    }
    host = host.trim_matches('.').to_string();
    if host.is_empty() {
        DEFAULT_HOST.to_string()
    } else {
        host
    }
}

fn split_email(email: &str) -> Result<(String, String), String> {
    let trimmed = email.trim();
    let Some((local, host)) = trimmed.split_once('@') else {
        return Err("tempmail-cn: invalid email address".into());
    };
    if local.is_empty() || host.is_empty() {
        return Err("tempmail-cn: invalid email address".into());
    }
    Ok((local.to_string(), normalize_host(Some(host))))
}

fn socket_request(host: &str, version: u8) -> Result<tungstenite::http::Request<()>, String> {
    let ua = get_current_ua();
    let url = format!("wss://{host}/socket.io/?EIO={version}&transport=websocket");
    tungstenite::http::Request::builder()
        .uri(url)
        .header("Host", host)
        .header("Origin", format!("https://{host}"))
        .header("Referer", format!("https://{host}/"))
        .header("User-Agent", ua)
        .header("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6")
        .body(())
        .map_err(|e| e.to_string())
}

fn socket_text(msg: Message) -> Option<String> {
    match msg {
        Message::Text(t) => Some(t.to_string()),
        Message::Binary(b) => String::from_utf8(b.to_vec()).ok(),
        Message::Close(_) => None,
        Message::Ping(_) | Message::Pong(_) | Message::Frame(_) => Some(String::new()),
    }
}

fn connect_socket(host: &str) -> Result<WsSocket, String> {
    let mut last_err = String::from("tempmail-cn: websocket handshake failed");
    for version in SOCKET_IO_VERSIONS {
        let request = match socket_request(host, *version) {
            Ok(r) => r,
            Err(e) => {
                last_err = e;
                continue;
            }
        };
        match tungstenite::connect(request) {
            Ok((mut socket, _)) => {
                let first = match socket.read() {
                    Ok(msg) => match socket_text(msg) {
                        Some(text) => text,
                        None => {
                            last_err = "tempmail-cn: websocket closed during handshake".to_string();
                            continue;
                        }
                    },
                    Err(e) => {
                        last_err = format!("tempmail-cn open packet: {e}");
                        continue;
                    }
                };
                if !first.starts_with('0') {
                    last_err = format!("tempmail-cn: unexpected open packet for EIO={version}: {first}");
                    continue;
                }
                if let Err(e) = socket.send(Message::Text("40".into())) {
                    last_err = format!("tempmail-cn connect namespace: {e}");
                    continue;
                }
                return Ok(socket);
            }
            Err(e) => {
                last_err = e.to_string();
            }
        }
    }
    Err(last_err)
}

fn send_event(socket: &mut WsSocket, event: &str, payload: Value) -> Result<(), String> {
    let packet = format!("42{}", json!([event, payload]));
    socket
        .send(Message::Text(packet.into()))
        .map_err(|e| e.to_string())
}

fn parse_event(packet: &str) -> Option<(String, Value)> {
    if !packet.starts_with("42") {
        return None;
    }
    let decoded: Vec<Value> = serde_json::from_str(&packet[2..]).ok()?;
    let event = decoded.first()?.as_str()?.to_string();
    let payload = decoded.get(1).cloned().unwrap_or(Value::Null);
    Some((event, payload))
}

fn value_string(v: Option<&Value>) -> String {
    let Some(v) = v else {
        return String::new();
    };
    match v {
        Value::String(s) => s.trim().to_string(),
        Value::Null => String::new(),
        _ => v.to_string().trim().to_string(),
    }
}

fn stable_message_id(raw: &Value, headers: &Value, recipient: &str) -> String {
    let candidates = [
        raw.get("id"),
        raw.get("messageId"),
        headers.get("message-id"),
        headers.get("messageId"),
    ];
    for candidate in candidates {
        let id = value_string(candidate);
        if !id.is_empty() {
            return id;
        }
    }
    format!(
        "{}\n{}\n{}\n{}",
        value_string(headers.get("from")),
        value_string(headers.get("subject")),
        value_string(headers.get("date")),
        recipient,
    )
}

fn flatten_mail(raw: &Value, recipient: &str) -> Value {
    let headers = raw.get("headers").unwrap_or(&Value::Null);
    json!({
        "id": stable_message_id(raw, headers, recipient),
        "from": value_string(headers.get("from")),
        "to": recipient,
        "subject": value_string(headers.get("subject")),
        "text": value_string(raw.get("text")),
        "html": value_string(raw.get("html")),
        "date": value_string(headers.get("date")),
        "isRead": false,
        "attachments": raw.get("attachments").cloned().unwrap_or(Value::Array(vec![])),
    })
}

fn request_shortid(host: &str) -> Result<String, String> {
    let mut socket = connect_socket(host)?;
    send_event(&mut socket, "request shortid", Value::Bool(true))?;

    loop {
        let packet = socket
            .read()
            .map_err(|e| e.to_string())
            .and_then(|msg| socket_text(msg).ok_or_else(|| "tempmail-cn: websocket closed before shortid".to_string()))?;
        if packet.is_empty() {
            continue;
        }
        if packet == "2" {
            socket
                .send(Message::Text("3".into()))
                .map_err(|e| e.to_string())?;
            continue;
        }
        let Some((event, payload)) = parse_event(&packet) else {
            continue;
        };
        if event == "shortid" {
            if let Some(id) = payload.as_str() {
                if !id.trim().is_empty() {
                    return Ok(id.trim().to_string());
                }
            }
        }
    }
}

fn ws_loop(email: String, local: String, host: String, arc: Arc<Mutex<TempmailCnBox>>) {
    let result = (|| -> Result<(), String> {
        let mut socket = connect_socket(&host)?;
        send_event(&mut socket, "set shortid", Value::String(local))?;

        loop {
            let packet = match socket.read() {
                Ok(msg) => match socket_text(msg) {
                    Some(t) => t,
                    None => break,
                },
                Err(e) => return Err(e.to_string()),
            };
            if packet.is_empty() {
                continue;
            }
            if packet == "2" {
                let _ = socket.send(Message::Text("3".into()));
                continue;
            }
            let Some((event, payload)) = parse_event(&packet) else {
                continue;
            };
            if event != "mail" {
                continue;
            }
            let em = normalize_email(&flatten_mail(&payload, &email), &email);
            if em.id.is_empty() {
                continue;
            }
            let mut inner = arc.lock().map_err(|e| e.to_string())?;
            if inner.seen.contains_key(&em.id) {
                continue;
            }
            inner.seen.insert(em.id.clone(), ());
            inner.emails.push(em);
        }

        Ok(())
    })();

    if let Err(e) = result {
        log::debug!("tempmail-cn ws loop end: {}", e);
    }
    if let Ok(mut inner) = arc.lock() {
        inner.started = false;
    }
}

fn ensure_ws(email: &str) -> Result<(), String> {
    let (local, host) = split_email(email)?;
    let arc = get_box(email);
    let need_start = {
        let mut inner = arc.lock().map_err(|e| e.to_string())?;
        if inner.started {
            false
        } else {
            inner.started = true;
            true
        }
    };

    if need_start {
        let email_owned = email.to_string();
        let arc_cloned = arc.clone();
        thread::spawn(move || ws_loop(email_owned, local, host, arc_cloned));
        thread::sleep(Duration::from_millis(80));
    }

    Ok(())
}

pub fn generate_email(domain: Option<&str>) -> Result<EmailInfo, String> {
    let host = normalize_host(domain);
    let shortid = request_shortid(&host)?;
    Ok(EmailInfo {
        channel: Channel::TempmailCn,
        email: format!("{}@{}", shortid, host),
        token: None,
        expires_at: None,
        created_at: None,
    })
}

pub fn get_emails(email: &str) -> Result<Vec<Email>, String> {
    ensure_ws(email)?;
    let arc = get_box(email);
    let inner = arc.lock().map_err(|e| e.to_string())?;
    Ok(inner.emails.clone())
}
