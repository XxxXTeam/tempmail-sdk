# frozen_string_literal: true

module TempmailSdk
  module Providers
    # anonbox.net (CCC) — GET /en/ 解析 HTML，HTTP mbox 取信
    module Anonbox
      CHANNEL = "anonbox"
      PAGE_URL = "https://anonbox.net/en/"
      BASE = "https://anonbox.net"

      MAIL_LINK_RE = %r{<a href="https://anonbox\.net/([a-z0-9-]+)/([A-Za-z0-9._~-]+)">https://anonbox\.net/[^"]+</a>}i
      DD_RE = /<dd([^>]*)>([\s\S]*?)<\/dd>/mi
      DISPLAY_NONE_RE = /display\s*:\s*none/i
      P_RE = /<p>([\s\S]*?)<\/p>/mi
      SPAN_RE = /<span\b[^>]*>[\s\S]*?<\/span>/mi
      TAG_RE = /<[^>]+>/
      EXPIRES_RE = /Your mail address is valid until:<\/dt>\s*<dd><p>([^<]+)<\/p>/mi
      MBOX_SPLIT_RE = /\r?\n(?=From )/

      HEADERS = {
        "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36",
        "Accept" => "text/html,application/xhtml+xml"
      }.freeze

      module_function

      # 去除 HTML 标签并还原常见实体
      def strip_tags(html_str)
        s = html_str.gsub(TAG_RE, "")
        s.gsub("&nbsp;", " ")
         .gsub("&amp;", "&")
         .gsub("&lt;", "<")
         .gsub("&gt;", ">")
         .strip
      end

      # 简单哈希，用于生成邮件 ID
      def simple_hash(s)
        h = 0
        s.each_char { |c| h = (h * 31 + c.ord) & 0xFFFFFFFF }
        return "0" if h == 0

        digits = "0123456789abcdefghijklmnopqrstuvwxyz"
        out = []
        n = h
        while n > 0
          out.unshift(digits[n % 36])
          n /= 36
        end
        out.join
      end

      # 从首页 HTML 中解析邮箱地址、token、过期时间
      def parse_en_page(html_str)
        m = html_str.match(MAIL_LINK_RE)
        raise "anonbox: mailbox link not found" unless m

        inbox = m[1]
        secret = m[2]
        token = "#{inbox}/#{secret}"

        # 从 <dd> 中提取邮箱地址
        address_html = nil
        html_str.scan(DD_RE).each do |attrs, inner|
          next if attrs.match?(DISPLAY_NONE_RE)

          pm = inner.match(P_RE)
          next unless pm

          p_inner = pm[1].gsub(SPAN_RE, "")
          display = strip_tags(p_inner)
          next unless display.include?("@")

          at = display.rindex("@")
          local = display[0...at].strip
          domain = display[(at + 1)..].strip.downcase
          next if domain == "googlemail.com"

          expected_domain = "#{inbox}.anonbox.net".downcase
          next if domain != expected_domain
          next if local.empty?

          address_html = display
          break
        end

        raise "anonbox: address paragraph not found" unless address_html

        at = address_html.index("@")
        raise "anonbox: bad address" unless at

        local = address_html[0...at].strip
        raise "anonbox: empty local part" if local.empty?

        email = "#{local}@#{inbox}.anonbox.net"
        em = html_str.match(EXPIRES_RE)
        exp = em ? em[1].strip : nil
        [email, token, exp]
      end

      # 解码 quoted-printable 编码
      def decode_qp_if_needed(body, header_block)
        te = header_block.match(/(?i)^content-transfer-encoding:\s*([^\s]+)/m)
        enc = te ? te[1].downcase.strip : ""
        return body.rstrip unless enc == "quoted-printable"

        soft = body.gsub(/=\r?\n/, "")
        result = []
        bytes = soft.bytes
        i = 0
        while i < bytes.length
          if bytes[i] == 61 && i + 2 < bytes.length # '='
            hex = [bytes[i + 1], bytes[i + 2]].pack("C*")
            begin
              result << hex.to_i(16)
              i += 3
              next
            rescue ArgumentError
              # 非法十六进制，作为普通字符处理
            end
          end
          result << bytes[i]
          i += 1
        end
        result.pack("C*").force_encoding("UTF-8").rstrip
      end

      # 解析 mbox 格式单条邮件
      def mbox_block_to_raw(block, recipient)
        normalized = block.gsub("\r\n", "\n")
        lines = normalized.split("\n")
        i = 0
        i = 1 if lines.first&.start_with?("From ")

        headers = {}
        cur_key = ""
        while i < lines.length
          line = lines[i]
          if line == ""
            i += 1
            break
          end
          if (line[0] == " " || line[0] == "\t") && !cur_key.empty?
            headers[cur_key] = "#{headers[cur_key]} #{line.strip}"
          else
            idx = line.index(":")
            if idx && idx > 0
              cur_key = line[0...idx].strip.downcase
              headers[cur_key] = line[(idx + 1)..].strip
            end
          end
          i += 1
        end

        body = lines[i..].join("\n")
        ct = (headers["content-type"] || "").downcase
        text = ""
        html = ""

        if ct.include?("multipart/")
          bm = headers["content-type"]&.match(/(?i)boundary="?([^";\s]+)"?/)
          if bm
            boundary = Regexp.escape(bm[1])
            parts = body.split(/\r?\n--#{boundary}(?:--)?\r?\n/)
            parts.each do |part|
              part = part.strip
              next if part.empty? || part == "--"

              sep = part.index("\n\n")
              next unless sep

              ph = part[0...sep]
              pb = part[(sep + 2)..]
              pct_m = ph.match(/(?i)^content-type:\s*([^\s;]+)/m)
              pct = pct_m ? pct_m[1].downcase : ""

              if pct == "text/plain"
                text = decode_qp_if_needed(pb, ph)
              elsif pct == "text/html"
                html = decode_qp_if_needed(pb, ph)
              end
            end
          end
        end

        if text.empty? && html.empty?
          if ct.include?("text/html")
            html = decode_qp_if_needed(body, headers["content-type"] || "")
          else
            text = decode_qp_if_needed(body, headers["content-type"] || "")
          end
        end

        date_str = headers["date"] || ""
        unless date_str.empty?
          begin
            date_str = Time.parse(date_str.strip).iso8601
          rescue ArgumentError, TypeError
            # 保持原值
          end
        end
        date_str = Time.now.utc.iso8601 if date_str.empty?

        msg_id = headers["message-id"] || simple_hash(block[0, 512])
        {
          "id" => msg_id,
          "from" => headers["from"] || "",
          "to" => headers["to"] || recipient,
          "subject" => headers["subject"] || "",
          "body_text" => text,
          "body_html" => html,
          "date" => date_str,
          "isRead" => false,
          "attachments" => []
        }
      end

      # 创建 anonbox 临时邮箱
      # @return [EmailInfo]
      def generate_email
        r = Http.get(PAGE_URL, headers: HEADERS, timeout: 15)
        r.raise_for_status
        email, token, exp = parse_en_page(r.body)
        EmailInfo.new(channel: CHANNEL, email: email, token: token, created_at: exp)
      end

      # 获取邮件列表
      # @param token [String] inbox/secret 路径
      # @param email [String]
      # @return [Array<Email>]
      def get_emails(token, email)
        raise "token is required for anonbox channel" if token.nil? || token.to_s.empty?

        path = token.end_with?("/") ? token : "#{token}/"
        url = "#{BASE}/#{path}"
        r = Http.get(url, headers: HEADERS.merge("Accept" => "text/plain,*/*"), timeout: 15)
        return [] if r.status_code == 404

        r.raise_for_status
        raw = r.body.strip
        return [] if raw.empty?

        blocks = raw.split(MBOX_SPLIT_RE).map(&:strip).reject(&:empty?)
        blocks.map { |b| Normalize.normalize_email(mbox_block_to_raw(b, email), email) }
      end
    end
  end
end
