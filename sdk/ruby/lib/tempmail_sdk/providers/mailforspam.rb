# frozen_string_literal: true

require "uri"

module TempmailSdk
  module Providers
    # MailForSpam 渠道 — https://mailforspam.com
    # 匿名 REST API：邮箱列表接口 + 逐封详情
    module Mailforspam
      CHANNEL = "mailforspam"
      API_BASE = "https://api.mailforspam.com/api"
      DEFAULT_DOMAIN = "mailforspam.com"
      DOMAINS = %w[mailforspam.com tempmail.io disposable.email].freeze
      HEADERS = {
        "Accept" => "application/json",
        "Referer" => "https://mailforspam.com/",
        "Origin" => "https://mailforspam.com",
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

      def mailbox_emails_url(email, page_size = 100)
        "#{API_BASE}/mailboxes/#{URI.encode_www_form_component(email)}/emails" \
          "?page=1&page_size=#{page_size}&sort_by=date&sort_order=desc"
      end

      def flatten(raw, recipient)
        {
          "id" => raw["id"] || "", "from" => raw["sender"] || "",
          "to" => raw["receiver"] || recipient, "subject" => raw["subject"] || "",
          "text" => raw["body_text"] || "", "html" => raw["body_html"] || "",
          "date" => raw["date"] || "", "isRead" => !raw["readAt"].nil?,
          "attachments" => raw["attachments"]
        }
      end

      def fetch_message(message_id, email)
        resp = Http.get(
          "#{API_BASE}/mailboxes/#{URI.encode_www_form_component(email)}/emails/#{URI.encode_www_form_component(message_id)}",
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
        Http.get(mailbox_emails_url(email, 1), headers: HEADERS, timeout: 15).raise_for_status
        EmailInfo.new(channel: CHANNEL, email: email)
      end

      # @param email [String]
      # @return [Array<Email>]
      def get_emails(email)
        address = email.to_s.strip
        raise "mailforspam: empty email" if address.empty?

        resp = Http.get(mailbox_emails_url(address, 100), headers: HEADERS, timeout: 15)
        resp.raise_for_status
        data = resp.json
        rows = data.is_a?(Hash) ? data["emails"] : nil
        return [] unless rows.is_a?(Array)

        rows.filter_map do |item|
          next unless item.is_a?(Hash)

          message_id = item["id"].to_s.strip
          next if message_id.empty?

          detail = fetch_message(message_id, address)
          Normalize.normalize_email(flatten(detail || item, address), address)
        end
      end
    end
  end
end
