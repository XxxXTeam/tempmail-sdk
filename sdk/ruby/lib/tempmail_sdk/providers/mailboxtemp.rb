# frozen_string_literal: true

require "cgi"

module TempmailSdk
  module Providers
    # MailboxTemp 渠道 — https://mailboxtemp.com
    # 无需密钥；创建免费邮箱后，地址即读取凭据。
    module MailboxTemp
      CHANNEL = "mailboxtemp"
      BASE_URL = "https://mailboxtemp.com"
      HEADERS = { "Accept" => "application/json" }.freeze

      module_function

      def fetch_json(url)
        resp = Http.get(url, headers: HEADERS, timeout: 20)
        data = resp.json
        raise "mailboxtemp: HTTP #{resp.status_code}" if resp.status_code >= 400 || data["ok"] == false

        data
      end

      # 创建 MailboxTemp 临时邮箱
      # @return [EmailInfo]
      def generate_email
        resp = Http.post("#{BASE_URL}/api/inbox/generate", headers: HEADERS, timeout: 20)
        data = resp.json
        address = data["address"].to_s.strip
        if resp.status_code >= 400 || data["ok"] == false || address.empty?
          raise "mailboxtemp: 创建邮箱 HTTP #{resp.status_code}"
        end

        EmailInfo.new(channel: CHANNEL, email: address, expires_at: data["expires"])
      end

      # @param email [String]
      # @return [Array<Email>]
      def get_emails(email)
        address = email.to_s.strip
        raise "mailboxtemp: 邮箱地址为空" if address.empty?

        path = CGI.escape(address)
        data = fetch_json("#{BASE_URL}/api/inbox/#{path}/emails")
        (data["emails"] || []).filter_map do |item|
          message_id = item["id"].to_s.strip
          next if message_id.empty?

          begin
            detail = fetch_json("#{BASE_URL}/api/email/#{path}/#{CGI.escape(message_id)}")
            message = detail["email"]
            if message.is_a?(Hash)
              next Normalize.normalize_email({
                                               "id" => message["id"], "from" => message["from_address"],
                                               "to" => address, "subject" => message["subject"],
                                               "text" => message["body_text"], "html" => message["body_html"],
                                               "date" => message["received_at"], "read" => message["read"],
                                               "attachments" => detail["attachments"]
                                             }, address)
            end
          rescue StandardError
            # 详情失败时退回摘要
          end

          Normalize.normalize_email({
                                      "id" => message_id, "from" => item["from_address"], "to" => address,
                                      "subject" => item["subject"], "date" => item["received_at"],
                                      "read" => item["read"]
                                    }, address)
        end
      end
    end
  end
end
