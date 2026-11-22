# frozen_string_literal: true

require "cgi"

module TempmailSdk
  module Providers
    # tempmails.cc 渠道 — https://tempmails.cc
    # 地址即邮箱：在公开免费域名上生成地址，无需注册。
    # 详情正文位于 textBody / htmlBody。
    module TempmailsCc
      CHANNEL = "tempmails-cc"
      BASE_URL = "https://tempmails.cc"
      HEADERS = { "Accept" => "application/json" }.freeze

      module_function

      def random_local
        chars = ("a".."z").to_a + ("0".."9").to_a
        Array.new(16) { chars.sample }.join
      end

      def get_json(url)
        resp = Http.get(url, headers: HEADERS, timeout: 20)
        resp.raise_for_status
        resp.json
      end

      def pick_domain
        data = get_json("#{BASE_URL}/api/domains")
        domain = (data["domainInfo"] || []).filter_map do |item|
          d = item["domain"].to_s.strip
          d unless d.empty?
        end.first
        raise "tempmails-cc: 无可用公开域名" unless domain

        domain
      end

      # 在公开免费域名上生成地址
      # @return [EmailInfo]
      def generate_email
        EmailInfo.new(channel: CHANNEL, email: "#{random_local}@#{pick_domain}")
      end

      # 跳过选域，在指定固定域名上生成地址
      # @param domain [String]
      # @param channel [String]
      # @return [EmailInfo]
      def generate_email_at_domain(domain, channel = CHANNEL)
        selected = domain.to_s.strip
        raise "tempmails-cc: 域名为空" if selected.empty?

        EmailInfo.new(channel: channel, email: "#{random_local}@#{selected}")
      end

      # @param email [String]
      # @return [Array<Email>]
      def get_emails(email)
        address = email.to_s.strip
        raise "tempmails-cc: 邮箱地址为空" if address.empty?

        data = get_json("#{BASE_URL}/api/inbox/#{CGI.escape(address).gsub('%40', '@')}")
        (data["messages"] || []).filter_map do |item|
          message_id = item["id"].to_s.strip
          next if message_id.empty?

          begin
            detail = get_json("#{BASE_URL}/api/message/#{CGI.escape(message_id)}" \
                              "?address=#{CGI.escape(address).gsub('%40', '@')}&mark_read=false")
            to = detail["to"].is_a?(Array) ? detail["to"].first : detail["to"]
            next Normalize.normalize_email({
                                             "id" => detail["id"] || message_id,
                                             "from" => detail["from"], "to" => to || address,
                                             "subject" => detail["subject"],
                                             "text" => detail["textBody"], "html" => detail["htmlBody"],
                                             "date" => detail["date"], "seen" => detail["isRead"]
                                           }, address)
          rescue StandardError
            # 详情失败时退回摘要
          end

          Normalize.normalize_email({
                                      "id" => message_id, "from" => item["from"], "to" => address,
                                      "subject" => item["subject"], "date" => item["date"],
                                      "seen" => item["isRead"]
                                    }, address)
        end
      end
    end
  end
end
