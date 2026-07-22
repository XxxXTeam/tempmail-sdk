# frozen_string_literal: true

require "base64"
require "json"
require "uri"

module TempmailSdk
  module Providers
    # temp-mail.fyi 临时邮箱渠道
    # GET / 获取 PHPSESSID cookie + CSRF token
    # POST /api/generate_email.php 创建邮箱
    # POST /api/get_emails.php 获取邮件列表
    module TempmailFyi
      CHANNEL = "tempmail-fyi"
      BASE_URL = "https://temp-mail.fyi"
      TOK_PREFIX = "tmf1:"

      CSRF_RE = /csrfToken"\s*value="([^"]+)"/

      UA = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"

      module_function

      def browser_headers
        {
          "User-Agent" => UA,
          "Accept" => "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8",
          "Accept-Language" => "en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7"
        }
      end

      def api_headers(csrf, cookie_hdr)
        h = {
          "User-Agent" => UA,
          "Accept" => "application/json, text/javascript, */*; q=0.01",
          "Accept-Language" => "en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7",
          "Content-Type" => "application/json",
          "X-CSRF-Token" => csrf,
          "X-Requested-With" => "XMLHttpRequest",
          "Referer" => "#{BASE_URL}/"
        }
        h["Cookie"] = cookie_hdr unless cookie_hdr.empty?
        h
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

      def encode_token(csrf, cookie_hdr)
        raw = JSON.generate({ "t" => csrf, "c" => cookie_hdr })
        "#{TOK_PREFIX}#{Base64.strict_encode64(raw)}"
      end

      def decode_token(token)
        raise "tempmail-fyi: 无效的 token" unless token&.start_with?(TOK_PREFIX)

        data = JSON.parse(Base64.decode64(token[TOK_PREFIX.length..]))
        csrf = (data["t"] || "").strip
        cookie_hdr = (data["c"] || "").strip
        raise "tempmail-fyi: token 中缺少 CSRF" if csrf.empty?

        [csrf, cookie_hdr]
      end

      # @return [EmailInfo]
      def generate_email
        resp = Http.get("#{BASE_URL}/", headers: browser_headers)
        resp.raise_for_status

        cookie_hdr = extract_cookie_header(resp)
        m = resp.body.match(CSRF_RE)
        raise "tempmail-fyi: 未能从首页提取 CSRF token" unless m

        csrf = m[1]

        resp2 = Http.post("#{BASE_URL}/api/generate_email.php",
                          headers: api_headers(csrf, cookie_hdr), body: "{}")
        resp2.raise_for_status
        data = resp2.json
        raise "tempmail-fyi: 创建邮箱响应格式异常" unless data.is_a?(Hash)
        raise "tempmail-fyi: 创建邮箱失败" unless data["success"]

        email = (data["email_address"] || "").strip
        raise "tempmail-fyi: 获取到的邮箱地址无效" if email.empty? || !email.include?("@")

        EmailInfo.new(channel: CHANNEL, email: email, token: encode_token(csrf, cookie_hdr),
                      expires_at: data["expires_at"])
      end

      # @param email [String]
      # @param token [String]
      # @return [Array<Email>]
      def get_emails(email, token)
        addr = (email || "").strip
        raise "tempmail-fyi: 邮箱地址为空" if addr.empty?

        csrf, cookie_hdr = decode_token((token || "").strip)

        resp = Http.post("#{BASE_URL}/api/get_emails.php",
                         headers: api_headers(csrf, cookie_hdr),
                         body: JSON.generate({ "email_address" => addr }))
        resp.raise_for_status
        data = resp.json
        raise "tempmail-fyi: 邮件列表响应格式异常" unless data.is_a?(Hash)

        emails = data["emails"]
        return [] unless emails.is_a?(Array) && !emails.empty?

        emails.filter_map do |item|
          next unless item.is_a?(Hash)

          Normalize.normalize_email(item, addr)
        end
      end
    end
  end
end
