# frozen_string_literal: true

require "securerandom"

module TempmailSdk
  module Providers
    # Temporam 渠道实现
    # API: https://temporam.com/api
    module Temporam
      CHANNEL = "temporam"
      ORIGIN = "https://temporam.com"

      HEADERS = {
        "Accept" => "application/json",
        "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36"
      }.freeze

      module_function

      # 生成随机用户名
      def random_local(length = 18)
        chars = ("a".."z").to_a + ("0".."9").to_a
        "sdk" + Array.new(length) { chars.sample }.join
      end

      # GET 请求并解析 JSON
      def get_json(path)
        resp = Http.get("#{ORIGIN}#{path}", headers: HEADERS)
        resp.raise_for_status
        resp.json
      end

      # 获取可用域名列表
      def fetch_domains
        data = get_json("/api/domains")
        domains = (data["data"] || []).select { |item| item.is_a?(Hash) }.map { |item| (item["domain"] || "").strip }.reject(&:empty?)
        raise "temporam: 域名列表为空" if domains.empty?

        domains
      end

      # 将原始邮件映射为标准化格式
      def normalize_raw(raw, email)
        flat = raw.merge(
          "id" => raw["id"] || raw["uuid"] || "",
          "from" => raw["fromEmail"] || raw["from"] || "",
          "to" => raw["toEmail"] || raw["to"] || email,
          "date" => raw["createdAt"] || raw["created_at"] || raw["date"] || ""
        )
        Normalize.normalize_email(flat, email)
      end

      # 创建临时邮箱
      # @param domain [String, nil]
      # @return [EmailInfo]
      def generate_email(domain = nil)
        domains = fetch_domains
        if domain && !domain.empty?
          raise "temporam: 不支持的域名 #{domain}" unless domains.include?(domain)

          selected = domain
        else
          selected = domains.sample
        end
        email = "#{random_local}@#{selected}"
        EmailInfo.new(channel: CHANNEL, email: email, token: email)
      end

      # 获取邮件列表
      # @param token [String]
      # @param email [String]
      # @return [Array<Email>]
      def get_emails(token, email)
        recipient = token.to_s.empty? ? email : token
        raise "temporam: 缺少邮箱地址" if recipient.to_s.strip.empty?

        data = get_json("/api/emails?email=#{URI.encode_www_form_component(recipient)}&limit=50")
        rows = data["data"] || []
        rows.select { |row| row.is_a?(Hash) }.map do |row|
          raw = row
          msg_id = (row["id"] || row["uuid"] || "").to_s
          unless msg_id.empty?
            begin
              detail = get_json("/api/emails/#{URI.encode_www_form_component(msg_id)}")
              raw = detail["data"] if detail["data"].is_a?(Hash)
            rescue StandardError
              # 详情获取失败时使用列表数据
            end
          end
          normalize_raw(raw, recipient)
        end
      end
    end
  end
end
