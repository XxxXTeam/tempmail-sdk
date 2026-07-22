# frozen_string_literal: true

require "uri"

module TempmailSdk
  module Providers
    # xkx.me 临时邮箱渠道（CSRF + session cookie）
    # GET / 获取 CSRF token + session cookie
    # POST /mailbox/create/random 创建邮箱（不跟随重定向，从 Location 提取邮箱）
    # GET /mailbox/{email}/messages 获取邮件列表
    module XkxMe
      CHANNEL = "xkx-me"
      BASE_URL = "https://xkx.me"

      CSRF_RE = /csrf-token"\s+content="([^"]+)"/

      HEADERS = {
        "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36"
      }.freeze

      module_function

      def extract_cookie_header(resp)
        cookies = {}
        resp.set_cookies.each do |line|
          pair = line.split(";").first.to_s.strip
          k, v = pair.split("=", 2)
          cookies[k] = v if k && v
        end
        cookies.sort.map { |k, v| "#{k}=#{v}" }.join("; ")
      end

      # @return [EmailInfo]
      def generate_email
        # 获取 CSRF token 和 session cookie
        resp = Http.get(BASE_URL, headers: HEADERS, timeout: 15)
        resp.raise_for_status

        m = resp.body.match(CSRF_RE)
        raise "xkx-me: 无法获取 CSRF token" unless m

        csrf = m[1]
        cookie_hdr = extract_cookie_header(resp)

        # POST 创建邮箱（不跟随重定向）
        post_headers = HEADERS.merge(
          "Content-Type" => "application/x-www-form-urlencoded",
          "Referer" => "#{BASE_URL}/"
        )
        post_headers["Cookie"] = cookie_hdr unless cookie_hdr.empty?

        body = "_token=#{URI.encode_www_form_component(csrf)}"
        resp2 = Http.post("#{BASE_URL}/mailbox/create/random",
                          headers: post_headers, body: body, timeout: 15)

        # 从 Location 头提取邮箱地址（302 重定向）
        location = resp2.header("Location") || ""
        email_match = location.match(/mailbox\/([^\/"'<>\s]+@xkx\.me)/)
        raise "xkx-me: 无法从响应中提取邮箱地址" unless email_match

        email = email_match[1]

        EmailInfo.new(channel: CHANNEL, email: email, token: cookie_hdr)
      end

      # @param token [String]
      # @param email [String]
      # @return [Array<Email>]
      def get_emails(token, email)
        address = (email || "").strip
        raise "xkx-me: 缺少邮箱地址" if address.empty?

        cookie_str = (token || "").strip

        headers = HEADERS.merge(
          "Accept" => "application/json",
          "X-Requested-With" => "XMLHttpRequest"
        )
        headers["Cookie"] = cookie_str unless cookie_str.empty?

        encoded_email = URI.encode_www_form_component(address).gsub("%40", "@")
        resp = Http.get("#{BASE_URL}/mailbox/#{encoded_email}/messages",
                        headers: headers, timeout: 15)
        return [] if resp.status_code == 404

        resp.raise_for_status
        data = resp.json
        return [] unless data

        message_list = if data.is_a?(Array)
                         data
                       elsif data.is_a?(Hash)
                         if data.key?("messages")
                           data["messages"]
                         elsif data["message"].is_a?(Hash)
                           [data["message"]]
                         else
                           []
                         end
                       else
                         []
                       end

        return [] unless message_list.is_a?(Array)

        message_list.filter_map do |msg|
          next unless msg.is_a?(Hash)

          raw = {
            "id" => (msg["id"] || "").to_s,
            "from" => (msg["from"] || "").to_s,
            "to" => address,
            "subject" => (msg["subject"] || "").to_s,
            "date" => (msg["date"] || "").to_s,
            "html" => (msg["html"] || msg["body"] || "").to_s,
            "text" => (msg["text"] || "").to_s,
            "is_read" => false,
            "attachments" => []
          }
          Normalize.normalize_email(raw, address)
        end
      end
    end
  end
end
