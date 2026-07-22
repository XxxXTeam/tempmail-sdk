# frozen_string_literal: true

module TempmailSdk
  module Providers
    # inboxes.com 渠道实现
    module Inboxes
      CHANNEL = "inboxes"
      API_BASE = "https://inboxes.com/api/v2"
      DEFAULT_DOMAIN = "blondmail.com"
      HEADERS = {
        "Accept" => "application/json",
        "Origin" => "https://inboxes.com",
        "Referer" => "https://inboxes.com/",
        "User-Agent" => "Mozilla/5.0"
      }.freeze

      module_function

      def random_local
        chars = "abcdefghijklmnopqrstuvwxyz0123456789".chars
        "sdk" + Array.new(16) { chars.sample }.join
      end

      # 获取可用域名列表
      def get_domains
        resp = Http.get("#{API_BASE}/domain", headers: HEADERS, timeout: 15)
        resp.raise_for_status
        data = resp.json
        items = data.is_a?(Hash) ? (data["domains"] || []) : []
        domains = items.select { |i| i.is_a?(Hash) && !i["qdn"].to_s.strip.empty? }
                       .map { |i| i["qdn"].to_s.strip }
        raise "inboxes: no domains" if domains.empty?

        domains
      end

      def pick_domain(domains, preferred)
        wanted = preferred.to_s.strip.delete_prefix("@").downcase
        unless wanted.empty?
          domains.each { |d| return d if d.downcase == wanted }
        end
        return DEFAULT_DOMAIN if domains.include?(DEFAULT_DOMAIN)

        domains.first
      end

      def flatten(raw, recipient)
        {
          "id" => (raw["uid"] || "").to_s,
          "from" => (raw["sf"] || raw["f"] || "").to_s,
          "to" => (raw["ib"] || recipient).to_s,
          "subject" => (raw["s"] || "").to_s,
          "text" => (raw["text"] || "").to_s,
          "html" => (raw["html"] || "").to_s,
          "preview_text" => (raw["ph"] || "").to_s,
          "date" => (raw["cr"] || "").to_s,
          "isRead" => !!raw["ru"],
          "attachments" => raw["at"].is_a?(Array) ? raw["at"] : []
        }
      end

      # @param domain [String, nil] 可选指定域名
      # @return [EmailInfo]
      def generate_email(domain = nil)
        domains = get_domains
        selected = pick_domain(domains, domain)
        EmailInfo.new(channel: CHANNEL, email: "#{random_local}@#{selected}")
      end

      def fetch_detail(uid)
        return nil if uid.to_s.strip.empty?

        begin
          resp = Http.get(
            "#{API_BASE}/message/#{URI.encode_www_form_component(uid)}",
            headers: HEADERS, timeout: 15
          )
          return nil unless resp.ok?

          data = resp.json
          data.is_a?(Hash) ? data : nil
        rescue StandardError
          nil
        end
      end

      # @param email [String]
      # @return [Array<Email>]
      def get_emails(email)
        address = email.to_s.strip
        raise "inboxes: empty email" if address.empty?

        resp = Http.get(
          "#{API_BASE}/inbox/#{URI.encode_www_form_component(address)}",
          headers: HEADERS, timeout: 15
        )
        resp.raise_for_status
        data = resp.json
        rows = data.is_a?(Hash) ? (data["msgs"] || []) : []
        return [] unless rows.is_a?(Array)

        rows.select { |r| r.is_a?(Hash) }.map do |row|
          uid = (row["uid"] || "").to_s.strip
          detail = uid.empty? ? nil : fetch_detail(uid)
          Normalize.normalize_email(flatten(detail || row, address), address)
        end
      end
    end
  end
end
