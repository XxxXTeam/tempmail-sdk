# frozen_string_literal: true

module TempmailSdk
  module Providers
    # Mail Sunls 渠道实现
    # API: https://mail.sunls.de
    # 无需 token，无需 session
    module MailSunls
      CHANNEL = "mail-sunls"
      BASE = "https://mail.sunls.de"

      HEADERS = {
        "Accept" => "application/json, text/plain, */*",
        "Cache-Control" => "no-cache",
        "DNT" => "1",
        "Referer" => "https://mail.sunls.de/",
        "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"
      }.freeze

      module_function

      # 生成随机本地部分
      def random_local(length = 10)
        chars = ("a".."z").to_a + ("0".."9").to_a
        Array.new(length) { chars.sample }.join
      end

      # 从 API 获取可用域名列表
      def fetch_domains
        resp = Http.get("#{BASE}/api/domain", headers: HEADERS, timeout: 15)
        resp.raise_for_status
        data = resp.json
        raise "mail-sunls: 域名列表响应格式无效" unless data.is_a?(Array)

        out = data.select { |d| d.is_a?(String) && !d.strip.empty? }.map(&:strip)
        raise "mail-sunls: 无可用域名" if out.empty?

        out
      end

      # 创建临时邮箱：获取域名列表后随机生成地址
      # @return [EmailInfo]
      def generate_email
        domains = fetch_domains
        domain = domains.sample
        local = random_local(10)
        EmailInfo.new(channel: CHANNEL, email: "#{local}@#{domain}")
      end

      # 获取邮件列表
      # @param email [String]
      # @return [Array<Email>]
      def get_emails(email)
        addr = email.to_s.strip
        raise "mail-sunls: 邮箱地址不能为空" if addr.empty?

        resp = Http.get("#{BASE}/api/fetch?to=#{URI.encode_www_form_component(addr)}", headers: HEADERS, timeout: 15)
        resp.raise_for_status
        data = resp.json
        return [] unless data.is_a?(Array)

        data.select { |raw| raw.is_a?(Hash) }.map do |raw|
          Normalize.normalize_email(raw, addr)
        end
      end
    end
  end
end
