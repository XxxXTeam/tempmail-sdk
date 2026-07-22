# frozen_string_literal: true

module TempmailSdk
  module Providers
    # 1SecMail 渠道 — https://tmaily.com
    module OneSecMail
      CHANNEL = "1sec-mail"
      BASE_URL = "https://tmaily.com/"
      HEADERS = { "Accept" => "application/json", "User-Agent" => "Mozilla/5.0" }.freeze

      module_function

      # 生成临时邮箱，从 Set-Cookie 提取会话 Cookie
      # @return [EmailInfo]
      def generate_email
        resp = Http.get("#{BASE_URL}generate", headers: HEADERS, timeout: 15)
        resp.raise_for_status
        cookie_val = resp.cookie_value("TMaily_sid")
        raise "1sec-mail: 未获取到会话 Cookie" if cookie_val.nil? || cookie_val.empty?

        cookie = "TMaily_sid=#{cookie_val}"
        data = resp.json
        email = data.is_a?(Hash) ? data["address"].to_s.strip : ""
        raise "1sec-mail: 无效的邮箱响应" unless email.include?("@")

        EmailInfo.new(channel: CHANNEL, email: email, token: cookie)
      end

      # @param email [String]
      # @param token [String] 会话 Cookie
      # @return [Array<Email>]
      def get_emails(email, token)
        address = email.to_s.strip
        raise "1sec-mail: 邮箱地址为空" if address.empty?

        resp = Http.get("#{BASE_URL}emails?address=#{address}",
                        headers: HEADERS.merge("Cookie" => token.to_s), timeout: 15)
        resp.raise_for_status
        rows = resp.json
        raise "1sec-mail: 无效的邮件列表响应" unless rows.is_a?(Array)

        rows.select { |r| r.is_a?(Hash) }.map { |item| Normalize.normalize_email(item, address) }
      end
    end
  end
end
