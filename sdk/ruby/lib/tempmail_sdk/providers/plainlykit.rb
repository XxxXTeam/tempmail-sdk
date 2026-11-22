# frozen_string_literal: true

require "cgi"

module TempmailSdk
  module Providers
    # PlainlyKit 渠道 — https://plainlykit.com
    # 创建邮箱返回内部令牌；内部令牌格式为 “邮箱ID|读取令牌”。
    module PlainlyKit
      CHANNEL = "plainlykit"
      BASE_URL = "https://plainlykit.com/tools/temporary-email/api"

      module_function

      def split_token(token)
        box_id, bearer = token.to_s.strip.split("|", 2)
        raise "plainlykit: 内部令牌格式错误" if box_id.to_s.empty? || bearer.to_s.empty?

        [box_id, bearer]
      end

      def fetch_json(url, bearer)
        resp = Http.get(url, headers: { "Accept" => "application/json",
                                        "Authorization" => "Bearer #{bearer}" }, timeout: 20)
        resp.raise_for_status
        resp.json
      end

      # 创建 PlainlyKit 临时邮箱
      # @return [EmailInfo]
      def generate_email
        resp = Http.post("#{BASE_URL}/mailboxes",
                         headers: { "Accept" => "application/json",
                                    "Content-Type" => "application/json" },
                         json: {}, timeout: 20)
        resp.raise_for_status
        data = resp.json
        address = data["address"].to_s.strip
        box_id = data["id"].to_s.strip
        token = data["token"].to_s.strip
        if address.empty? || box_id.empty? || token.empty?
          raise "plainlykit: 创建响应不完整"
        end

        EmailInfo.new(channel: CHANNEL, email: address, token: "#{box_id}|#{token}",
                      created_at: data["createdAt"], expires_at: data["expiresAt"])
      end

      # @param email [String]
      # @param token [String]
      # @return [Array<Email>]
      def get_emails(email, token)
        box_id, bearer = split_token(token)
        base = "#{BASE_URL}/mailboxes/#{CGI.escape(box_id)}/messages"
        data = fetch_json(base, bearer)
        (data["messages"] || []).filter_map do |item|
          message_id = item["id"].to_s.strip
          next if message_id.empty?

          begin
            detail = fetch_json("#{base}/#{CGI.escape(message_id)}", bearer)
            raw = detail["message"].is_a?(Hash) ? detail["message"] : detail
            from = raw["sender"].to_s.empty? ? item["sender"] : raw["sender"]
            subject = raw["subject"].to_s.empty? ? item["subject"] : raw["subject"]
            next Normalize.normalize_email({
                                             "id" => raw["id"] || message_id, "from" => from,
                                             "to" => email, "subject" => subject,
                                             "text" => raw["text"], "html" => raw["html"],
                                             "date" => raw["receivedAt"]
                                           }, email)
          rescue StandardError
            # 详情失败时退回摘要
          end

          Normalize.normalize_email({
                                      "id" => message_id, "from" => item["sender"], "to" => email,
                                      "subject" => item["subject"], "text" => item["preview"],
                                      "date" => item["receivedAt"]
                                    }, email)
        end
      end
    end
  end
end
