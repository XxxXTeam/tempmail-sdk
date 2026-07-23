# frozen_string_literal: true

require "json"
require "uri"

module TempmailSdk
  module Providers
    # chatgpt.org.uk 渠道实现
    #
    # 流程：
    #   1. GET /api/domains/public 获取可用域名列表（过滤 is_active=1）
    #   2. 本地生成随机用户名，拼接邮箱地址
    #   3. POST /api/inbox-token {"email":"..."} 创建收件箱，返回 inbox JWT 和 gm_sid cookie
    #   4. GET /api/emails?email=... 携带 x-inbox-token 和 gm_sid cookie 获取邮件
    # token 格式: JSON {"gmSid":"...","inbox":"..."}
    module ChatgptOrgUk
      CHANNEL = "chatgpt-org-uk"
      API_BASE = "https://mail.chatgpt.org.uk/api"
      USERNAME_CHARS = ("a".."z").to_a + ("0".."9").to_a

      HEADERS = {
        "Accept" => "*/*",
        "DNT" => "1",
        "Origin" => "https://mail.chatgpt.org.uk",
        "Referer" => "https://mail.chatgpt.org.uk/zh/",
        "sec-fetch-dest" => "empty",
        "sec-fetch-mode" => "cors",
        "sec-fetch-site" => "same-origin",
        "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " \
                       "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36"
      }.freeze

      module_function

      # 生成随机用户名
      def random_username(length = 10)
        Array.new(length) { USERNAME_CHARS.sample }.join
      end

      # 获取可用域名列表
      def fetch_domains
        resp = Http.get("#{API_BASE}/domains/public", headers: HEADERS)
        resp.raise_for_status
        data = resp.json
        raise "chatgpt-org-uk: 获取域名失败" unless data["success"]

        domains = data.dig("data", "domains") || []
        active = domains.select { |d| d["is_active"] == 1 && !d["domain_name"].to_s.empty? }
                        .map { |d| d["domain_name"] }
        raise "chatgpt-org-uk: 无可用域名" if active.empty?

        active
      end

      # 创建收件箱，返回 [inbox_token, gm_sid]
      def create_inbox(email)
        resp = Http.post("#{API_BASE}/inbox-token",
                         headers: HEADERS.merge("Content-Type" => "application/json"),
                         json: { "email" => email })
        resp.raise_for_status
        gm_sid = resp.cookie_value("gm_sid").to_s
        data = resp.json
        raise "chatgpt-org-uk: inbox-token 响应缺少 token" \
          unless data["success"] && !data.dig("auth", "token").to_s.empty?

        [data.dig("auth", "token"), gm_sid]
      end

      # 创建临时邮箱
      # @return [EmailInfo]
      def generate_email
        domains = fetch_domains
        domain = domains.sample
        email = "#{random_username}@#{domain}"
        inbox, gm_sid = create_inbox(email)
        packed = JSON.generate("gmSid" => gm_sid, "inbox" => inbox)
        EmailInfo.new(channel: CHANNEL, email: email, token: packed)
      end

      # 获取邮件列表
      # @param email [String]
      # @param token [String] JSON {"gmSid":"...","inbox":"..."}
      # @return [Array<Email>]
      def get_emails(email, token)
        parsed = JSON.parse(token)
        gm_sid = parsed["gmSid"].to_s
        inbox = parsed["inbox"].to_s
        raise "chatgpt-org-uk: inbox token 缺失" if inbox.empty?

        # gm_sid 丢失时重新创建 session
        if gm_sid.empty?
          inbox, gm_sid = create_inbox(email)
        end

        fetch_emails_with(email, inbox, gm_sid)
      rescue JSON::ParserError
        raise "chatgpt-org-uk: 无效的 token 格式"
      end

      # 实际拉取邮件，401/403 时重建 session 重试一次
      def fetch_emails_with(email, inbox, gm_sid)
        encoded = URI.encode_www_form_component(email)
        resp = Http.get("#{API_BASE}/emails?email=#{encoded}",
                        headers: HEADERS.merge(
                          "Cookie" => "gm_sid=#{gm_sid}",
                          "x-inbox-token" => inbox
                        ))
        if [401, 403].include?(resp.status_code)
          new_inbox, new_sid = create_inbox(email)
          resp = Http.get("#{API_BASE}/emails?email=#{encoded}",
                          headers: HEADERS.merge(
                            "Cookie" => "gm_sid=#{new_sid}",
                            "x-inbox-token" => new_inbox
                          ))
        end
        resp.raise_for_status
        data = resp.json
        raise "chatgpt-org-uk: 获取邮件失败" unless data["success"]

        emails_raw = data.dig("data", "emails") || []
        emails_raw.filter_map do |raw|
          next unless raw.is_a?(Hash)

          Normalize.normalize_email(raw, email)
        end
      end
    end
  end
end
