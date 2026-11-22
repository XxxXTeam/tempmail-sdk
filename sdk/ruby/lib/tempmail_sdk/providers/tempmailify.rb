# frozen_string_literal: true

require "cgi"

module TempmailSdk
  module Providers
    # Tempmailify 渠道 — https://tempmailify.com
    # 创建邮箱返回收件箱专属 Bearer 令牌；详情提供完整 text/html。
    module Tempmailify
      CHANNEL = "tempmailify"
      BASE_URL = "https://tempmailify.com"

      module_function

      def from_address(value)
        return value["address"].to_s if value.is_a?(Hash)

        value.to_s
      end

      def fetch_json(url, bearer)
        resp = Http.get(url, headers: { "Accept" => "application/json",
                                        "Authorization" => "Bearer #{bearer}" }, timeout: 20)
        resp.raise_for_status
        resp.json
      end

      # 创建 Tempmailify 临时邮箱
      # @return [EmailInfo]
      def generate_email
        resp = Http.post("#{BASE_URL}/api/inbox",
                         headers: { "Accept" => "application/json",
                                    "Content-Type" => "application/json" },
                         json: {}, timeout: 20)
        resp.raise_for_status
        data = resp.json
        address = data["address"].to_s.strip
        token = data["token"].to_s.strip
        raise "tempmailify: 创建响应不完整" if address.empty? || token.empty?

        EmailInfo.new(channel: CHANNEL, email: address, token: token,
                      created_at: data["createdAt"], expires_at: data["expiresAt"])
      end

      # @param email [String]
      # @param token [String]
      # @return [Array<Email>]
      def get_emails(email, token)
        address = email.to_s.strip
        bearer = token.to_s.strip
        raise "tempmailify: 邮箱地址或令牌为空" if address.empty? || bearer.empty?

        base = "#{BASE_URL}/api/inbox/#{CGI.escape(address).gsub('%40', '@')}"
        data = fetch_json(base, bearer)
        (data["messages"] || []).filter_map do |item|
          message_id = item["id"].to_s.strip
          next if message_id.empty?

          begin
            detail = fetch_json("#{base}/messages/#{CGI.escape(message_id)}", bearer)
            next Normalize.normalize_email({
                                             "id" => detail["id"] || message_id,
                                             "from" => from_address(detail["from"]),
                                             "to" => detail["to"] || address,
                                             "subject" => detail["subject"] || item["subject"],
                                             "text" => detail["text"], "html" => detail["html"],
                                             "date" => detail["receivedAt"] || item["receivedAt"],
                                             "seen" => detail["seen"],
                                             "attachments" => detail["attachments"]
                                           }, address)
          rescue StandardError
            # 详情失败时退回摘要
          end

          Normalize.normalize_email({
                                      "id" => message_id, "from" => from_address(item["from"]),
                                      "to" => address, "subject" => item["subject"],
                                      "text" => item["preview"], "date" => item["receivedAt"],
                                      "seen" => item["seen"]
                                    }, address)
        end
      end
    end
  end
end
