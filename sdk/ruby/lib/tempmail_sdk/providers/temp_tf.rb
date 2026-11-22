# frozen_string_literal: true

require "cgi"

module TempmailSdk
  module Providers
    # temp.tf 渠道 — https://temp.tf
    # 创建返回一次性地址；收件列表响应按 bodyContentType 归位正文。
    module TempTf
      CHANNEL = "temp-tf"
      BASE_URL = "https://temp.tf/api"
      HEADERS = { "Accept" => "application/json", "Content-Type" => "application/json" }.freeze

      module_function

      # 获取一个一次性地址
      # @return [EmailInfo]
      def generate_email
        resp = Http.get("#{BASE_URL}/account?plus=1", headers: HEADERS, timeout: 20)
        resp.raise_for_status
        address = resp.json["email"].to_s.strip
        raise "temp-tf: 创建响应缺少地址" if address.empty?

        EmailInfo.new(channel: CHANNEL, email: address)
      end

      # @param email [String]
      # @return [Array<Email>]
      def get_emails(email)
        address = email.to_s.strip
        raise "temp-tf: 邮箱地址为空" if address.empty?

        resp = Http.post("#{BASE_URL}/check", headers: HEADERS, json: { email: address }, timeout: 20)
        resp.raise_for_status
        (resp.json["data"] || []).map do |item|
          is_text = item["bodyContentType"].to_s.downcase == "text"
          body = item["body"].to_s
          Normalize.normalize_email({
                                      "id" => item["id"], "from" => item["from"], "to" => address,
                                      "subject" => item["subject"],
                                      "text" => is_text ? body : "", "html" => is_text ? "" : body,
                                      "date" => item["date"], "attachments" => item["attachments"]
                                    }, address)
        end
      end
    end
  end
end
