# frozen_string_literal: true

module TempmailSdk
  module Providers
    # tempmail.lol 渠道实现
    # API: https://api.tempmail.lol/v2
    module TempmailLol
      CHANNEL = "tempmail-lol"
      BASE_URL = "https://api.tempmail.lol/v2"
      DEFAULT_HEADERS = {
        "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/143.0.0.0 Safari/537.36",
        "Content-Type" => "application/json",
        "Origin" => "https://tempmail.lol",
        "DNT" => "1"
      }.freeze

      module_function

      # 创建临时邮箱，支持指定域名
      # @param domain [String, nil]
      # @return [EmailInfo]
      def generate_email(domain = nil)
        resp = Http.post("#{BASE_URL}/inbox/create", headers: DEFAULT_HEADERS,
                                                     json: { "domain" => domain, "captcha" => nil })
        resp.raise_for_status
        data = resp.json
        raise "Failed to generate email" if data["address"].to_s.empty? || data["token"].to_s.empty?

        EmailInfo.new(channel: CHANNEL, email: data["address"], token: data["token"])
      end

      # @param email [String]
      # @param token [String]
      # @return [Array<Email>]
      def get_emails(email, token)
        resp = Http.get("#{BASE_URL}/inbox?token=#{URI.encode_www_form_component(token)}", headers: DEFAULT_HEADERS)
        resp.raise_for_status
        (resp.json["emails"] || []).map { |raw| Normalize.normalize_email(raw, email) }
      end
    end
  end
end
