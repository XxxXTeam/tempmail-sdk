# frozen_string_literal: true

require "uri"

module TempmailSdk
  module Providers
    # Catchmail 渠道 — https://catchmail.io
    # 匿名 REST API：创建邮箱即列表接口，逐封拉取详情
    module Catchmail
      CHANNEL = "catchmail"
      API_BASE = "https://api.catchmail.io/api/v1"
      DEFAULT_DOMAIN = "catchmail.io"
      DOMAINS = %w[catchmail.io mailistry.com zeppost.com].freeze
      HEADERS = {
        "Accept" => "application/json",
        "Referer" => "https://catchmail.io/",
        "Origin" => "https://catchmail.io",
        "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"
      }.freeze

      module_function

      def random_local
        chars = ("a".."z").to_a + ("0".."9").to_a
        "sdk#{Array.new(16) { chars.sample }.join}"
      end

      # 选择域名：命中别名域名则用之，否则默认域名
      def pick_domain(preferred)
        p = preferred.to_s.strip.sub(/\A@/, "").downcase
        unless p.empty?
          found = DOMAINS.find { |d| d.downcase == p }
          return found if found
        end
        DEFAULT_DOMAIN
      end

      # 从可能带显示名的地址字段提取纯地址
      def clean_address(value)
        value = value.first if value.is_a?(Array)
        text = value.to_s.strip
        m = text.match(/<([^>]+)>/)
        m ? m[1].strip : text
      end

      def flatten_detail(raw, recipient)
        body = raw["body"].is_a?(Hash) ? raw["body"] : {}
        {
          "id" => raw["id"] || "", "from" => clean_address(raw["from"]),
          "to" => (a = clean_address(raw["to"])).empty? ? recipient : a,
          "subject" => raw["subject"] || "", "text" => body["text"] || "",
          "html" => body["html"] || "", "date" => raw["date"] || "",
          "attachments" => raw["attachments"]
        }
      end

      def fetch_message(message_id, email)
        resp = Http.get(
          "#{API_BASE}/message/#{URI.encode_www_form_component(message_id)}?mailbox=#{URI.encode_www_form_component(email)}",
          headers: HEADERS, timeout: 15
        )
        return nil unless resp.ok?

        data = resp.json
        data.is_a?(Hash) ? data : nil
      end

      # @param domain [String, nil]
      # @return [EmailInfo]
      def generate_email(domain = nil)
        email = "#{random_local}@#{pick_domain(domain)}"
        Http.get("#{API_BASE}/mailbox?address=#{URI.encode_www_form_component(email)}",
                 headers: HEADERS, timeout: 15).raise_for_status
        EmailInfo.new(channel: CHANNEL, email: email)
      end

      # @param email [String]
      # @return [Array<Email>]
      def get_emails(email)
        address = email.to_s.strip
        raise "catchmail: empty email" if address.empty?

        resp = Http.get("#{API_BASE}/mailbox?address=#{URI.encode_www_form_component(address)}",
                        headers: HEADERS, timeout: 15)
        resp.raise_for_status
        data = resp.json
        rows = data.is_a?(Hash) ? data["messages"] : nil
        return [] unless rows.is_a?(Array)

        rows.filter_map do |item|
          next unless item.is_a?(Hash)

          message_id = item["id"].to_s.strip
          next if message_id.empty?

          detail = fetch_message(message_id, address)
          if detail
            Normalize.normalize_email(flatten_detail(detail, address), address)
          else
            Normalize.normalize_email({
                                        "id" => message_id, "from" => clean_address(item["from"]),
                                        "to" => item["mailbox"] || address,
                                        "subject" => item["subject"] || "", "date" => item["date"] || ""
                                      }, address)
          end
        end
      end
    end
  end
end
