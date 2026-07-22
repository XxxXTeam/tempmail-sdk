# frozen_string_literal: true

require "json"

module TempmailSdk
  module Providers
    # TempGmailer 渠道 — https://tempgmailer.com
    # Laravel + CSRF，POST /get-gmail 创建，POST /get-inbox 获取邮件
    module Tempgmailer
      CHANNEL = "tempgmailer"
      BASE_URL = "https://tempgmailer.com"

      # 从 HTML 中提取 CSRF token
      CSRF_RE = /<meta\s+name="csrf-token"\s+content="([^"]+)"/

      HEADERS = {
        "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
        "Accept" => "application/json, text/plain, */*",
        "Accept-Language" => "zh-CN,zh;q=0.9,en;q=0.8",
        "Referer" => "#{BASE_URL}/",
        "X-Requested-With" => "XMLHttpRequest",
        "X-TempGmailer-Auth" => "frontend",
        "Content-Type" => "application/json"
      }.freeze

      module_function

      def extract_cookies(resp)
        cookies = {}
        resp.set_cookies.each do |line|
          pair = line.split(";").first.to_s.strip
          k, v = pair.split("=", 2)
          cookies[k] = v if k && v
        end
        cookies
      end

      def init_session
        resp = Http.get(BASE_URL, headers: {
          "User-Agent" => HEADERS["User-Agent"],
          "Accept" => "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8"
        }, timeout: 15)
        resp.raise_for_status

        m = resp.body.match(CSRF_RE)
        raise "tempgmailer: 无法提取 CSRF token" unless m

        csrf_token = m[1]
        cookies = extract_cookies(resp)
        cookie_str = cookies.map { |k, v| "#{k}=#{v}" }.join("; ")
        raise "tempgmailer: 未获取到 session cookie" if cookie_str.empty?

        [csrf_token, cookie_str]
      end

      # 创建临时邮箱
      # @return [EmailInfo]
      def generate_email
        csrf_token, cookie_str = init_session

        headers = HEADERS.merge(
          "X-CSRF-TOKEN" => csrf_token,
          "Cookie" => cookie_str
        )

        resp = Http.post("#{BASE_URL}/get-gmail", headers: headers,
                                                   json: { "refresh" => true, "adblock" => 0 })
        resp.raise_for_status
        data = resp.json
        raise "tempgmailer: 创建邮箱失败: #{data['message'] || '未知错误'}" unless data["success"]

        email = ((data["data"] || {})["email"] || "").strip
        raise "tempgmailer: 创建邮箱失败，未返回邮箱地址" if email.empty?

        token_payload = JSON.generate({ "csrfToken" => csrf_token, "cookies" => cookie_str })
        EmailInfo.new(channel: CHANNEL, email: email, token: token_payload)
      end

      # 获取邮件列表
      # @param token [String] JSON 格式认证信息
      # @param email [String]
      # @return [Array<Email>]
      def get_emails(token, email)
        session = JSON.parse(token)
        csrf_token = session["csrfToken"]
        cookie_str = session["cookies"]

        headers = HEADERS.merge(
          "X-CSRF-TOKEN" => csrf_token,
          "Cookie" => cookie_str
        )

        resp = Http.post("#{BASE_URL}/get-inbox", headers: headers,
                                                   json: { "email" => email, "adblock" => 0 })
        resp.raise_for_status
        data = resp.json
        return [] unless data["success"]

        messages = (data["data"] || {})["messages"] || []
        return [] unless messages.is_a?(Array)

        messages.filter_map do |msg|
          next unless msg.is_a?(Hash)

          from_field = msg["from"]
          from_field = from_field["address"] if from_field.is_a?(Hash)
          from_field = from_field.to_s

          html_content = msg["body"] || msg["intro"] || ""
          text_content = msg["intro"] || ""

          raw = {
            "id" => msg["id"] || "",
            "from" => from_field,
            "to" => email,
            "subject" => msg["subject"] || "",
            "text" => text_content,
            "html" => html_content,
            "date" => msg["createdAt"] || ""
          }
          Normalize.normalize_email(raw, email)
        end
      end
    end
  end
end
