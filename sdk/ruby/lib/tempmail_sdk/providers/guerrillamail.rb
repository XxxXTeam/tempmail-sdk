# frozen_string_literal: true

module TempmailSdk
  module Providers
    # guerrillamail.com 渠道实现
    # API: https://api.guerrillamail.com/ajax.php
    module Guerrillamail
      CHANNEL = "guerrillamail"
      BASE_URL = "https://api.guerrillamail.com/ajax.php"
      DEFAULT_HEADERS = {
        "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36"
      }.freeze

      module_function

      # @return [EmailInfo]
      def generate_email
        resp = Http.get("#{BASE_URL}?f=get_email_address&lang=en", headers: DEFAULT_HEADERS)
        resp.raise_for_status
        data = resp.json
        if data["email_addr"].to_s.empty? || data["sid_token"].to_s.empty?
          raise "Failed to generate email: missing email_addr or sid_token"
        end

        expires_at = nil
        expires_at = (data["email_timestamp"] + 3600) * 1000 if data["email_timestamp"]
        EmailInfo.new(channel: CHANNEL, email: data["email_addr"], token: data["sid_token"], expires_at: expires_at)
      end

      # @param email [String]
      # @param token [String]
      # @return [Array<Email>]
      def get_emails(email, token)
        sid = URI.encode_www_form_component(token)
        resp = Http.get("#{BASE_URL}?f=check_email&seq=0&sid_token=#{sid}", headers: DEFAULT_HEADERS)
        resp.raise_for_status
        list = resp.json["list"]
        return [] unless list.is_a?(Array)

        list.map do |item|
          body = item["mail_body"].to_s
          if body.empty? && item["mail_id"]
            begin
              mid = URI.encode_www_form_component(item["mail_id"].to_s)
              dr = Http.get("#{BASE_URL}?f=fetch_email&sid_token=#{sid}&email_id=#{mid}", headers: DEFAULT_HEADERS)
              body = dr.json["mail_body"].to_s if dr.ok?
            rescue StandardError
              # 忽略单封详情失败
            end
          end
          text = item["mail_excerpt"].to_s
          text = body.gsub(/<[^>]+>/, " ").split.join(" ") if text.empty?
          Normalize.normalize_email({
                                      "id" => item["mail_id"], "from" => item["mail_from"], "to" => email,
                                      "subject" => item["mail_subject"], "text" => text, "html" => body,
                                      "date" => item["mail_date"] || "", "isRead" => item["mail_read"] == 1
                                    }, email)
        end
      end
    end
  end
end
