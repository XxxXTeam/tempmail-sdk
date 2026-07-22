# frozen_string_literal: true

require "cgi"

module TempmailSdk
  module Providers
    # linshiyou.com 临时邮 — NEXUS_TOKEN + tmail-emails Cookie，邮件接口返回 HTML 分片
    module Linshiyou
      CHANNEL = "linshiyou"
      ORIGIN = "https://linshiyou.com"

      DEFAULT_HEADERS = {
        "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " \
                        "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
        "accept-language" => "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
        "cache-control" => "no-cache",
        "dnt" => "1",
        "pragma" => "no-cache",
        "priority" => "u=1, i",
        "referer" => "#{ORIGIN}/",
        "sec-ch-ua" => '"Chromium";v="146", "Not-A.Brand";v="24", "Microsoft Edge";v="146"',
        "sec-ch-ua-mobile" => "?0",
        "sec-ch-ua-platform" => '"Windows"',
        "sec-fetch-dest" => "empty",
        "sec-fetch-mode" => "cors",
        "sec-fetch-site" => "same-origin"
      }.freeze

      RE_LIST_ID = /id="tmail-email-list-([a-f0-9]+)"/i
      RE_DIV = /<div class="([^"]+)"[^>]*>([\s\S]*?)<\/div>/i
      RE_SRCDOC = /\ssrcdoc="([^"]*)"/i
      RE_DOWNLOAD = /href="(\/api\/download\?id=[^"]+)"/i
      RE_TAGS = /<[^>]+>/

      module_function

      def strip_tags(s)
        t = s.gsub(RE_TAGS, " ")
        CGI.unescapeHTML(t).split.join(" ")
      end

      def pick_div(fragment, class_name)
        fragment.scan(RE_DIV).each do |cls, inner|
          return strip_tags(inner).strip if cls == class_name
        end
        ""
      end

      def parse_date(s)
        s = s.to_s.strip
        return "" if s.empty?

        ["%Y-%m-%d %H:%M:%S", "%Y-%m-%dT%H:%M:%S"].each do |fmt|
          begin
            dt = Time.strptime(s, fmt)
            return dt.utc.strftime("%Y-%m-%dT%H:%M:%SZ")
          rescue ArgumentError
            next
          end
        end
        ""
      end

      def extract_srcdoc(body_part)
        m = body_part.match(RE_SRCDOC)
        return "" unless m

        CGI.unescapeHTML(m[1])
      end

      # 解析邮件分段
      def parse_segments(raw, recipient)
        raw = raw.to_s.strip
        return [] if raw.empty?

        out = []
        raw.split("<-----TMAILNEXTMAIL----->").each do |seg|
          seg = seg.strip
          next if seg.empty?

          parts = seg.split("<-----TMAILCHOPPER----->", 2)
          list_part = parts[0]
          body_part = parts.length > 1 ? parts[1] : ""

          mid = list_part.match(RE_LIST_ID)
          next unless mid

          mid_str = mid[1]

          from_list = pick_div(list_part, "name")
          subject_list = pick_div(list_part, "subject")
          preview_list = pick_div(list_part, "body")

          from_body = pick_div(body_part, "tmail-email-sender")
          time_body = pick_div(body_part, "tmail-email-time")
          title_body = pick_div(body_part, "tmail-email-title")
          html_body = extract_srcdoc(body_part)

          from_addr = from_body.empty? ? from_list : from_body
          subject = title_body.empty? ? subject_list : title_body
          text = preview_list.empty? ? strip_tags(html_body) : preview_list

          attachments = []
          dm = body_part.match(RE_DOWNLOAD)
          if dm
            attachments << EmailAttachment.new(filename: "", url: "#{ORIGIN}#{dm[1]}")
          end

          out << Email.new(
            id: mid_str,
            from_addr: from_addr,
            to: recipient,
            subject: subject,
            text: text,
            html: html_body,
            date: parse_date(time_body),
            is_read: false,
            attachments: attachments
          )
        end
        out
      end

      # 创建临时邮箱
      # @return [EmailInfo]
      def generate_email
        resp = Http.get("#{ORIGIN}/api/user?user", headers: DEFAULT_HEADERS)
        resp.raise_for_status

        nexus = resp.cookie_value("NEXUS_TOKEN")
        raise "linshiyou: NEXUS_TOKEN not in Set-Cookie" if nexus.nil? || nexus.empty?

        email = resp.body.strip
        raise "linshiyou: invalid email in response body" if email.empty? || !email.include?("@")

        token = "NEXUS_TOKEN=#{nexus}; tmail-emails=#{email}"
        EmailInfo.new(channel: CHANNEL, email: email, token: token)
      end

      # 获取邮件列表
      # @param token [String]
      # @param email [String]
      # @return [Array<Email>]
      def get_emails(token, email = "")
        resp = Http.get(
          "#{ORIGIN}/api/mail?unseen=1",
          headers: DEFAULT_HEADERS.merge(
            "Cookie" => token,
            "x-requested-with" => "XMLHttpRequest"
          )
        )
        resp.raise_for_status
        parse_segments(resp.body, email)
      end
    end
  end
end
