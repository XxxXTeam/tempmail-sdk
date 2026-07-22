# frozen_string_literal: true

require "base64"
require "json"
require "uri"

module TempmailSdk
  module Providers
    # webmailtemp.com 临时邮箱渠道
    # GET /api/create 创建邮箱
    # GET /api/check/{username} 获取邮件列表
    module Webmailtemp
      CHANNEL = "webmailtemp"
      BASE_URL = "https://webmailtemp.com"
      TOK_PREFIX = "wmt1:"

      HEADERS = {
        "Accept" => "application/json",
        "User-Agent" => "Mozilla/5.0"
      }.freeze

      module_function

      def encode_token(username, cookie_hdr)
        raw = JSON.generate({ "u" => username, "c" => cookie_hdr })
        "#{TOK_PREFIX}#{Base64.strict_encode64(raw)}"
      end

      def decode_token(token)
        raise "webmailtemp: 无效的 token" unless token&.start_with?(TOK_PREFIX)

        data = JSON.parse(Base64.decode64(token[TOK_PREFIX.length..]))
        username = (data["u"] || "").strip
        cookie_hdr = (data["c"] || "").strip
        raise "webmailtemp: token 数据无效" if username.empty? || cookie_hdr.empty?

        { "username" => username, "cookie" => cookie_hdr }
      end

      # @return [EmailInfo]
      def generate_email
        resp = Http.get("#{BASE_URL}/api/create", headers: HEADERS, timeout: 15)
        resp.raise_for_status
        data = resp.json
        raise "webmailtemp: 创建邮箱响应无效" unless data.is_a?(Hash)

        email = (data["email"] || "").strip
        username = (data["username"] || "").strip

        # 从 set_cookies 提取第一个 cookie
        cookie_hdr = ""
        cookies = resp.set_cookies
        unless cookies.empty?
          pair = cookies.first.split(";").first.to_s.strip
          cookie_hdr = pair unless pair.empty?
        end

        unless data["success"] && !email.empty? && email.include?("@") && !username.empty? && !cookie_hdr.empty?
          raise "webmailtemp: 创建邮箱响应无效"
        end

        expires_at = nil
        ttl = data["ttl"]
        if ttl.is_a?(Numeric) && ttl > 0
          expires_at = ((Time.now.to_f + ttl) * 1000).to_i
        end

        EmailInfo.new(channel: CHANNEL, email: email, token: encode_token(username, cookie_hdr),
                      expires_at: expires_at)
      end

      # @param token [String]
      # @param email [String]
      # @return [Array<Email>]
      def get_emails(token, email)
        address = (email || "").strip
        raise "webmailtemp: 邮箱地址为空" if address.empty?

        session = decode_token((token || "").strip)
        headers = HEADERS.merge("Cookie" => session["cookie"])

        resp = Http.get("#{BASE_URL}/api/check/#{URI.encode_www_form_component(session["username"])}",
                        headers: headers, timeout: 15)
        resp.raise_for_status
        data = resp.json

        rows = data.is_a?(Hash) ? data["emails"] : nil
        return [] unless rows.is_a?(Array)

        rows.filter_map do |row|
          next unless row.is_a?(Hash)

          raw = {
            "id" => row["id"],
            "from" => row["from"],
            "to" => address,
            "subject" => row["subject"],
            "text" => row["body"],
            "html" => row["html"],
            "date" => row["received_at"] || row["timestamp"],
            "attachments" => row["attachments"]
          }
          Normalize.normalize_email(raw, address)
        end
      end
    end
  end
end
