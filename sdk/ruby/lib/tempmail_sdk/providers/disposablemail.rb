# frozen_string_literal: true

require "json"

module TempmailSdk
  module Providers
    # disposablemail.com 渠道实现
    #
    # 流程：
    #   1. GET / 获取 PHPSESSID cookie + HTML 中的 CSRF token（CSRF="xxx"）
    #   2. GET /index/index?csrf_token={csrf} 创建邮箱，返回 {"email":"user@domain.com"}
    #   3. GET /index/refresh 携带 session cookie 获取邮件列表（捷克语字段: predmet/od/kdy/precteno）
    #   4. GET /email/id/{id} 获取邮件正文 HTML
    # token 存储 "PHPSESSID=xxx" cookie 字符串。
    module Disposablemail
      CHANNEL = "disposablemail"
      BASE_URL = "https://www.disposablemail.com"

      CSRF_RE = /CSRF="([^"]+)"/.freeze

      BROWSER_HEADERS = {
        "Accept" => "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8",
        "Accept-Language" => "en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7",
        "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " \
                       "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36"
      }.freeze

      AJAX_HEADERS = {
        "Accept" => "application/json, text/javascript, */*; q=0.01",
        "Accept-Language" => "en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7",
        "Referer" => "#{BASE_URL}/",
        "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " \
                       "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36",
        "X-Requested-With" => "XMLHttpRequest"
      }.freeze

      module_function

      # 提取 Set-Cookie 中的 PHPSESSID
      def extract_session(resp)
        resp.set_cookies.each do |line|
          first = line.split(";", 2).first.to_s.strip
          k, v = first.split("=", 2)
          return "#{k}=#{v}" if k == "PHPSESSID"
        end
        ""
      end

      # 创建临时邮箱
      # @return [EmailInfo]
      def generate_email
        r1 = Http.get("#{BASE_URL}/", headers: BROWSER_HEADERS)
        r1.raise_for_status
        m = r1.body.match(CSRF_RE)
        raise "disposablemail: 未能从首页提取 CSRF token" unless m

        csrf = m[1]
        session_cookie = extract_session(r1)

        r2 = Http.get("#{BASE_URL}/index/index?csrf_token=#{csrf}",
                      headers: AJAX_HEADERS.merge(session_cookie.empty? ? {} : { "Cookie" => session_cookie }))
        r2.raise_for_status

        # 若 index 阶段刷新了 PHPSESSID，改用新的
        new_sess = extract_session(r2)
        session_cookie = new_sess unless new_sess.empty?

        data = JSON.parse(r2.body)
        email = data["email"].to_s.strip
        raise "disposablemail: 邮箱地址无效: #{email.inspect}" if email.empty? || !email.include?("@")

        EmailInfo.new(channel: CHANNEL, email: email, token: session_cookie)
      end

      # 获取邮件列表
      # @param email [String]
      # @param token [String] PHPSESSID cookie 字符串
      # @return [Array<Email>]
      def get_emails(email, token)
        raise "disposablemail: 邮箱地址为空" if email.to_s.strip.empty?

        headers = AJAX_HEADERS.dup
        headers = headers.merge("Cookie" => token) unless token.to_s.empty?

        resp = Http.get("#{BASE_URL}/index/refresh", headers: headers)
        resp.raise_for_status
        trimmed = resp.body.strip
        return [] if trimmed == "0" || trimmed.empty? || trimmed == "[]"

        list = begin
          JSON.parse(trimmed)
        rescue JSON::ParserError
          []
        end
        return [] unless list.is_a?(Array) && !list.empty?

        list.map do |item|
          mail_id = item["id"].to_s
          html_body = fetch_body(mail_id, token)
          Normalize.normalize_email({
            "id" => mail_id,
            "from" => item["od"].to_s,
            "to" => email,
            "subject" => item["predmet"].to_s,
            "html" => html_body,
            "date" => item["kdy"].to_s,
            "isRead" => item["precteno"] == "precteno"
          }, email)
        end
      end

      # 获取单封邮件 HTML 正文
      def fetch_body(mail_id, token)
        return "" if mail_id.empty?

        headers = AJAX_HEADERS.dup
        headers = headers.merge("Cookie" => token) unless token.to_s.empty?
        resp = Http.get("#{BASE_URL}/email/id/#{mail_id}", headers: headers)
        resp.ok? ? resp.body : ""
      rescue StandardError
        ""
      end
    end
  end
end
