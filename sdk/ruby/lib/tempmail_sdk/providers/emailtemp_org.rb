# frozen_string_literal: true

require "json"
require "uri"

module TempmailSdk
  module Providers
    # emailtemp.org 渠道实现（与 tempmailten.com 共享 Laravel 模板）
    #
    # 流程：
    #   1. GET /en 获取 session cookie + CSRF meta token
    #   2. POST /messages（form: _token={csrf}&captcha=）获取邮箱地址和邮件列表
    #   3. GET /view/{id} 获取单封邮件 HTML 正文
    # token 存储 "cookie\ncsrf"
    module EmailtempOrg
      CHANNEL = "emailtemp-org"
      BASE_URL = "https://emailtemp.org"

      CSRF_RE = /<meta\s+name="csrf-token"\s+content="([^"]+)"/.freeze

      BROWSER_HEADERS = {
        "Accept" => "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8",
        "Accept-Language" => "en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7",
        "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " \
                       "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36"
      }.freeze

      AJAX_HEADERS = {
        "Accept" => "application/json, text/javascript, */*; q=0.01",
        "Accept-Language" => "en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7",
        "Content-Type" => "application/x-www-form-urlencoded",
        "Referer" => "#{BASE_URL}/en",
        "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " \
                       "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36",
        "X-Requested-With" => "XMLHttpRequest"
      }.freeze

      module_function

      # 合并 Set-Cookie 为 Cookie 字符串
      def merge_cookies(resp)
        parts = []
        resp.set_cookies.each do |line|
          first = line.split(";", 2).first.to_s.strip
          parts << first unless first.empty?
        end
        parts.join("; ")
      end

      # POST /messages 携带 CSRF 与 Cookie
      def post_messages(cookie, csrf)
        form = "_token=#{URI.encode_www_form_component(csrf)}&captcha="
        headers = AJAX_HEADERS.merge("X-CSRF-TOKEN" => csrf)
        headers = headers.merge("Cookie" => cookie) unless cookie.to_s.empty?
        Http.post("#{BASE_URL}/messages", headers: headers, body: form)
      end

      # 创建临时邮箱
      # @return [EmailInfo]
      def generate_email
        r1 = Http.get("#{BASE_URL}/en", headers: BROWSER_HEADERS)
        r1.raise_for_status
        cookie = merge_cookies(r1)
        m = r1.body.match(CSRF_RE)
        raise "emailtemp-org: 未能从首页提取 CSRF token" unless m

        csrf = m[1]

        r2 = post_messages(cookie, csrf)
        r2.raise_for_status
        new_cookie = merge_cookies(r2)
        cookie = new_cookie unless new_cookie.empty?

        data = JSON.parse(r2.body)
        email = data["mailbox"].to_s.strip
        raise "emailtemp-org: 邮箱地址无效: #{email.inspect}" if email.empty? || !email.include?("@")

        token = "#{cookie}\n#{csrf}"
        EmailInfo.new(channel: CHANNEL, email: email, token: token)
      end

      # 获取邮件列表
      # @param email [String]
      # @param token [String] "cookie\ncsrf"
      # @return [Array<Email>]
      def get_emails(email, token)
        raise "emailtemp-org: 邮箱地址为空" if email.to_s.strip.empty?

        cookie, csrf = token.to_s.split("\n", 2)
        csrf = cookie if csrf.nil? # 兼容旧 token 仅存 CSRF 的情形
        cookie = "" if csrf.nil? || csrf == cookie
        raise "emailtemp-org: 缺少 CSRF token" if csrf.to_s.empty?

        resp = post_messages(cookie, csrf)
        resp.raise_for_status
        data = JSON.parse(resp.body)
        messages = data["messages"]
        return [] unless messages.is_a?(Array) && !messages.empty?

        messages.map do |msg|
          id = msg["id"].to_s
          from_addr = msg["from_email"].to_s
          if !msg["from"].to_s.empty? && msg["from"] != from_addr
            from_addr = "#{msg['from']} <#{msg['from_email']}>"
          end
          html_body = fetch_view(cookie, id)
          Normalize.normalize_email({
            "id" => id,
            "from" => from_addr,
            "to" => email,
            "subject" => msg["subject"].to_s,
            "html" => html_body,
            "isRead" => msg["is_seen"] == true
          }, email)
        end
      end

      # 获取单封邮件 HTML 正文
      def fetch_view(cookie, id)
        return "" if id.to_s.empty?

        headers = BROWSER_HEADERS.merge("Referer" => "#{BASE_URL}/en")
        headers = headers.merge("Cookie" => cookie) unless cookie.to_s.empty?
        resp = Http.get("#{BASE_URL}/view/#{URI.encode_www_form_component(id)}", headers: headers)
        resp.ok? ? resp.body : ""
      rescue StandardError
        ""
      end
    end
  end
end
