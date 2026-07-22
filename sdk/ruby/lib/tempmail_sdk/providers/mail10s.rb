# frozen_string_literal: true

module TempmailSdk
  module Providers
    # mail10s.com 渠道实现
    module Mail10s
      CHANNEL = "mail10s"
      BASE_URL = "https://mail10s.com"
      DOMAIN = "mail10s.com"
      HEADERS = { "Accept" => "application/json", "User-Agent" => "Mozilla/5.0" }.freeze

      module_function

      def random_local
        chars = ("a".."z").to_a + ("0".."9").to_a
        "sdk" + Array.new(16) { chars.sample }.join
      end

      def flatten(raw, recipient)
        {
          "id" => raw["id"].to_s,
          "from" => (raw["sender"] || "").to_s,
          "to" => recipient,
          "subject" => (raw["subject"] || "").to_s,
          "text" => (raw["body_text"] || "").to_s,
          "html" => (raw["body_html"] || "").to_s,
          "date" => (raw["received_at"] || "").to_s,
          "attachments" => raw["attachments"].is_a?(Array) ? raw["attachments"] : []
        }
      end

      # @return [EmailInfo]
      def generate_email
        email = "#{random_local}@#{DOMAIN}"
        EmailInfo.new(channel: CHANNEL, email: email, token: email)
      end

      # @param email [String]
      # @return [Array<Email>]
      def get_emails(email)
        address = email.to_s.strip
        raise "mail10s: empty email" if address.empty?

        resp = Http.get(
          "#{BASE_URL}/api/emails/#{URI.encode_www_form_component(address)}/inbox",
          headers: HEADERS, timeout: 15
        )
        resp.raise_for_status
        data = resp.json
        messages = if data.is_a?(Hash) && data["data"].is_a?(Hash)
                     data["data"]["messages"]
                   end
        return [] unless messages.is_a?(Array)

        messages.select { |r| r.is_a?(Hash) }.map { |row| Normalize.normalize_email(flatten(row, address), address) }
      end
    end
  end
end
