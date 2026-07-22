# frozen_string_literal: true

module TempmailSdk
  module Providers
    # tempmail.ing 渠道实现
    # API: https://api.tempmail.ing/api
    module Tempmail
      CHANNEL = "tempmail"
      BASE_URL = "https://api.tempmail.ing/api"

      DEFAULT_HEADERS = {
        "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/143.0.0.0 Safari/537.36",
        "Content-Type" => "application/json",
        "Referer" => "https://tempmail.ing/",
        "DNT" => "1"
      }.freeze

      module_function

      # 创建临时邮箱，支持自定义有效期（分钟）
      # @param duration [Integer]
      # @return [EmailInfo]
      def generate_email(duration = 30)
        resp = Http.post("#{BASE_URL}/generate", headers: DEFAULT_HEADERS, json: { "duration" => duration })
        resp.raise_for_status
        data = resp.json
        raise "Failed to generate email" unless data["success"]

        info = data["email"]
        EmailInfo.new(
          channel: CHANNEL,
          email: info["address"],
          expires_at: info["expiresAt"],
          created_at: info["createdAt"]
        )
      end

      # 获取邮件列表
      # @param email [String]
      # @return [Array<Email>]
      def get_emails(email)
        resp = Http.get("#{BASE_URL}/emails/#{URI.encode_www_form_component(email)}", headers: DEFAULT_HEADERS)
        resp.raise_for_status
        data = resp.json
        raise "Failed to get emails" unless data["success"]

        (data["emails"] || []).map { |raw| Normalize.normalize_email(flatten(raw, email), email) }
      end

      # 将原始格式扁平化：content -> html, from_address -> from, received_at -> date
      def flatten(raw, recipient)
        {
          "id" => raw["id"] || "",
          "from" => raw["from_address"] || raw["from"] || "",
          "to" => recipient,
          "subject" => raw["subject"] || "",
          "text" => raw["text"] || "",
          "html" => raw["content"] || raw["html"] || "",
          "date" => raw["received_at"] || raw["date"] || "",
          "is_read" => raw["is_read"] == 1 || raw["is_read"] == true,
          "attachments" => raw["attachments"] || []
        }
      end
    end
  end
end
