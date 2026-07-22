# frozen_string_literal: true

require "uri"

module TempmailSdk
  module Providers
    # TempMail Pro 渠道 — https://tempmailpro.us
    # POST /api/v1/mailbox/create 创建邮箱，GET /api/v1/mailbox/{token}/emails 获取邮件
    module Tempmailpro
      CHANNEL = "tempmailpro"
      API_BASE = "https://tempmailpro.us/api/v1"
      HEADERS = { "Accept" => "application/json", "User-Agent" => "Mozilla/5.0" }.freeze

      module_function

      def flatten(raw, recipient)
        {
          "id" => raw["id"] || raw["message_id"],
          "from" => raw["from_address"] || raw["from_name"],
          "to" => recipient,
          "subject" => raw["subject"],
          "text" => raw["body_text"],
          "html" => raw["body_html"],
          "date" => raw["received_at"],
          "attachments" => raw["attachments"]
        }
      end

      # @return [EmailInfo]
      def generate_email
        resp = Http.post("#{API_BASE}/mailbox/create",
                         headers: HEADERS.merge("Content-Type" => "application/json"),
                         json: {}, timeout: 15)
        resp.raise_for_status
        data = resp.json
        raise "tempmailpro: 创建邮箱响应无效" unless data.is_a?(Hash)

        box = data["data"] || {}
        raise "tempmailpro: 创建邮箱响应无效" unless box.is_a?(Hash)

        email = (box["address"] || "").strip
        token = (box["token"] || "").strip
        raise "tempmailpro: 创建邮箱响应无效" if email.empty? || !email.include?("@") || token.empty?

        EmailInfo.new(channel: CHANNEL, email: email, token: token,
                      expires_at: box["expires_at"], created_at: box["created_at"])
      end

      # @param token [String]
      # @param email [String]
      # @return [Array<Email>]
      def get_emails(token, email)
        mailbox_token = (token || "").strip
        address = (email || "").strip
        raise "tempmailpro: token 为空" if mailbox_token.empty?
        raise "tempmailpro: 邮箱地址为空" if address.empty?

        resp = Http.get("#{API_BASE}/mailbox/#{URI.encode_www_form_component(mailbox_token)}/emails",
                        headers: HEADERS, timeout: 15)
        resp.raise_for_status
        data = resp.json
        return [] unless data.is_a?(Hash)

        rows = data["data"]
        return [] unless rows.is_a?(Array)

        rows.filter_map do |item|
          next unless item.is_a?(Hash)

          message_id = (item["id"] || "").to_s.strip
          detail = nil
          unless message_id.empty?
            begin
              dr = Http.get("#{API_BASE}/mailbox/#{URI.encode_www_form_component(mailbox_token)}/emails/#{URI.encode_www_form_component(message_id)}",
                            headers: HEADERS, timeout: 15)
              if dr.ok?
                dd = dr.json
                detail = dd["data"] if dd.is_a?(Hash) && dd["data"].is_a?(Hash)
              end
            rescue StandardError
              # 忽略
            end
          end
          Normalize.normalize_email(flatten(detail || item, address), address)
        end
      end
    end
  end
end
