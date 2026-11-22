# frozen_string_literal: true

require "cgi"

module TempmailSdk
  module Providers
    # Nextmail 渠道 — https://nextmail.sh
    # 创建收件箱返回内部令牌；内部令牌格式为 “收件箱ID|读取令牌”。
    # 详情正文位于 message.decodedBody（HTML）。
    module Nextmail
      CHANNEL = "nextmail"
      BASE_URL = "https://api.nextmail.sh"

      module_function

      def split_token(token)
        box_id, bearer = token.to_s.strip.split("|", 2)
        raise "nextmail: 内部令牌格式错误" if box_id.to_s.empty? || bearer.to_s.empty?

        [box_id, bearer]
      end

      def fetch_json(url, bearer)
        resp = Http.get(url, headers: { "Accept" => "application/json",
                                        "Authorization" => "Bearer #{bearer}" }, timeout: 20)
        resp.raise_for_status
        resp.json
      end

      def from_email(value)
        value.is_a?(Hash) ? value["email"].to_s : value.to_s
      end

      # 租用 Nextmail 临时收件箱
      # @return [EmailInfo]
      def generate_email
        resp = Http.post("#{BASE_URL}/temp-mail/inboxes",
                         headers: { "Accept" => "application/json",
                                    "Content-Type" => "application/json" },
                         json: {}, timeout: 20)
        resp.raise_for_status
        data = resp.json
        address = data["address"].to_s.strip
        box_id = data["id"].to_s.strip
        token = data["token"].to_s.strip
        if address.empty? || box_id.empty? || token.empty?
          raise "nextmail: 创建响应不完整"
        end

        EmailInfo.new(channel: CHANNEL, email: address, token: "#{box_id}|#{token}",
                      created_at: data["createdAt"], expires_at: data["expiresAt"])
      end

      # @param email [String]
      # @param token [String]
      # @return [Array<Email>]
      def get_emails(email, token)
        box_id, bearer = split_token(token)
        base = "#{BASE_URL}/temp-mail/inboxes/#{CGI.escape(box_id)}/messages"
        data = fetch_json("#{base}?limit=50", bearer)
        (data["data"] || []).filter_map do |item|
          message_id = item["id"].to_s.strip
          next if message_id.empty?

          begin
            detail = fetch_json("#{base}/#{CGI.escape(message_id)}", bearer)
            message = detail["message"].is_a?(Hash) ? detail["message"] : {}
            next Normalize.normalize_email({
                                             "id" => detail["id"] || message_id,
                                             "from" => from_email(detail["from"]),
                                             "to" => detail["to"] || email,
                                             "subject" => detail["subject"],
                                             "html" => message["decodedBody"],
                                             "date" => detail["receivedAt"], "seen" => detail["seen"]
                                           }, email)
          rescue StandardError
            # 详情失败时退回摘要
          end

          Normalize.normalize_email({
                                      "id" => message_id, "from" => from_email(item["from"]),
                                      "to" => item["to"] || email, "subject" => item["subject"],
                                      "text" => item["snippet"], "date" => item["receivedAt"],
                                      "seen" => item["seen"]
                                    }, email)
        end
      end
    end
  end
end
