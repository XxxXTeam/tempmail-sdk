# frozen_string_literal: true

require "securerandom"

module TempmailSdk
  module Providers
    # MailCatch 渠道 — https://mailcatch.com
    # 公开收件箱，无需认证。GET /api/list/{inbox} + GET /api/data/{inbox}/{id}
    module Mailcatch
      CHANNEL = "mailcatch"
      BASE_URL = "https://mailcatch.com"
      DOMAIN = "mailcatch.com"

      # 匹配邮件列表 HTML 中的邮件项
      EMAIL_ITEM_RE = /class="email-item"\s+data-email-id="([^"]*)"\s+data-subject="([^"]*)"\s+data-timestamp="([^"]*)"\s+data-sender="([^"]*)"/i

      HEADERS = {
        "Accept" => "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
        "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " \
                        "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"
      }.freeze

      module_function

      # 生成随机用户名
      def random_local
        chars = ("a".."z").to_a + ("0".."9").to_a
        "sdk" + Array.new(16) { chars.sample }.join
      end

      # 提取邮箱地址的 @ 前部分
      def local_part(email)
        (email || "").strip.split("@").first.to_s.strip
      end

      # 创建 mailcatch 临时邮箱
      # @return [EmailInfo]
      def generate_email
        local = random_local
        email = "#{local}@#{DOMAIN}"
        EmailInfo.new(channel: CHANNEL, email: email, token: local)
      end

      # 获取邮件列表
      # @param email [String]
      # @param token [String]
      # @return [Array<Email>]
      def get_emails(email, token)
        raise "mailcatch: token 不能为空" if token.nil? || token.to_s.empty?

        addr = (email || "").strip
        raise "mailcatch: 邮箱地址不能为空" if addr.empty?

        inbox = token.strip
        inbox = local_part(addr) if inbox.empty?
        raise "mailcatch: 无法确定收件箱名称" if inbox.empty?

        # 获取邮件列表 HTML
        resp = Http.get("#{BASE_URL}/api/list/#{inbox}", headers: HEADERS, timeout: 15)
        resp.raise_for_status

        items = resp.body.scan(EMAIL_ITEM_RE)
        return [] if items.empty?

        items.filter_map do |email_id, subject, timestamp, sender|
          email_id = email_id.strip
          next if email_id.empty?

          # 获取邮件正文 HTML
          body_html = ""
          begin
            detail_resp = Http.get("#{BASE_URL}/api/data/#{inbox}/#{email_id}", headers: HEADERS, timeout: 15)
            body_html = detail_resp.body.strip if detail_resp.ok?
          rescue StandardError
            # 忽略正文获取失败
          end

          raw = {
            "id" => email_id,
            "from" => sender.strip,
            "to" => addr,
            "subject" => subject.strip,
            "html" => body_html,
            "date" => timestamp.strip
          }
          Normalize.normalize_email(raw, addr)
        end
      end
    end
  end
end
