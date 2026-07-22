# frozen_string_literal: true

module TempmailSdk
  module Providers
    # devmail.uk 渠道实现
    module DevmailUk
      CHANNEL = "devmail-uk"
      API_BASE = "https://www.devmail.uk/api"
      HEADERS = { "Accept" => "application/json" }.freeze

      module_function

      def mailbox_from_email(email)
        address = email.to_s.strip
        return "" if address.empty?
        return address.split("@", 2).first.to_s.strip if address.include?("@")

        address
      end

      # @return [EmailInfo]
      def generate_email
        resp = Http.get("#{API_BASE}/new", headers: HEADERS, timeout: 15)
        resp.raise_for_status
        data = resp.json
        raise "devmail-uk: invalid new email response" unless data.is_a?(Hash)

        email = data["email"].to_s.strip
        if email.empty? && !data["mailbox"].to_s.strip.empty?
          email = "#{data['mailbox'].to_s.strip}@devmail.uk"
        end
        raise "devmail-uk: invalid new email response" if email.empty? || !email.include?("@")

        EmailInfo.new(channel: CHANNEL, email: email)
      end

      # @param email [String]
      # @return [Array<Email>]
      def get_emails(email)
        mailbox = mailbox_from_email(email)
        raise "devmail-uk: empty email" if mailbox.empty?

        resp = Http.get(
          "#{API_BASE}/inbox/#{URI.encode_www_form_component(mailbox)}?detail=true",
          headers: HEADERS, timeout: 15
        )
        resp.raise_for_status
        data = resp.json
        return [] unless data.is_a?(Hash)

        rows = data["emails"]
        return [] unless rows.is_a?(Array)

        rows.select { |r| r.is_a?(Hash) }.map { |item| Normalize.normalize_email(item, email) }
      end
    end
  end
end
