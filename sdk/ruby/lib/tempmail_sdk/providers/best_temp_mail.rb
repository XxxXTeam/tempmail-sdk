# frozen_string_literal: true

require "json"
require "securerandom"

module TempmailSdk
  module Providers
    # best-temp-mail.com 渠道实现
    #
    # 纯 JSON REST API，无需认证，无需 Session。
    # 创建邮箱: POST /api/v3/createEmail  body: {"intToken":"<uuid>"}
    # 获取邮件: POST /api/v3/getEmailList  body: {"address":"...","id":"...","intToken":"...","update_tag":"..."}
    module BestTempMail
      CHANNEL = "best-temp-mail"
      API_BASE = "https://best-temp-mail.com/api/v3"

      HEADERS = {
        "Accept" => "application/json, text/plain, */*",
        "Accept-Language" => "zh-CN,zh;q=0.9,en;q=0.8",
        "Content-Type" => "application/json",
        "Origin" => "https://best-temp-mail.com",
        "Referer" => "https://best-temp-mail.com/",
        "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " \
                       "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36"
      }.freeze

      module_function

      # 创建临时邮箱
      # @return [EmailInfo]
      def generate_email
        int_token = SecureRandom.uuid
        resp = Http.post("#{API_BASE}/createEmail",
                         headers: HEADERS,
                         json: { "intToken" => int_token })
        resp.raise_for_status
        data = resp.json
        raise "best-temp-mail: 创建邮箱失败 status=#{data['status']}" \
          if data["status"] != "success" || data.dig("data", "address").to_s.empty?

        d = data["data"]
        token = JSON.generate("intToken" => int_token, "id" => d["id"].to_s, "update_tag" => d["update_tag"].to_s)
        EmailInfo.new(channel: CHANNEL, email: d["address"], token: token)
      end

      # 获取邮件列表
      # @param email [String]
      # @param token [String] JSON 编码的 intToken/id/update_tag
      # @return [Array<Email>]
      def get_emails(email, token)
        tkn = JSON.parse(token)
        resp = Http.post("#{API_BASE}/getEmailList",
                         headers: HEADERS,
                         json: {
                           "address" => email,
                           "id" => tkn["id"].to_s,
                           "intToken" => tkn["intToken"].to_s,
                           "update_tag" => tkn["update_tag"].to_s
                         })
        resp.raise_for_status
        data = resp.json
        return [] unless data.dig("data", "hasNewEmail")

        emails_raw = data.dig("data", "emails")
        return [] unless emails_raw.is_a?(Array)

        emails_raw.filter_map do |raw|
          next unless raw.is_a?(Hash)

          Normalize.normalize_email(raw, email)
        end
      rescue JSON::ParserError
        raise "best-temp-mail: 无效的 token 格式"
      end
    end
  end
end
