# frozen_string_literal: true

module TempmailSdk
  module Providers
    # dropmail.click 渠道实现
    # 独立后端临时邮箱，免注册、无鉴权。Token 即邮箱地址本身。
    module DropmailClick
      CHANNEL = "dropmail-click"
      BASE_URL = "https://dropmail.click"
      DEFAULT_UA = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " \
                   "(KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36"

      module_function

      def get_ua
        cfg = Config.get_config
        (cfg.headers || {})["User-Agent"] || DEFAULT_UA
      end

      def first_val(msg, *keys)
        keys.each do |key|
          val = msg[key]
          return val.to_s if !val.nil? && val != ""
        end
        ""
      end

      # @return [EmailInfo]
      def generate_email
        resp = Http.post(
          "#{BASE_URL}/api/v1/public/mailbox",
          headers: { "User-Agent" => get_ua, "Accept" => "application/json" }
        )
        resp.raise_for_status
        data = resp.json
        raise "dropmail-click: invalid response" unless data.is_a?(Hash)

        address = data["address"].to_s
        raise "dropmail-click: missing address" if address.empty?

        EmailInfo.new(
          channel: CHANNEL,
          email: address,
          token: address,
          expires_at: data["expires_at"]
        )
      end

      # @param email [String]
      # @return [Array<Email>]
      def get_emails(email)
        addr = email.to_s.strip
        raise "dropmail-click: missing email" if addr.empty?

        resp = Http.get(
          "#{BASE_URL}/api/v1/public/mailbox/#{URI.encode_www_form_component(addr)}",
          headers: { "User-Agent" => get_ua, "Accept" => "application/json" }
        )
        resp.raise_for_status
        data = resp.json
        return [] unless data.is_a?(Hash)

        messages = data["messages"]
        return [] unless messages.is_a?(Array)

        messages.select { |m| m.is_a?(Hash) }.map do |msg|
          row = {
            "id" => first_val(msg, "id"),
            "from" => first_val(msg, "from"),
            "to" => first_val(msg, "address").empty? ? addr : first_val(msg, "address"),
            "subject" => first_val(msg, "subject"),
            "text" => first_val(msg, "text"),
            "html" => first_val(msg, "html"),
            "date" => first_val(msg, "received_at", "date")
          }
          Normalize.normalize_email(row, addr)
        end
      end
    end
  end
end
