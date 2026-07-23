# frozen_string_literal: true

require "json"

module TempmailSdk
  module Providers
    # mytempmail.cc 渠道实现
    #
    # 流程：POST /api/address 创建邮箱（JSON body），返回 token + address
    #       GET /api/mails/{token} 获取邮件列表
    # 域名固定 nilvaro.com，有效期 600 秒
    module MytempmailCc
      CHANNEL = "mytempmail-cc"
      BASE_URL = "https://api.mytempmail.cc"

      HEADERS = {
        "Content-Type" => "application/json",
        "Accept" => "application/json"
      }.freeze

      CHARS = ("a".."z").to_a.freeze

      module_function

      # 生成随机 8 位小写字母用户名
      # @return [String]
      def random_name
        Array.new(8) { CHARS.sample }.join
      end

      # 创建 mytempmail.cc 临时邮箱
      # @return [EmailInfo]
      def generate_email
        body = JSON.generate("domain" => "nilvaro.com", "name" => random_name, "expiry" => 600)
        resp = Http.post(BASE_URL + "/api/address", headers: HEADERS, body: body, timeout: 15)
        resp.raise_for_status
        data = resp.json
        email = data["address"].to_s.strip
        token = data["token"].to_s.strip
        raise "mytempmail-cc: 无效的邮箱响应" if email.empty? || !email.include?("@")
        raise "mytempmail-cc: 响应中缺少 token" if token.empty?

        EmailInfo.new(channel: CHANNEL, email: email, token: token)
      end

      # 获取 mytempmail.cc 邮件列表
      # @param token [String]
      # @param email [String]
      # @return [Array<Email>]
      def get_emails(token, email)
        token = token.to_s.strip
        raise "mytempmail-cc: token 为空" if token.empty?

        resp = Http.get("#{BASE_URL}/api/mails/#{token}", headers: HEADERS, timeout: 15)
        resp.raise_for_status
        data = resp.json
        results = data.is_a?(Hash) ? Array(data["results"]) : []

        results.filter_map do |msg|
          next unless msg.is_a?(Hash)

          flat = {
            "id" => msg["id"],
            "from" => msg["from"],
            "to" => email,
            "subject" => msg["subject"],
            "text" => msg["body_text"],
            "html" => msg["body_html"],
            "received_at" => msg["received_at"]
          }
          Normalize.normalize_email(flat, email)
        end
      end
    end
  end
end
