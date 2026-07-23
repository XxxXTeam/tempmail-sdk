# frozen_string_literal: true

require "json"

module TempmailSdk
  module Providers
    # awamail.com 渠道实现
    #
    # 流程：
    #   1. POST /welcome/change_mailbox（空 body）不跟随重定向，从 Set-Cookie 提取 awamail_session
    #   2. GET /welcome/get_emails 携带 Cookie 获取邮件列表
    module Awamail
      CHANNEL = "awamail"
      BASE_URL = "https://awamail.com/welcome"

      HEADERS = {
        "Accept" => "application/json, text/javascript, */*; q=0.01",
        "Accept-Language" => "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
        "Cache-Control" => "no-cache",
        "DNT" => "1",
        "Origin" => "https://awamail.com",
        "Pragma" => "no-cache",
        "Referer" => "https://awamail.com/?lang=zh",
        "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " \
                       "(KHTML, like Gecko) Chrome/144.0.0.0 Safari/537.36 Edg/144.0.0.0",
        "X-Requested-With" => "XMLHttpRequest"
      }.freeze

      module_function

      # 创建临时邮箱
      # POST /welcome/change_mailbox，从 Set-Cookie 提取 awamail_session
      # @return [EmailInfo]
      def generate_email
        resp = Http.post("#{BASE_URL}/change_mailbox",
                         headers: HEADERS.merge("Content-Length" => "0"))
        # 不跟随重定向，直接从响应提取 cookie
        session_cookie = resp.cookie_value("awamail_session")
        raise "awamail: 未能提取 awamail_session cookie" if session_cookie.to_s.empty?

        cookie_str = "awamail_session=#{session_cookie}"

        data = begin
          resp.json
        rescue StandardError
          {}
        end

        email_addr = data.is_a?(Hash) ? data.dig("data", "email_address").to_s : ""
        if email_addr.empty?
          # 若 POST 响应无邮箱，尝试 GET get_emails 获取
          gr = Http.get("#{BASE_URL}/get_emails",
                        headers: HEADERS.merge("Cookie" => cookie_str))
          gr.raise_for_status
          gd = gr.json
          email_addr = gd.is_a?(Hash) ? gd.dig("data", "email_address").to_s : ""
        end

        raise "awamail: 未能获取邮箱地址" if email_addr.empty?

        info = EmailInfo.new(channel: CHANNEL, email: email_addr, token: cookie_str)
        if data.is_a?(Hash)
          info.expires_at = data.dig("data", "expired_at").to_s
          info.created_at = data.dig("data", "created_at").to_s
        end
        info
      end

      # 获取邮件列表
      # GET /welcome/get_emails，携带 Cookie
      # @param token [String] awamail_session cookie 字符串
      # @param email [String]
      # @return [Array<Email>]
      def get_emails(token, email)
        resp = Http.get("#{BASE_URL}/get_emails",
                        headers: HEADERS.merge(
                          "Cookie" => token,
                          "X-Requested-With" => "XMLHttpRequest"
                        ))
        resp.raise_for_status
        data = resp.json
        return [] unless data.is_a?(Hash) && data["success"]

        emails_raw = data.dig("data", "emails")
        return [] unless emails_raw.is_a?(Array)

        emails_raw.filter_map do |raw|
          next unless raw.is_a?(Hash)

          Normalize.normalize_email(raw, email)
        end
      end
    end
  end
end
