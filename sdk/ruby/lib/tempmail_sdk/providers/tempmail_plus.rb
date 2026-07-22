# frozen_string_literal: true

module TempmailSdk
  module Providers
    # TempMail Plus 渠道 — https://tempmail.plus
    # 无需认证的简单 REST API，默认域名 mailto.plus
    module TempmailPlus
      CHANNEL = "tempmail-plus"
      API_BASE = "https://tempmail.plus/api/mails"
      DOMAIN = "mailto.plus"
      HEADERS = {
        "Accept" => "application/json",
        "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36",
        "Referer" => "https://tempmail.plus/",
        "Origin" => "https://tempmail.plus"
      }.freeze

      module_function

      def random_local(length = 12)
        chars = ("a".."z").to_a + ("0".."9").to_a
        Array.new(length) { chars.sample }.join
      end

      # 创建 tempmail-plus 临时邮箱
      # @param domain [String]
      # @param channel [String]
      # @return [EmailInfo]
      def generate_email(domain = DOMAIN, channel = CHANNEL)
        selected_domain = (domain || DOMAIN).to_s.strip
        selected_domain = DOMAIN if selected_domain.empty?
        selected_channel = (channel || CHANNEL).to_s.strip
        selected_channel = CHANNEL if selected_channel.empty?
        email = "#{random_local}@#{selected_domain}"
        # 调用列表接口验证地址可用
        Http.get("#{API_BASE}/?email=#{email}&epin=", headers: HEADERS, timeout: 15).raise_for_status
        EmailInfo.new(channel: selected_channel, email: email)
      end

      def fetch_body(mail_id, email)
        return {} unless mail_id && mail_id != 0

        resp = Http.get("#{API_BASE}/#{mail_id}?email=#{email}&epin=", headers: HEADERS, timeout: 15)
        resp.raise_for_status
        data = resp.json
        data.is_a?(Hash) ? data : {}
      rescue StandardError
        {}
      end

      # @param email [String]
      # @return [Array<Email>]
      def get_emails(email)
        e = email.to_s.strip
        raise "tempmail-plus: 邮箱地址为空" if e.empty?

        resp = Http.get("#{API_BASE}/?email=#{e}&epin=", headers: HEADERS, timeout: 15)
        resp.raise_for_status
        data = resp.json
        return [] unless data.is_a?(Hash)

        rows = data["mail_list"]
        return [] unless rows.is_a?(Array)

        rows.select { |r| r.is_a?(Hash) }.map do |raw|
          mail_id = raw["mail_id"] || 0
          detail = fetch_body(mail_id, e)
          flat = {
            "id" => mail_id != 0 ? mail_id.to_s : "",
            "from" => raw["from_mail"] || raw["from_name"] || "",
            "to" => e, "subject" => raw["subject"] || "", "date" => raw["time"] || "",
            "html" => detail["html"] || "", "text" => detail["text"] || "",
            "isRead" => !(raw["is_new"].nil? ? true : raw["is_new"])
          }
          Normalize.normalize_email(flat, e)
        end
      end
    end
  end
end
