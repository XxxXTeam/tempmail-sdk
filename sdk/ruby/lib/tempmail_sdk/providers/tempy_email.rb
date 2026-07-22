# frozen_string_literal: true

module TempmailSdk
  module Providers
    # Tempy Email 渠道实现
    # API: https://tempy.email/api/v1
    module TempyEmail
      CHANNEL = "tempy-email"
      API_BASE = "https://tempy.email/api/v1"
      HEADERS = {
        "Accept" => "application/json",
        "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36"
      }.freeze

      module_function

      # 扁平化邮件字段
      def flatten_message(raw, recipient)
        {
          "id" => raw["id"] || raw["messageId"] || raw["message_id"] || raw["mail_id"] || "",
          "from" => raw["from"] || raw["sender"] || raw["from_addr"] || raw["from_address"] || "",
          "to" => raw["to"] || recipient,
          "subject" => raw["subject"] || raw["mail_title"] || "",
          "text" => raw["text"] || raw["body_text"] || raw["text_body"] || raw["body"] || "",
          "html" => raw["html"] || raw["body_html"] || raw["html_body"] || "",
          "date" => raw["date"] || raw["received_at"] || raw["created_at"] || "",
          "is_read" => raw["is_read"] || raw["isRead"] || raw["seen"] || false,
          "attachments" => raw["attachments"] || []
        }
      end

      # 创建临时邮箱
      # @param domain [String, nil]
      # @return [EmailInfo]
      def generate_email(domain = nil)
        body = {}
        wanted = domain.to_s.strip
        body["domain"] = wanted unless wanted.empty?

        resp = Http.post("#{API_BASE}/mailbox",
                         headers: HEADERS.merge("Content-Type" => "application/json"),
                         json: body, timeout: 15)
        resp.raise_for_status
        data = resp.json
        raise "tempy-email: 无效的创建响应" unless data.is_a?(Hash)

        email = data["email"].to_s.strip
        raise "tempy-email: 无效的创建响应" if email.empty?

        EmailInfo.new(channel: CHANNEL, email: email, expires_at: data["expires_at"])
      end

      # 获取邮件列表
      # @param email [String]
      # @return [Array<Email>]
      def get_emails(email)
        address = email.to_s.strip
        raise "tempy-email: 邮箱地址为空" if address.empty?

        resp = Http.get("#{API_BASE}/mailbox/#{URI.encode_www_form_component(address)}/messages",
                        headers: HEADERS, timeout: 15)
        resp.raise_for_status
        data = resp.json
        rows = data.is_a?(Hash) ? data["messages"] : data
        return [] unless rows.is_a?(Array)

        rows.select { |item| item.is_a?(Hash) }.map do |item|
          Normalize.normalize_email(flatten_message(item, address), address)
        end
      end
    end
  end
end
