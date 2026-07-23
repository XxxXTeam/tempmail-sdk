# frozen_string_literal: true

require "json"
require "uri"

module TempmailSdk
  module Providers
    # expressinboxhub.com 渠道实现
    #
    # 流程：
    #   1. GET / 获取 CSRF meta token + session cookies
    #   2. POST /messages（form: _token={csrf}）创建邮箱并返回消息列表
    # token 格式："cookie\ncsrf"
    module Expressinboxhub
      CHANNEL = "expressinboxhub"
      BASE_URL = "https://expressinboxhub.com"

      CSRF_RE = /<meta\s+name="csrf-token"\s+content="([^"]+)"/.freeze

      BROWSER_HEADERS = {
        "Accept" => "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8",
        "Accept-Language" => "zh-CN,zh;q=0.9,en;q=0.8",
        "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " \
                       "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36"
      }.freeze

      AJAX_HEADERS = {
        "Accept" => "application/json, text/plain, */*",
        "Accept-Language" => "zh-CN,zh;q=0.9,en;q=0.8",
        "Content-Type" => "application/x-www-form-urlencoded; charset=UTF-8",
        "Origin" => BASE_URL,
        "Referer" => "#{BASE_URL}/",
        "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " \
                       "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36",
        "X-Requested-With" => "XMLHttpRequest"
      }.freeze

      module_function

      # 合并 Set-Cookie 为 Cookie 字符串
      def merge_cookies(existing, resp)
        parts = existing.to_s.empty? ? [] : [existing]
        resp.set_cookies.each do |line|
          first = line.split(";", 2).first.to_s.strip
          parts << first unless first.empty?
        end
        parts.join("; ")
      end

      # POST /messages 携带 CSRF 与 Cookie
      def post_messages(cookie, csrf)
        headers = AJAX_HEADERS.merge("X-CSRF-TOKEN" => csrf)
        headers = headers.merge("Cookie" => cookie) unless cookie.to_s.empty?
        Http.post("#{BASE_URL}/messages", headers: headers, body: "_token=#{URI.encode_www_form_component(csrf)}")
      end

      # 创建临时邮箱
      # @return [EmailInfo]
      def generate_email
        r1 = Http.get(BASE_URL, headers: BROWSER_HEADERS)
        r1.raise_for_status
        cookie = merge_cookies("", r1)
        m = r1.body.match(CSRF_RE)
        raise "expressinboxhub: 未找到 CSRF token" unless m

        csrf = m[1]

        r2 = post_messages(cookie, csrf)
        r2.raise_for_status
        cookie = merge_cookies(cookie, r2)
        data = JSON.parse(r2.body)
        email = data["mailbox"].to_s.strip
        raise "expressinboxhub: 响应中未包含邮箱地址" if email.empty?

        EmailInfo.new(channel: CHANNEL, email: email, token: "#{cookie}\n#{csrf}")
      end

      # 获取邮件列表
      # @param email [String]
      # @param token [String] "cookie\ncsrf"
      # @return [Array<Email>]
      def get_emails(email, token)
        cookie, csrf = token.to_s.split("\n", 2)
        raise "expressinboxhub: 无效的 session token" if cookie.to_s.empty? || csrf.to_s.empty?

        resp = post_messages(cookie, csrf)
        resp.raise_for_status
        data = JSON.parse(resp.body)
        messages = data["messages"]
        return [] unless messages.is_a?(Array) && !messages.empty?

        messages.each_with_index.map do |msg, idx|
          id = msg["id"].to_s
          id = idx.to_s if id.empty?
          from_addr = msg["from"].to_s
          from_addr = msg["sender"].to_s if from_addr.empty?
          to_addr = msg["to"].to_s
          to_addr = email if to_addr.empty?
          date = msg["receivedAt"].to_s
          date = msg["date"].to_s if date.empty?
          date = msg["created_at"].to_s if date.empty?
          text = msg["text"].to_s
          text = msg["body"].to_s if text.empty?
          Normalize.normalize_email({
            "id" => id,
            "from" => from_addr,
            "to" => to_addr,
            "subject" => msg["subject"].to_s,
            "text" => text,
            "html" => msg["content"].to_s,
            "date" => date,
            "isRead" => false
          }, email)
        end
      end
    end
  end
end
