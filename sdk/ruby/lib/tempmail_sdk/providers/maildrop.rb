# frozen_string_literal: true

module TempmailSdk
  module Providers
    # Maildrop — https://maildrop.cx
    # GET /api/suffixes.php、GET /api/emails.php
    module Maildrop
      CHANNEL = "maildrop"
      BASE = "https://maildrop.cx"
      EXCLUDED_SUFFIX = "transformer.edu.kg"
      HEADERS = {
        "Accept" => "application/json, text/plain, */*",
        "Referer" => "https://maildrop.cx/zh-cn/app",
        "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36",
        "x-requested-with" => "XMLHttpRequest"
      }.freeze

      module_function

      def random_local(length = 10)
        chars = ("a".."z").to_a + ("0".."9").to_a
        Array.new(length) { chars.sample }.join
      end

      def fetch_suffixes
        r = Http.get("#{BASE}/api/suffixes.php", headers: HEADERS)
        r.raise_for_status
        data = r.json
        raise "maildrop: invalid suffixes response" unless data.is_a?(Array)

        ex = EXCLUDED_SUFFIX.downcase
        out = data.select { |x| x.is_a?(String) }.map(&:strip).reject { |t| t.empty? || t.downcase == ex }
        raise "maildrop: no domains available" if out.empty?

        out
      end

      def pick_suffix(suffixes, preferred)
        if preferred && !preferred.to_s.strip.empty?
          p = preferred.to_s.strip.downcase
          found = suffixes.find { |d| d.downcase == p }
          raise "maildrop: domain not available: #{p}" unless found

          return found
        end
        suffixes.sample
      end

      def cx_date_to_iso(str)
        s = str.to_s.strip
        return "#{s[0, 10]}T#{s[11..]}Z" if s.length == 19 && s[10] == " "

        s
      end

      # @param domain [String, nil]
      # @return [EmailInfo]
      def generate_email(domain = nil)
        suffixes = fetch_suffixes
        dom = pick_suffix(suffixes, domain)
        email = "#{random_local}@#{dom}"
        EmailInfo.new(channel: CHANNEL, email: email, token: email)
      end

      # 通过详情接口获取单封邮件完整内容
      # GET /api/email_content.php?id={id}
      # 详情响应字段（从前端代码确认）:
      #   content: 完整 HTML 正文
      #   subject / from_addr / date: 邮件元数据
      #   attachment: JSON 字符串数组 [{filename, path, size}]
      def fetch_detail(id)
        mid = id.to_s.strip
        return nil if mid.empty?

        begin
          resp = Http.get("#{BASE}/api/email_content.php?id=#{URI.encode_www_form_component(mid)}",
                          headers: HEADERS, timeout: 15)
          return nil if resp.status_code < 200 || resp.status_code >= 300

          data = resp.body.empty? ? nil : resp.json
          data.is_a?(Hash) ? data : nil
        rescue StandardError
          nil
        end
      end

      # 解析详情接口的 attachment 字段（JSON 字符串）为附件数组
      def parse_attachments(raw)
        return [] if raw.nil? || !raw.is_a?(String) || raw.strip.empty?

        begin
          items = JSON.parse(raw)
        rescue JSON::ParserError
          return []
        end
        return [] unless items.is_a?(Array)

        items.filter_map do |it|
          next unless it.is_a?(Hash)

          filename = (it["filename"] || "").to_s.strip
          next if filename.empty?

          entry = { "filename" => filename }
          entry["size"] = it["size"].to_i if it["size"].is_a?(Numeric)
          entry
        end
      end

      # 获取邮件列表并对每封邮件补拉详情
      # 1. GET /api/emails.php 拉取列表（仅含 description 摘要）
      # 2. 对每封邮件 GET /api/email_content.php?id={id} 拉取详情（含 content 完整 HTML）
      # 3. 详情失败时保留列表 description 作为回退
      # @param email [String]
      # @param token [String, nil]
      # @return [Array<Email>]
      def get_emails(email, token = nil)
        addr = email.to_s.strip
        addr = token.to_s.strip if addr.empty?
        raise "maildrop: empty address" if addr.empty?

        qs = URI.encode_www_form(addr: addr, page: "1", limit: "20")
        r = Http.get("#{BASE}/api/emails.php?#{qs}", headers: HEADERS)
        r.raise_for_status
        data = r.body.empty? ? {} : r.json
        rows = data["emails"] || []
        return [] unless rows.is_a?(Array)

        rows.select { |row| row.is_a?(Hash) }.map do |row|
          mail_id = row["id"].to_s
          desc = (row["description"] || "").strip
          ir = row["isRead"]
          is_read = ir == true || ir == 1

          from = (row["from_addr"] || "").strip
          subject = (row["subject"] || "").strip
          date = cx_date_to_iso(row["date"] || "")
          html = ""
          attachments = []

          # 拉取详情覆盖 html/from/subject/date/attachments
          unless mail_id.empty?
            detail = fetch_detail(mail_id)
            if detail
              d_content = detail["content"]
              html = d_content if d_content.is_a?(String) && !d_content.strip.empty?
              d_from = detail["from_addr"]
              from = d_from.strip if d_from.is_a?(String) && !d_from.strip.empty?
              d_subj = detail["subject"]
              subject = d_subj.strip if d_subj.is_a?(String) && !d_subj.strip.empty?
              d_date = detail["date"]
              date = cx_date_to_iso(d_date) if d_date.is_a?(String) && !d_date.strip.empty?
              attachments = parse_attachments(detail["attachment"])
            end
          end

          Email.new(
            id: mail_id, from_addr: from, to: addr,
            subject: subject, text: desc,
            html: html, date: date,
            is_read: is_read, attachments: attachments
          )
        end
      end
    end
  end
end
