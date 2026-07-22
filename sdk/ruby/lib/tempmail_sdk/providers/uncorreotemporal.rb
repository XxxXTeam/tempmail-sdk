# frozen_string_literal: true

require "uri"

module TempmailSdk
  module Providers
    # UnCorreoTemporal 渠道 — https://uncorreotemporal.com
    # POST /api/v1/mailboxes 创建邮箱，GET /api/v1/mailboxes/{email}/messages 获取邮件
    module Uncorreotemporal
      CHANNEL = "uncorreotemporal"
      API_BASE = "https://uncorreotemporal.com/api/v1"
      HEADERS = {
        "Accept" => "application/json",
        "Origin" => "https://uncorreotemporal.com",
        "Referer" => "https://uncorreotemporal.com/",
        "User-Agent" => "Mozilla/5.0"
      }.freeze

      module_function

      def flatten(raw, recipient)
        {
          "id" => raw["id"] || "",
          "from" => raw["from_address"] || "",
          "to" => raw["to_address"] || recipient,
          "subject" => raw["subject"] || "",
          "text" => raw["body_text"] || "",
          "html" => raw["body_html"] || "",
          "date" => raw["received_at"] || "",
          "isRead" => !!raw["is_read"],
          "attachments" => raw["attachments"].is_a?(Array) ? raw["attachments"] : []
        }
      end

      # @return [EmailInfo]
      def generate_email
        resp = Http.post("#{API_BASE}/mailboxes",
                         headers: HEADERS.merge("Content-Type" => "application/json"), timeout: 15)
        resp.raise_for_status
        data = resp.json
        raise "uncorreotemporal: 创建邮箱响应无效" unless data.is_a?(Hash)

        email = (data["address"] || "").strip
        token = (data["session_token"] || "").strip
        raise "uncorreotemporal: 创建邮箱响应无效" if email.empty? || !email.include?("@") || token.empty?

        EmailInfo.new(channel: CHANNEL, email: email, token: token, expires_at: data["expires_at"])
      end

      # @param token [String] session_token
      # @param email [String]
      # @return [Array<Email>]
      def get_emails(token, email)
        session_token = (token || "").strip
        address = (email || "").strip
        raise "uncorreotemporal: session_token 为空" if session_token.empty?
        raise "uncorreotemporal: 邮箱地址为空" if address.empty?

        resp = Http.get("#{API_BASE}/mailboxes/#{URI.encode_www_form_component(address)}/messages?limit=50",
                        headers: HEADERS.merge("X-Session-Token" => session_token), timeout: 15)
        resp.raise_for_status
        data = resp.json
        rows = data.is_a?(Array) ? data : []

        rows.filter_map do |row|
          next unless row.is_a?(Hash)

          message_id = (row["id"] || "").to_s.strip
          detail = nil
          unless message_id.empty?
            begin
              dr = Http.get("#{API_BASE}/mailboxes/#{URI.encode_www_form_component(address)}/messages/#{URI.encode_www_form_component(message_id)}",
                            headers: HEADERS.merge("X-Session-Token" => session_token), timeout: 15)
              detail = dr.json if dr.ok? && dr.json.is_a?(Hash)
            rescue StandardError
              # 忽略
            end
          end
          Normalize.normalize_email(flatten(detail || row, address), address)
        end
      end
    end
  end
end
