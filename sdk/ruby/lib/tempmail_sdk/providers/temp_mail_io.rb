# frozen_string_literal: true

module TempmailSdk
  module Providers
    # temp-mail.io 渠道实现
    # API: https://api.internal.temp-mail.io/api/v3
    module TempMailIo
      CHANNEL = "temp-mail-io"
      BASE_URL = "https://api.internal.temp-mail.io/api/v3"
      PAGE_URL = "https://temp-mail.io/en"
      UA = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/144.0.0.0 Safari/537.36"

      @cors_header = nil

      module_function

      def fetch_cors_header
        return @cors_header if @cors_header

        begin
          resp = Http.get(PAGE_URL, headers: { "User-Agent" => UA }, timeout: 15)
          m = resp.body.match(/mobileTestingHeader\s*:\s*"([^"]+)"/)
          @cors_header = m ? m[1] : "1"
        rescue StandardError
          @cors_header = "1"
        end
        @cors_header
      end

      def headers
        {
          "Content-Type" => "application/json",
          "Application-Name" => "web",
          "Application-Version" => "4.0.0",
          "X-CORS-Header" => fetch_cors_header,
          "User-Agent" => UA,
          "Origin" => "https://temp-mail.io",
          "Referer" => "https://temp-mail.io/"
        }
      end

      # @return [EmailInfo]
      def generate_email
        resp = Http.post("#{BASE_URL}/email/new", headers: headers,
                                                  json: { "min_name_length" => 10, "max_name_length" => 10 }, timeout: 15)
        resp.raise_for_status
        data = resp.json
        email = data.is_a?(Hash) ? data["email"] : nil
        token = data.is_a?(Hash) ? data["token"] : nil
        raise "temp-mail-io: invalid generate response" if email.to_s.empty? || token.to_s.empty?

        EmailInfo.new(channel: CHANNEL, email: email, token: token)
      end

      # @param email [String]
      # @return [Array<Email>]
      def get_emails(email)
        resp = Http.get("#{BASE_URL}/email/#{email}/messages", headers: headers, timeout: 15)
        resp.raise_for_status
        rows = resp.json
        return [] unless rows.is_a?(Array)

        rows.select { |r| r.is_a?(Hash) }.map { |raw| Normalize.normalize_email(raw, email) }
      end
    end
  end
end
