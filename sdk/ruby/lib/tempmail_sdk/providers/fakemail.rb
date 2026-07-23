# frozen_string_literal: true

require "json"
require "uri"

module TempmailSdk
  module Providers
    # fakemail.net 渠道实现（与 disposablemail.com 共享 PHP API 结构）
    #
    # 流程：
    #   1. GET / 获取 session cookie + HTML 中的 CSRF token（const CSRF = "xxx"）
    #   2. GET /index/index?csrf_token={csrf} 创建邮箱，返回 {"email":"...","heslo":"..."}
    #   3. GET /index/refresh 携带 session cookie 获取邮件列表（捷克语字段: predmet/od/kdy/precteno）
    #   4. POST /index/email（body: id={id}）获取邮件 HTML 正文
    # token 存储 session cookie 字符串
    module Fakemail
      CHANNEL = "fakemail"
      BASE_URL = "https://www.fakemail.net"

      CSRF_RE = /const\s+CSRF\s*=\s*"([^"]+)"/.freeze

      BROWSER_HEADERS = {
        "Accept" => "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
        "User-Agent" => "Mozilla/5.0"
      }.freeze

      AJAX_HEADERS = {
        "Accept" => "application/json, text/javascript, */*; q=0.01",
        "Referer" => "#{BASE_URL}/",
        "User-Agent" => "Mozilla/5.0",
        "X-Requested-With" => "XMLHttpRequest"
      }.freeze

      module_function

      # 合并 Set-Cookie 为 Cookie 字符串
      def merge_cookies(existing, resp)
        jar = {}
        existing.to_s.split(";").each do |part|
          part = part.strip
          next unless part.include?("=")

          k, v = part.split("=", 2)
          jar[k.strip] = v.to_s
        end
        resp.set_cookies.each do |line|
          first = line.split(";", 2).first.to_s.strip
          next unless first.include?("=")

          k, v = first.split("=", 2)
          jar[k.strip] = v.to_s
        end
        jar.map { |k, v| "#{k}=#{v}" }.join("; ")
      end

      # 去除 UTF-8 BOM
      def clean_json(body)
        body = body.dup.force_encoding("BINARY")
        body = body[3..] if body.start_with?("\xEF\xBB\xBF".b)
        body.force_encoding("UTF-8")
      end

      # 创建临时邮箱
      # @return [EmailInfo]
      def generate_email
        r1 = Http.get("#{BASE_URL}/", headers: BROWSER_HEADERS)
        r1.raise_for_status
        cookie = merge_cookies("", r1)
        m = r1.body.match(CSRF_RE)
        raise "fakemail: csrf token not found" unless m

        csrf = m[1]
        r2 = Http.get("#{BASE_URL}/index/index?csrf_token=#{URI.encode_www_form_component(csrf)}",
                      headers: AJAX_HEADERS.merge(cookie.empty? ? {} : { "Cookie" => cookie }))
        r2.raise_for_status
        cookie = merge_cookies(cookie, r2)

        data = JSON.parse(clean_json(r2.body))
        email = data["email"].to_s.strip
        raise "fakemail: invalid mailbox response" if email.empty? || !email.include?("@")

        EmailInfo.new(channel: CHANNEL, email: email, token: cookie)
      end

      # 获取邮件列表
      # @param token [String] session cookie 字符串
      # @param email [String]
      # @return [Array<Email>]
      def get_emails(token, email)
        raise "fakemail: empty session token" if token.to_s.strip.empty?
        raise "fakemail: empty email" if email.to_s.strip.empty?

        headers = AJAX_HEADERS.merge("Cookie" => token)
        resp = Http.get("#{BASE_URL}/index/refresh", headers: headers)
        resp.raise_for_status

        rows = begin
          JSON.parse(clean_json(resp.body))
        rescue JSON::ParserError
          []
        end
        return [] unless rows.is_a?(Array) && !rows.empty?

        rows.map do |row|
          id = row["id"].to_s
          detail = fetch_detail(token, id) unless id.empty?
          subject = detail&.dig("predmet").to_s
          subject = row["predmet"].to_s if subject.empty?
          from_addr = detail&.dig("od").to_s
          from_addr = row["od"].to_s if from_addr.empty?
          html_body = detail&.dig("telo").to_s
          Normalize.normalize_email({
            "id" => id,
            "from" => from_addr,
            "to" => email,
            "subject" => subject,
            "html" => html_body,
            "date" => row["kdy"].to_s,
            "isRead" => row["precteno"] == "precteno"
          }, email)
        end
      end

      # 获取单封邮件详情
      def fetch_detail(cookie, id)
        return nil if id.empty?

        form = "id=#{URI.encode_www_form_component(id)}"
        resp = Http.post("#{BASE_URL}/index/email",
                         headers: AJAX_HEADERS.merge(
                           "Content-Type" => "application/x-www-form-urlencoded",
                           "Cookie" => cookie
                         ),
                         body: form)
        return nil unless resp.ok?

        JSON.parse(clean_json(resp.body))
      rescue StandardError
        nil
      end
    end
  end
end
