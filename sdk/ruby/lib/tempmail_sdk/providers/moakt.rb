# frozen_string_literal: true

require "base64"
require "cgi"
require "json"
require "uri"

module TempmailSdk
  module Providers
    # moakt.com 渠道实现（HTML 抓取型）
    #
    # 流程：
    #   1. GET 语言首页，收集初始 Cookie 与可用域名 <option> 列表；
    #   2. POST /{locale}/inbox 创建邮箱（随机域名或指定域名），从 302 响应捕获 tm_session Cookie；
    #   3. GET /{locale}/inbox 解析 #email-address 得到邮箱地址；
    #   4. 会话凭证（locale + 序列化 Cookie 头）经 base64 打包进 token；
    #   5. 收信时列表解析 /{locale}/email/{uuid}，正文 GET .../html 解析 .email-body。
    #
    # 多个渠道别名（moakt.cc/moakt.co/moakt.ws/tmail.ws 等）共用此实现，仅固定域名不同。
    module Moakt
      CHANNEL = "moakt"
      ORIGIN = "https://www.moakt.com"
      TOK_PREFIX = "mok1:"

      # 默认 User-Agent（config.headers 未指定时使用）
      DEFAULT_UA = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " \
                   "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"

      # 页面请求的固定基础头
      DEFAULT_HEADERS = {
        "Accept-Language" => "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
        "Cache-Control" => "no-cache",
        "DNT" => "1",
        "Pragma" => "no-cache",
        "Upgrade-Insecure-Requests" => "1"
      }.freeze

      EMAIL_DIV_RE = /<div\s+id="email-address"\s*>([^<]+)<\/div>/im
      DOMAIN_OPTION_RE = /<option\s+value="([^"]+)">\s*@[^<]+<\/option>/im
      MAIL_DOMAIN_RE = /\A[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?(?:\.[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?)+\z/i
      HREF_EMAIL_RE = /href="(\/[^"]+\/email\/[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12})"/
      TITLE_RE = /<li\s+class="title"\s*>([^<]*)<\/li>/im
      DATE_RE = /<li\s+class="date"[^>]*>[\s\S]*?<span[^>]*>([^<]+)<\/span>/im
      SENDER_RE = /<li\s+class="sender"[^>]*>[\s\S]*?<span[^>]*>([\s\S]*?)<\/span>\s*<\/li>/im
      BODY_RE = /<div\s+class="email-body"\s*>([\s\S]*?)<\/div>/im
      FROM_ADDR_RE = /<([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,})>/
      TAG_RE = /<[^>]+>/

      module_function

      # 解析域名参数，返回 [locale, mail_domain]
      # - 空或含非法字符：语言页取默认 "zh"，随机域名
      # - 形如域名：locale="zh"，使用指定域名
      # - 其它字符串：视为 locale，随机域名
      def request_parts(domain)
        s = domain.to_s.strip
        return ["zh", ""] if s.empty? || s.match?(/[\/?#\\]/)
        return ["zh", s.sub(/\A@/, "").downcase] if s.match?(MAIL_DOMAIN_RE)

        [s, ""]
      end

      # 从首页 HTML 收集服务端提供的可用域名集合
      def parse_server_domains(page)
        page.scan(DOMAIN_OPTION_RE)
            .map { |m| m[0].strip.sub(/\A@/, "").downcase }
            .reject(&:empty?)
      end

      # 生成随机本地部分（小写字母 + 数字）
      def random_local(length = 12)
        chars = ("a".."z").to_a + ("0".."9").to_a
        Array.new(length) { chars.sample }.join
      end

      # 提取邮箱地址的域名部分（小写）
      def email_domain(email)
        _head, sep, domain = email.to_s.rpartition("@")
        sep.empty? ? "" : domain.strip.downcase
      end

      # 将 Cookie 头字符串解析为 name => value 映射
      def parse_cookie_map(hdr)
        map = {}
        hdr.to_s.split(";").each do |part|
          part = part.strip
          next unless part.include?("=")

          k, v = part.split("=", 2)
          k = k.strip
          map[k] = v.to_s.strip unless k.empty?
        end
        map
      end

      # 从响应的 Set-Cookie 头提取本次新设置的 Cookie
      def cookies_from_response(resp)
        map = {}
        resp.set_cookies.each do |line|
          pair = line.split(";", 2).first.to_s.strip
          next unless pair.include?("=")

          k, v = pair.split("=", 2)
          k = k.strip
          map[k] = v.to_s.strip unless k.empty?
        end
        map
      end

      # 合并旧 Cookie 头与响应新设置的 Cookie，按 key 排序拼接（保证稳定）
      def merge_cookie_hdr(prev, resp)
        d = parse_cookie_map(prev)
        d.merge!(cookies_from_response(resp))
        d.keys.sort.map { |k| "#{k}=#{d[k]}" }.join("; ")
      end

      # 构造页面请求头
      def page_headers(referer, ua)
        DEFAULT_HEADERS.merge(
          "User-Agent" => ua,
          "Accept" => "text/html,application/xhtml+xml,application/xml;q=0.9," \
                      "image/avif,image/webp,image/apng,*/*;q=0.8",
          "Referer" => referer
        )
      end

      # 从 config 取 UA，缺省用内置默认值
      def user_agent
        (Config.get_config.headers || {})["User-Agent"] || DEFAULT_UA
      end

      # 将会话（locale + Cookie 头）编码为不透明 token
      def encode_sess(locale, cookie_hdr)
        raw = JSON.generate("l" => locale, "c" => cookie_hdr)
        TOK_PREFIX + Base64.strict_encode64(raw)
      end

      # 解码 token，返回 [locale, cookie_hdr]
      def decode_sess(tok)
        raise "moakt: invalid session token" unless tok.to_s.start_with?(TOK_PREFIX)

        data = Base64.strict_decode64(tok[TOK_PREFIX.length..])
        o = JSON.parse(data)
        loc = o["l"].to_s.strip
        c = o["c"].to_s.strip
        raise "moakt: invalid session token" if loc.empty? || c.empty?

        [loc, c]
      rescue ArgumentError, JSON::ParserError
        raise "moakt: invalid session token"
      end

      # 从收件箱页解析邮箱地址
      def parse_inbox_email(html_s)
        m = html_s.match(EMAIL_DIV_RE)
        raise "moakt: email-address not found" unless m

        addr = CGI.unescapeHTML(m[1].strip)
        raise "moakt: empty email-address" if addr.empty?

        addr
      end

      # 去除 HTML 标签，压缩为文本
      def strip_tags(s)
        s.gsub(TAG_RE, " ").strip
      end

      # 从收件箱页提取邮件 UUID 列表（去重、排除删除链接）
      def list_mail_ids(html_s)
        seen = {}
        out = []
        html_s.scan(HREF_EMAIL_RE).each do |cap|
          path = cap[0]
          next if path.include?("/delete")

          mid = path.split("/").last
          if mid.length == 36 && !seen[mid]
            seen[mid] = true
            out << mid
          end
        end
        out
      end

      # 解析邮件详情页，返回标准化前的原始 Hash
      def parse_detail(page, mid, recipient)
        from_s = ""
        if (sm = page.match(SENDER_RE))
          inner = CGI.unescapeHTML(sm[1])
          from_s = strip_tags(inner)
          if (em = inner.match(FROM_ADDR_RE))
            from_s = em[1].strip
          end
        end

        subj = ""
        subj = CGI.unescapeHTML(page.match(TITLE_RE)[1].strip) if page.match?(TITLE_RE)

        date_s = ""
        date_s = CGI.unescapeHTML(page.match(DATE_RE)[1].strip) if page.match?(DATE_RE)

        body = ""
        body = page.match(BODY_RE)[1].strip if page.match?(BODY_RE)

        {
          "id" => mid,
          "to" => recipient,
          "from" => from_s,
          "subject" => subj,
          "date" => date_s,
          "html" => body
        }
      end

      # 创建临时邮箱
      # @param domain [String, nil] 指定接入域名或语言标识
      # @return [EmailInfo]
      def generate_email(domain = nil)
        loc, mail_domain = request_parts(domain)
        base = "#{ORIGIN}/#{loc}"
        inbox = "#{base}/inbox"
        ua = user_agent

        r1 = Http.get(base, headers: page_headers(base, ua))
        r1.raise_for_status
        cookie_hdr = merge_cookie_hdr("", r1)

        if mail_domain.empty?
          post_data = { "random" => "1" }
        else
          unless parse_server_domains(r1.body).include?(mail_domain)
            raise "moakt: unsupported domain #{mail_domain}"
          end

          post_data = {
            "setemail" => "",
            "username" => random_local,
            "domain" => mail_domain,
            "preferred_domain" => ""
          }
        end

        # POST /inbox 创建邮箱，从 302 响应捕获 tm_session Cookie（Net::HTTP 默认不跟随重定向）
        r2 = Http.post(
          inbox,
          headers: page_headers(base, ua).merge(
            "Content-Type" => "application/x-www-form-urlencoded",
            "Cookie" => cookie_hdr
          ),
          body: URI.encode_www_form(post_data)
        )
        cookie_hdr = merge_cookie_hdr(cookie_hdr, r2)

        unless parse_cookie_map(cookie_hdr).key?("tm_session")
          raise "moakt: missing tm_session cookie"
        end

        # GET /inbox 获取邮箱地址
        r3 = Http.get(inbox, headers: page_headers(base, ua).merge("Cookie" => cookie_hdr))
        r3.raise_for_status
        cookie_hdr = merge_cookie_hdr(cookie_hdr, r3)

        email = parse_inbox_email(r3.body)
        if !mail_domain.empty? && email_domain(email) != mail_domain
          raise "moakt: domain mismatch expected=#{mail_domain} actual=#{email_domain(email)}"
        end

        EmailInfo.new(channel: CHANNEL, email: email, token: encode_sess(loc, cookie_hdr))
      end

      # 收取邮件列表
      # @param email [String]
      # @param token [String]
      # @return [Array<Email>]
      def get_emails(email, token)
        loc, cookie_hdr = decode_sess(token)
        inbox = "#{ORIGIN}/#{loc}/inbox"
        base_ref = "#{ORIGIN}/#{loc}"
        ua = user_agent

        r = Http.get(inbox, headers: page_headers(base_ref, ua).merge("Cookie" => cookie_hdr))
        r.raise_for_status

        list_mail_ids(r.body).filter_map do |mid|
          detail = "#{ORIGIN}/#{loc}/email/#{mid}/html"
          refer = "#{ORIGIN}/#{loc}/email/#{mid}"
          rd = Http.get(detail, headers: page_headers(refer, ua).merge("Cookie" => cookie_hdr))
          next unless rd.status_code == 200

          Normalize.normalize_email(parse_detail(rd.body, mid, email), email)
        end
      end
    end
  end
end
