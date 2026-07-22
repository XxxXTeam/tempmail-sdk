using System;
using System.Collections.Generic;
using System.Net;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>DevMail UK 渠道（devmail.uk）。GET JSON API，无认证。</summary>
public static class DevMailUk
{
    private const string ApiBase = "https://www.devmail.uk/api";
    private static readonly Dictionary<string, string> Headers = new() { ["Accept"] = "application/json" };

    private static string MailboxFromEmail(string email)
    {
        var address = (email ?? "").Trim();
        if (address.Length == 0) return "";
        return address.Contains('@') ? address.Split('@', 2)[0].Trim() : address;
    }

    /// <summary>创建临时邮箱</summary>
    public static EmailInfo Generate()
    {
        var resp = Http.Get($"{ApiBase}/new", Headers);
        resp.EnsureSuccess();
        var data = Json.Parse(resp.Body) as JsonObject;
        var email = (data?["email"]?.GetValue<string>() ?? "").Trim();
        if (email.Length == 0 && data is not null)
        {
            var mailbox = (data["mailbox"]?.GetValue<string>() ?? "").Trim();
            if (mailbox.Length > 0) email = $"{mailbox}@devmail.uk";
        }
        if (data is null || email.Length == 0 || !email.Contains('@'))
            throw new Exception("devmail-uk: invalid new email response");
        return new EmailInfo("devmail-uk", email);
    }

    /// <summary>获取邮件列表</summary>
    public static List<Email> GetEmails(string email)
    {
        var mailbox = MailboxFromEmail(email);
        if (mailbox.Length == 0) throw new Exception("devmail-uk: empty email");

        var resp = Http.Get($"{ApiBase}/inbox/{WebUtility.UrlEncode(mailbox)}?detail=true", Headers);
        resp.EnsureSuccess();
        var data = Json.Parse(resp.Body) as JsonObject;
        var result = new List<Email>();
        if (data is null) return result;
        if (data["emails"] is not JsonArray rows) return result;
        foreach (var item in rows)
            if (item is JsonObject) result.Add(Normalize.NormalizeEmail(Json.ToDict(item), email ?? ""));
        return result;
    }
}
