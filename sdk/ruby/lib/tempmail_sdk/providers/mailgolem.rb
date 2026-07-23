# frozen_string_literal: true

require "uri"

module TempmailSdk
  module Providers
    # mailgolem.com 渠道实现
    #
    # 流程：GET / 获取 session cookie + CSRF token → GET /random-email-address 创建邮箱
    #       GET / 重新获取 session + CSRF → POST /fetch-emails/{email} 获取邮件列表
    # token 存储 CSRF token 值（收信时重新建立 session）
    module Mailgolem
      CHANNEL = "mailgolem"
      BASE_URL = "https://mailgolem.com"

      BROWSER_HEADERS = {
        "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " \
                        "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
        "Accept" => "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif," \
                    "image/webp,image/apng,*/*;q=0.8",
        "Accept-Language" => "en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7"
      }.freeze

      CSRF_RE = /<input[^>]+name="_token"[^>]+id="token"[^>]+value="([^"]*)"/im

      module_function

      # 从首页 HTML 提取 CSRF token
      # @param html [String]
      # @return [String]
      def extract_csrf(html)
        m = html.match(CSRF_RE)
        m ? m[1].strip : ""
      end

      # 访问首页建立 session 并提取 CSRF token
      # @return [String] csrf token
      def fetch_csrf
        resp = Http.get(BASE_URL + "/", headers: BROWSER_HEADERS, timeout: 15)
        resp.raise_for_status
        csrf = extract_csrf(resp.body)
        raise "mailgolem: 未能从首页提取 CSRF token" if csrf.empty?

        csrf
      end

      # 创建 mailgolem.com 临时邮箱
      # @return [EmailInfo]
      def generate_email
        csrf = fetch_csrf
        resp = Http.get(
          BASE_URL + "/random-email-address",
          headers: BROWSER_HEADERS.merge("Referer" => BASE_URL + "/"),
          timeout: 15
        )
        resp.raise_for_status
        email = resp.body.to_s.strip
        raise "mailgolem: 获取到的邮箱地址无效" if email.empty? || !email.include?("@")

        EmailInfo.new(channel: CHANNEL, email: email, token: csrf)
      end

      # 获取 mailgolem.com 邮件列表
      # @param email [String]
      # @param token [String] 旧 CSRF token（收信时重新建立 session）
      # @return [Array<Email>]
      def get_emails(email, token)
        _ = token
        csrf = fetch_csrf
        form = URI.encode_www_form("_token" => csrf)
        fetch_url = "#{BASE_URL}/fetch-emails/#{URI.encode_uri_component(email)}"
        resp = Http.post(
          fetch_url,
          headers: BROWSER_HEADERS.merge(
            "Content-Type" => "application/x-www-form-urlencoded",
            "X-Requested-With" => "XMLHttpRequest",
            "Accept" => "application/json, text/plain, */*",
            "Referer" => BASE_URL + "/",
            "Origin" => BASE_URL
          ),
          body: form,
          timeout: 15
        )
        resp.raise_for_status
        items = resp.json
        return [] unless items.is_a?(Array)

        items.filter_map do |item|
          next unless item.is_a?(Hash)

          Normalize.normalize_email(item.merge("to" => email), email)
        end
      end
    end
  end
end
