# frozen_string_literal: true

require "json"
require "uri"

module TempmailSdk
  module Providers
    # 10minutemail.net 临时邮箱渠道
    # GET /address.api.php 创建邮箱（获取 PHPSESSID）
    # GET /address.api.php 获取邮件列表（带 session cookie）
    # GET /mail.api.php?mailid={id} 获取单封邮件详情
    module TenMinuteMailNet
      CHANNEL = "10minutemail-net"
      BASE_URL = "https://10minutemail.net"

      HEADERS = {
        "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36",
        "Accept" => "application/json"
      }.freeze

      module_function

      # @return [EmailInfo]
      def generate_email
        resp = Http.get("#{BASE_URL}/address.api.php", headers: HEADERS, timeout: 15)
        resp.raise_for_status
        data = resp.json
        raise "10minutemail-net: 响应格式无效" unless data.is_a?(Hash)

        address = (data["mail_get_mail"] || "").strip
        raise "10minutemail-net: 创建邮箱失败" if address.empty?

        phpsessid = resp.cookie_value("PHPSESSID")
        raise "10minutemail-net: 未获取到 PHPSESSID cookie" if phpsessid.nil? || phpsessid.empty?

        token = JSON.generate({ "cookie" => "PHPSESSID=#{phpsessid}" })

        expires_at = nil
        duetime = data["mail_get_duetime"]
        if duetime
          begin
            expires_at = duetime.to_i * 1000
          rescue StandardError
            # 忽略
          end
        end

        EmailInfo.new(channel: CHANNEL, email: address, token: token, expires_at: expires_at)
      end

      # @param token [String]
      # @param email [String]
      # @return [Array<Email>]
      def get_emails(token, email)
        address = (email || "").strip
        raise "10minutemail-net: 邮箱地址为空" if address.empty?

        token_data = JSON.parse(token)
        cookie_str = token_data["cookie"] || ""

        headers = HEADERS.merge("Cookie" => cookie_str)

        resp = Http.get("#{BASE_URL}/address.api.php", headers: headers, timeout: 15)
        resp.raise_for_status
        data = resp.json

        mail_list = data["mail_list"]
        return [] unless mail_list.is_a?(Array)

        mail_list.filter_map do |item|
          next unless item.is_a?(Hash)

          mail_id = (item["mail_id"] || "").to_s.strip
          next if mail_id.empty? || mail_id == "welcome"

          detail = fetch_detail(headers, mail_id)
          next unless detail.is_a?(Hash)

          html_body = ""
          text_body = ""
          body_list = detail["body"]
          if body_list.is_a?(Array)
            body_list.each do |part|
              next unless part.is_a?(Hash)

              content_type = part["content"] || ""
              if content_type.include?("text/html")
                html_body = part["body"] || ""
              elsif content_type.include?("text/plain")
                text_body = part["body"] || ""
              end
            end
          end

          if text_body.empty?
            plain_list = detail["plain"]
            if plain_list.is_a?(Array) && !plain_list.empty? && plain_list[0].is_a?(String)
              text_body = plain_list[0]
            end
          end

          raw = {
            "id" => mail_id,
            "from" => (detail["from"] || item["from"] || "").to_s,
            "to" => (detail["to"] || address).to_s,
            "subject" => (detail["subject"] || item["subject"] || "").to_s,
            "text" => text_body,
            "html" => html_body,
            "date" => (detail["datetime"] || item["datetime"] || "").to_s
          }
          Normalize.normalize_email(raw, address)
        end
      end

      def fetch_detail(headers, mail_id)
        resp = Http.get("#{BASE_URL}/mail.api.php?mailid=#{URI.encode_www_form_component(mail_id)}",
                        headers: headers, timeout: 15)
        resp.raise_for_status
        resp.json
      rescue StandardError
        nil
      end
    end
  end
end
