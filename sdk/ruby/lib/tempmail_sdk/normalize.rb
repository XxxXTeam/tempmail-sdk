# frozen_string_literal: true

require "time"

module TempmailSdk
  # 邮件数据标准化模块
  # 将各提供商返回的原始邮件数据（Hash，键为字符串）标准化为统一的 Email 格式
  module Normalize
    # 各字段的候选键顺序（frozen 常量，避免每次调用重复分配数组）
    ID_KEYS = %w[id eid _id mailboxId messageId mail_id].freeze
    FROM_KEYS = %w[from_addr from_address fromAddress mail_sender sender
                   address_from from_email from messageFrom].freeze
    TO_KEYS = %w[to to_address toAddress name_to email_address address].freeze
    SUBJECT_KEYS = %w[subject e_subject mail_title].freeze
    TEXT_KEYS = %w[text text_body preview_text mail_body_text body content
                   body_text text_content description].freeze
    HTML_KEYS = %w[html html_body html_content body_html mail_body_html].freeze
    DATE_STR_KEYS = %w[received_at receivedAt created_at createdAt date].freeze
    DATE_NUM_KEYS = %w[timestamp e_date].freeze
    IS_READ_BOOL_KEYS = %w[seen read isRead].freeze
    IS_READ_MIXED_KEYS = %w[is_read is_seen].freeze

    # 日期解析用预编译正则（避免方法内每次新建 Regexp）
    TRAILING_Z_RE = /Z\z/.freeze

    module_function

    # 将各提供商返回的原始邮件数据标准化为统一的 Email 格式
    # 不同渠道字段名各异，此函数通过多字段候选策略统一映射
    # @param raw [Hash] 原始邮件数据（字符串键）
    # @param recipient_email [String] 收件人邮箱（无匹配字段时回退）
    # @return [Email]
    def normalize_email(raw, recipient_email = "")
      raw = stringify_keys(raw)
      text = normalize_text(raw)
      html = normalize_html(raw)

      # 修正 text/html 错配
      if !text.empty? && html.empty? && html_content?(text)
        html = text
        text = ""
      end
      text = HtmlUtils.html_to_text(html) if text.empty? && !html.empty?
      html = "<html><body><pre>#{HtmlUtils.escape(text)}</pre></body></html>" if !text.empty? && html.empty?

      Email.new(
        id: normalize_id(raw),
        from_addr: normalize_from(raw),
        to: normalize_to(raw, recipient_email),
        subject: normalize_subject(raw),
        text: text,
        html: html,
        date: normalize_date(raw),
        is_read: normalize_is_read(raw),
        attachments: normalize_attachments(raw["attachments"])
      )
    end

    # 将 Hash 的符号键统一转为字符串键（浅层）
    def stringify_keys(raw)
      return {} unless raw.is_a?(Hash)

      raw.each_with_object({}) { |(k, v), h| h[k.to_s] = v }
    end

    # 检测内容是否为 HTML（只取前 200 字符）
    def html_content?(content)
      prefix = content[0, 200].to_s.strip.downcase
      return true if prefix.start_with?("<!doctype html", "<html", "<body")

      trimmed = content.strip.downcase
      return true if trimmed.include?("<div") && trimmed.include?("</div>")
      return true if trimmed.include?("<table") && trimmed.include?("</table>")
      return true if trimmed.include?("<p") && trimmed.include?("</p>") && trimmed.include?("<")

      false
    end

    # 从 Hash 中按优先级顺序提取字符串值
    def get_str(raw, *keys)
      get_str_keys(raw, keys)
    end

    # 从 Hash 中按给定候选键数组（可复用 frozen 常量）提取字符串值
    def get_str_keys(raw, keys)
      keys.each do |key|
        val = raw[key]
        return val.to_s unless val.nil?
      end
      ""
    end

    def normalize_id(raw)
      get_str_keys(raw, ID_KEYS)
    end

    def normalize_from(raw)
      get_str_keys(raw, FROM_KEYS)
    end

    def normalize_to(raw, recipient_email)
      result = get_str_keys(raw, TO_KEYS)
      result.empty? ? recipient_email : result
    end

    def normalize_subject(raw)
      get_str_keys(raw, SUBJECT_KEYS)
    end

    def normalize_text(raw)
      get_str_keys(raw, TEXT_KEYS)
    end

    def normalize_html(raw)
      get_str_keys(raw, HTML_KEYS)
    end

    # 提取并统一日期格式为 ISO 8601
    def normalize_date(raw)
      DATE_STR_KEYS.each do |key|
        val = raw[key]
        next if val.nil?

        return parse_date_value(val)
      end

      DATE_NUM_KEYS.each do |key|
        val = raw[key]
        next unless val.is_a?(Numeric) && val.positive?

        secs = key == "timestamp" && val < 1e12 ? val : val / 1000.0
        return Time.at(secs).utc.iso8601
      end

      ""
    end

    # 解析单个日期值（字符串/秒/毫秒时间戳）
    def parse_date_value(val)
      if val.is_a?(String) && !val.empty?
        begin
          return Time.iso8601(val.sub(TRAILING_Z_RE, "+00:00")).iso8601
        rescue ArgumentError
          begin
            return Time.strptime("#{val} UTC", "%Y-%m-%d %H:%M:%S %Z").utc.iso8601
          rescue ArgumentError
            return val
          end
        end
      elsif val.is_a?(Numeric) && val.positive?
        secs = val > 1e12 ? val / 1000.0 : val
        return Time.at(secs).utc.iso8601
      end
      ""
    end

    # 提取已读状态，支持 bool / int(0|1) / str('0'|'1')
    def normalize_is_read(raw)
      IS_READ_BOOL_KEYS.each do |key|
        val = raw[key]
        return val if val == true || val == false
      end

      IS_READ_MIXED_KEYS.each do |key|
        val = raw[key]
        next if val.nil?

        return val if [true, false].include?(val)
        return val.to_i != 0 if val.is_a?(Numeric)
        return val == "1" if val.is_a?(String)
      end

      false
    end

    # 提取并标准化附件列表
    def normalize_attachments(attachments)
      return [] unless attachments.is_a?(Array)

      attachments.filter_map do |a|
        a = stringify_keys(a)
        next unless a.is_a?(Hash)

        EmailAttachment.new(
          filename: a["filename"] || a["name"] || "",
          size: a["size"] || a["filesize"],
          content_type: a["contentType"] || a["content_type"] || a["mimeType"] || a["mime_type"],
          url: a["url"] || a["download_url"] || a["downloadUrl"]
        )
      end
    end
  end
end
