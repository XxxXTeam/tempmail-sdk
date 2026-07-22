# frozen_string_literal: true

module TempmailSdk
  module Providers
    # MailholeDe 渠道实现
    # API: https://mailhole.de
    # 公共临时邮箱，无需认证，token 即邮箱地址本身
    module MailholeDe
      CHANNEL = "mailhole-de"
      BASE_URL = "https://mailhole.de"

      # 从 /api/random 返回的 HTML 中提取邮箱地址
      EMAIL_RE = /([a-z0-9.]+@mailhole\.de)/

      module_function

      # 按优先级从消息字典中提取首个非空字符串值
      def first_str(msg, *keys)
        keys.each do |key|
          val = msg[key]
          return val.to_s if val.is_a?(String) && !val.empty?
        end
        ""
      end

      # 创建 mailhole.de 临时邮箱
      # @return [EmailInfo]
      def generate_email
        resp = Http.get("#{BASE_URL}/api/random", headers: { "Accept" => "text/html" }, timeout: 15)
        resp.raise_for_status

        m = resp.body.match(EMAIL_RE)
        raise "mailhole-de: 无法从响应中解析邮箱地址" unless m

        email = m[1]
        EmailInfo.new(channel: CHANNEL, email: email, token: email)
      end

      # 获取邮件列表
      # @param email [String]
      # @return [Array<Email>]
      def get_emails(email)
        addr = email.to_s.strip
        raise "mailhole-de: 缺少邮箱地址" if addr.empty?

        resp = Http.get("#{BASE_URL}/json/#{URI.encode_www_form_component(addr)}",
                        headers: { "Accept" => "application/json" }, timeout: 15)
        resp.raise_for_status

        return [] if resp.body.nil? || resp.body.strip.empty? || resp.body.strip == "[]"

        messages = resp.json
        return [] unless messages.is_a?(Array)

        messages.select { |msg| msg.is_a?(Hash) }.map do |msg|
          row = {
            "id" => (msg["id"] || "").to_s,
            "from" => first_str(msg, "sender", "from"),
            "to" => addr,
            "subject" => (msg["subject"] || "").to_s,
            "text" => first_str(msg, "body", "text"),
            "html" => first_str(msg, "html", "body"),
            "received_at" => first_str(msg, "date", "received")
          }
          Normalize.normalize_email(row, addr)
        end
      end
    end
  end
end
