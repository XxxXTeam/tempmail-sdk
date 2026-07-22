# frozen_string_literal: true

require "uri"

module TempmailSdk
  module Providers
    # ThrowawayMail 渠道 — https://throwawaymail.app
    # POST /api/mailboxes 创建邮箱，GET /api/mailboxes/{id}/messages 获取邮件
    module Throwawaymail
      CHANNEL = "throwawaymail"
      API_BASE = "https://throwawaymail.app/api"
      HEADERS = { "Accept" => "application/json" }.freeze

      module_function

      # @return [EmailInfo]
      def generate_email
        resp = Http.post("#{API_BASE}/mailboxes", headers: HEADERS, timeout: 15)
        resp.raise_for_status
        data = resp.json
        raise "throwawaymail: 创建邮箱响应无效" unless data.is_a?(Hash)

        mailbox_id = (data["mailbox_id"] || "").strip
        email = (data["address"] || "").strip
        raise "throwawaymail: 创建邮箱响应无效" if mailbox_id.empty? || email.empty? || !email.include?("@")

        EmailInfo.new(channel: CHANNEL, email: email, token: mailbox_id,
                      expires_at: data["expires_at"], created_at: data["created_at"])
      end

      # @param token [String] mailbox_id
      # @param email [String]
      # @return [Array<Email>]
      def get_emails(token, email)
        mailbox_id = (token || "").strip
        address = (email || "").strip
        raise "throwawaymail: mailbox_id 为空" if mailbox_id.empty?
        raise "throwawaymail: 邮箱地址为空" if address.empty?

        resp = Http.get("#{API_BASE}/mailboxes/#{URI.encode_www_form_component(mailbox_id)}/messages",
                        headers: HEADERS, timeout: 15)
        resp.raise_for_status
        rows = resp.json
        return [] unless rows.is_a?(Array)

        rows.filter_map do |item|
          next unless item.is_a?(Hash)

          message_id = (item["message_id"] || "").strip
          detail = nil
          unless message_id.empty?
            begin
              dr = Http.get("#{API_BASE}/mailboxes/#{URI.encode_www_form_component(mailbox_id)}/messages/#{URI.encode_www_form_component(message_id)}",
                            headers: HEADERS, timeout: 15)
              detail = dr.json if dr.ok? && dr.json.is_a?(Hash)
            rescue StandardError
              # 忽略
            end
          end

          src = detail || item
          to_addresses = src["to_addresses"]
          to_val = to_addresses.is_a?(Array) && !to_addresses.empty? ? to_addresses[0] : address

          raw = {
            "id" => src["message_id"],
            "from_address" => src["from_address"],
            "fromName" => src["from_name"],
            "to" => to_val,
            "subject" => src["subject"] || "",
            "received_at" => src["received_at"],
            "text" => src["text"] || "",
            "html" => src["html"] || ""
          }
          Normalize.normalize_email(raw, address)
        end
      end
    end
  end
end
