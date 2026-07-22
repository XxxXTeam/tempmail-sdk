# frozen_string_literal: true

require "json"
require "uri"

module TempmailSdk
  module Providers
    # Rootsh(BccTo) 渠道 — https://rootsh.com
    # 需要 cookie session 认证，token 存储 time 值用于增量获取邮件
    module Rootsh
      CHANNEL = "rootsh"
      BASE = "https://rootsh.com"
      DEFAULT_DOMAIN = "bccto.cc"
      TOK_PREFIX = "rootsh1:"

      HEADERS = {
        "Accept" => "application/json, text/plain, */*",
        "Accept-Language" => "zh-CN,zh;q=0.9,en;q=0.8",
        "Cache-Control" => "no-cache",
        "DNT" => "1",
        "Pragma" => "no-cache",
        "Referer" => "#{BASE}/",
        "X-Requested-With" => "XMLHttpRequest",
        "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"
      }.freeze

      module_function

      def random_local(length = 10)
        chars = ("a".."z").to_a + ("0".."9").to_a
        Array.new(length) { chars.sample }.join
      end

      def encode_token(last_check_time, cookies)
        payload = JSON.generate({ "t" => last_check_time, "c" => cookies })
        "#{TOK_PREFIX}#{payload}"
      end

      def decode_token(token)
        raise "rootsh: 无效的 session token" unless token&.start_with?(TOK_PREFIX)

        data = JSON.parse(token[TOK_PREFIX.length..])
        last_check_time = data["t"] || 0
        cookies = data["c"] || {}
        raise "rootsh: 无效的 session token" unless cookies.is_a?(Hash)

        [last_check_time, cookies]
      end

      def build_cookie_header(cookies)
        cookies.map { |k, v| "#{k}=#{v}" }.join("; ")
      end

      def extract_cookies(resp)
        cookies = {}
        resp.set_cookies.each do |line|
          pair = line.split(";").first.to_s.strip
          k, v = pair.split("=", 2)
          cookies[k] = v if k && v
        end
        cookies
      end

      # 创建临时邮箱
      # @param domain [String, nil] 可选域名
      # @param channel [String] 渠道标识
      # @return [EmailInfo]
      def generate_email(domain = nil, channel = CHANNEL)
        dom = (domain || "").strip
        dom = DEFAULT_DOMAIN if dom.empty?
        local = random_local
        mail_addr = "#{local}@#{dom}"

        # 获取 session cookie
        r1 = Http.get("#{BASE}/", headers: {
          "Accept" => "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
          "User-Agent" => HEADERS["User-Agent"]
        })
        r1.raise_for_status
        cookies = extract_cookies(r1)

        # POST /applymail 申请邮箱
        r2 = Http.post("#{BASE}/applymail", headers: HEADERS.merge(
          "Content-Type" => "application/x-www-form-urlencoded; charset=UTF-8",
          "Cookie" => build_cookie_header(cookies)
        ), body: "mail=#{mail_addr}")
        r2.raise_for_status
        cookies.merge!(extract_cookies(r2))

        body = r2.json
        raise "rootsh: applymail 响应格式无效" unless body.is_a?(Hash)
        raise "rootsh: 邮箱申请失败: #{body['tips']}" unless body["success"].to_s == "true"

        actual_email = (body["user"] || "").strip
        actual_email = mail_addr if actual_email.empty?
        last_check_time = body["time"] || 0

        token = encode_token(last_check_time, cookies)
        EmailInfo.new(channel: channel, email: actual_email, token: token)
      end

      # 获取邮件列表
      # @param token [String]
      # @param email [String]
      # @return [Array<Email>]
      def get_emails(token, email)
        raise "rootsh: token 不能为空" if token.nil? || token.empty?

        addr = (email || "").strip
        raise "rootsh: 邮箱地址不能为空" if addr.empty?

        last_check_time, cookies = decode_token(token)
        ts = (Time.now.to_f * 1000).to_i

        r = Http.post("#{BASE}/getmail", headers: HEADERS.merge(
          "Content-Type" => "application/x-www-form-urlencoded; charset=UTF-8",
          "Cookie" => build_cookie_header(cookies)
        ), body: "mail=#{addr}&time=#{last_check_time}&_=#{ts}")
        r.raise_for_status
        body = r.json

        return [] unless body.is_a?(Hash) && body["success"].to_s == "true"

        mail_list = body["mail"]
        return [] unless mail_list.is_a?(Array)

        mail_list.filter_map do |item|
          next unless item.is_a?(Array) && item.length >= 5

          from_email = item[1].to_s
          from_display = item[0].to_s
          from_addr = from_email.empty? ? from_display : from_email
          subject = item[2].to_s
          date_str = item[3].to_s
          mail_fid = item[4].to_s

          # 获取邮件正文
          html_content = ""
          unless mail_fid.empty?
            begin
              rv = Http.post("#{BASE}/viewmail", headers: HEADERS.merge(
                "Content-Type" => "application/x-www-form-urlencoded; charset=UTF-8",
                "Cookie" => build_cookie_header(cookies)
              ), body: "mail=#{mail_fid}&to=#{addr}")
              if rv.ok?
                vb = rv.json
                html_content = vb["mail"] || "" if vb.is_a?(Hash) && vb["success"].to_s == "true"
              end
            rescue StandardError
              # 单封邮件获取失败不影响其他
            end
          end

          raw = {
            "id" => mail_fid,
            "from" => from_addr,
            "to" => addr,
            "subject" => subject,
            "date" => date_str,
            "html" => html_content
          }
          Normalize.normalize_email(raw, addr)
        end
      end
    end
  end
end
