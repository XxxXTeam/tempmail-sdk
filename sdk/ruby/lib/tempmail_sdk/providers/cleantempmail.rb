# frozen_string_literal: true

module TempmailSdk
  module Providers
    # cleantempmail.com 渠道实现
    module Cleantempmail
      CHANNEL = "cleantempmail"
      API_BASE = "https://cleantempmail.com/api"
      HEADERS = {
        "Accept" => "application/json",
        "User-Agent" => "Mozilla/5.0",
        "X-API-Key" => (ENV["CLEANTEMPMAIL_API_KEY"].to_s.strip.empty? ? "ct-test" : ENV["CLEANTEMPMAIL_API_KEY"].to_s.strip)
      }.freeze

      module_function

      def fetch_json(path)
        resp = Http.get("#{API_BASE}#{path}", headers: HEADERS, timeout: 15)
        resp.raise_for_status
        data = resp.json
        data.is_a?(Hash) ? data : {}
      end

      # @return [EmailInfo]
      def generate_email
        data = fetch_json("/generate-email")
        payload = data["data"].is_a?(Hash) ? data["data"] : {}
        email = (payload["email"] || payload["mailbox"]).to_s.strip
        raise "cleantempmail: invalid generate-email response" if email.empty? || !email.include?("@")

        EmailInfo.new(channel: CHANNEL, email: email)
      end

      # @param email [String]
      # @return [Array<Email>]
      def get_emails(email)
        address = email.to_s.strip
        raise "cleantempmail: empty email" if address.empty?

        data = fetch_json("/emails?email=#{URI.encode_www_form_component(address)}")
        payload = data["data"].is_a?(Hash) ? data["data"] : {}
        rows = payload["emails"]
        return [] unless rows.is_a?(Array)

        rows.select { |r| r.is_a?(Hash) }.map { |item| Normalize.normalize_email(item, address) }
      end
    end
  end
end
