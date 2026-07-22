# frozen_string_literal: true

require "json"

module TempmailSdk
  module Providers
    # Lroid 渠道 — https://lroid.com
    # HTML 解析模式，session cookies 维持邮箱身份
    module Lroid
      CHANNEL = "lroid"
      BASE_URL = "https://lroid.com"

      EMAIL_RE = /<input[^>]+id=["']eposta_adres["'][^>]+value=["']([^"']+@[^"']+)["']/i
      EMAIL_RE_ALT = /<input[^>]+value=["']([^"']+@[^"']+)["'][^>]+id=["']eposta_adres["']/i

      HEADERS = {
        "Accept" => "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
        "Accept-Language" => "en-US,en;q=0.9",
        "Referer" => "#{BASE_URL}/",
        "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " \
                        "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"
      }.freeze

      module_function

      # 从响应中收集并序列化 cookies
      def cookie_header_from_resp(resp)
        map = {}
        resp.set_cookies.each do |line|
          pair = line.split(";", 2).first.to_s.strip
          next unless pair.include?("=")

          k, v = pair.split("=", 2)
          map[k.strip] = v.to_s.strip unless k.strip.empty?
        end
        map.keys.sort.map { |k| "#{k}=#{map[k]}" }.join("; ")
      end

      # 从 HTML 中提取邮箱地址
      def extract_email(html_str)
        m = html_str.match(EMAIL_RE) || html_str.match(EMAIL_RE_ALT)
        raise "lroid: 无法从 HTML 响应中解析邮箱地址" unless m

        addr = m[1].strip
        raise "lroid: 解析到的邮箱地址无效" unless addr.include?("@")

        addr
      end

      # 解码 token
      def decode_token(token)
        data = JSON.parse(token)
        cookie = (data["cookie"] || "").strip
        raise "lroid: session token 中缺少 cookie" if cookie.empty?

        cookie
      rescue JSON::ParserError
        raise "lroid: 无效的 session token"
      end

      # 创建临时邮箱
      # @return [EmailInfo]
      def generate_email
        resp = Http.get(BASE_URL, headers: HEADERS, timeout: 15)
        resp.raise_for_status
        email = extract_email(resp.body)
        cookie_hdr = cookie_header_from_resp(resp)
        token = JSON.generate("cookie" => cookie_hdr)
        EmailInfo.new(channel: CHANNEL, email: email, token: token)
      end

      # 获取邮件列表
      # @param token [String]
      # @param email [String]
      # @return [Array<Email>]
      def get_emails(token, email = "")
        cookie = decode_token(token)
        address = (email || "").strip
        raise "lroid: 邮箱地址不能为空" if address.empty?

        # 尝试 kontrol API
        begin
          resp = Http.get(
            "#{BASE_URL}/en/api-kontrol/",
            headers: {
              "Accept" => "application/json, text/html, */*;q=0.8",
              "Referer" => "#{BASE_URL}/",
              "Cookie" => cookie,
              "User-Agent" => HEADERS["User-Agent"]
            },
            timeout: 15
          )
          resp.raise_for_status
          data = resp.json
          if data.is_a?(Array)
            return parse_json_emails(data, address)
          elsif data.is_a?(Hash)
            %w[mails emails messages data inbox].each do |key|
              items = data[key]
              return parse_json_emails(items, address) if items.is_a?(Array)
            end
            if data["id"] || data["subject"] || data["from"]
              return parse_json_emails([data], address)
            end
          end
        rescue StandardError
          # API 不可用，回退到 HTML 解析
        end

        parse_html_emails(cookie, address)
      end

      def parse_json_emails(items, recipient)
        items.filter_map do |raw|
          next unless raw.is_a?(Hash)

          normalized = raw.dup
          normalized["html"] = raw["body"] || raw["content"] if !raw.key?("html")
          Normalize.normalize_email(normalized, recipient)
        end
      end

      def parse_html_emails(cookie, recipient)
        resp = Http.get(
          BASE_URL,
          headers: {
            "Accept" => "text/html,application/xhtml+xml,*/*;q=0.8",
            "Referer" => "#{BASE_URL}/",
            "Cookie" => cookie,
            "User-Agent" => HEADERS["User-Agent"]
          },
          timeout: 15
        )
        resp.raise_for_status
        html_str = resp.body

        # 解析 <li class="mail"> 元素
        mail_items = html_str.scan(/<li[^>]*class=["'][^"']*\bmail\b[^"']*["'][^>]*>(.*?)<\/li>/mi)
        return [] if mail_items.empty?

        mail_items.each_with_index.filter_map do |(item_html), idx|
          subject = ""
          from_addr = ""
          date = ""
          mail_id = (idx + 1).to_s

          # 提取主题
          sm = item_html.match(/class=["'][^"']*\bsubject\b[^"']*["'][^>]*>(.*?)<\//mi)
          subject = sm[1].gsub(/<[^>]+>/, "").strip if sm

          # 提取发件人
          fm = item_html.match(/class=["'][^"']*\b(?:from|sender)\b[^"']*["'][^>]*>(.*?)<\//mi)
          from_addr = fm[1].gsub(/<[^>]+>/, "").strip if fm

          # 提取日期
          dm = item_html.match(/class=["'][^"']*\b(?:date|time)\b[^"']*["'][^>]*>(.*?)<\//mi)
          date = dm[1].gsub(/<[^>]+>/, "").strip if dm

          Normalize.normalize_email({
            "id" => mail_id,
            "from" => from_addr,
            "to" => recipient,
            "subject" => subject,
            "date" => date
          }, recipient)
        end
      end
    end
  end
end
