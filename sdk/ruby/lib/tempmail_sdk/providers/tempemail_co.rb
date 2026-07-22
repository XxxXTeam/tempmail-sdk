# frozen_string_literal: true

require "json"

module TempmailSdk
  module Providers
    # tempemail.co 渠道 — https://tempemail.co
    # Cookie Session + REST JSON API
    module TempemailCo
      CHANNEL = "tempemail-co"
      BASE_URL = "https://tempemail.co"

      # 从邮件列表 HTML 中提取邮件 ID（data-id="数字"）
      MAIL_ID_RE = /data-id="(\d+)"/

      HEADERS = {
        "Accept" => "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
        "Accept-Language" => "en-US,en;q=0.9",
        "Cache-Control" => "no-cache",
        "DNT" => "1",
        "Pragma" => "no-cache",
        "Upgrade-Insecure-Requests" => "1",
        "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"
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
        cookies.sort.map { |k, v| "#{k}=#{v}" }.join("; ")
      end

      def encode_token(address, cookies)
        JSON.generate({ "address" => address, "cookies" => cookies })
      end

      def decode_token(token)
        data = JSON.parse(token)
        address = data["address"] || ""
        cookies = data["cookies"] || {}
        raise "tempemail-co: token 数据不完整" if address.empty? || !cookies.is_a?(Hash)

        [address, cookies]
      end

      # 创建临时邮箱
      # @return [EmailInfo]
      def generate_email
        r = Http.get("#{BASE_URL}/mail/random", headers: HEADERS.merge(
          "Referer" => "#{BASE_URL}/",
          "Accept" => "application/json, text/javascript, */*; q=0.01"
        ))
        r.raise_for_status
        data = r.json
        raise "tempemail-co: 创建邮箱失败" unless data["result"]

        address = (data["address"] || data["id"] || "").strip
        raise "tempemail-co: 返回的邮箱地址无效" if address.empty? || !address.include?("@")

        cookies = extract_cookies(r)
        tok = encode_token(address, cookies)
        EmailInfo.new(channel: CHANNEL, email: address, token: tok)
      end

      # 获取邮件列表
      # @param email [String]
      # @param token [String]
      # @return [Array<Email>]
      def get_emails(email, token)
        raise "tempemail-co: token 不能为空" if token.nil? || token.empty?

        addr = (email || "").strip
        raise "tempemail-co: 邮箱地址不能为空" if addr.empty?

        address, cookies = decode_token(token)
        cookie_str = cookies_to_str(cookies)

        # 获取邮件列表
        r = Http.get("#{BASE_URL}/get-mails?mail_id=#{URI.encode_www_form_component(address)}&unseen=0&is_new=1",
                     headers: HEADERS.merge(
                       "Referer" => "#{BASE_URL}/",
                       "Cookie" => cookie_str,
                       "Accept" => "application/json, text/javascript, */*; q=0.01"
                     ))
        r.raise_for_status
        data = r.json
        count = data["count"] || 0
        return [] if count <= 0

        mails_html = data["mails"] || ""
        return [] unless mails_html.is_a?(String) && !mails_html.empty?

        mail_ids = mails_html.scan(MAIL_ID_RE).flatten.uniq
        return [] if mail_ids.empty?

        mail_ids.filter_map do |mail_id|
          begin
            detail_resp = Http.get("#{BASE_URL}/mail/info?id=#{mail_id}",
                                   headers: HEADERS.merge(
                                     "Referer" => "#{BASE_URL}/",
                                     "Cookie" => cookie_str,
                                     "Accept" => "application/json, text/javascript, */*; q=0.01"
                                   ))
            next unless detail_resp.ok?

            detail_data = detail_resp.json
            next unless detail_data["result"]

            mail_info = detail_data["mail"]
            next unless mail_info.is_a?(Hash)

            from_name = (mail_info["fromName"] || "").strip
            from_addr = (mail_info["fromAddress"] || "").strip
            from_display = from_name.empty? ? from_addr : "#{from_name} <#{from_addr}>"

            raw = {
              "id" => mail_id.to_s,
              "from" => from_display,
              "from_address" => from_addr,
              "to" => addr,
              "subject" => mail_info["subject"] || "",
              "html" => mail_info["textHtml"] || "",
              "date" => mail_info["displayDate"] || ""
            }
            Normalize.normalize_email(raw, addr)
          rescue StandardError
            next
          end
        end
      end
    end
  end
end
