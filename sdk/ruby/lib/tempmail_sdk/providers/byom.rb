# frozen_string_literal: true

module TempmailSdk
  module Providers
    # byom.de 渠道实现
    # 纯 GET 无认证，直接生成随机用户名
    module Byom
      CHANNEL = "byom"
      DOMAIN = "byom.de"
      BASE = "https://api.byom.de"
      HEADERS = {
        "Accept" => "application/json, text/plain, */*",
        "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " \
                        "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"
      }.freeze

      module_function

      def random_username(length = 10)
        chars = ("a".."z").to_a + ("0".."9").to_a
        Array.new(length) { chars.sample }.join
      end

      # @return [EmailInfo]
      def generate_email
        username = random_username
        EmailInfo.new(channel: CHANNEL, email: "#{username}@#{DOMAIN}")
      end

      # @param email [String]
      # @return [Array<Email>]
      def get_emails(email)
        e = email.to_s.strip
        raise "byom: empty email" if e.empty?

        username = e.split("@").first.to_s
        raise "byom: invalid email format" if username.empty?

        resp = Http.get("#{BASE}/mails/#{username}", headers: HEADERS, timeout: 15)
        resp.raise_for_status
        data = resp.json
        return [] unless data.is_a?(Array)

        data.select { |r| r.is_a?(Hash) }.map { |raw| Normalize.normalize_email(raw, e) }
      end
    end
  end
end
