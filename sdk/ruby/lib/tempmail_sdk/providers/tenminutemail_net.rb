# frozen_string_literal: true

require "time"

module TempmailSdk
  module Providers
    # 10minutemail.net 渠道实现
    #
    # 流程：GET / 获取 PHPSESSID session cookie + 从 HTML 输入框提取邮箱地址
    #       GET /mailbox.ajax.php?_={毫秒时间戳} 获取邮件列表（HTML 表格）
    #       GET /readmail.html?mid={id} 获取单封邮件正文
    # token 为空字符串（session 由 cookie jar 自动维护）
    module TenminutemailNet
      CHANNEL = "10minutemail-net"
      BASE_URL = "https://10minutemail.net"

      EMAIL_RE    = /value="([^"]+@[^"]+)"/
      ROW_RE      = /<tr[^>]*>(.*?)<\/tr>/im
      MID_RE      = /readmail\.html\?mid=([^'"&]+)/i
      CELL_RE     = /<td[^>]*>(.*?)<\/td>/im
      TITLE_RE    = /title="([^"]+)"/i
      CF_RE       = /data-cfemail="([0-9a-fA-F]+)"/i
      TAG_RE      = /<[^>]+>/
      BODY_MARK   = 'class="mailinhtml"'

      BROWSER_HEADERS = {
        "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " \
                        "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
        "Accept" => "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif," \
                    "image/webp,image/apng,*/*;q=0.8",
        "Accept-Language" => "en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7"
      }.freeze

      AJAX_HEADERS = BROWSER_HEADERS.merge(
        "Accept" => "*/*",
        "X-Requested-With" => "XMLHttpRequest",
        "Referer" => BASE_URL + "/"
      ).freeze

      module_function

      # 解码 Cloudflare 邮箱保护混淆字符串
      # @param encoded [String] 十六进制字符串
      # @return [String]
      def cf_decode(encoded)
        data = [encoded].pack("H*")
        return "" if data.length < 2

        key = data.getbyte(0)
        out = data.bytes[1..].map { |b| b ^ key }.pack("C*")
        out.include?("@") ? out : ""
      rescue StandardError
        ""
      end

      # 从单元格 HTML 提取纯文本（优先解码 CF 混淆）
      # @param cell [String]
      # @return [String]
      def extract_text(cell)
        if (m = cell.match(CF_RE))
          decoded = cf_decode(m[1])
          return decoded unless decoded.empty?
        end

        text = cell.gsub(TAG_RE, "")
        text = text.gsub("&nbsp;", " ").gsub("&#160;", " ")
                   .gsub("&amp;", "&").gsub("&lt;", "<")
                   .gsub("&gt;", ">").gsub("&quot;", '"')
        text.strip
      end

      # 从整页 HTML 提取 mailinhtml div 正文
      # @param page [String]
      # @return [String]
      def extract_body(page)
        si = page.index(BODY_MARK)
        return "" unless si

        gt = page.index(">", si)
        return "" unless gt

        content_start = gt + 1
        rest = page[content_start..]

        ei = rest.index("email-decode.min.js")
        segment = if ei
                    s = rest[0...ei]
                    s = s[0...s.rindex("<script")] if s.rindex("<script")
                    s.strip.sub(/<\/div>\s*\z/, "").strip.sub(/<\/div>\s*\z/, "").strip
                  else
                    di = rest.index("</div>")
                    di ? rest[0...di].strip : rest.strip
                  end
        segment
      end

      # 创建 10minutemail.net 临时邮箱
      # @return [EmailInfo]
      def generate_email
        resp = Http.get(BASE_URL + "/", headers: BROWSER_HEADERS, timeout: 15)
        resp.raise_for_status
        m = resp.body.match(EMAIL_RE)
        raise "10minutemail-net: 未能从首页提取邮箱地址" unless m

        email = m[1].strip
        raise "10minutemail-net: 获取到的邮箱地址无效" if email.empty? || !email.include?("@")

        EmailInfo.new(channel: CHANNEL, email: email, token: "")
      end

      # 获取 10minutemail.net 邮件列表
      # @param email [String]
      # @param token [String] 未使用（session 由 cookie jar 维护）
      # @return [Array<Email>]
      def get_emails(email, token)
        _ = token
        ts = (Time.now.to_f * 1000).to_i
        list_url = "#{BASE_URL}/mailbox.ajax.php?_=#{ts}"
        resp = Http.get(list_url, headers: AJAX_HEADERS, timeout: 15)
        resp.raise_for_status

        rows = resp.body.scan(ROW_RE)
        emails = []
        rows.each do |row_inner_arr|
          row_inner = row_inner_arr[0]
          next if row_inner.downcase.include?("<th")

          mid_m = row_inner.match(MID_RE)
          next unless mid_m

          mail_id = mid_m[1].strip
          next if mail_id.empty?

          cells = row_inner.scan(CELL_RE).map { |c| c[0] }
          from_cell    = cells[0] || ""
          subject_cell = cells[1] || ""
          date_cell    = cells[2] || ""

          from    = extract_text(from_cell)
          subject = extract_text(subject_cell)
          date    = if (tm = date_cell.match(TITLE_RE))
                      tm[1].strip
                    else
                      extract_text(date_cell)
                    end

          # 未读行的 <tr> 样式含 font-weight: bold
          full_row = "<tr#{row_inner}</tr>"
          is_read = !full_row.downcase.include?("font-weight: bold")

          html_body = fetch_body(mail_id)

          flat = {
            "id" => mail_id, "from" => from, "to" => email,
            "subject" => subject, "html" => html_body,
            "date" => date, "isRead" => is_read
          }
          emails << Normalize.normalize_email(flat, email)
        end
        emails
      end

      # 获取单封邮件正文
      # @param mail_id [String]
      # @return [String]
      def fetch_body(mail_id)
        return "" if mail_id.empty?

        url = "#{BASE_URL}/readmail.html?mid=#{mail_id}"
        resp = Http.get(
          url,
          headers: BROWSER_HEADERS.merge("Referer" => BASE_URL + "/"),
          timeout: 15
        )
        return "" unless resp.status_code == 200

        extract_body(resp.body)
      rescue StandardError
        ""
      end
    end
  end
end
