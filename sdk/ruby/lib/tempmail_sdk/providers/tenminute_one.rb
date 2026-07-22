# frozen_string_literal: true

require "uri"
require "json"
require "securerandom"
require "base64"
require "time"

module TempmailSdk
  module Providers
    # 10minutemail.one 渠道
    # SSR __NUXT_DATA__ 中的 mailServiceToken（JWT）+ 页面 emailDomains；
    # 收信 GET web.10minutemail.one/api/v1/mailbox/{email}
    module TenminuteOne
      CHANNEL = "10minute-one"
      SITE_ORIGIN = "https://10minutemail.one"
      API_BASE = "https://web.10minutemail.one/api/v1"
      KNOWN_EMAIL_DOMAINS = %w[xghff.com oqqaj.com psovv.com dbwot.com ygwpr.com imxwe.com].freeze
      DEFAULT_UA = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"

      NUXT_DATA_RE = /<script[^>]*\bid="__NUXT_DATA__"[^>]*>([\s\S]*?)<\/script>/i
      JWT_RE = /\A[A-Za-z0-9_-]+\.[A-Za-z0-9_-]+\.[A-Za-z0-9_-]+\z/

      module_function

      # URL 编码邮箱（@ 编码为 %40）
      def enc_mailbox_email(email)
        if email.include?("@")
          local, _, dom = email.partition("@")
          "#{URI.encode_www_form_component(local)}%40#{URI.encode_www_form_component(dom)}"
        else
          URI.encode_www_form_component(email)
        end
      end

      # 解析 __NUXT_DATA__ JSON 数组
      def parse_nuxt_array(html)
        m = html.match(NUXT_DATA_RE)
        raise "10minute-one: __NUXT_DATA__ not found" unless m

        JSON.parse(m[1].strip)
      end

      # 解引用 NUXT 数组中的值（递归追踪索引引用）
      def resolve_ref(arr, value, depth = 0)
        return value if depth > 64
        return value if value == true || value == false
        return value unless value.is_a?(Integer) && value >= 0 && value < arr.length

        resolve_ref(arr, arr[value], depth + 1)
      end

      # 从 NUXT 数组中提取 mailServiceToken（JWT）
      def parse_mail_service_token(arr)
        # 先扫描前 48 项
        n = [arr.length, 48].min
        n.times do |i|
          el = arr[i]
          next unless el.is_a?(Hash) && el.key?("mailServiceToken")

          t = resolve_ref(arr, el["mailServiceToken"])
          return t if t.is_a?(String) && t.match?(JWT_RE)
        end
        # 全量扫描
        arr.each do |el|
          next unless el.is_a?(Hash) && el.key?("mailServiceToken")

          t = resolve_ref(arr, el["mailServiceToken"])
          return t if t.is_a?(String) && t.match?(JWT_RE)
        end
        # 最后尝试找裸 JWT 字符串
        arr.each do |el|
          return el if el.is_a?(String) && el.match?(JWT_RE)
        end
        raise "10minute-one: mailServiceToken not found"
      end

      # 从 HTML 页面中提取字段对应的 JSON 数组（如 emailDomains、blockedUsernames）
      def parse_quoted_json_array(html, field)
        key = "#{field}:\""
        start = html.index(key)
        return [] unless start

        slice_start = start + key.length
        return [] if slice_start >= html.length || html[slice_start] != "["

        depth = 0
        j = slice_start
        while j < html.length
          c = html[j]
          if c == "["
            depth += 1
          elsif c == "]"
            depth -= 1
            if depth.zero?
              j += 1
              break
            end
          end
          j += 1
        end
        raw = html[slice_start...j]
        unesc = raw.gsub('\\"', '"')
        v = JSON.parse(unesc)
        v.is_a?(Array) ? v : []
      rescue JSON::ParserError
        []
      end

      def pick_locale(domain)
        s = domain.to_s.strip
        return "zh" if s.empty?
        return "zh" if s.include?(".") && !s.include?("/")

        s
      end

      def pick_mailbox_domain(hint, available)
        raise "10minute-one: no email domains" if available.empty?

        if hint && !hint.empty?
          h = hint.strip.downcase
          if h.include?(".")
            found = available.find { |d| d.downcase == h }
            return found if found
          end
        end
        available.sample
      end

      def random_local(n)
        chars = "abcdefghijklmnopqrstuvwxyz0123456789"
        Array.new(n) { chars[rand(chars.length)] }.join
      end

      # 从 JWT 中提取过期时间戳（毫秒）
      def jwt_exp_ms(token)
        parts = token.split(".")
        return nil if parts.length < 2

        b = parts[1]
        # 补齐 Base64 填充
        b += "=" * ((-b.length) % 4)
        b = b.tr("-_", "+/")
        payload = JSON.parse(Base64.decode64(b))
        exp = payload["exp"]
        return (exp * 1000).to_i if exp.is_a?(Numeric)

        nil
      rescue StandardError
        nil
      end

      def api_headers(tok)
        {
          "Accept" => "*/*",
          "Accept-Language" => "zh-CN,zh;q=0.9,en;q=0.8",
          "Authorization" => "Bearer #{tok}",
          "Cache-Control" => "no-cache",
          "Content-Type" => "application/json",
          "DNT" => "1",
          "Origin" => SITE_ORIGIN,
          "Pragma" => "no-cache",
          "Referer" => "#{SITE_ORIGIN}/",
          "User-Agent" => DEFAULT_UA,
          "X-Request-ID" => SecureRandom.hex(16),
          "X-Timestamp" => Time.now.to_i.to_s
        }
      end

      def item_needs_detail(m)
        return false if m["id"].to_s.strip.empty?

        body = (m["text"] || m["body"] || m["html"] || m["mail_text"] || "").to_s.strip
        body.empty?
      end

      # @param domain [String, nil] 域名或 locale 提示
      # @return [EmailInfo]
      def generate_email(domain = nil)
        loc = pick_locale(domain)
        page_url = "#{SITE_ORIGIN}/#{loc}"
        r = Http.get(page_url, headers: {
                       "User-Agent" => DEFAULT_UA,
                       "Accept" => "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
                       "Accept-Language" => "zh-CN,zh;q=0.9,en;q=0.8",
                       "Cache-Control" => "no-cache",
                       "Pragma" => "no-cache",
                       "DNT" => "1",
                       "Referer" => page_url
                     })
        r.raise_for_status
        html = r.body

        arr = parse_nuxt_array(html)
        token = parse_mail_service_token(arr)

        domains = (parse_quoted_json_array(html, "emailDomains") + KNOWN_EMAIL_DOMAINS).uniq
        domains = KNOWN_EMAIL_DOMAINS.dup if domains.empty?

        blocked = parse_quoted_json_array(html, "blockedUsernames").map(&:downcase).to_set

        dom_hint = domain && domain.to_s.strip.include?(".") ? domain.to_s.strip : nil
        mail_domain = pick_mailbox_domain(dom_hint, domains)

        local = nil
        32.times do
          cand = random_local(10)
          unless blocked.include?(cand.downcase)
            local = cand
            break
          end
        end
        raise "10minute-one: could not pick username" unless local

        address = "#{local}@#{mail_domain}"
        exp_ms = jwt_exp_ms(token)
        EmailInfo.new(channel: CHANNEL, email: address, token: token, expires_at: exp_ms)
      end

      # @param email [String]
      # @param token [String]
      # @return [Array<Email>]
      def get_emails(email, token)
        url = "#{API_BASE}/mailbox/#{enc_mailbox_email(email)}"
        r = Http.get(url, headers: api_headers(token))
        r.raise_for_status
        data = r.json
        raise "10minute-one: invalid inbox JSON" unless data.is_a?(Array)

        data.filter_map do |raw|
          next unless raw.is_a?(Hash)

          row = raw.dup
          if item_needs_detail(row)
            mid = row["id"].to_s
            detail_url = "#{API_BASE}/mailbox/#{enc_mailbox_email(email)}/#{URI.encode_www_form_component(mid)}"
            begin
              d = Http.get(detail_url, headers: api_headers(token))
              if d.ok?
                detail = d.json
                if detail.is_a?(Hash)
                  detail.each { |k, v| row[k] = v unless row.key?(k) }
                end
              end
            rescue StandardError
              # 忽略详情失败
            end
          end
          Normalize.normalize_email(row, email)
        end
      end
    end
  end
end
