# frozen_string_literal: true

module TempmailSdk
  module Providers
    # restmail-net 渠道 — https://restmail.net
    # Mozilla 开源项目，无需创建邮箱，ad-hoc 模式
    module RestmailNet
      CHANNEL = "restmail-net"
      BASE_URL = "https://restmail.net"
      HEADERS = {
        "Accept" => "application/json",
        "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36"
      }.freeze

      module_function

      def random_username
        chars = ("a".."z").to_a + ("0".."9").to_a
        Array.new(rand(10..12)) { chars.sample }.join
      end

      # 创建邮箱：随机 username 即可使用，无需请求服务端
      # @return [EmailInfo]
      def generate_email
        EmailInfo.new(channel: CHANNEL, email: "#{random_username}@restmail.net")
      end

      # @param email [String]
      # @return [Array<Email>]
      def get_emails(email)
        address = email.to_s.strip
        raise "restmail-net: 邮箱地址为空" if address.empty?

        username = address.split("@").first.to_s
        raise "restmail-net: 无法提取用户名" if username.empty?

        resp = Http.get("#{BASE_URL}/mail/#{username}", headers: HEADERS, timeout: 15)
        return [] if resp.status_code == 404

        resp.raise_for_status
        data = resp.json
        return [] unless data.is_a?(Array)

        data.select { |m| m.is_a?(Hash) }.map do |msg|
          from_addr = ""
          if msg["from"].is_a?(Array) && msg["from"][0].is_a?(Hash)
            from_addr = msg["from"][0]["address"].to_s
          end
          from_addr = msg.dig("headers", "from").to_s if from_addr.empty? && msg["headers"].is_a?(Hash)
          Normalize.normalize_email({
                                      "id" => msg["id"] || "", "from" => from_addr, "to" => address,
                                      "subject" => msg["subject"] || "", "text" => msg["text"] || "",
                                      "html" => msg["html"] || "", "date" => msg["receivedAt"] || "",
                                      "is_read" => false
                                    }, address)
        end
      end
    end
  end
end
