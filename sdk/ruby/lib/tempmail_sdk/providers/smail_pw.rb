# frozen_string_literal: true

require "json"
require "uri"

module TempmailSdk
  module Providers
    # smail.pw 渠道实现
    #
    # 流程：POST /_root.data body: intent=generate 创建邮箱，保存 __session cookie
    #       GET /_root.data + Cookie 获取邮件列表（解析 React Flight 格式或正则提取）
    #       GET /api/email/{id} 获取单封邮件正文
    # token 存储 __session=xxx cookie 字符串
    module SmailPw
      CHANNEL = "smail-pw"
      ROOT_DATA_URL = "https://smail.pw/_root.data"

      HEADERS = {
        "Accept" => "*/*",
        "accept-language" => "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
        "cache-control" => "no-cache",
        "dnt" => "1",
        "origin" => "https://smail.pw",
        "pragma" => "no-cache",
        "referer" => "https://smail.pw/",
        "sec-fetch-dest" => "empty",
        "sec-fetch-mode" => "cors",
        "sec-fetch-site" => "same-origin",
        "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " \
                        "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"
      }.freeze

      QUOTED_INBOX_RE = /"([a-z0-9][a-z0-9.\-]*@smail\.pw)"/i
      PLAIN_INBOX_RE  = /\b([a-z0-9][a-z0-9.\-]*@smail\.pw)\b/i
      MAIL_BLOCK_RE   = /"id","([^"]+)","to_address","([^"]*)","from_name","([^"]*)","from_address","([^"]*)","subject","([^"]*)","time",(\d+)/
      MAIL_BLOCK2_RE  = /"id","([^"]+)","from_name","([^"]*)","from_address","([^"]*)","subject","([^"]*)","time",(\d+)/

      module_function

      # 从响应 Set-Cookie 中提取 __session cookie
      # @param set_cookie_lines [Array<String>]
      # @return [String]
      def extract_session(set_cookie_lines)
        set_cookie_lines.each do |line|
          pair = line.split(";", 2).first.to_s.strip
          return pair if pair.start_with?("__session=")
        end
        ""
      end

      # 从响应体中提取邮箱地址
      # @param body [String]
      # @return [String]
      def extract_inbox(body)
        m = body.match(QUOTED_INBOX_RE)
        return m[1] if m

        m = body.match(PLAIN_INBOX_RE)
        m ? m[1] : ""
      end

      # 从响应体解析邮件列表
      # @param body [String]
      # @param recipient [String]
      # @return [Array<Hash>]
      def parse_mails(body, recipient)
        out = []
        body.scan(MAIL_BLOCK_RE) do |id, to_addr, from_name, from_addr, subj, ts|
          out << {
            "id" => id, "to_address" => to_addr.empty? ? recipient : to_addr,
            "from_name" => from_name, "from_address" => from_addr,
            "subject" => subj, "date" => ts.to_f,
            "text" => "", "html" => "", "attachments" => []
          }
        end
        return out unless out.empty?

        body.scan(MAIL_BLOCK2_RE) do |id, from_name, from_addr, subj, ts|
          out << {
            "id" => id, "to_address" => recipient,
            "from_name" => from_name, "from_address" => from_addr,
            "subject" => subj, "date" => ts.to_f,
            "text" => "", "html" => "", "attachments" => []
          }
        end
        out
      end

      # 创建 smail.pw 临时邮箱
      # @return [EmailInfo]
      def generate_email
        resp = Http.post(
          ROOT_DATA_URL,
          headers: HEADERS.merge("Content-Type" => "application/x-www-form-urlencoded;charset=UTF-8"),
          body: "intent=generate",
          timeout: 15
        )
        raise "smail.pw generate failed: #{resp.status_code}" unless resp.ok?

        cookie = extract_session(resp.set_cookies)
        raise "smail.pw: 未能提取 __session cookie" if cookie.empty?

        email = extract_inbox(resp.body)
        raise "smail.pw: 未能解析邮箱地址" if email.empty?

        EmailInfo.new(channel: CHANNEL, email: email, token: cookie)
      end

      # 获取 smail.pw 邮件列表
      # @param token [String] __session=xxx cookie 字符串
      # @param email [String]
      # @return [Array<Email>]
      def get_emails(token, email)
        resp = Http.get(
          ROOT_DATA_URL,
          headers: HEADERS.merge("Cookie" => token),
          timeout: 15
        )
        raise "smail.pw get emails failed: #{resp.status_code}" unless resp.ok?

        mails = parse_mails(resp.body, email)
        return [] if mails.empty?

        mails.map do |m|
          ne = Normalize.normalize_email(m, email)
          id = m["id"].to_s
          unless id.empty?
            body_html = fetch_body(token, id)
            ne.html = body_html unless body_html.empty?
          end
          ne
        end
      end

      # 获取单封邮件正文
      # @param token [String]
      # @param id [String]
      # @return [String]
      def fetch_body(token, id)
        return "" if id.empty?

        resp = Http.get(
          "https://smail.pw/api/email/#{URI.encode_uri_component(id)}",
          headers: HEADERS.merge("Cookie" => token, "Accept" => "application/json"),
          timeout: 15
        )
        return "" unless resp.ok?

        data = resp.json
        data.is_a?(Hash) ? data["body"].to_s : ""
      rescue StandardError
        ""
      end
    end
  end
end
