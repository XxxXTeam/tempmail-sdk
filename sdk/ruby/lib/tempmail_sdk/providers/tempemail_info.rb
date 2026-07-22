# frozen_string_literal: true

require "base64"
require "json"

module TempmailSdk
  module Providers
    # tempemail.info 临时邮箱渠道
    # GET / 获取 PHPSESSID 会话 Cookie + base64 编码的邮箱地址
    # POST /template/checker.php 获取邮件列表
    # GET /view/{date} 获取单封邮件 HTML 正文
    module TempemailInfo
      CHANNEL = "tempemail-info"
      BASE_URL = "https://tempemail.info"

      # 匹配 var emailEncoded = "base64..."
      EMAIL_RE = /var\s+emailEncoded\s*=\s*"([^"]+)"/

      UA = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"

      module_function

      def base_headers
        {
          "User-Agent" => UA,
          "Accept-Language" => "zh-CN,zh;q=0.9,en;q=0.8",
          "Referer" => "#{BASE_URL}/",
          "Origin" => BASE_URL
        }
      end

      def extract_cookie_header(resp)
        cookies = {}
        resp.set_cookies.each do |line|
          pair = line.split(";").first.to_s.strip
          k, v = pair.split("=", 2)
          cookies[k] = v if k && v
        end
        cookies.sort.map { |k, v| "#{k}=#{v}" }.join("; ")
      end

      # 创建临时邮箱
      # @return [EmailInfo]
      def generate_email
        headers = base_headers.merge("Accept" => "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8")
        resp = Http.get("#{BASE_URL}/", headers: headers)
        resp.raise_for_status

        cookie_hdr = extract_cookie_header(resp)
        raise "tempemail-info: 未获取到会话 Cookie" if cookie_hdr.empty?

        m = resp.body.match(EMAIL_RE)
        raise "tempemail-info: 未在页面中找到 emailEncoded" unless m

        decoded = Base64.decode64(m[1]).force_encoding("UTF-8").strip
        raise "tempemail-info: 解码出的邮箱地址无效" if decoded.empty? || !decoded.include?("@")

        EmailInfo.new(channel: CHANNEL, email: decoded, token: cookie_hdr)
      end

      # 获取邮件列表
      # @param email [String]
      # @param token [String] 会话 Cookie 串
      # @return [Array<Email>]
      def get_emails(email, token)
        cookie_hdr = (token || "").strip
        raise "tempemail-info: 缺少会话 Cookie" if cookie_hdr.empty?

        headers = base_headers.merge(
          "Accept" => "application/json, text/javascript, */*; q=0.01",
          "X-Requested-With" => "XMLHttpRequest",
          "Content-Type" => "application/x-www-form-urlencoded",
          "Cookie" => cookie_hdr
        )

        resp = Http.post("#{BASE_URL}/template/checker.php", headers: headers, body: "last_id=0")
        resp.raise_for_status

        rows = resp.json
        return [] unless rows.is_a?(Array) && !rows.empty?

        rows.filter_map do |row|
          next unless row.is_a?(Hash)

          from_addr = (row["from"] || "").to_s
          name = (row["name"] || "").to_s
          from_addr = "#{name} <#{from_addr}>" if !name.empty? && name != from_addr

          date = (row["date"] || "").to_s
          html_body = fetch_body(cookie_hdr, date)

          raw = {
            "id" => (row["id"] || "").to_s,
            "from" => from_addr,
            "to" => email,
            "subject" => row["subject"] || "",
            "date" => date,
            "html" => html_body,
            "isRead" => !!row["read"]
          }
          Normalize.normalize_email(raw, email)
        end
      end

      def fetch_body(cookie_hdr, date)
        return "" if date.nil? || date.to_s.strip.empty?

        headers = base_headers.merge(
          "Accept" => "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
          "Cookie" => cookie_hdr
        )
        resp = Http.get("#{BASE_URL}/view/#{URI.encode_www_form_component(date)}", headers: headers)
        return "" unless resp.ok?

        resp.body
      rescue StandardError
        ""
      end
    end
  end
end
