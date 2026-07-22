# frozen_string_literal: true

require "time"

module TempmailSdk
  module Providers
    # MFFAC 渠道实现
    # API: https://www.mffac.com/api
    module Mffac
      CHANNEL = "mffac"
      BASE = "https://www.mffac.com/api"

      HEADERS = {
        "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
        "Content-Type" => "application/json",
        "Accept" => "*/*",
        "Origin" => "https://www.mffac.com",
        "Referer" => "https://www.mffac.com/"
      }.freeze

      GET_HEADERS = HEADERS.reject { |k, _| k == "Content-Type" }.freeze

      module_function

      # 将 receivedAt 秒级时间戳转为 ISO8601
      def received_at_to_iso(value)
        seconds = Float(value)
        return "" if seconds <= 0

        Time.at(seconds).utc.strftime("%Y-%m-%dT%H:%M:%SZ")
      rescue TypeError, ArgumentError
        ""
      end

      # 扁平化邮件字段
      def flatten_email(raw, recipient)
        {
          "id" => raw["id"] || "",
          "from" => raw["fromAddress"] || "",
          "to" => raw["toAddress"] || recipient,
          "subject" => raw["subject"] || "",
          "text" => raw["textContent"] || "",
          "html" => raw["htmlContent"] || "",
          "date" => received_at_to_iso(raw["receivedAt"]),
          "isRead" => raw["isRead"] || false,
          "attachments" => []
        }
      end

      # 获取单封邮件详情
      def fetch_detail(message_id)
        r = Http.get("#{BASE}/emails/#{URI.encode_www_form_component(message_id)}", headers: GET_HEADERS)
        return nil unless r.ok?

        data = r.json
        return nil unless data.is_a?(Hash) && data["success"]

        email = data["email"]
        email.is_a?(Hash) ? email : nil
      rescue StandardError
        nil
      end

      # 创建临时邮箱
      # @return [EmailInfo]
      def generate_email
        r = Http.post("#{BASE}/mailboxes", headers: HEADERS, json: { "expiresInHours" => 24 })
        r.raise_for_status
        data = r.json
        raise "mffac: 创建失败" unless data.is_a?(Hash) && data["success"] && data["mailbox"]

        mb = data["mailbox"]
        addr = (mb["address"] || "").strip
        mid = (mb["id"] || "").strip
        raise "mffac: 无效的邮箱数据" if addr.empty? || mid.empty?

        EmailInfo.new(channel: CHANNEL, email: "#{addr}@mffac.com", token: mid, created_at: mb["createdAt"])
      end

      # 获取邮件列表
      # @param email [String]
      # @param token [String]
      # @return [Array<Email>]
      def get_emails(email, token = "")
        local = email.to_s.split("@", 2).first.to_s.strip
        raise "mffac: 地址为空" if local.empty?

        r = Http.get("#{BASE}/mailboxes/#{local}/emails", headers: GET_HEADERS)
        r.raise_for_status
        data = r.json
        raise "mffac: 获取列表失败" unless data.is_a?(Hash) && data["success"]

        (data["emails"] || []).select { |raw| raw.is_a?(Hash) }.map do |raw|
          mid = raw["id"].to_s.strip
          detail = mid.empty? ? nil : fetch_detail(mid)
          Normalize.normalize_email(flatten_email(detail || raw, email), email)
        end
      end
    end
  end
end
