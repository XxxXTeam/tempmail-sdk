# frozen_string_literal: true

module TempmailSdk
  module Providers
    # Temp-Mail.Now 渠道 — https://temp-mail.now
    # 基于会话 Cookie 的临时邮箱服务
    module TempMailNow
      CHANNEL = "temp-mail-now"
      BASE_URL = "https://temp-mail.now"

      PAGE_HEADERS = {
        "Accept" => "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
        "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36"
      }.freeze

      API_HEADERS = {
        "Accept" => "application/json, text/javascript, */*; q=0.01",
        "X-Requested-With" => "XMLHttpRequest",
        "Referer" => "#{BASE_URL}/en/",
        "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36"
      }.freeze

      module_function

      def extract_cookies(resp)
        cookies = {}
        resp.set_cookies.each do |line|
          pair = line.split(";").first.to_s.strip
          k, v = pair.split("=", 2)
          cookies[k] = v if k && v
        end
        cookies
      end

      def cookies_to_str(cookies)
        cookies.map { |k, v| "#{k}=#{v}" }.join("; ")
      end

      # 创建临时邮箱
      # @return [EmailInfo]
      def generate_email
        # 获取 session cookie
        resp = Http.get("#{BASE_URL}/en/", headers: PAGE_HEADERS, timeout: 15)
        resp.raise_for_status
        cookies = extract_cookies(resp)
        cookie_str = cookies_to_str(cookies)
        raise "temp-mail-now: 无法获取 session cookie" if cookie_str.empty?

        # 创建新邮箱
        resp2 = Http.post("#{BASE_URL}/change_email",
                          headers: API_HEADERS.merge("Cookie" => cookie_str), timeout: 15)
        resp2.raise_for_status

        # 合并可能更新的 cookie
        extract_cookies(resp2).each { |k, v| cookies[k] = v }
        cookie_str = cookies_to_str(cookies)

        data = resp2.json
        raise "temp-mail-now: 响应格式无效" unless data.is_a?(Hash)

        address = data["new_email"] || ""
        raise "temp-mail-now: 创建邮箱失败" if address.empty? || !address.include?("@")

        EmailInfo.new(channel: CHANNEL, email: address, token: cookie_str)
      end

      # 获取邮件列表
      # @param token [String] session cookie
      # @param email [String]
      # @return [Array<Email>]
      def get_emails(token, email)
        raise "temp-mail-now: session cookie 不能为空" if token.nil? || token.empty?

        addr = (email || "").strip
        raise "temp-mail-now: 邮箱地址不能为空" if addr.empty?

        resp = Http.get("#{BASE_URL}/fetch_emails",
                        headers: API_HEADERS.merge("Cookie" => token), timeout: 15)
        resp.raise_for_status
        data = resp.json
        return [] unless data.is_a?(Hash)

        emails_list = data["emails"]
        return [] unless emails_list.is_a?(Array)

        emails_list.filter_map do |item|
          next unless item.is_a?(Hash)

          raw = {
            "id" => item["id"] || "",
            "from" => item["from"] || item["from_address"] || item["sender"] || "",
            "to" => item["to"] || addr,
            "subject" => item["subject"] || "",
            "text" => item["text"] || item["body_text"] || "",
            "html" => item["html"] || item["body_html"] || "",
            "date" => item["date"] || item["received_at"] || ""
          }
          Normalize.normalize_email(raw, addr)
        end
      end
    end
  end
end
