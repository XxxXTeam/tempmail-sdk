# frozen_string_literal: true

require "json"
require "uri"

module TempmailSdk
  module Providers
    # TempMail Fish 渠道 — https://tempmail.fish
    # POST /emails/new-email 创建邮箱，GET /emails/emails 获取邮件
    module TempmailFish
      CHANNEL = "tempmail-fish"
      API_BASE = "https://api.tempmail.fish"
      HEADERS = {
        "Accept" => "application/json",
        "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36"
      }.freeze

      module_function

      # 将毫秒时间戳转换为 ISO8601 字符串
      def to_iso(timestamp)
        return "" if timestamp.nil?

        millis = Float(timestamp)
        Time.at(millis / 1000.0).utc.iso8601
      rescue ArgumentError, TypeError
        ""
      end

      # @return [EmailInfo]
      def generate_email
        resp = Http.post("#{API_BASE}/emails/new-email",
                         headers: HEADERS.merge("Content-Type" => "application/json"))
        resp.raise_for_status
        data = resp.json
        raise "tempmail-fish: 创建邮箱响应无效" unless data.is_a?(Hash)

        email = (data["email"] || "").strip
        auth_key = (data["authKey"] || "").strip
        raise "tempmail-fish: 创建邮箱响应无效" if email.empty? || !email.include?("@") || auth_key.empty?

        token = JSON.generate({ "email" => email, "authKey" => auth_key })
        EmailInfo.new(channel: CHANNEL, email: email, token: token)
      end

      # @param token [String] JSON 字符串
      # @param email [String]
      # @return [Array<Email>]
      def get_emails(token, email)
        raise "tempmail-fish: token 不能为空" if token.nil? || token.empty?

        parsed = JSON.parse(token)
        raise "tempmail-fish: token 格式无效" unless parsed.is_a?(Hash)

        address = (parsed["email"] || email || "").strip
        auth_key = (parsed["authKey"] || "").strip
        raise "tempmail-fish: token 缺少 email 或 authKey" if address.empty? || auth_key.empty?

        resp = Http.get("#{API_BASE}/emails/emails?emailAddress=#{URI.encode_www_form_component(address)}",
                        headers: HEADERS.merge("Authorization" => auth_key))
        resp.raise_for_status
        data = resp.json

        rows = if data.is_a?(Array)
                 data
               elsif data.is_a?(Hash) && data["emails"].is_a?(Array)
                 data["emails"]
               else
                 []
               end

        rows.filter_map do |row|
          next unless row.is_a?(Hash)

          raw = {
            "from" => (row["from"] || "").to_s,
            "to" => (row["to"] || "").to_s.empty? ? address : row["to"].to_s,
            "subject" => (row["subject"] || "").to_s,
            "text" => (row["textBody"] || "").to_s,
            "html" => (row["htmlBody"] || "").to_s,
            "date" => to_iso(row["timestamp"]),
            "attachments" => row["attachments"] || []
          }
          Normalize.normalize_email(raw, address)
        end
      end
    end
  end
end
