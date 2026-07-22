# frozen_string_literal: true

require "securerandom"

module TempmailSdk
  module Providers
    # mail.tm 渠道实现
    # API: https://api.mail.tm
    # 流程: 获取域名 -> 生成随机邮箱/密码 -> 创建账号 -> 获取 Bearer Token
    module MailTm
      CHANNEL = "mail-tm"
      BASE_URL = "https://api.mail.tm"
      DEFAULT_HEADERS = { "Content-Type" => "application/json", "Accept" => "application/json" }.freeze

      module_function

      def random_string(length)
        chars = ("a".."z").to_a + ("0".."9").to_a
        Array.new(length) { chars.sample }.join
      end

      # 获取可用域名列表，兼容 Hydra 格式和纯数组
      def fetch_domains
        resp = Http.get("#{BASE_URL}/domains", headers: DEFAULT_HEADERS)
        resp.raise_for_status
        data = resp.json
        members = data.is_a?(Array) ? data : (data["hydra:member"] || [])
        members.select { |d| d["isActive"] }.map { |d| d["domain"] }
      end

      def create_account(address, password)
        resp = Http.post("#{BASE_URL}/accounts",
                         headers: DEFAULT_HEADERS.merge("Content-Type" => "application/ld+json"),
                         json: { "address" => address, "password" => password })
        resp.raise_for_status
        resp.json
      end

      def fetch_token(address, password)
        resp = Http.post("#{BASE_URL}/token", headers: DEFAULT_HEADERS,
                                              json: { "address" => address, "password" => password })
        resp.raise_for_status
        resp.json["token"]
      end

      # @return [EmailInfo]
      def generate_email
        domains = fetch_domains
        raise "No available domains" if domains.empty?

        address = "#{random_string(12)}@#{domains.sample}"
        password = random_string(16)
        account = create_account(address, password)
        token = fetch_token(address, password)
        EmailInfo.new(channel: CHANNEL, email: address, token: token, created_at: account["createdAt"])
      end

      # @param email [String]
      # @param token [String]
      # @return [Array<Email>]
      def get_emails(email, token)
        resp = Http.get("#{BASE_URL}/messages",
                        headers: DEFAULT_HEADERS.merge("Authorization" => "Bearer #{token}"))
        resp.raise_for_status
        data = resp.json
        messages = data.is_a?(Array) ? data : (data["hydra:member"] || [])
        return [] if messages.empty?

        messages.map do |msg|
          flat = begin
            dr = Http.get("#{BASE_URL}/messages/#{msg['id']}",
                          headers: DEFAULT_HEADERS.merge("Authorization" => "Bearer #{token}"))
            flatten(dr.ok? ? dr.json : msg, email)
          rescue StandardError
            flatten(msg, email)
          end
          Normalize.normalize_email(flat, email)
        end
      end

      # 将 mail.tm 消息格式扁平化
      def flatten(msg, recipient)
        from_addr = msg["from"].is_a?(Hash) ? msg["from"]["address"].to_s : msg["from"].to_s
        to_addr = recipient
        if msg["to"].is_a?(Array) && msg["to"][0].is_a?(Hash)
          to_addr = msg["to"][0]["address"] || recipient
        end
        html = msg["html"]
        html = html.join if html.is_a?(Array)
        {
          "id" => msg["id"] || "", "from" => from_addr, "to" => to_addr,
          "subject" => msg["subject"] || "", "text" => msg["text"] || "",
          "html" => html || "", "createdAt" => msg["createdAt"] || "",
          "seen" => msg["seen"] || false
        }
      end
    end
  end
end
