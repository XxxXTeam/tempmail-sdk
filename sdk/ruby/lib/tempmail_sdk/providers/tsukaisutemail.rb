# frozen_string_literal: true

require "cgi"

module TempmailSdk
  module Providers
    # Tsukaisute Mail 渠道 — https://tsukaisutemail.com
    # 无需密钥；创建随机地址后，地址即读取凭据。
    module TsukaisuteMail
      CHANNEL = "tsukaisutemail"
      BASE_URL = "https://tsukaisutemail.com/api/v1"
      HEADERS = { "Accept" => "application/json" }.freeze

      module_function

      def fetch_json(url)
        resp = Http.get(url, headers: HEADERS, timeout: 20)
        resp.raise_for_status
        resp.json
      end

      # 创建 Tsukaisute Mail 临时地址
      # @return [EmailInfo]
      def generate_email
        resp = Http.post("#{BASE_URL}/address", headers: HEADERS, timeout: 20)
        resp.raise_for_status
        address = resp.json["address"].to_s.strip
        raise "tsukaisutemail: 创建响应缺少地址" if address.empty?

        EmailInfo.new(channel: CHANNEL, email: address)
      end

      # @param email [String]
      # @return [Array<Email>]
      def get_emails(email)
        address = email.to_s.strip
        raise "tsukaisutemail: 邮箱地址为空" if address.empty?

        data = fetch_json("#{BASE_URL}/inbox/#{CGI.escape(address).gsub("%40", "@")}")
        (data["messages"] || []).filter_map do |item|
          message_id = item["id"].to_s.strip
          next if message_id.empty?

          begin
            detail = fetch_json("#{BASE_URL}/message/#{CGI.escape(message_id)}")
            message = detail["message"]
            if message.is_a?(Hash)
              next Normalize.normalize_email({
                                               "id" => message["id"], "from" => message["from_address"],
                                               "to" => address, "subject" => message["subject"],
                                               "text" => message["text_body"], "html" => message["html_body"],
                                               "date" => message["received_at"]
                                             }, address)
            end
          rescue StandardError
            # 详情失败时退回摘要
          end

          Normalize.normalize_email({
                                      "id" => message_id, "from" => item["from_address"], "to" => address,
                                      "subject" => item["subject"], "text" => item["snippet"],
                                      "date" => item["received_at"]
                                    }, address)
        end
      end
    end
  end
end
