# frozen_string_literal: true

require "json"

module TempmailSdk
  module Providers
    # tempemails.net 渠道 — https://tempemails.net
    # Laravel Cookie Session + CSRF + REST JSON API
    module TempemailsNet
      CHANNEL = "tempemails-net"
      BASE_URL = "https://tempemails.net"

      # 从 HTML 中提取 CSRF token
      CSRF_RE = /<meta\s+name="csrf-token"\s+content="([^"]+)"/

      UA = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"

      module_function

      def build_headers(extra = {})
        h = {
          "Accept" => "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
          "Accept-Language" => "en-US,en;q=0.9",
          "Cache-Control" => "no-cache",
          "DNT" => "1",
          "Pragma" => "no-cache",
          "Upgrade-Insecure-Requests" => "1",
          "User-Agent" => UA
        }
        h.merge(extra)
      end

      def extract_cookies(resp)
        cookies = {}
        resp.set_cookies.each do |line|
          pair = line.split(";").first.to_s.strip
          k, v = pair.split("=", 2)
          cookies[k] = v if k && v
        end
        cookies
      end

      def cookies_to_str(cookies)
        cookies.sort.map { |k, v| "#{k}=#{v}" }.join("; ")
      end

      def encode_token(csrf, cookies)
        JSON.generate({ "csrf" => csrf, "cookies" => cookies })
      end

      def decode_token(token)
        data = JSON.parse(token)
        csrf = data["csrf"] || ""
        cookies = data["cookies"] || {}
        raise "tempemails-net: token 数据不完整" if csrf.empty? || !cookies.is_a?(Hash)

        [csrf, cookies]
      end

      # 创建临时邮箱
      # @return [EmailInfo]
      def generate_email
        # 获取首页，建立 session 并提取 CSRF
        r = Http.get("#{BASE_URL}/", headers: build_headers)
        r.raise_for_status

        m = r.body.match(CSRF_RE)
        raise "tempemails-net: 无法从首页提取 CSRF token" unless m

        csrf = m[1]
        cookies = extract_cookies(r)
        cookie_str = cookies_to_str(cookies)

        # POST /get_messages 获取自动分配的邮箱
        r2 = Http.post("#{BASE_URL}/get_messages", headers: build_headers(
          "Accept" => "application/json",
          "X-Requested-With" => "XMLHttpRequest",
          "X-CSRF-TOKEN" => csrf,
          "Cookie" => cookie_str,
          "Referer" => "#{BASE_URL}/"
        ))
        r2.raise_for_status
        data = r2.json
        raise "tempemails-net: 获取邮箱失败" unless data["status"]

        mailbox = (data["mailbox"] || "").strip
        raise "tempemails-net: 返回的邮箱地址无效" if mailbox.empty? || !mailbox.include?("@")

        merged_cookies = cookies.merge(extract_cookies(r2))
        tok = encode_token(csrf, merged_cookies)
        EmailInfo.new(channel: CHANNEL, email: mailbox, token: tok)
      end

      # 获取邮件列表
      # @param email [String]
      # @param token [String]
      # @return [Array<Email>]
      def get_emails(email, token)
        raise "tempemails-net: token 不能为空" if token.nil? || token.empty?

        addr = (email || "").strip
        raise "tempemails-net: 邮箱地址不能为空" if addr.empty?

        csrf, cookies = decode_token(token)
        cookie_str = cookies_to_str(cookies)

        r = Http.post("#{BASE_URL}/get_messages", headers: build_headers(
          "Accept" => "application/json",
          "X-Requested-With" => "XMLHttpRequest",
          "X-CSRF-TOKEN" => csrf,
          "Cookie" => cookie_str,
          "Referer" => "#{BASE_URL}/"
        ))
        r.raise_for_status
        data = r.json

        messages = data["messages"]
        return [] unless messages.is_a?(Array) && !messages.empty?

        messages.filter_map do |msg|
          next unless msg.is_a?(Hash)

          mail_id = msg["id"] || ""
          html_body = ""
          unless mail_id.to_s.empty?
            begin
              view_resp = Http.get("#{BASE_URL}/view/#{mail_id}", headers: build_headers(
                "Cookie" => cookie_str,
                "Referer" => "#{BASE_URL}/"
              ))
              html_body = view_resp.body if view_resp.ok?
            rescue StandardError
              # 单封邮件正文获取失败不影响其他
            end
          end

          from_name = (msg["from"] || "").strip
          from_addr = (msg["from_email"] || "").strip
          from_display = if !from_name.empty? && !from_addr.empty?
                           "#{from_name} <#{from_addr}>"
                         else
                           from_addr.empty? ? from_name : from_addr
                         end

          raw = {
            "id" => mail_id.to_s,
            "from" => from_display,
            "from_address" => from_addr,
            "to" => addr,
            "subject" => msg["subject"] || "",
            "html" => html_body,
            "date" => msg["receivedAt"] || ""
          }
          Normalize.normalize_email(raw, addr)
        end
      end
    end
  end
end
