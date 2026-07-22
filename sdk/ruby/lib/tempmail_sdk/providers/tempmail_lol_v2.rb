# frozen_string_literal: true

require "uri"

module TempmailSdk
  module Providers
    # tempmail.lol V2 渠道 — https://api.tempmail.lol
    # GET /generate 创建邮箱，GET /auth/{token} 获取邮件
    module TempmailLolV2
      CHANNEL = "tempmail-lol-v2"
      API_BASE = "https://api.tempmail.lol"
      HEADERS = {
        "Accept" => "application/json",
        "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36"
      }.freeze

      module_function

      # @return [EmailInfo]
      def generate_email
        resp = Http.get("#{API_BASE}/generate", headers: HEADERS)
        resp.raise_for_status
        data = resp.json
        raise "tempmail-lol-v2: 缺少 address 或 token" if (data["address"] || "").empty? || (data["token"] || "").empty?

        EmailInfo.new(channel: CHANNEL, email: data["address"], token: data["token"])
      end

      # @param token [String]
      # @param email [String]
      # @return [Array<Email>]
      def get_emails(token, email)
        resp = Http.get("#{API_BASE}/auth/#{URI.encode_www_form_component(token)}", headers: HEADERS)
        resp.raise_for_status
        data = resp.json

        mail_list = data["email"] || []
        return [] unless mail_list.is_a?(Array)

        mail_list.filter_map do |raw|
          next unless raw.is_a?(Hash)

          normalized = {
            "id" => raw["id"] || raw["_id"] || "",
            "from" => raw["from"] || raw["sender"] || "",
            "to" => email,
            "subject" => raw["subject"] || "",
            "text" => raw["body"] || raw["text"] || "",
            "html" => raw["html"] || raw["body"] || "",
            "date" => raw["date"] || raw["receivedAt"] || ""
          }
          Normalize.normalize_email(normalized, email)
        end
      end
    end
  end
end
