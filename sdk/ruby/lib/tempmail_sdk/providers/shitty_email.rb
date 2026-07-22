# frozen_string_literal: true

require "uri"

module TempmailSdk
  module Providers
    # shitty.email 渠道 — https://shitty.email
    module ShittyEmail
      CHANNEL = "shitty-email"
      API_BASE = "https://shitty.email/api"
      HEADERS = { "Accept" => "application/json", "User-Agent" => "Mozilla/5.0" }.freeze

      module_function

      def headers_with_token(token = nil)
        h = HEADERS.dup
        h["X-Session-Token"] = token if token && !token.empty?
        h
      end

      # 创建临时邮箱
      # @return [EmailInfo]
      def generate_email
        resp = Http.post("#{API_BASE}/inbox", headers: HEADERS, timeout: 15)
        resp.raise_for_status
        data = resp.json
        raise "shitty-email: 响应格式无效" unless data.is_a?(Hash)

        email = (data["email"] || "").strip
        token = (data["token"] || "").strip
        raise "shitty-email: 创建邮箱响应无效" if email.empty? || !email.include?("@") || token.empty?

        EmailInfo.new(channel: CHANNEL, email: email, token: token, expires_at: data["expiresAt"])
      end

      # 获取邮件列表
      # @param token [String] 会话令牌
      # @param email [String] 邮箱地址
      # @return [Array<Email>]
      def get_emails(token, email)
        session_token = (token || "").strip
        address = (email || "").strip
        raise "shitty-email: token 为空" if session_token.empty?
        raise "shitty-email: 邮箱地址为空" if address.empty?

        resp = Http.get("#{API_BASE}/inbox", headers: headers_with_token(session_token), timeout: 15)
        resp.raise_for_status
        data = resp.json
        return [] unless data.is_a?(Hash)

        rows = data["emails"]
        return [] unless rows.is_a?(Array)

        rows.filter_map do |item|
          next unless item.is_a?(Hash)

          message_id = (item["id"] || "").to_s.strip
          detail = nil
          unless message_id.empty?
            begin
              dr = Http.get("#{API_BASE}/email/#{URI.encode_www_form_component(message_id)}",
                            headers: headers_with_token(session_token), timeout: 15)
              detail = dr.json if dr.ok?
            rescue StandardError
              # 单封邮件获取失败不影响其他
            end
          end

          source = (detail.is_a?(Hash) ? detail : item)
          raw = {
            "id" => source["id"],
            "from" => source["from"],
            "to" => source["to"] || address,
            "subject" => source["subject"],
            "text" => source["text"],
            "html" => source["html"],
            "date" => source["date"]
          }
          Normalize.normalize_email(raw, address)
        end
      end
    end
  end
end
