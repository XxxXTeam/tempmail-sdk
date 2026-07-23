# frozen_string_literal: true

module TempmailSdk
  module Providers
    # mailcat.ai 渠道实现
    #
    # 流程：POST /mailboxes 创建邮箱（无 body），返回 email + token
    #       GET /inbox 获取邮件列表，需要 Authorization: Bearer {token}
    module MailcatAi
      CHANNEL = "mailcat-ai"
      BASE_URL = "https://api.mailcat.ai"

      HEADERS = {
        "Accept" => "application/json",
        "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " \
                        "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"
      }.freeze

      module_function

      # 创建 mailcat.ai 临时邮箱
      # @return [EmailInfo]
      def generate_email
        resp = Http.post("#{BASE_URL}/mailboxes", headers: HEADERS, timeout: 15)
        resp.raise_for_status
        data = resp.json
        email = data.dig("data", "email").to_s.strip
        token = data.dig("data", "token").to_s.strip
        raise "mailcat-ai: 响应缺少 email 或 token" if email.empty? || token.empty?

        EmailInfo.new(channel: CHANNEL, email: email, token: token)
      end

      # 获取 mailcat.ai 邮件列表
      # @param token [String] Bearer token
      # @param email [String] 邮箱地址
      # @return [Array<Email>]
      def get_emails(token, email)
        hdrs = HEADERS.merge("Authorization" => "Bearer #{token.to_s.strip}")
        resp = Http.get("#{BASE_URL}/inbox", headers: hdrs, timeout: 15)
        resp.raise_for_status
        data = resp.json
        items = data.is_a?(Hash) ? Array(data["data"]) : []
        items.filter_map do |raw|
          next unless raw.is_a?(Hash)

          Normalize.normalize_email(raw.merge("to" => email), email)
        end
      end
    end
  end
end
