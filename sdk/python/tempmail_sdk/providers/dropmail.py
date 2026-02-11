"""
dropmail.me 渠道实现
API: GraphQL https://dropmail.me/api/graphql/MY_TOKEN
"""

from .. import http as tm_http
from ..types import EmailInfo, Email
from ..normalize import normalize_email

CHANNEL = "dropmail"
BASE_URL = "https://dropmail.me/api/graphql/MY_TOKEN"

CREATE_SESSION_QUERY = "mutation {introduceSession {id, expiresAt, addresses{id, address}}}"

GET_MAILS_QUERY = """query ($id: ID!) {
  session(id:$id) {
    mails {
      id, rawSize, fromAddr, toAddr, receivedAt,
      text, headerFrom, headerSubject, html
    }
  }
}"""


def _graphql_request(query: str, variables: dict = None) -> dict:
    """执行 GraphQL 请求（application/x-www-form-urlencoded）"""
    data = {"query": query}
    if variables:
        import json
        data["variables"] = json.dumps(variables)

    resp = tm_http.post(BASE_URL, data=data)
    resp.raise_for_status()
    result = resp.json()

    if result.get("errors"):
        raise Exception(f"GraphQL error: {result['errors'][0].get('message', 'Unknown')}")

    return result.get("data", {})


def _flatten_message(mail: dict, recipient_email: str) -> dict:
    """将 dropmail 消息格式扁平化"""
    return {
        "id": mail.get("id", ""),
        "from": mail.get("fromAddr", ""),
        "to": mail.get("toAddr", recipient_email),
        "subject": mail.get("headerSubject", ""),
        "text": mail.get("text", ""),
        "html": mail.get("html", ""),
        "received_at": mail.get("receivedAt", ""),
        "attachments": [],
    }


def generate_email(**kwargs) -> EmailInfo:
    """创建临时邮箱"""
    data = _graphql_request(CREATE_SESSION_QUERY)
    session = data.get("introduceSession")

    if not session or not session.get("addresses"):
        raise Exception("Failed to generate email")

    return EmailInfo(
        channel=CHANNEL,
        email=session["addresses"][0]["address"],
        token=session["id"],
        expires_at=session.get("expiresAt"),
    )


def get_emails(token: str, email: str = "", **kwargs) -> list:
    """获取邮件列表"""
    data = _graphql_request(GET_MAILS_QUERY, {"id": token})
    mails = (data.get("session") or {}).get("mails") or []
    return [normalize_email(_flatten_message(m, email), email) for m in mails]
