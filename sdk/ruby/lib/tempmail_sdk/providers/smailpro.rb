# frozen_string_literal: true

module TempmailSdk
  module Providers
    # smailpro.com 渠道实现
    # 两段式流程：先获取 JWT payload，再调用 api.sonjj.com 接口
    module Smailpro
      CHANNEL = "smailpro"
      BASE_URL = "https://smailpro.com"
      API_BASE_URL = "https://api.sonjj.com/v1/temp_email"

      HEADERS = {
        "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
        "Accept" => "application/json, text/plain, */*",
        "Referer" => "#{BASE_URL}/"
      }.freeze

      module_function

      # 获取访问 sonjj API 所需的 JWT
      def fetch_payload(target_url, extra = nil)
        url = "#{BASE_URL}/app/payload?url=#{URI.encode_www_form_component(target_url)}"
        if extra
          extra.each { |k, v| url += "&#{URI.encode_www_form_component(k)}=#{URI.encode_www_form_component(v)}" }
        end
        resp = Http.get(url, headers: HEADERS)
        resp.raise_for_status
        payload = resp.body.strip.delete('"')
        raise "smailpro: payload 为空" if payload.empty?

        payload
      end

      # 携带 JWT 调用 sonjj API
      def call_api(target_url, extra = nil)
        payload = fetch_payload(target_url, extra)
        resp = Http.get("#{target_url}?payload=#{URI.encode_www_form_component(payload)}", headers: HEADERS)
        resp.raise_for_status
        resp.json
      end

      # 创建临时邮箱
      # @return [EmailInfo]
      def generate_email
        data = call_api("#{API_BASE_URL}/create")
        raise "smailpro: 创建响应格式异常" unless data.is_a?(Hash)

        email = data["email"].to_s.strip
        raise "smailpro: 创建邮箱失败" if email.empty?

        EmailInfo.new(channel: CHANNEL, email: email, token: email, expires_at: data["expired_at"])
      end

      # 获取单封邮件正文
      def fetch_message(email, mid)
        return ["", ""] if mid.to_s.empty?

        data = call_api("#{API_BASE_URL}/message", { "email" => email, "mid" => mid })
        return ["", ""] unless data.is_a?(Hash)

        [(data["body"] || "").to_s, (data["textBody"] || "").to_s]
      rescue StandardError
        ["", ""]
      end

      # 获取邮件列表
      # @param email [String]
      # @param token [String]
      # @return [Array<Email>]
      def get_emails(email, token = "")
        addr = email.to_s.strip
        raise "smailpro: 邮箱地址为空" if addr.empty?

        data = call_api("#{API_BASE_URL}/inbox", { "email" => addr })
        raise "smailpro: 邮件列表响应格式异常" unless data.is_a?(Hash)

        # API 响应: {"status": true, "data": {"messages": [...]}}
        inner = data["data"]
        messages = if inner.is_a?(Hash)
                     inner["messages"]
                   else
                     data["messages"]
                   end
        return [] unless messages.is_a?(Array) && !messages.empty?

        messages.select { |m| m.is_a?(Hash) }.map do |m|
          mid = m["mid"].to_s.strip
          html_body, text_body = fetch_message(addr, mid)
          raw = {
            "id" => mid,
            "from" => m["from"] || "",
            "to" => addr,
            "subject" => m["subject"] || "",
            "date" => m["datetime"] || ""
          }
          raw["html"] = html_body if !html_body.empty? || !text_body.empty?
          raw["text"] = text_body if !html_body.empty? || !text_body.empty?
          Normalize.normalize_email(raw, addr)
        end
      end
    end
  end
end
