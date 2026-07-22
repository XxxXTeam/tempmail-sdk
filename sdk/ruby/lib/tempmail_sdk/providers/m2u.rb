# frozen_string_literal: true

require "json"

module TempmailSdk
  module Providers
    # m2u.io 渠道实现
    module M2u
      CHANNEL = "m2u"
      API_BASE = "https://api.m2u.io"
      HEADERS = {
        "Accept" => "application/json",
        "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " \
                        "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36"
      }.freeze

      module_function

      def pack_token(token, view_token)
        JSON.generate({ "token" => token, "viewToken" => view_token })
      end

      def unpack_token(token)
        value = token.to_s.strip
        return ["", ""] if value.empty?

        if value.start_with?("{")
          begin
            data = JSON.parse(value)
            if data.is_a?(Hash)
              return [data["token"].to_s.strip, data["viewToken"].to_s.strip]
            end
          rescue StandardError
            return ["", ""]
          end
        end
        [value, ""]
      end

      def flatten_message(raw, recipient)
        {
          "id" => (raw["id"] || raw["message_id"]).to_s,
          "from" => (raw["from_addr"] || raw["from_address"] || raw["from"]).to_s,
          "to" => recipient,
          "subject" => (raw["subject"] || "").to_s,
          "text" => (raw["text_body"] || raw["body_text"] || raw["text"]).to_s,
          "html" => (raw["html_body"] || raw["body_html"] || raw["html"]).to_s,
          "date" => (raw["received_at"] || raw["created_at"]).to_s,
          "is_read" => raw["is_read"] || raw["isRead"] || raw["seen"] || false,
          "attachments" => raw["attachments"].is_a?(Array) ? raw["attachments"] : []
        }
      end

      # @return [EmailInfo]
      def generate_email
        resp = Http.post(
          "#{API_BASE}/v1/mailboxes/auto",
          headers: HEADERS.merge("Content-Type" => "application/json"),
          json: {},
          timeout: 15
        )
        resp.raise_for_status
        data = resp.json
        mailbox = data.is_a?(Hash) ? data["mailbox"] : nil
        raise "m2u: invalid mailbox response" unless mailbox.is_a?(Hash)

        local_part = mailbox["local_part"].to_s.strip
        domain = mailbox["domain"].to_s.strip
        token = mailbox["token"].to_s.strip
        view_token = mailbox["view_token"].to_s.strip
        email = !local_part.empty? && !domain.empty? ? "#{local_part}@#{domain}" : ""
        raise "m2u: invalid mailbox response" if email.empty? || token.empty? || view_token.empty?

        EmailInfo.new(
          channel: CHANNEL,
          email: email,
          token: pack_token(token, view_token),
          expires_at: mailbox["expires_at"],
          created_at: mailbox["created_at"]
        )
      end

      def fetch_detail(token, view_token, message_id)
        resp = Http.get(
          "#{API_BASE}/v1/mailboxes/#{URI.encode_www_form_component(token)}/messages/#{URI.encode_www_form_component(message_id)}?view=#{URI.encode_www_form_component(view_token)}",
          headers: HEADERS, timeout: 15
        )
        return nil unless resp.ok?

        data = resp.json
        return nil unless data.is_a?(Hash)

        detail = data["message"]
        detail.is_a?(Hash) ? detail : nil
      rescue StandardError
        nil
      end

      # @param token [String] JSON 打包的 token
      # @param email [String]
      # @return [Array<Email>]
      def get_emails(token, email)
        mailbox_token, view_token = unpack_token(token)
        address = email.to_s.strip
        raise "m2u: missing token" if mailbox_token.empty?
        raise "m2u: missing view token" if view_token.empty?
        raise "m2u: empty email" if address.empty?

        resp = Http.get(
          "#{API_BASE}/v1/mailboxes/#{URI.encode_www_form_component(mailbox_token)}/messages?view=#{URI.encode_www_form_component(view_token)}",
          headers: HEADERS, timeout: 15
        )
        resp.raise_for_status
        data = resp.json
        rows = data.is_a?(Hash) ? (data["messages"] || []) : []
        return [] unless rows.is_a?(Array)

        rows.select { |r| r.is_a?(Hash) }.map do |item|
          message_id = (item["id"] || item["message_id"]).to_s.strip
          detail = message_id.empty? ? nil : fetch_detail(mailbox_token, view_token, message_id)
          Normalize.normalize_email(flatten_message(detail || item, address), address)
        end
      end
    end
  end
end
