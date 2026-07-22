# frozen_string_literal: true

module TempmailSdk
  module Providers
    # TempInbox 渠道实现
    # API: https://endpoint.tempinbox.xyz
    module Tempinbox
      CHANNEL = "tempinbox"
      BASE = "https://endpoint.tempinbox.xyz"

      # 支持的域名列表
      DOMAINS = %w[tempinbox.xyz thepiratebay.cloud cryptoblad.nl].freeze

      HEADERS = {
        "Accept" => "application/json, text/plain, */*",
        "Cache-Control" => "no-cache",
        "DNT" => "1",
        "Referer" => "https://www.tempinbox.xyz/",
        "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"
      }.freeze

      module_function

      # 生成随机用户名
      def random_user(length = 10)
        chars = ("a".."z").to_a + ("0".."9").to_a
        Array.new(length) { chars.sample }.join
      end

      # 创建临时邮箱
      # @param domain [String, nil]
      # @return [EmailInfo]
      def generate_email(domain = nil)
        if domain && !domain.to_s.strip.empty?
          d = domain.strip.downcase
          raise "tempinbox: 域名不可用: #{d}" unless DOMAINS.include?(d)

          user = random_user
          addr = "#{user}@#{d}"
          Http.get("#{BASE}/email/#{addr}", headers: HEADERS, timeout: 15).raise_for_status
        else
          resp = Http.get("#{BASE}/email/Random", headers: HEADERS, timeout: 15)
          resp.raise_for_status
          addr = resp.body.strip.delete('"')
        end

        raise "tempinbox: 无效的邮箱地址" if addr.to_s.empty? || !addr.include?("@")

        EmailInfo.new(channel: CHANNEL, email: addr)
      end

      # 获取邮件列表
      # @param email [String]
      # @return [Array<Email>]
      def get_emails(email)
        addr = email.to_s.strip
        raise "tempinbox: 邮箱地址为空" if addr.empty?

        resp = Http.get("#{BASE}/messages/#{addr}", headers: HEADERS, timeout: 15)
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
