# frozen_string_literal: true

module TempmailSdk
  module Providers
    # harakirimail.com 渠道实现
    # 无需认证、无需 Cookie、无需 Token 的简单 REST API
    module Harakirimail
      CHANNEL = "harakirimail"
      BASE = "https://harakirimail.com"
      HEADERS = {
        "Accept" => "application/json, text/plain, */*",
        "Accept-Language" => "zh-CN,zh;q=0.9,en;q=0.8",
        "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " \
                        "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"
      }.freeze

      module_function

      def random_name(length = 12)
        chars = ("a".."z").to_a + ("0".."9").to_a
        Array.new(length) { chars.sample }.join
      end

      # @return [EmailInfo]
      def generate_email
        name = random_name
        email = "#{name}@harakirimail.com"

        # 调用收件箱接口验证地址可用
        resp = Http.get("#{BASE}/api/v1/inbox/#{name}", headers: HEADERS, timeout: 15)
        resp.raise_for_status

        EmailInfo.new(channel: CHANNEL, email: email)
      end

      # 获取单封邮件正文
      def fetch_body(email_id)
        return {} if email_id.to_s.empty?

        begin
          resp = Http.get("#{BASE}/api/v1/email/#{email_id}", headers: HEADERS, timeout: 15)
          resp.raise_for_status
          data = resp.json
          data.is_a?(Hash) ? data : {}
        rescue StandardError
          {}
        end
      end

      # @param email [String]
      # @return [Array<Email>]
      def get_emails(email)
        e = email.to_s.strip
        raise "harakirimail: empty email" if e.empty?

        name = e.include?("@") ? e.split("@").first.to_s : e

        resp = Http.get("#{BASE}/api/v1/inbox/#{name}", headers: HEADERS, timeout: 15)
        resp.raise_for_status
        data = resp.json
        return [] unless data.is_a?(Hash)

        rows = data["emails"]
        return [] unless rows.is_a?(Array)

        rows.select { |r| r.is_a?(Hash) }.map do |raw|
          email_id = raw["_id"].to_s
          detail = fetch_body(email_id)
          flat = {
            "id" => email_id,
            "from" => raw["from"].to_s,
            "to" => e,
            "subject" => raw["subject"].to_s,
            "date" => raw["received"].to_s,
            "html" => (detail["body_html"] || detail["html"]).to_s,
            "text" => (detail["body_text"] || detail["text"]).to_s,
            "isRead" => false
          }
          Normalize.normalize_email(flat, e)
        end
      end
    end
  end
end
