# frozen_string_literal: true

require "uri"

module TempmailSdk
  module Providers
    # temp-mail.org 渠道 — https://temp-mail.org
    # API Base: https://web2.temp-mail.org
    module TempMailOrg
      CHANNEL = "temp-mail-org"
      BASE_URL = "https://web2.temp-mail.org"

      HEADERS = {
        "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36",
        "Origin" => "https://temp-mail.org",
        "Referer" => "https://temp-mail.org/"
      }.freeze

      module_function

      def auth_headers(token)
        HEADERS.merge("Authorization" => "Bearer #{token}")
      end

      # 创建临时邮箱
      # @return [EmailInfo]
      def generate_email
        resp = Http.post("#{BASE_URL}/mailbox", headers: HEADERS, timeout: 15)
        resp.raise_for_status
        data = resp.json
        raise "temp-mail-org: 响应格式无效" unless data.is_a?(Hash)

        token = data["token"] || ""
        mailbox = data["mailbox"] || ""
        raise "temp-mail-org: 创建邮箱失败" if token.empty? || mailbox.empty?

        EmailInfo.new(channel: CHANNEL, email: mailbox, token: token)
      end

      # 获取邮件列表
      # @param token [String] JWT 令牌
      # @param email [String]
      # @return [Array<Email>]
      def get_emails(token, email)
        raise "temp-mail-org: token 不能为空" if token.nil? || token.empty?

        addr = (email || "").strip
        raise "temp-mail-org: 邮箱地址不能为空" if addr.empty?

        resp = Http.get("#{BASE_URL}/messages", headers: auth_headers(token), timeout: 15)
        resp.raise_for_status
        data = resp.json
        return [] unless data.is_a?(Hash)

        messages = data["messages"]
        return [] unless messages.is_a?(Array) && !messages.empty?

        messages.filter_map do |msg|
          next unless msg.is_a?(Hash)

          msg_id = msg["_id"] || ""
          next if msg_id.empty?

          detail = fetch_message_detail(token, msg_id)
          raw = {
            "id" => msg_id,
            "from" => detail["from"] || msg["from"] || "",
            "to" => addr,
            "subject" => detail["subject"] || msg["subject"] || "",
            "text" => detail["bodyPreview"] || msg["bodyPreview"] || "",
            "html" => detail["bodyHtml"] || "",
            "date" => detail["createdAt"] || msg["receivedAt"].to_s
          }
          Normalize.normalize_email(raw, addr)
        end
      end

      def fetch_message_detail(token, msg_id)
        resp = Http.get("#{BASE_URL}/messages/#{msg_id}", headers: auth_headers(token), timeout: 15)
        return {} unless resp.ok?

        data = resp.json
        data.is_a?(Hash) ? data : {}
      rescue StandardError
        {}
      end
    end
  end
end
