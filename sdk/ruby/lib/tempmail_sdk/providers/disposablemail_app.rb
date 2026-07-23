# frozen_string_literal: true

require "json"

module TempmailSdk
  module Providers
    # disposablemail.app 渠道实现
    #
    # 纯 REST JSON API，无需认证。
    # 创建收件箱: POST /api/inbox （body: {}）
    # 获取邮件: GET /api/inbox/emails?token={token}
    module DisposablemailApp
      CHANNEL = "disposablemail-app"
      API_BASE = "https://disposablemail.app/api"

      HEADERS = {
        "Accept" => "application/json, text/plain, */*",
        "Accept-Language" => "zh-CN,zh;q=0.9,en;q=0.8",
        "Content-Type" => "application/json",
        "Origin" => "https://disposablemail.app",
        "Referer" => "https://disposablemail.app/",
        "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " \
                       "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36"
      }.freeze

      module_function

      # 创建临时邮箱
      # @return [EmailInfo]
      def generate_email
        resp = Http.post("#{API_BASE}/inbox", headers: HEADERS, body: "{}")
        resp.raise_for_status
        data = resp.json
        address = data["address"].to_s.strip
        token = data["token"].to_s.strip
        raise "disposablemail-app: address 或 token 为空" if address.empty? || token.empty?

        info = EmailInfo.new(channel: CHANNEL, email: address, token: token)
        info.expires_at = data["expiresAt"].to_s
        info.created_at = data["createdAt"].to_s
        info
      end

      # 获取邮件列表
      # @param email [String]
      # @param token [String] API 返回的 token
      # @return [Array<Email>]
      def get_emails(email, token)
        raise "disposablemail-app: token 为空" if token.to_s.empty?

        get_headers = HEADERS.reject { |k, _| k == "Content-Type" }
        resp = Http.get("#{API_BASE}/inbox/emails?token=#{token}", headers: get_headers)
        resp.raise_for_status
        data = resp.json
        emails_raw = data["emails"]
        return [] unless emails_raw.is_a?(Array) && !emails_raw.empty?

        emails_raw.filter_map do |raw|
          next unless raw.is_a?(Hash)

          Normalize.normalize_email(raw, email)
        end
      end
    end
  end
end
