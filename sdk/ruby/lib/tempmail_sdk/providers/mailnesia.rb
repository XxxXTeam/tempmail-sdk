# frozen_string_literal: true

require "cgi"
require "securerandom"
require "uri"

module TempmailSdk
  module Providers
    # Mailnesia 渠道 — https://mailnesia.com
    # HTML 解析公开邮箱
    module Mailnesia
      CHANNEL = "mailnesia"
      BASE_URL = "https://mailnesia.com"
      DOMAIN = "mailnesia.com"
      HEADERS = { "Accept" => "text/html,*/*" }.freeze

      ROW_RE = /<tr\s+id="([^"]+)"[^>]*class="[^"]*\bemailheader\b[^"]*"[^>]*>(.*?)<\/tr>/mi
      TIME_RE = /<time\s+datetime="([^"]+)"/i
      ANCHOR_RE = /<a\b[^>]*class="email"[^>]*>(.*?)<\/a>/mi
      TAG_RE = /<[^>]+>/

      module_function

      def random_local
        chars = ("a".."z").to_a + ("0".."9").to_a
        "sdk" + Array.new(16) { chars.sample }.join
      end

      def local_part(email)
        (email || "").strip.split("@").first.to_s.strip
      end

      def mailbox_url(local)
        "#{BASE_URL}/mailbox/#{URI.encode_www_form_component(local)}"
      end

      def detail_url(local, message_id)
        "#{mailbox_url(local)}/#{URI.encode_www_form_component(message_id)}"
      end

      def fetch_html(url)
        resp = Http.get(url, headers: HEADERS, timeout: 15)
        resp.raise_for_status
        resp.body
      end

      def clean_text(raw)
        CGI.unescapeHTML(raw.to_s.gsub(TAG_RE, " ")).split.join(" ")
      end

      # 从邮箱列表页解析邮件条目
      def parse_rows(page)
        rows = []
        page.scan(ROW_RE).each do |message_id, row_html|
          message_id = message_id.strip
          date_match = row_html.match(TIME_RE)
          anchors = row_html.scan(ANCHOR_RE).map { |m| clean_text(m[0]) }
          next if anchors.length < 3

          rows << {
            "id" => message_id,
            "date" => date_match ? CGI.unescapeHTML(date_match[1].strip) : "",
            "from" => anchors[0],
            "to" => anchors[1],
            "subject" => anchors[2]
          }
        end
        rows
      end

      # 从详情页提取指定 div 内容
      def extract_div_by_id(page, div_id, next_id = "")
        needle = "id=\"#{div_id}\""
        pos = page.index(needle)
        return "" unless pos

        open_end = page.index(">", pos)
        return "" unless open_end

        start = open_end + 1
        end_pos = -1
        end_pos = page.index("<div id=\"#{next_id}\"", start) unless next_id.empty?
        if end_pos < 0
          close = page.index("</div>", start)
          end_pos = close + 6 if close
        end
        return "" if end_pos < 0

        content = page[start...end_pos].strip
        content = content[0...-6].strip if content.end_with?("</div>")
        content
      end

      # 提取纯文本部分
      def extract_plain(page, message_id)
        pattern = /<div\s+id="text_plain_#{Regexp.escape(message_id)}"[^>]*>\s*<pre>(.*?)<\/pre>\s*<\/div>/mi
        m = page.match(pattern)
        m ? CGI.unescapeHTML(m[1]).strip : ""
      end

      # 获取单封详情
      def fetch_detail(local, row)
        message_id = (row["id"] || "").strip
        return row if message_id.empty?

        page = fetch_html(detail_url(local, message_id))
        detail = row.dup
        detail["text"] = extract_plain(page, message_id)
        detail["html"] = extract_div_by_id(page, "text_html_#{message_id}", "text_plain_#{message_id}")
        detail
      end

      # 创建临时邮箱
      # @return [EmailInfo]
      def generate_email
        local = random_local
        fetch_html(mailbox_url(local))
        EmailInfo.new(channel: CHANNEL, email: "#{local}@#{DOMAIN}")
      end

      # 获取邮件列表
      # @param email [String]
      # @return [Array<Email>]
      def get_emails(email)
        local = local_part(email)
        raise "mailnesia: empty email" if local.empty?

        page = fetch_html(mailbox_url(local))
        rows = parse_rows(page)

        rows.map do |row|
          begin
            Normalize.normalize_email(fetch_detail(local, row), email)
          rescue StandardError
            Normalize.normalize_email(row, email)
          end
        end
      end
    end
  end
end
