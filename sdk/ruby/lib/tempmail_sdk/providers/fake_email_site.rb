# frozen_string_literal: true

module TempmailSdk
  module Providers
    # fake-email.site 渠道实现
    # JSON REST API：创建临时邮箱 + 轮询收件箱
    module FakeEmailSite
      CHANNEL = "fake-email-site"
      BASE = "https://api.fake-email.site"
      HEADERS = {
        "Accept" => "application/json",
        "Content-Type" => "application/json",
        "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36"
      }.freeze

      module_function

      # @return [EmailInfo]
      def generate_email
        resp = Http.post("#{BASE}/api/temporary-address", headers: HEADERS, json: {})
        resp.raise_for_status
        data = resp.json
        raise "fake-email-site: invalid response" unless data.is_a?(Hash)

        email = (data["temp_email_addr"] || "").to_s.strip
        raise "fake-email-site: missing temp_email_addr" if email.empty?

        EmailInfo.new(channel: CHANNEL, email: email, token: email)
      end

      # @param email [String]
      # @return [Array<Email>]
      def get_emails(email)
        resp = Http.get("#{BASE}/api/inbox/poll?address=#{email}", headers: HEADERS)
        return [] if resp.status_code == 404

        resp.raise_for_status
        data = resp.json
        return [] unless data.is_a?(Hash)

        rows = data["messages"]
        return [] unless rows.is_a?(Array)

        rows.select { |r| r.is_a?(Hash) }.map { |row| Normalize.normalize_email(row, email) }
      end
    end
  end
end
