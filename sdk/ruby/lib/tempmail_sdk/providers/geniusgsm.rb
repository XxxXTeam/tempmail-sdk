# frozen_string_literal: true

require "cgi"

module TempmailSdk
  module Providers
    # GeniusGSM 渠道 — https://tempmail.geniusgsm.com
    # 创建收件箱返回内部 ID；收件箱列表响应已包含 body_text / body_html。
    module GeniusGSM
      CHANNEL = "geniusgsm"
      BASE_URL = "https://tempmail.geniusgsm.com/api"
      HEADERS = { "Accept" => "application/json", "Content-Type" => "application/json" }.freeze

      module_function

      def rows(data)
        return data if data.is_a?(Array)
        return data["results"] if data.is_a?(Hash) && data["results"].is_a?(Array)

        []
      end

      def from_value(raw)
        name = raw["from_name"].to_s
        addr = raw["from_email"].to_s
        name.empty? || addr.empty? ? addr : "#{name} <#{addr}>"
      end

      # 选择激活域名并创建收件箱
      # @return [EmailInfo]
      def generate_email
        domains_resp = Http.get("#{BASE_URL}/domains/", headers: HEADERS, timeout: 20)
        domains_resp.raise_for_status
        active = rows(domains_resp.json).find { |item| item["is_active"] }
        raise "geniusgsm: 无可用激活域名" unless active

        resp = Http.post("#{BASE_URL}/inboxes/create_inbox/",
                         headers: HEADERS, json: { domain_id: active["id"], ttl: 3600 }, timeout: 20)
        resp.raise_for_status
        data = resp.json
        inbox_id = data["id"].to_s.strip
        address = data["email"].to_s.strip
        raise "geniusgsm: 创建响应不完整" if inbox_id.empty? || address.empty?

        EmailInfo.new(channel: CHANNEL, email: address, token: inbox_id,
                      created_at: data["created_at"], expires_at: data["expires_at"])
      end

      # @param email [String]
      # @param token [String] 收件箱 ID
      # @return [Array<Email>]
      def get_emails(email, token)
        inbox_id = token.to_s.strip
        raise "geniusgsm: 收件箱 ID 为空" if inbox_id.empty?

        resp = Http.get("#{BASE_URL}/inboxes/#{CGI.escape(inbox_id)}/emails/",
                        headers: HEADERS, timeout: 20)
        resp.raise_for_status
        rows(resp.json).map do |item|
          Normalize.normalize_email({
                                      "id" => item["id"], "from" => from_value(item),
                                      "to" => item["inbox_email"] || email,
                                      "subject" => item["subject"],
                                      "text" => item["body_text"], "html" => item["body_html"],
                                      "date" => item["received_at"], "read" => item["is_read"],
                                      "attachments" => item["attachments"]
                                    }, email)
        end
      end
    end
  end
end
