# frozen_string_literal: true

module TempmailSdk
  module Providers
    # fmail.men 渠道实现
    module Fmail
      CHANNEL = "fmail"
      BASE_URL = "https://fmail.men"
      HEADERS = {
        "Accept" => "application/json",
        "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " \
                        "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36"
      }.freeze

      module_function

      def fetch_json(path)
        resp = Http.get("#{BASE_URL}#{path}", headers: HEADERS, timeout: 15)
        resp.raise_for_status
        data = resp.json
        data.is_a?(Hash) ? data : {}
      end

      def flatten_message(raw, recipient)
        {
          "id" => (raw["id"] || raw["uid"] || raw["token"]).to_s,
          "from" => (raw["sender"] || raw["from"]).to_s,
          "to" => (raw["recipient"] || raw["to"] || recipient).to_s,
          "subject" => (raw["subject"] || "").to_s,
          "text" => (raw["body_text"] || raw["text"] || raw["snippet"]).to_s,
          "html" => (raw["body_html"] || raw["html"]).to_s,
          "date" => (raw["received_at"] || raw["timestamp"] || raw["date"]).to_s,
          "is_read" => raw["is_read"] || false,
          "attachments" => raw["attachments"].is_a?(Array) ? raw["attachments"] : []
        }
      end

      # @param domain [String, nil] 可选域名
      # @return [EmailInfo]
      def generate_email(domain = nil)
        selected = domain.to_s.strip.delete_prefix("@")
        path = "/v1/random"
        path = "#{path}?domain=#{URI.encode_www_form_component(selected)}" unless selected.empty?

        data = fetch_json(path)
        email = data["address"].to_s.strip
        if email.empty?
          username = data["username"].to_s.strip
          dom = data["domain"].to_s.strip
          email = "#{username}@#{dom}" if !username.empty? && !dom.empty?
        end
        raise "fmail: invalid random response" if email.empty? || !email.include?("@")

        EmailInfo.new(channel: CHANNEL, email: email)
      end

      # @param email [String]
      # @return [Array<Email>]
      def get_emails(email)
        value = email.to_s.strip
        local, _, domain = value.partition("@")
        raise "fmail: invalid email" if local.empty? || domain.empty?

        data = fetch_json(
          "/v1/inbox/#{URI.encode_www_form_component(local)}?domain=#{URI.encode_www_form_component(domain)}&limit=50"
        )
        rows = data["emails"]
        return [] unless rows.is_a?(Array)

        rows.select { |r| r.is_a?(Hash) }.map do |row|
          token = (row["token"] || row["id"]).to_s.strip
          if token.empty?
            Normalize.normalize_email(flatten_message(row, value), value)
          else
            begin
              detail = fetch_json("/v1/email/#{URI.encode_www_form_component(token)}")
              nested = detail["email"].is_a?(Hash) ? detail["email"] : detail
              Normalize.normalize_email(flatten_message(nested.is_a?(Hash) ? nested : row, value), value)
            rescue StandardError
              Normalize.normalize_email(flatten_message(row, value), value)
            end
          end
        end
      end
    end
  end
end
