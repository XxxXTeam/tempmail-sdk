# frozen_string_literal: true

require "uri"

module TempmailSdk
  module Providers
    # Fake Legal 渠道 — https://imgui.de
    # imgui.de / pulsewebmenu.de 用 POST 保根域，其余域用 GET 创建
    module FakeLegal
      CHANNEL = "fake-legal"
      BASE = "https://imgui.de"
      HEADERS = {
        "Accept" => "application/json, text/plain, */*",
        "Accept-Language" => "zh-CN,zh;q=0.9,en;q=0.8",
        "Cache-Control" => "no-cache",
        "DNT" => "1",
        "Pragma" => "no-cache",
        "Referer" => "https://imgui.de/",
        "sec-ch-ua" => '"Chromium";v="146", "Not-A.Brand";v="24", "Microsoft Edge";v="146"',
        "sec-ch-ua-mobile" => "?0",
        "sec-ch-ua-platform" => '"Windows"',
        "sec-fetch-dest" => "empty",
        "sec-fetch-mode" => "cors",
        "sec-fetch-site" => "same-origin",
        "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"
      }.freeze

      module_function

      def fetch_domains
        resp = Http.get("#{BASE}/api/domains", headers: HEADERS, timeout: 15)
        resp.raise_for_status
        data = resp.json
        return [] unless data.is_a?(Hash)

        raw = data["domains"]
        return [] unless raw.is_a?(Array)

        raw.filter_map { |d| d.strip if d.is_a?(String) && !d.strip.empty? }
      end

      def pick_domain(domains, preferred)
        if preferred && !preferred.to_s.strip.empty?
          p = preferred.to_s.strip.downcase
          found = domains.find { |d| d.downcase == p }
          return found if found

          raise "fake-legal: domain not available: #{p}"
        end
        domains.sample
      end

      def random_username(length)
        chars = ("a".."z").to_a + ("A".."Z").to_a + ("0".."9").to_a
        Array.new(length) { chars.sample }.join
      end

      # @param domain [String, nil]
      # @param channel [String]
      # @return [EmailInfo]
      def generate_email(domain = nil, channel = CHANNEL)
        domains = fetch_domains
        raise "fake-legal: no domains" if domains.empty?

        d = pick_domain(domains, domain)
        resp = if d == "imgui.de" || d == "pulsewebmenu.de"
                 # imgui-de / pulsewebmenu-de 用 POST 保根域
                 Http.post("#{BASE}/api/inbox/custom",
                           json: { "username" => random_username(12), "domain" => d },
                           headers: HEADERS, timeout: 15)
               else
                 # fake-legal 用 GET 创建
                 Http.get("#{BASE}/api/inbox/new?domain=#{URI.encode_www_form_component(d)}",
                          headers: HEADERS, timeout: 15)
               end
        resp.raise_for_status
        data = resp.json
        raise "fake-legal: create inbox failed" unless data.is_a?(Hash) && data["success"]

        addr = data["address"]
        raise "fake-legal: missing address" if addr.nil? || addr.to_s.strip.empty?

        EmailInfo.new(channel: channel, email: addr.to_s.strip)
      end

      # @param email [String]
      # @return [Array<Email>]
      def get_emails(email)
        e = email.to_s.strip
        raise "fake-legal: empty email" if e.empty?

        resp = Http.get("#{BASE}/api/inbox/#{URI.encode_www_form_component(e)}",
                        headers: HEADERS, timeout: 15)
        resp.raise_for_status
        data = resp.json
        return [] unless data.is_a?(Hash) && data["success"]

        rows = data["emails"]
        return [] unless rows.is_a?(Array)

        rows.filter_map do |raw|
          next unless raw.is_a?(Hash)

          Normalize.normalize_email(raw, e)
        end
      end
    end
  end
end
