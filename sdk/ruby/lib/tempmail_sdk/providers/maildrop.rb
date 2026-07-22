# frozen_string_literal: true

module TempmailSdk
  module Providers
    # Maildrop — https://maildrop.cx
    # GET /api/suffixes.php、GET /api/emails.php
    module Maildrop
      CHANNEL = "maildrop"
      BASE = "https://maildrop.cx"
      EXCLUDED_SUFFIX = "transformer.edu.kg"
      HEADERS = {
        "Accept" => "application/json, text/plain, */*",
        "Referer" => "https://maildrop.cx/zh-cn/app",
        "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36",
        "x-requested-with" => "XMLHttpRequest"
      }.freeze

      module_function

      def random_local(length = 10)
        chars = ("a".."z").to_a + ("0".."9").to_a
        Array.new(length) { chars.sample }.join
      end

      def fetch_suffixes
        r = Http.get("#{BASE}/api/suffixes.php", headers: HEADERS)
        r.raise_for_status
        data = r.json
        raise "maildrop: invalid suffixes response" unless data.is_a?(Array)

        ex = EXCLUDED_SUFFIX.downcase
        out = data.select { |x| x.is_a?(String) }.map(&:strip).reject { |t| t.empty? || t.downcase == ex }
        raise "maildrop: no domains available" if out.empty?

        out
      end

      def pick_suffix(suffixes, preferred)
        if preferred && !preferred.to_s.strip.empty?
          p = preferred.to_s.strip.downcase
          found = suffixes.find { |d| d.downcase == p }
          raise "maildrop: domain not available: #{p}" unless found

          return found
        end
        suffixes.sample
      end

      def cx_date_to_iso(str)
        s = str.to_s.strip
        return "#{s[0, 10]}T#{s[11..]}Z" if s.length == 19 && s[10] == " "

        s
      end

      # @param domain [String, nil]
      # @return [EmailInfo]
      def generate_email(domain = nil)
        suffixes = fetch_suffixes
        dom = pick_suffix(suffixes, domain)
        email = "#{random_local}@#{dom}"
        EmailInfo.new(channel: CHANNEL, email: email, token: email)
      end

      # @param email [String]
      # @param token [String, nil]
      # @return [Array<Email>]
      def get_emails(email, token = nil)
        addr = email.to_s.strip
        addr = token.to_s.strip if addr.empty?
        raise "maildrop: empty address" if addr.empty?

        qs = URI.encode_www_form(addr: addr, page: "1", limit: "20")
        r = Http.get("#{BASE}/api/emails.php?#{qs}", headers: HEADERS)
        r.raise_for_status
        data = r.body.empty? ? {} : r.json
        rows = data["emails"] || []
        return [] unless rows.is_a?(Array)

        rows.select { |row| row.is_a?(Hash) }.map do |row|
          ir = row["isRead"]
          Email.new(
            id: row["id"].to_s, from_addr: (row["from_addr"] || "").strip, to: addr,
            subject: (row["subject"] || "").strip, text: (row["description"] || "").strip,
            html: "", date: cx_date_to_iso(row["date"] || ""),
            is_read: ir == true || ir == 1, attachments: []
          )
        end
      end
    end
  end
end
