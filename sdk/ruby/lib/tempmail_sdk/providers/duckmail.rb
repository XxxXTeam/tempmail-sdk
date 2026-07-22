# frozen_string_literal: true

module TempmailSdk
  module Providers
    # DuckMail 渠道实现（与 mail.tm 同构 API）
    # API: https://api.duckmail.sbs
    module Duckmail
      CHANNEL = "duckmail"
      BASE_URL = "https://api.duckmail.sbs"
      DEFAULT_HEADERS = {
        "Content-Type" => "application/json",
        "Accept" => "application/json",
        "Origin" => "https://duckmail.sbs",
        "Referer" => "https://duckmail.sbs/"
      }.freeze

      module_function

      def random_string(length)
        chars = ("a".."z").to_a + ("0".."9").to_a
        Array.new(length) { chars.sample }.join
      end

      def fetch_domains
        resp = Http.get("#{BASE_URL}/domains?page=1", headers: DEFAULT_HEADERS)
        resp.raise_for_status
        data = resp.json
        members = data.is_a?(Array) ? data : (data["hydra:member"] || [])
        members.select { |d| d["isActive"] }.map { |d| d["domain"] }
      end

      # @return [EmailInfo]
      def generate_email
        domains = fetch_domains
        raise "No available domains" if domains.empty?

        address = "#{random_string(12)}@#{domains.sample}"
        password = random_string(16)
        acc = Http.post("#{BASE_URL}/accounts",
                        headers: DEFAULT_HEADERS.merge("Content-Type" => "application/ld+json"),
                        json: { "address" => address, "password" => password })
        acc.raise_for_status
        tok = Http.post("#{BASE_URL}/token", headers: DEFAULT_HEADERS,
                                             json: { "address" => address, "password" => password })
        tok.raise_for_status
        EmailInfo.new(channel: CHANNEL, email: address, token: tok.json["token"],
                      created_at: acc.json["createdAt"])
      end

      # @param email [String]
      # @param token [String]
      # @return [Array<Email>]
      def get_emails(email, token)
        resp = Http.get("#{BASE_URL}/messages?page=1",
                        headers: DEFAULT_HEADERS.merge("Authorization" => "Bearer #{token}"))
        resp.raise_for_status
        data = resp.json
        messages = data.is_a?(Array) ? data : (data["hydra:member"] || [])
        messages.map { |msg| Normalize.normalize_email(MailTm.flatten(msg, email), email) }
      end
    end
  end
end
