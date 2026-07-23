# frozen_string_literal: true

require "base64"
require "json"

module TempmailSdk
  module Providers
    # email10min.com 渠道实现
    #
    # 流程：
    #   1. GET /zh 获取 CSRF meta token + session cookies + HTML 中的邮箱地址
    #   2. POST /messages?<ts> 携带 Cookie + _token 表单 获取邮件列表
    # token 格式: "e10m:" + base64(JSON{c: cookie, t: csrf})
    module Email10min
      CHANNEL = "email10min"
      BASE_URL = "https://email10min.com"
      TOK_PREFIX = "e10m:"

      CSRF_META_RE = /<meta\s+name="csrf-token"\s+content="([^"]+)"/.freeze
      CSRF_INPUT_RE = /<input[^>]+name="_token"[^>]+value="([^"]+)"/.freeze
      EMAIL_ID_RE = /id="emailAddress"[^>]*>([^<]+)/.freeze
      EMAIL_DATA_RE = /data-email="([^"]+@[^"]+)"/.freeze
      EMAIL_JSON_RE = /"mailbox"\s*:\s*"([^"]+@[^"]+)"/.freeze
      EMAIL_GENERIC_RE = /([a-zA-Z0-9._%+\-]+@[a-zA-Z0-9.\-]+\.[a-zA-Z]{2,})/.freeze

      BROWSER_HEADERS = {
        "Accept" => "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8",
        "Accept-Language" => "zh-CN,zh;q=0.9,en;q=0.8",
        "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " \
                       "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"
      }.freeze

      AJAX_HEADERS = {
        "Accept" => "application/json, text/plain, */*",
        "Accept-Language" => "zh-CN,zh;q=0.9,en;q=0.8",
        "Content-Type" => "application/x-www-form-urlencoded; charset=UTF-8",
        "Origin" => BASE_URL,
        "Referer" => "#{BASE_URL}/zh",
        "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " \
                       "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
        "X-Requested-With" => "XMLHttpRequest"
      }.freeze

      module_function

      # 从响应的 Set-Cookie 头合并为 Cookie 字符串
      def merge_cookies(resp)
        parts = []
        resp.set_cookies.each do |line|
          first = line.split(";", 2).first.to_s.strip
          parts << first unless first.empty?
        end
        parts.join("; ")
      end

      # token 编码
      def encode_token(cookie, csrf)
        raw = JSON.generate("c" => cookie, "t" => csrf)
        TOK_PREFIX + Base64.strict_encode64(raw)
      end

      # token 解码，返回 [cookie, csrf]
      def decode_token(token)
        raise "email10min: 无效 session token" unless token.to_s.start_with?(TOK_PREFIX)

        raw = Base64.strict_decode64(token[TOK_PREFIX.length..])
        data = JSON.parse(raw)
        cookie = data["c"].to_s
        csrf = data["t"].to_s
        raise "email10min: 无效 session token（字段缺失）" if cookie.empty? || csrf.empty?

        [cookie, csrf]
      rescue ArgumentError, JSON::ParserError
        raise "email10min: 无效 session token"
      end

      # 从 HTML 中提取 CSRF token
      def extract_csrf(html)
        m = html.match(CSRF_META_RE) || html.match(CSRF_INPUT_RE)
        raise "email10min: 未找到 CSRF token" unless m

        m[1]
      end

      # 从 HTML 中提取邮箱地址
      def extract_email(html)
        [EMAIL_ID_RE, EMAIL_DATA_RE, EMAIL_JSON_RE].each do |re|
          if (m = html.match(re))
            addr = m[1].strip
            return addr if addr.include?("@")
          end
        end
        if (m = html.match(EMAIL_GENERIC_RE))
          addr = m[1].strip
          return addr unless addr.include?("email10min") || addr.include?("example")
        end
        raise "email10min: 未找到邮箱地址"
      end

      # 创建临时邮箱
      # @return [EmailInfo]
      def generate_email
        resp = Http.get("#{BASE_URL}/zh", headers: BROWSER_HEADERS)
        resp.raise_for_status
        cookie = merge_cookies(resp)
        html = resp.body
        csrf = extract_csrf(html)
        email = extract_email(html)
        EmailInfo.new(channel: CHANNEL, email: email, token: encode_token(cookie, csrf))
      end

      # 获取邮件列表
      # @param email [String]
      # @param token [String]
      # @return [Array<Email>]
      def get_emails(email, token)
        cookie, csrf = decode_token(token)
        ts = (Time.now.to_f * 1000).to_i
        body = "_token=#{csrf}&captcha="
        resp = Http.post("#{BASE_URL}/messages?#{ts}",
                         headers: AJAX_HEADERS.merge("Cookie" => cookie),
                         body: body)
        resp.raise_for_status
        data = JSON.parse(resp.body)
        messages = data["messages"]
        return [] unless messages.is_a?(Array) && !messages.empty?

        messages.each_with_index.map do |msg, idx|
          id = msg["id"] || msg["message_id"] || idx
          from_addr = msg["from"].to_s
          from_addr = msg["sender"].to_s if from_addr.empty?
          text = msg["text"].to_s
          text = msg["body"].to_s if text.empty?
          html_body = msg["html"].to_s
          html_body = msg["body_html"].to_s if html_body.empty?
          date = msg["date"].to_s
          date = msg["created_at"].to_s if date.empty?
          Normalize.normalize_email({
            "id" => id.to_s,
            "from" => from_addr,
            "to" => msg["to"].to_s.empty? ? email : msg["to"],
            "subject" => msg["subject"].to_s,
            "text" => text,
            "html" => html_body,
            "date" => date,
            "isRead" => false
          }, email)
        end
      end
    end
  end
end
