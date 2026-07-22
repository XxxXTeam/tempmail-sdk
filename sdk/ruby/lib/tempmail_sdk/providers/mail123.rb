# frozen_string_literal: true

module TempmailSdk
  module Providers
    # mail123.fr 渠道实现
    module Mail123
      CHANNEL = "mail123"
      API_BASE = "https://mail123.fr/api/v1"
      HEADERS = { "Accept" => "application/json", "User-Agent" => "Mozilla/5.0" }.freeze

      module_function

      def flatten(raw, recipient)
        out = raw.dup
        out["id"] = raw["id"].to_s
        out["to"] = (raw["to"] || recipient).to_s
        out["text"] = (raw["text"] || raw["preview"] || "").to_s
        out["html"] = (raw["html"] || "").to_s
        if raw.key?("is_unread") && (raw["is_unread"] == true || raw["is_unread"] == false)
          out["isRead"] = !raw["is_unread"]
        end
        out
      end

      # @return [EmailInfo]
      def generate_email
        resp = Http.get("#{API_BASE}/mailbox/new", headers: HEADERS, timeout: 15)
        resp.raise_for_status
        data = resp.json
        raise "mail123: invalid mailbox response" unless data.is_a?(Hash)

        email = data["address"].to_s.strip
        raise "mail123: invalid mailbox response" if email.empty? || !email.include?("@")

        expires_at = nil
        if data["expires_in_days"].is_a?(Numeric) && data["expires_in_days"] > 0
          expires_at = ((Time.now.to_f + data["expires_in_days"].to_f * 86_400) * 1000).to_i
        end

        EmailInfo.new(channel: CHANNEL, email: email, token: email, expires_at: expires_at)
      end

      def fetch_detail(address, message_id)
        resp = Http.get(
          "#{API_BASE}/mailbox/#{URI.encode_www_form_component(address)}/messages/#{URI.encode_www_form_component(message_id)}",
          headers: HEADERS, timeout: 15
        )
        return nil unless resp.ok?

        data = resp.json
        if data.is_a?(Hash) && data["message"].is_a?(Hash)
          return data["message"]
        end
        nil
      rescue StandardError
        nil
      end

      # @param email [String]
      # @return [Array<Email>]
      def get_emails(email)
        address = email.to_s.strip
        raise "mail123: empty email" if address.empty?

        resp = Http.get(
          "#{API_BASE}/mailbox/#{URI.encode_www_form_component(address)}/messages?limit=50",
          headers: HEADERS, timeout: 15
        )
        resp.raise_for_status
        data = resp.json
        rows = data.is_a?(Hash) ? (data["messages"] || []) : []
        return [] unless rows.is_a?(Array)

        rows.select { |r| r.is_a?(Hash) }.map do |item|
          message_id = (item["id"] || "").to_s.strip
          detail = message_id.empty? ? nil : fetch_detail(address, message_id)
          Normalize.normalize_email(flatten(detail || item, address), address)
        end
      end
    end
  end
end
