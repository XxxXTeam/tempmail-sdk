# frozen_string_literal: true

module TempmailSdk
  module Providers
    # anonymmail.net 渠道实现
    # POST JSON API，需要 cookie session
    module Anonymmail
      CHANNEL = "anonymmail"
      BASE = "https://anonymmail.net"
      HEADERS = {
        "Accept" => "*/*",
        "Content-Type" => "application/x-www-form-urlencoded",
        "Referer" => "https://anonymmail.net/",
        "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " \
                        "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"
      }.freeze

      module_function

      def random_username(length = 10)
        chars = ("a".."z").to_a + ("0".."9").to_a
        Array.new(length) { chars.sample }.join
      end

      # 获取可用域名列表
      def fetch_domains
        resp = Http.post("#{BASE}/api/getDomains", headers: HEADERS, timeout: 15)
        resp.raise_for_status
        data = resp.json
        return [] unless data.is_a?(Array)

        data.select { |item| item.is_a?(Hash) && item["domain"].to_s.strip != "" }
            .map { |item| item["domain"].to_s.strip }
      end

      # @return [EmailInfo]
      def generate_email
        domains = fetch_domains
        raise "anonymmail: no domains available" if domains.empty?

        domain = domains.sample
        username = random_username
        email = "#{username}@#{domain}"

        # HEAD 请求获取 cookie session
        Http.get("#{BASE}/", headers: HEADERS, timeout: 15)

        # POST 创建邮箱
        resp = Http.post("#{BASE}/api/create", headers: HEADERS, body: "email=#{email}", timeout: 15)
        resp.raise_for_status
        data = resp.json
        raise "anonymmail: create inbox failed" unless data.is_a?(Hash) && data["success"]

        addr = data["email"].to_s.strip
        raise "anonymmail: missing email in response" if addr.empty?

        EmailInfo.new(channel: CHANNEL, email: addr)
      end

      # @param email [String]
      # @return [Array<Email>]
      def get_emails(email)
        e = email.to_s.strip
        raise "anonymmail: empty email" if e.empty?

        resp = Http.post("#{BASE}/api/get", headers: HEADERS, body: "email=#{e}", timeout: 15)
        resp.raise_for_status
        data = resp.json
        return [] unless data.is_a?(Hash)

        inbox = data[e]
        return [] unless inbox.is_a?(Hash)

        rows = inbox["emails"]
        return [] unless rows.is_a?(Array)

        rows.select { |r| r.is_a?(Hash) }.map do |raw|
          normalized = raw.dup
          # 将 token 字段映射为 id
          if normalized.key?("token") && !normalized.key?("id")
            normalized["id"] = normalized.delete("token")
          end
          # 将 body 字段映射为 html
          if normalized.key?("body") && !normalized.key?("html")
            normalized["html"] = normalized.delete("body")
          end
          Normalize.normalize_email(normalized, e)
        end
      end
    end
  end
end
