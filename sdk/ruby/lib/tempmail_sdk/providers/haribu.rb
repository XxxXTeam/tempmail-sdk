# frozen_string_literal: true

require "base64"
require "cgi"
require "json"

module TempmailSdk
  module Providers
    # Haribu 渠道 — https://haribu.net
    # Tempail 类模式，session cookies 维持会话
    module Haribu
      CHANNEL = "haribu"
      BASE = "https://haribu.net"
      TOK_PREFIX = "haribu1:"

      HEADERS = {
        "Accept" => "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
        "Accept-Language" => "zh-CN,zh;q=0.9,en;q=0.8",
        "Cache-Control" => "no-cache",
        "DNT" => "1",
        "Pragma" => "no-cache",
        "Upgrade-Insecure-Requests" => "1",
        "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " \
                        "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"
      }.freeze

      # 匹配 <input id="eposta_adres" value="xxx@yyy.com">
      EMAIL_INPUT_RE = /<input[^>]+id\s*=\s*["']eposta_adres["'][^>]+value\s*=\s*["']([^"']+)["']/i
      EMAIL_INPUT_RE2 = /<input[^>]+value\s*=\s*["']([^"']+@[^"']+)["'][^>]+id\s*=\s*["']eposta_adres["']/i
      MAIL_ITEM_RE = /<li\s+class\s*=\s*["']mail["'][^>]*>([\s\S]*?)<\/li>/mi
      FROM_RE = /<span\s+class\s*=\s*["']mail_gonderen["'][^>]*>([\s\S]*?)<\/span>/mi
      SUBJECT_RE = /<span\s+class\s*=\s*["']mail_konu["'][^>]*>([\s\S]*?)<\/span>/mi
      DATE_RE = /<span\s+class\s*=\s*["']mail_zaman["'][^>]*>([\s\S]*?)<\/span>/mi
      MAIL_LINK_RE = /href\s*=\s*["']([^"']*(?:mail|read|view)[^"']*)["']/i
      BODY_RE = /<div\s+(?:id|class)\s*=\s*["'](?:mail_icerik|icerik|mail-content|message-body)["'][^>]*>([\s\S]*?)<\/div>/mi
      TAG_RE = /<[^>]+>/

      module_function

      def strip_tags(html_str)
        html_str.gsub(TAG_RE, " ").strip
      end

      # 从响应中合并 cookies
      def merge_cookies(prev, resp)
        existing = {}
        prev.to_s.split(";").each do |part|
          part = part.strip
          next unless part.include?("=")

          k, v = part.split("=", 2)
          existing[k.strip] = v.to_s.strip unless k.strip.empty?
        end
        resp.set_cookies.each do |line|
          pair = line.split(";", 2).first.to_s.strip
          next unless pair.include?("=")

          k, v = pair.split("=", 2)
          existing[k.strip] = v.to_s.strip unless k.strip.empty?
        end
        existing.keys.sort.map { |k| "#{k}=#{existing[k]}" }.join("; ")
      end

      # 编码 session token
      def encode_sess(cookie_hdr)
        payload = JSON.generate("c" => cookie_hdr)
        TOK_PREFIX + Base64.strict_encode64(payload)
      end

      # 解码 session token
      def decode_sess(token)
        raise "haribu: 无效的会话令牌" unless token.to_s.start_with?(TOK_PREFIX)

        raw = token[TOK_PREFIX.length..]
        data = JSON.parse(Base64.strict_decode64(raw))
        cookie_hdr = (data["c"] || "").strip
        raise "haribu: 会话令牌中缺少 cookie" if cookie_hdr.empty?

        cookie_hdr
      rescue ArgumentError, JSON::ParserError
        raise "haribu: 无效的会话令牌"
      end

      # 从 HTML 中提取邮箱地址
      def extract_email(html_str)
        m = html_str.match(EMAIL_INPUT_RE) || html_str.match(EMAIL_INPUT_RE2)
        raise "haribu: 未找到邮箱地址" unless m

        addr = CGI.unescapeHTML(m[1]).strip
        raise "haribu: 邮箱地址无效" unless addr.include?("@")

        addr
      end

      # 创建临时邮箱
      # @return [EmailInfo]
      def generate_email
        resp = Http.get(BASE, headers: HEADERS, timeout: 15)
        resp.raise_for_status
        raise "haribu: 首页响应为空" if resp.body.empty?

        email_addr = extract_email(resp.body)
        cookie_hdr = merge_cookies("", resp)
        token = encode_sess(cookie_hdr)

        EmailInfo.new(channel: CHANNEL, email: email_addr, token: token)
      end

      # 获取邮件详情页正文
      def fetch_detail(detail_url, cookie_hdr)
        hdrs = HEADERS.merge("Cookie" => cookie_hdr, "Referer" => BASE)
        resp = Http.get(detail_url, headers: hdrs, timeout: 15)
        return "" unless resp.ok?

        m = resp.body.match(BODY_RE)
        m ? m[1].strip : ""
      rescue StandardError
        ""
      end

      # 获取邮件列表
      # @param token [String]
      # @param email [String]
      # @return [Array<Email>]
      def get_emails(token, email)
        e = (email || "").strip
        raise "haribu: 邮箱地址为空" if e.empty?

        cookie_hdr = decode_sess(token)

        # 触发新邮件检查
        begin
          kontrol_hdrs = HEADERS.merge(
            "Cookie" => cookie_hdr,
            "Referer" => BASE,
            "X-Requested-With" => "XMLHttpRequest"
          )
          Http.get("#{BASE}/en/api-kontrol/", headers: kontrol_hdrs, timeout: 15)
        rescue StandardError
          # 忽略
        end

        # GET 首页获取收件箱
        inbox_hdrs = HEADERS.merge("Cookie" => cookie_hdr, "Referer" => BASE)
        resp = Http.get(BASE, headers: inbox_hdrs, timeout: 15)
        resp.raise_for_status

        items = resp.body.scan(MAIL_ITEM_RE).map(&:first)
        return [] if items.empty?

        items.each_with_index.filter_map do |content, idx|
          raw = { "id" => "haribu-#{idx}", "to" => e }

          fm = content.match(FROM_RE)
          raw["from"] = CGI.unescapeHTML(strip_tags(fm[1])).strip if fm

          sm = content.match(SUBJECT_RE)
          raw["subject"] = CGI.unescapeHTML(strip_tags(sm[1])).strip if sm

          dm = content.match(DATE_RE)
          raw["date"] = CGI.unescapeHTML(strip_tags(dm[1])).strip if dm

          lm = content.match(MAIL_LINK_RE)
          if lm
            detail_url = lm[1]
            detail_url = "#{BASE}/#{detail_url.sub(%r{\A/}, '')}" unless detail_url.start_with?("http")
            html_body = fetch_detail(detail_url, cookie_hdr)
            raw["html"] = html_body unless html_body.empty?
          end

          Normalize.normalize_email(raw, e)
        end
      end
    end
  end
end
