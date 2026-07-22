# frozen_string_literal: true

require "time"

module TempmailSdk
  module Providers
    # Tempmail365 渠道实现
    # API: https://tempmail365.cn/tempemail.php
    module Tempmail365
      CHANNEL = "tempmail365"
      BASE = "https://tempmail365.cn/tempemail.php"

      # 后备域名列表
      FALLBACK_DOMAINS = %w[fengyou.cc shop345.com nutemail.com qvrf.cn].freeze

      HEADERS = {
        "Accept" => "application/json, text/plain, */*",
        "Cache-Control" => "no-cache",
        "DNT" => "1",
        "Referer" => "https://tempmail365.cn/",
        "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"
      }.freeze

      module_function

      # 从 API 获取可用域名列表
      def fetch_domains
        resp = Http.get("#{BASE}?action=get_config", headers: HEADERS, timeout: 15)
        resp.raise_for_status
        data = resp.json
        return FALLBACK_DOMAINS.dup unless data.is_a?(Hash)

        raw = data["domains"]
        return FALLBACK_DOMAINS.dup unless raw.is_a?(Array) && !raw.empty?

        out = raw.select { |d| d.is_a?(String) && !d.strip.empty? }.map(&:strip)
        out.empty? ? FALLBACK_DOMAINS.dup : out
      rescue StandardError
        FALLBACK_DOMAINS.dup
      end

      # 生成随机用户名
      def random_username(length = 8)
        chars = ("a".."z").to_a + ("0".."9").to_a
        Array.new(length) { chars.sample }.join
      end

      # 从 HTML 邮件内容中提取发件人
      def extract_sender(content)
        m = content.match(/(?:发件人|From)\s*[:：]\s*(.+?)(?:<br|<\/|<p|\n|\r)/i)
        m ? m[1].strip : ""
      end

      # 从 HTML 邮件内容中提取主题
      def extract_subject(content)
        m = content.match(/(?:主题|Subject)\s*[:：]\s*(.+?)(?:<br|<\/|<p|\n|\r)/i)
        m ? m[1].strip : ""
      end

      # 创建临时邮箱
      # @param domain [String, nil]
      # @return [EmailInfo]
      def generate_email(domain = nil)
        domains = fetch_domains
        raise "tempmail365: 无可用域名" if domains.empty?

        if domain && !domain.to_s.strip.empty?
          d = domain.strip.downcase
          matched = domains.select { |x| x.downcase == d }
          raise "tempmail365: 域名不可用: #{d}" if matched.empty?

          selected = matched.first
        else
          selected = domains.sample
        end

        username = random_username
        addr = "#{username}@#{selected}"

        url = "#{BASE}?action=create_email&email=#{URI.encode_www_form_component(addr)}&domain=#{URI.encode_www_form_component(selected)}"
        resp = Http.get(url, headers: HEADERS, timeout: 15)
        resp.raise_for_status
        data = resp.json
        raise "tempmail365: 创建邮箱失败" unless data.is_a?(Hash) && data["success"]

        EmailInfo.new(channel: CHANNEL, email: addr)
      end

      # 获取邮件列表
      # @param email [String]
      # @return [Array<Email>]
      def get_emails(email)
        addr = email.to_s.strip
        raise "tempmail365: 邮箱地址为空" if addr.empty?

        resp = Http.get("#{BASE}?action=fetch_mail&email=#{URI.encode_www_form_component(addr)}",
                        headers: HEADERS, timeout: 15)
        resp.raise_for_status
        data = resp.json
        return [] unless data.is_a?(Hash)

        content = data["content"]
        return [] if content.nil? || !content.is_a?(String) || content.strip.empty?
        return [] if content.strip == "无邮件"

        sender = extract_sender(content)
        subject = extract_subject(content)

        raw = {
          "from" => sender,
          "subject" => subject,
          "body" => content,
          "date" => Time.now.utc.iso8601
        }
        [Normalize.normalize_email(raw, addr)]
      end
    end
  end
end
