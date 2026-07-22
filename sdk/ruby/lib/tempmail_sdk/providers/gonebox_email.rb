# frozen_string_literal: true

module TempmailSdk
  module Providers
    # gonebox.email 渠道实现
    # 一次性临时邮箱服务，无需认证
    module GoneboxEmail
      CHANNEL = "gonebox-email"
      BASE_URL = "https://api.gonebox.email/api/v1"
      HEADERS = {
        "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " \
                        "(KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36",
        "Accept" => "application/json",
        "Content-Type" => "application/json"
      }.freeze

      module_function

      # @return [EmailInfo]
      def generate_email
        resp = Http.post(
          "#{BASE_URL}/inboxes",
          headers: HEADERS,
          json: { "domain" => "gonebox.email" },
          timeout: 15
        )
        resp.raise_for_status
        body = resp.json
        raise "gonebox-email: create failed" unless body.is_a?(Hash) && body["success"]

        data = body["data"]
        raise "gonebox-email: invalid data" unless data.is_a?(Hash)

        address = data["address"].to_s
        raise "gonebox-email: missing address" if address.empty?

        # expiresAt 为秒级时间戳，转为毫秒
        expires_at = nil
        raw_expires = data["expiresAt"]
        if raw_expires
          begin
            expires_at = raw_expires.to_i * 1000
          rescue StandardError
            # 忽略
          end
        end

        EmailInfo.new(channel: CHANNEL, email: address, token: "", expires_at: expires_at)
      end

      # @param token [String]
      # @param email [String]
      # @return [Array<Email>]
      def get_emails(token, email)
        address = email.to_s.strip
        raise "gonebox-email: missing email" if address.empty?

        headers = HEADERS.reject { |k, _| k == "Content-Type" }
        resp = Http.get("#{BASE_URL}/inboxes/#{address}/messages", headers: headers, timeout: 15)
        return [] if resp.status_code == 404

        resp.raise_for_status
        body = resp.json
        return [] unless body.is_a?(Hash) && body["success"]

        data = body["data"]
        return [] unless data.is_a?(Hash)

        messages = data["messages"]
        return [] unless messages.is_a?(Array) && !messages.empty?

        messages.select { |m| m.is_a?(Hash) }.map do |msg|
          raw = {
            "id" => msg["id"].to_s,
            "from" => (msg["from"] || msg["sender"]).to_s,
            "to" => address,
            "subject" => msg["subject"].to_s,
            "text" => (msg["text"] || msg["body_plain"]).to_s,
            "html" => (msg["html"] || msg["body_html"]).to_s,
            "date" => (msg["date"] || msg["received_at"]).to_s,
            "is_read" => false,
            "attachments" => []
          }
          Normalize.normalize_email(raw, address)
        end
      end
    end
  end
end
