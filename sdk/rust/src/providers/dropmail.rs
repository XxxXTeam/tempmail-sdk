/*!
 * dropmail.me 渠道实现 (GraphQL)
 */

use serde_json::Value;
use crate::types::{Channel, EmailInfo, Email};
use crate::normalize::normalize_email;
use crate::config::http_client;

const BASE_URL: &str = "https://dropmail.me/api/graphql/MY_TOKEN";

fn graphql_request(query: &str, variables: Option<&Value>) -> Result<Value, String> {
    let mut params = vec![("query", query.to_string())];
    if let Some(vars) = variables {
        params.push(("variables", vars.to_string()));
    }

    let resp = http_client()
        .post(BASE_URL)
        .form(&params)
        .send().map_err(|e| format!("dropmail request failed: {}", e))?;

    if !resp.status().is_success() {
        return Err(format!("dropmail request failed: {}", resp.status()));
    }

    let data: Value = resp.json().map_err(|e| format!("parse failed: {}", e))?;
    if let Some(errors) = data["errors"].as_array() {
        if !errors.is_empty() {
            return Err(format!("GraphQL error: {}", errors[0]["message"].as_str().unwrap_or("unknown")));
        }
    }

    Ok(data["data"].clone())
}

pub fn generate_email() -> Result<EmailInfo, String> {
    let data = graphql_request(
        "mutation {introduceSession {id, expiresAt, addresses{id, address}}}",
        None,
    )?;

    let session = &data["introduceSession"];
    let addr = session["addresses"][0]["address"].as_str()
        .ok_or("Failed to generate email")?;

    Ok(EmailInfo {
        channel: Channel::Dropmail,
        email: addr.to_string(),
        token: session["id"].as_str().map(|s| s.to_string()),
        expires_at: session["expiresAt"].as_i64(),
        created_at: None,
    })
}

pub fn get_emails(token: &str, email: &str) -> Result<Vec<Email>, String> {
    let query = "query ($id: ID!) { session(id:$id) { mails { id, fromAddr, toAddr, receivedAt, text, headerSubject, html } } }";
    let vars = serde_json::json!({"id": token});
    let data = graphql_request(query, Some(&vars))?;

    let mails = data["session"]["mails"].as_array().cloned().unwrap_or_default();
    Ok(mails.iter().map(|m| {
        let flat = serde_json::json!({
            "id": m["id"], "from": m["fromAddr"], "to": m["toAddr"].as_str().unwrap_or(email),
            "subject": m["headerSubject"], "text": m["text"], "html": m["html"],
            "received_at": m["receivedAt"],
        });
        normalize_email(&flat, email)
    }).collect())
}
