# frozen_string_literal: true

module TempmailSdk
  module Providers
    # ta-easy.com 临时邮箱渠道
    # API: https://api-endpoint.ta-easy.com/temp-email/
    module TaEasy
      CHANNEL = "ta-easy"
      API_BASE = "https://api-endpoint.ta-easy.com"
      ORIGIN = "https://www.ta-easy.com"

      HEADERS = {
        "Accept" => "application/json",
        "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
        "origin" => ORIGIN,
        "referer" => "#{ORIGIN}/"
      }.freeze

      module_function

      # 创建临时邮箱
      # @return [EmailInfo]
      def generate_email
        resp = Http.post("#{API_BASE}/temp-email/address/new",
                         headers: HEADERS.merge("Content-Length" => "0"), body: "")
        resp.raise_for_status
        data = resp.json
        raise "ta-easy: #{data['message'] || 'create failed'}" unless data["status"] == "success" && data["address"] && data["token"]

        expires_at = data["expiresAt"].is_a?(Numeric) ? data["expiresAt"].to_i : nil
        EmailInfo.new(channel: CHANNEL, email: data["address"], token: data["token"], expires_at: expires_at)
      end

      # 获取邮件列表
      # @param email [String]
      # @param token [String]
      # @return [Array<Email>]
      def get_emails(email, token)
        resp = Http.post("#{API_BASE}/temp-email/inbox/list",
                         headers: HEADERS.merge("Content-Type" => "application/json"),
                         json: { "token" => token, "email" => email })
        resp.raise_for_status
        data = resp.json
        raise "ta-easy: #{data['message'] || 'inbox failed'}" unless data["status"] == "success"

        raw_list = data["data"]
        return [] unless raw_list.is_a?(Array)

        raw_list.map { |raw| Normalize.normalize_email(raw, email) }
      end
    end
  end
end
