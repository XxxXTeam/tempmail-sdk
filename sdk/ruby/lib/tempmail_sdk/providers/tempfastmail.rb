# frozen_string_literal: true

require "uri"

module TempmailSdk
  module Providers
    # TempFastMail 渠道 — https://tempfastmail.com
    module Tempfastmail
      CHANNEL = "tempfastmail"
      BASE_URL = "https://tempfastmail.com"
      HEADERS = { "Accept" => "application/json", "User-Agent" => "Mozilla/5.0" }.freeze

      module_function

      def flatten(raw, recipient)
        {
          "id" => raw["uuid"],
          "from" => raw["from"],
          "to" => raw["real_to"] || recipient,
          "subject" => raw["subject"],
          "html" => raw["html"],
          "date" => raw["received_at"],
          "attachments" => raw["attachments"]
        }
      end

      # 创建临时邮箱
      # @return [EmailInfo]
      def generate_email
        resp = Http.post("#{BASE_URL}/api/email-box", headers: HEADERS)
        resp.raise_for_status
        data = resp.json
        email = (data["email"] || "").strip
        uuid = (data["uuid"] || "").strip
        raise "tempfastmail: 创建邮箱响应无效" if email.empty? || !email.include?("@") || uuid.empty?

        EmailInfo.new(channel: CHANNEL, email: email, token: uuid)
      end

      # 获取邮件列表
      # @param token [String] UUID
      # @param email [String]
      # @return [Array<Email>]
      def get_emails(token, email)
        uuid = (token || "").strip
        address = (email || "").strip
        raise "tempfastmail: token 为空" if uuid.empty?
        raise "tempfastmail: 邮箱地址为空" if address.empty?

        resp = Http.get("#{BASE_URL}/api/email-box/#{URI.encode_www_form_component(uuid)}/emails",
                        headers: HEADERS)
        resp.raise_for_status
        rows = resp.json
        return [] unless rows.is_a?(Array)

        rows.filter_map do |row|
          raw = row.is_a?(Hash) ? row : {}
          message_id = (raw["uuid"] || "").strip
          unless message_id.empty?
            begin
              dr = Http.get("#{BASE_URL}/api/email-box/#{URI.encode_www_form_component(uuid)}/email/#{URI.encode_www_form_component(message_id)}",
                            headers: HEADERS)
              if dr.ok?
                detail = dr.json
                raw = detail if detail.is_a?(Hash)
              end
            rescue StandardError
              # 忽略
            end
          end
          Normalize.normalize_email(flatten(raw, address), address)
        end
      end
    end
  end
end
