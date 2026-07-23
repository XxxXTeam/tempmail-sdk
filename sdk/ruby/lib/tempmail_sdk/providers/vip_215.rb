# frozen_string_literal: true

require "json"
require "uri"
require "cgi"

module TempmailSdk
  module Providers
    # vip.215.im 渠道实现
    # 流程：GET / 取首页 cookie → POST /api/temp-inbox 创建邮箱（得到 address + token）
    #       GET /v1/messages 拉取邮件列表（HTTP 优先）
    #       GET /v1/messages/{id} 补拉单封详情
    # token 为 JWT，用于后续 API 鉴权
    module Vip215
      CHANNEL = "vip-215"
      BASE = "https://vip.215.im"
      UA = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " \
           "(KHTML, like Gecko) Chrome/148.0.0.0 Safari/537.36 Edg/148.0.0.0"

      HOME_HEADERS = {
        "User-Agent" => UA,
        "Accept" => "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
        "Accept-Language" => "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
        "Cache-Control" => "no-cache",
        "DNT" => "1",
        "Pragma" => "no-cache",
        "Sec-Fetch-Dest" => "document",
        "Sec-Fetch-Mode" => "navigate",
        "Sec-Fetch-Site" => "same-origin",
        "Upgrade-Insecure-Requests" => "1"
      }.freeze

      API_HEADERS = {
        "User-Agent" => UA,
        "Accept" => "*/*",
        "Accept-Language" => "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
        "Cache-Control" => "no-cache",
        "Content-Type" => "application/json",
        "DNT" => "1",
        "Origin" => BASE,
        "Pragma" => "no-cache",
        "Referer" => "#{BASE}/",
        "Sec-Fetch-Dest" => "empty",
        "Sec-Fetch-Mode" => "cors",
        "Sec-Fetch-Site" => "same-origin",
        "X-Locale" => "zh"
      }.freeze

      module_function

      # 合并 Set-Cookie 响应头到 cookie 字符串
      def merge_cookies(existing, set_cookie_lines)
        map = {}
        existing.to_s.split(";").each do |part|
          part = part.strip
          next unless part.include?("=")

          k, v = part.split("=", 2)
          map[k.strip] = v.to_s.strip unless k.strip.empty?
        end
        set_cookie_lines.each do |line|
          pair = line.split(";", 2).first.to_s.strip
          next unless pair.include?("=")

          k, v = pair.split("=", 2)
          map[k.strip] = v.to_s.strip unless k.strip.empty?
        end
        map.map { |k, v| "#{k}=#{v}" }.join("; ")
      end

      # 访问首页获取必要 cookie（yyds_homepage_bridge / yyds_homepage_device）
      def establish_session
        resp = Http.get(BASE + "/", headers: HOME_HEADERS, timeout: 15)
        resp.raise_for_status
        cookie = merge_cookies("", resp.set_cookies)
        raise "vip-215: 缺少首页 cookie" unless cookie.include?("yyds_homepage_bridge=") &&
                                                  cookie.include?("yyds_homepage_device=")

        cookie
      end

      # 创建 vip-215 临时邮箱
      # @return [EmailInfo]
      def generate_email
        cookie = establish_session
        resp = Http.post(
          BASE + "/api/temp-inbox",
          headers: API_HEADERS.merge("Cookie" => cookie),
          body: "",
          timeout: 15
        )
        resp.raise_for_status
        data = JSON.parse(resp.body)
        raise "vip-215: 创建失败" unless data["success"] && data["data"]

        address = data["data"]["address"].to_s.strip
        token = data["data"]["token"].to_s.strip
        raise "vip-215: 响应缺少 address 或 token" if address.empty? || token.empty?

        EmailInfo.new(channel: CHANNEL, email: address, token: token)
      end

      # 判断邮件条目是否已含真实正文
      def has_real_body?(row)
        %w[text body_text html body_html content body].any? do |key|
          v = row[key]
          v.is_a?(String) && !v.strip.empty?
        end
      end

      # 提取邮件 ID
      def extract_message_id(row)
        %w[id messageId message_id].each do |key|
          v = row[key]
          next if v.nil?
          return v.to_s.strip if v.is_a?(String) && !v.strip.empty?
          return v.to_i.to_s if v.is_a?(Numeric)
        end
        ""
      end

      # 补拉单封邮件详情
      def fetch_message_detail(token, id)
        mid = id.to_s.strip
        return nil if mid.empty?

        resp = Http.get(
          "#{BASE}/v1/messages/#{URI.encode_www_form_component(mid)}",
          headers: API_HEADERS.merge("Authorization" => "Bearer #{token}"),
          timeout: 15
        )
        return nil unless resp.status_code == 200

        parsed = JSON.parse(resp.body)
        return nil unless parsed["success"] && parsed["data"].is_a?(Hash)

        parsed["data"]
      rescue StandardError
        nil
      end

      # 获取 vip-215 邮件列表（HTTP GET /v1/messages）
      # @param token [String] JWT
      # @param email [String]
      # @return [Array<Email>]
      def get_emails(token, email)
        raise "vip-215: token 为空" if token.to_s.strip.empty?

        resp = Http.get(
          BASE + "/v1/messages",
          headers: API_HEADERS.merge("Authorization" => "Bearer #{token}"),
          timeout: 15
        )
        resp.raise_for_status
        parsed = JSON.parse(resp.body)
        raise "vip-215: messages success=false" unless parsed["success"]

        messages = parsed.dig("data", "messages") || []
        messages.map do |row|
          id = extract_message_id(row)
          unless has_real_body?(row) || id.empty?
            detail = fetch_message_detail(token, id)
            row = row.merge(detail) if detail
          end
          row["to"] ||= email
          Normalize.normalize_email(row, email)
        end
      end
    end
  end
end
