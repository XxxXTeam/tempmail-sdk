# frozen_string_literal: true

require "base64"
require "json"
require "uri"

module TempmailSdk
  module Providers
    # tempmailten.com 临时邮箱渠道（Laravel + CSRF）
    # GET /en 获取 session cookie + CSRF token
    # POST /messages 获取邮箱地址和邮件列表
    # GET /view/{id} 获取单封邮件 HTML 正文
    module Tempmailten
      CHANNEL = "tempmailten"
      BASE_URL = "https://tempmailten.com"
      TOK_PREFIX = "tmt1:"

      CSRF_RE = /<meta\s+name="csrf-token"\s+content="([^"]+)"/

      UA = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"

      module_function

      def browser_headers
        {
          "User-Agent" => UA,
          "Accept" => "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8",
          "Accept-Language" => "en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7"
        }
      end

      def ajax_headers
        {
          "User-Agent" => UA,
          "Accept" => "application/json, text/javascript, */*; q=0.01",
          "Accept-Language" => "en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7",
          "X-Requested-With" => "XMLHttpRequest",
          "Referer" => "#{BASE_URL}/en"
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

      def encode_token(csrf, cookie_hdr)
        raw = JSON.generate({ "t" => csrf, "c" => cookie_hdr })
        "#{TOK_PREFIX}#{Base64.strict_encode64(raw)}"
      end

      def decode_token(token)
        raise "tempmailten: 无效的 token" unless token&.start_with?(TOK_PREFIX)

        data = JSON.parse(Base64.decode64(token[TOK_PREFIX.length..]))
        csrf = (data["t"] || "").strip
        cookie_hdr = (data["c"] || "").strip
        raise "tempmailten: token 中缺少 CSRF" if csrf.empty?

        [csrf, cookie_hdr]
      end

      def post_messages(csrf, cookie_hdr)
        headers = ajax_headers.merge(
          "Content-Type" => "application/x-www-form-urlencoded",
          "X-CSRF-TOKEN" => csrf
        )
        headers["Cookie"] = cookie_hdr unless cookie_hdr.empty?

        resp = Http.post("#{BASE_URL}/messages", headers: headers,
                                                  body: "_token=#{URI.encode_www_form_component(csrf)}&captcha=")
        resp.raise_for_status
        data = resp.json
        raise "tempmailten: 解析响应 JSON 失败" unless data.is_a?(Hash)

        data
      end

      # @return [EmailInfo]
      def generate_email
        resp = Http.get("#{BASE_URL}/en", headers: browser_headers)
        resp.raise_for_status

        cookie_hdr = extract_cookie_header(resp)
        m = resp.body.match(CSRF_RE)
        raise "tempmailten: 未能从首页提取 CSRF token" unless m

        csrf = m[1]
        data = post_messages(csrf, cookie_hdr)
        mailbox = (data["mailbox"] || "").strip
        raise "tempmailten: 邮箱地址无效" if mailbox.empty? || !mailbox.include?("@")

        EmailInfo.new(channel: CHANNEL, email: mailbox, token: encode_token(csrf, cookie_hdr))
      end

      # @param email [String]
      # @param token [String]
      # @return [Array<Email>]
      def get_emails(email, token)
        addr = (email || "").strip
        raise "tempmailten: 邮箱地址为空" if addr.empty?

        csrf, cookie_hdr = decode_token((token || "").strip)
        data = post_messages(csrf, cookie_hdr)

        msgs = data["messages"]
        return [] unless msgs.is_a?(Array) && !msgs.empty?

        msgs.filter_map do |msg|
          next unless msg.is_a?(Hash)

          mid = (msg["id"] || "").to_s.strip
          next if mid.empty?

          from_addr = (msg["from_email"] || "").to_s
          from_name = (msg["from"] || "").to_s
          from_addr = "#{from_name} <#{from_addr}>" if !from_name.empty? && from_name != from_addr

          raw = {
            "id" => mid,
            "from" => from_addr,
            "to" => addr,
            "subject" => msg["subject"] || "",
            "html" => fetch_view(cookie_hdr, mid),
            "isRead" => !!msg["is_seen"]
          }
          Normalize.normalize_email(raw, addr)
        end
      end

      def fetch_view(cookie_hdr, mid)
        return "" if mid.nil? || mid.empty?

        headers = browser_headers.merge("Referer" => "#{BASE_URL}/en")
        headers["Cookie"] = cookie_hdr unless cookie_hdr.empty?
        resp = Http.get("#{BASE_URL}/view/#{URI.encode_www_form_component(mid)}", headers: headers)
        return "" unless resp.ok?

        resp.body
      rescue StandardError
        ""
      end
    end
  end
end
