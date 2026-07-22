# frozen_string_literal: true

require "securerandom"
require "rexml/document"

module TempmailSdk
  module Providers
    # Eyepaste 渠道 — https://eyepaste.com
    # RSS XML API，无认证，直接生成随机用户名
    module Eyepaste
      CHANNEL = "eyepaste"
      DOMAIN = "eyepaste.com"
      BASE = "https://www.eyepaste.com"

      HEADERS = {
        "Accept" => "application/rss+xml, application/xml, text/xml, */*",
        "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " \
                        "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"
      }.freeze

      TAG_RE = /<[^>]+>/

      module_function

      # 生成随机用户名
      def random_username(length = 10)
        chars = ("a".."z").to_a + ("0".."9").to_a
        Array.new(length) { chars.sample }.join
      end

      # 解析 RSS description 中的邮件元数据
      def parse_description(desc)
        result = { "from" => "", "to" => "", "subject" => "", "date" => "", "body" => "" }
        return result if desc.nil? || desc.empty?

        p_match = desc.match(/<p[^>]*>(.*?)<\/p>/mi)
        if p_match
          meta = p_match[1].strip
          from_m = meta.match(/From:\s*(.*?)(?:\s+To:|$)/m)
          result["from"] = from_m[1].strip if from_m
          to_m = meta.match(/To:\s*(.*?)(?:\s+Subject:|$)/m)
          result["to"] = to_m[1].strip if to_m
          subject_m = meta.match(/Subject:\s*(.*?)(?:\s+Date:|$)/m)
          result["subject"] = subject_m[1].strip if subject_m
          date_m = meta.match(/Date:\s*(.*?)$/m)
          result["date"] = date_m[1].strip if date_m

          p_end = desc.index("</p>")
          if p_end
            body = desc[(p_end + 4)..].strip
            result["body"] = body unless body.empty?
          end
        end
        result
      end

      # 生成临时邮箱（无需 API 调用）
      # @return [EmailInfo]
      def generate_email
        username = random_username
        email = "#{username}@#{DOMAIN}"
        EmailInfo.new(channel: CHANNEL, email: email)
      end

      # 获取邮件列表，通过 RSS XML 解析
      # @param email [String]
      # @return [Array<Email>]
      def get_emails(email)
        e = (email || "").strip
        raise "eyepaste: empty email" if e.empty?

        resp = Http.get("#{BASE}/inbox/#{e}.rss", headers: HEADERS, timeout: 15)
        resp.raise_for_status
        content = resp.body
        return [] if content.nil? || content.strip.empty?

        begin
          doc = REXML::Document.new(content)
        rescue REXML::ParseException
          return []
        end

        channel_elem = doc.root&.elements&.to_a("channel")&.first
        return [] unless channel_elem

        emails = []
        channel_elem.elements.each("item") do |item|
          idx = emails.length
          title = item.elements["title"]&.text.to_s.strip
          desc = item.elements["description"]&.text.to_s.strip

          parsed = parse_description(desc)
          subject = parsed["subject"].empty? ? title : parsed["subject"]
          from_addr = parsed["from"]
          to_addr = parsed["to"].empty? ? e : parsed["to"]
          date = parsed["date"]
          body_html = parsed["body"]

          text = ""
          text = body_html.gsub(TAG_RE, "").strip unless body_html.empty?

          emails << Email.new(
            id: (idx + 1).to_s,
            from_addr: from_addr,
            to: to_addr,
            subject: subject,
            text: text,
            html: body_html,
            date: date,
            is_read: false,
            attachments: []
          )
        end

        emails
      end
    end
  end
end
