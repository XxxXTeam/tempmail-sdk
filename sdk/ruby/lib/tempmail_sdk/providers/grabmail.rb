# frozen_string_literal: true

require "cgi"

module TempmailSdk
  module Providers
    # GrabMail 渠道 — https://grabmail.io
    # 地址即邮箱：首封邮件到达时自动存在，无需创建。
    module Grabmail
      CHANNEL = "grabmail"
      BASE_URL = "https://grabmail.io/api/v1"
      DOMAIN = "grabmail.io"
      HEADERS = { "Accept" => "application/json" }.freeze

      module_function

      def random_local
        chars = ("a".."z").to_a + ("0".."9").to_a
        "sdk" + Array.new(16) { chars.sample }.join
      end

      def get_json(url)
        resp = Http.get(url, headers: HEADERS, timeout: 20)
        resp.raise_for_status
        resp.json
      end

      def from_value(raw)
        name = raw["from_name"].to_s
        addr = raw["from"].to_s
        name.empty? ? addr : "#{name} <#{addr}>"
      end

      # 生成 GrabMail 地址（无需请求服务端）
      # @return [EmailInfo]
      def generate_email
        EmailInfo.new(channel: CHANNEL, email: "#{random_local}@#{DOMAIN}")
      end

      # @param email [String]
      # @return [Array<Email>]
      def get_emails(email)
        address = email.to_s.strip
        raise "grabmail: 邮箱地址为空" if address.empty?

        data = get_json("#{BASE_URL}/mailbox?address=#{CGI.escape(address).gsub('%40', '@')}")
        (data["messages"] || []).filter_map do |item|
          message_id = item["id"].to_s.strip
          next if message_id.empty?

          begin
            detail = get_json("#{BASE_URL}/message/#{CGI.escape(message_id)}" \
                              "?mailbox=#{CGI.escape(address).gsub('%40', '@')}")
            next Normalize.normalize_email({
                                             "id" => detail["id"] || message_id,
                                             "from" => from_value(detail), "to" => address,
                                             "subject" => detail["subject"],
                                             "text" => detail["text"], "html" => detail["html"],
                                             "date" => detail["date"], "seen" => detail["seen"],
                                             "attachments" => detail["attachments"]
                                           }, address)
          rescue StandardError
            # 详情失败时退回摘要
          end

          Normalize.normalize_email({
                                      "id" => message_id, "from" => from_value(item),
                                      "to" => address, "subject" => item["subject"],
                                      "text" => item["preview"], "date" => item["date"],
                                      "seen" => item["seen"]
                                    }, address)
        end
      end
    end
  end
end
