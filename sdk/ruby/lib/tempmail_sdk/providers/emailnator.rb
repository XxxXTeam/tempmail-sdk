# frozen_string_literal: true

require "json"
require "uri"

module TempmailSdk
  module Providers
    # emailnator.com 渠道实现
    #
    # 流程：
    #   1. GET / 获取 XSRF-TOKEN cookie（需 URL 解码）和 session cookie
    #   2. POST /generate-email {"email":["plusGmail","dotGmail","googleMail"]} 创建邮箱
    #   3. POST /message-list {"email":"..."} 获取邮件列表
    #   4. POST /message-list {"email":"...","messageID":"..."} 获取单封 HTML
    # token 存储 JSON {"xsrfToken":"...","cookie":"..."}
    module Emailnator
      CHANNEL = "emailnator"
      BASE_URL = "https://www.emailnator.com"
      OPTIONS = %w[plusGmail dotGmail googleMail].freeze

      BROWSER_HEADERS = {
        "Accept" => "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
        "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " \
                       "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36"
      }.freeze

      AJAX_HEADERS = {
        "Accept" => "application/json, text/plain, */*",
        "Content-Type" => "application/json",
        "Origin" => BASE_URL,
        "Referer" => "#{BASE_URL}/",
        "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " \
                       "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36",
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

      # 初始化 session：GET / 建立 XSRF-TOKEN 与 session cookie
      # @return [Hash] {"xsrfToken" => ..., "cookie" => ...}
      def init_session
        resp = Http.get(BASE_URL, headers: BROWSER_HEADERS)
        resp.raise_for_status
        cookie = merge_cookies(resp)
        xsrf = resp.cookie_value("XSRF-TOKEN").to_s
        xsrf = URI.decode_www_form_component(xsrf) unless xsrf.empty?
        raise "emailnator: 缺少 session cookie 或 XSRF token" if cookie.empty? || xsrf.empty?

        { "xsrfToken" => xsrf, "cookie" => cookie }
      end

      # 携带 session 发送 POST JSON 请求，返回原始 body
      def session_post(session, path, body)
        resp = Http.post("#{BASE_URL}#{path}",
                         headers: AJAX_HEADERS.merge(
                           "Cookie" => session["cookie"],
                           "X-XSRF-TOKEN" => session["xsrfToken"]
                         ),
                         json: body)
        resp.raise_for_status
        resp.body
      end

      # 创建临时邮箱
      # @return [EmailInfo]
      def generate_email
        session = init_session
        raw = session_post(session, "/generate-email", { "email" => OPTIONS })
        data = JSON.parse(raw)
        emails_arr = data["email"]
        email = emails_arr.is_a?(Array) ? emails_arr.find { |e| !e.to_s.empty? } : nil
        raise "emailnator: 创建邮箱返回为空" if email.nil? || email.empty?

        EmailInfo.new(channel: CHANNEL, email: email, token: JSON.generate(session))
      end

      # 获取邮件列表
      # @param email [String]
      # @param token [String] JSON {"xsrfToken":"...","cookie":"..."}
      # @return [Array<Email>]
      def get_emails(email, token)
        session = JSON.parse(token)
        raise "emailnator: 无效 session token" \
          if session["cookie"].to_s.empty? || session["xsrfToken"].to_s.empty?

        raw = session_post(session, "/message-list", { "email" => email })
        data = JSON.parse(raw)
        rows = data["messageData"]
        return [] unless rows.is_a?(Array) && !rows.empty?

        rows.filter_map do |row|
          next unless row.is_a?(Hash)

          mid = row["messageID"].to_s
          next if mid.empty? || mid.start_with?("ADS")

          html_body = fetch_detail(session, email, mid)
          Normalize.normalize_email({
            "id" => mid,
            "from" => row["from"].to_s,
            "to" => email,
            "subject" => row["subject"].to_s,
            "html" => html_body,
            "date" => row["time"].to_s,
            "isRead" => false
          }, email)
        end
      rescue JSON::ParserError
        raise "emailnator: 无效 session token"
      end

      # 拉取单封邮件详情 HTML
      def fetch_detail(session, email, message_id)
        raw = session_post(session, "/message-list", { "email" => email, "messageID" => message_id })
        # 详情响应可能是 JSON 编码的 HTML 字符串，也可能是原始 HTML
        JSON.parse(raw)
      rescue StandardError
        raw.to_s
      end
    end
  end
end
