# frozen_string_literal: true

require "cgi"

module TempmailSdk
  module Providers
    # mohmal.com 渠道实现（HTML 页面解析，session cookie）
    #
    # 流程：GET /en/create/random 创建邮箱（跟随重定向到 /en/inbox），从 data-email 属性提取邮箱地址
    #       GET /en/inbox 解析邮件链接列表
    #       GET /en/message/{id}/html 获取邮件详情
    # token 存储 connect.sid cookie 字符串
    # 注意：正文提取使用栈式深度匹配，避免非贪婪正则截断嵌套 div
    module Mohmal
      CHANNEL = "mohmal"
      BASE_URL = "https://www.mohmal.com"

      DATA_EMAIL_RE = /data-email=["']([^"']+)["']/im
      MSG_HREF_RE = /href=["'](\/en\/message\/([^"']+))["']/i
      DETAIL_FROM_RE = /<span[^>]+class=["'][^"']*from[^"']*["'][^>]*>([\s\S]*?)<\/span>/im
      DETAIL_SUBJECT_RE = /<span[^>]+class=["'][^"']*subject[^"']*["'][^>]*>([\s\S]*?)<\/span>/im
      DETAIL_DATE_RE = /<span[^>]+class=["'][^"']*date[^"']*["'][^>]*>([\s\S]*?)<\/span>/im
      TAG_RE = /<[^>]+>/
      BODY_OPEN_RE = /<div\s+class=["'][^"']*(?:mail-body|message-body|message_body)[^"']*["']\s*>/im

      BROWSER_HEADERS = {
        "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " \
                        "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
        "Accept" => "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif," \
                    "image/webp,image/apng,*/*;q=0.8",
        "Accept-Language" => "en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7",
        "Cache-Control" => "no-cache",
        "DNT" => "1",
        "Pragma" => "no-cache",
        "Upgrade-Insecure-Requests" => "1"
      }.freeze

      module_function

      # 从 HTML 中提取 data-email 属性值
      # @param html [String]
      # @return [String]
      def extract_email(html)
        m = html.match(DATA_EMAIL_RE)
        m ? CGI.unescapeHTML(m[1].strip) : ""
      end

      # 合并 Cookie 头与响应 Set-Cookie
      # @param existing [String]
      # @param set_cookie_lines [Array<String>]
      # @return [String]
      def merge_cookies(existing, set_cookie_lines)
        map = {}
        existing.to_s.split(";").each do |part|
          part = part.strip
          next unless part.include?("=")

          k, v = part.split("=", 2)
          map[k.strip] = v.to_s.strip unless k.strip.empty?
        end
        set_cookie_lines.each do |line|
          pair = line.split(";", 2).first.to_s.strip
          next unless pair.include?("=")

          k, v = pair.split("=", 2)
          map[k.strip] = v.to_s.strip unless k.strip.empty?
        end
        map.map { |k, v| "#{k}=#{v}" }.join("; ")
      end

      # 去除 HTML 标签并修剪空白
      # @param s [String]
      # @return [String]
      def strip_tags(s)
        s.gsub(TAG_RE, " ").strip
      end

      # 使用栈式深度匹配提取指定 class 的 div 完整内部 HTML
      # 避免非贪婪正则在嵌套 div 时截断正文
      # @param page [String]
      # @return [String]
      def extract_body_html(page)
        m = page.match(BODY_OPEN_RE)
        return "" unless m

        start = m.end(0)
        pos = start
        depth = 1
        while pos < page.length && depth > 0
          next_open = page.index("<div", pos)
          next_close = page.index("</div>", pos)
          break unless next_close

          if next_open && next_open < next_close
            depth += 1
            pos = next_open + 4
          else
            depth -= 1
            return page[start...next_close].strip if depth.zero?

            pos = next_close + 6
          end
        end
        ""
      end

      # 创建 mohmal.com 临时邮箱
      # @return [EmailInfo]
      def generate_email
        r1 = Http.get(
          BASE_URL + "/en/create/random",
          headers: BROWSER_HEADERS.merge("Referer" => BASE_URL + "/en"),
          timeout: 15
        )
        r1.raise_for_status
        cookie_hdr = merge_cookies("", r1.set_cookies)
        email = extract_email(r1.body)

        if email.empty?
          r2 = Http.get(
            BASE_URL + "/en/inbox",
            headers: BROWSER_HEADERS.merge("Referer" => BASE_URL + "/en", "Cookie" => cookie_hdr),
            timeout: 15
          )
          r2.raise_for_status
          cookie_hdr = merge_cookies(cookie_hdr, r2.set_cookies)
          email = extract_email(r2.body)
        end

        raise "mohmal: 未能从页面中提取邮箱地址" if email.empty?

        EmailInfo.new(channel: CHANNEL, email: email, token: cookie_hdr)
      end

      # 获取 mohmal.com 邮件列表
      # @param email [String]
      # @param token [String] connect.sid cookie 字符串
      # @return [Array<Email>]
      def get_emails(email, token)
        raise "mohmal: session cookie 为空" if token.to_s.strip.empty?

        r = Http.get(
          BASE_URL + "/en/inbox",
          headers: BROWSER_HEADERS.merge(
            "Referer" => BASE_URL + "/en",
            "Cookie" => token
          ),
          timeout: 15
        )
        r.raise_for_status

        seen = {}
        msgs = []
        r.body.scan(MSG_HREF_RE) do |path, id|
          next if path.include?("/delete")
          next if seen[id]

          seen[id] = true
          msgs << { path: path, id: id }
        end
        return [] if msgs.empty?

        msgs.filter_map do |msg|
          raw = fetch_detail(token, msg[:id], email)
          Normalize.normalize_email(raw, email)
        end
      end

      # 获取单封邮件详情页
      # @param cookie [String]
      # @param id [String]
      # @param recipient [String]
      # @return [Hash]
      def fetch_detail(cookie, id, recipient)
        raw = { "id" => id, "to" => recipient }
        detail_url = "#{BASE_URL}/en/message/#{id}/html"
        resp = Http.get(
          detail_url,
          headers: BROWSER_HEADERS.merge(
            "Referer" => "#{BASE_URL}/en/inbox",
            "Cookie" => cookie
          ),
          timeout: 15
        )
        return raw unless resp.status_code == 200

        page = resp.body

        if (m = page.match(DETAIL_FROM_RE))
          raw["from"] = strip_tags(CGI.unescapeHTML(m[1])).strip
        end
        if (m = page.match(DETAIL_SUBJECT_RE))
          raw["subject"] = CGI.unescapeHTML(strip_tags(m[1])).strip
        end
        if (m = page.match(DETAIL_DATE_RE))
          raw["date"] = CGI.unescapeHTML(strip_tags(m[1])).strip
        end

        body_html = extract_body_html(page)
        raw["html"] = body_html unless body_html.empty?

        raw
      rescue StandardError
        raw
      end
    end
  end
end
