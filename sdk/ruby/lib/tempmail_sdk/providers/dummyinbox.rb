# frozen_string_literal: true

require "cgi"

module TempmailSdk
  module Providers
    # DummyInbox 渠道 — https://dummyinbox.com
    # 无需密钥；创建随机地址后，地址本身即读取凭据。
    module DummyInbox
      CHANNEL = "dummyinbox"
      BASE_URL = "https://dummyinbox.com/api/v1"
      HEADERS = { "Accept" => "application/json" }.freeze

      module_function

      def path_address(address)
        CGI.escape(address.strip).gsub("%40", "@")
      end

      def fetch_json(url)
        resp = Http.get(url, headers: HEADERS, timeout: 20)
        resp.raise_for_status
        resp.json
      end

      # 创建 DummyInbox 临时邮箱
      # @return [EmailInfo]
      def generate_email
        resp = Http.post("#{BASE_URL}/inboxes", headers: HEADERS, timeout: 20)
        resp.raise_for_status
        address = resp.json["address"].to_s.strip
        raise "dummyinbox: 创建响应缺少地址" if address.empty?

        EmailInfo.new(channel: CHANNEL, email: address)
      end

      # @param email [String]
      # @return [Array<Email>]
      def get_emails(email)
        address = email.to_s.strip
        raise "dummyinbox: 邮箱地址为空" if address.empty?

        path = path_address(address)
        data = fetch_json("#{BASE_URL}/inboxes/#{path}/messages?limit=50")
        (data["messages"] || []).filter_map do |item|
          message_id = item["id"].to_s.strip
          next if message_id.empty?

          begin
            detail = fetch_json("#{BASE_URL}/inboxes/#{path}/messages/#{CGI.escape(message_id)}")
            message = detail["message"]
            if message.is_a?(Hash)
              to = message["to"].is_a?(Array) ? message["to"].first : message["to"]
              next Normalize.normalize_email({
                                               "id" => message["id"], "from" => message["from"],
                                               "to" => to || address, "subject" => message["subject"],
                                               "text" => message["text"], "html" => message["html"],
                                               "date" => message["receivedAt"],
                                               "attachments" => message["attachments"]
                                             }, address)
            end
          rescue StandardError
            # 详情失败时退回摘要
          end

          Normalize.normalize_email({
                                      "id" => message_id, "from" => item["from"], "to" => address,
                                      "subject" => item["subject"], "text" => item["snippet"],
                                      "date" => item["receivedAt"]
                                    }, address)
        end
      end
    end
  end
end
