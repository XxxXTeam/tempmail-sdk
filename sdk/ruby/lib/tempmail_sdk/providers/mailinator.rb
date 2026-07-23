# frozen_string_literal: true

require "time"

module TempmailSdk
  module Providers
    # Mailinator 渠道实现 -- https://mailinator.com
    #
    # 匿名公共信箱：邮箱本地部分即为信箱名，收信统一走
    # mailinator.com 的 public 域名 REST API。众多姊妹域名（如
    # blackhole.djurby.se、sink.fblay.com 等）本质是同一公共信箱的别名，
    # 仅生成邮箱时域名不同，取信逻辑完全一致。
    module Mailinator
      CHANNEL = "mailinator"
      BASE_URL = "https://mailinator.com"
      # 收信统一使用的公共域名标识
      PUBLIC_DOMAIN = "public"
      # 默认生成邮箱所用域名
      DEFAULT_DOMAIN = "mailinator.com"

      HEADERS = {
        "Accept" => "application/json",
        "Accept-Language" => "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
        "Cache-Control" => "no-cache",
        "Pragma" => "no-cache",
        "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " \
                        "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"
      }.freeze

      module_function

      # 生成随机本地部分：length 位小写字母数字
      def random_string(length)
        chars = ("a".."z").to_a + ("0".."9").to_a
        Array.new(length) { chars.sample }.join
      end

      # 生成临时邮箱
      # @param domain [String, nil] 邮箱域名（姊妹渠道派生用），缺省 mailinator.com
      # @param channel [String] 渠道标识
      # @return [EmailInfo]
      def generate_email(domain = nil, channel = CHANNEL)
        dom = domain.to_s.strip
        dom = DEFAULT_DOMAIN if dom.empty?
        EmailInfo.new(channel: channel, email: "#{random_string(12)}@#{dom}")
      end

      # 拉取邮件列表并逐封获取正文/附件
      # @param token [String] 未使用（公共信箱无需鉴权）
      # @param email [String] 邮箱地址
      # @return [Array<Email>]
      def get_emails(token, email = "")
        _ = token
        inbox = email.to_s.strip
        inbox = inbox.split("@", 2).first.to_s if inbox.include?("@")
        return [] if inbox.empty?

        data = request_json("#{BASE_URL}/api/v2/domains/#{PUBLIC_DOMAIN}/inboxes/#{inbox}")
        messages = parse_messages(data)
        return [] if messages.empty?

        messages.map do |msg|
          message_id = (msg["id"] || msg["messageId"]).to_s.strip
          text_payload = message_id.empty? ? nil : request_json(message_path(message_id, "text"))
          html_payload = message_id.empty? ? nil : request_json(message_path(message_id, "texthtml"))
          att_payload  = message_id.empty? ? nil : request_json(message_path(message_id, "attachments"))
          Normalize.normalize_email(
            flatten_message(msg, email, text_payload, html_payload, att_payload), email
          )
        end
      end

      # 拼接单封邮件某子资源的 URL
      def message_path(message_id, suffix)
        "#{BASE_URL}/api/v2/domains/#{PUBLIC_DOMAIN}/messages/#{message_id}/#{suffix}"
      end

      # 发起 GET 并解析 JSON，失败返回 nil；非 Hash 结果包裹为 { "data" => ... }
      def request_json(url)
        resp = Http.get(url, headers: HEADERS, timeout: 15)
        return nil unless resp.ok?

        data = resp.json
        data.is_a?(Hash) ? data : { "data" => data }
      rescue StandardError
        nil
      end

      # 从响应中解析消息数组（兼容顶层数组或 msgs/data 字段）
      def parse_messages(data)
        return data.select { |item| item.is_a?(Hash) } if data.is_a?(Array)

        if data.is_a?(Hash)
          %w[msgs data].each do |key|
            value = data[key]
            return value.select { |item| item.is_a?(Hash) } if value.is_a?(Array)
          end
        end
        []
      end

      # 从 payload 中取指定 content-type 的文本
      def text_from_payload(payload, key)
        return "" unless payload.is_a?(Hash)

        value = payload[key]
        return "" if value.nil?

        value.is_a?(String) ? value : value.to_s
      end

      # 补全附件下载 URL（相对路径补上 BASE_URL）
      def attachment_url(value)
        return "" unless value.is_a?(String) && !value.empty?
        return value if value.start_with?("http://", "https://")

        "#{BASE_URL}#{value}"
      end

      # 将时间值统一为 ISO 8601（毫秒/秒时间戳或字符串）
      def to_iso_time(value)
        return "" if value.nil? || value == ""

        if value.is_a?(Numeric)
          millis = value > 1e12 ? value : value * 1000
          return Time.at(millis / 1000.0).utc.iso8601
        end

        text = value.to_s
        begin
          Time.iso8601(text.sub(/Z\z/, "+00:00")).utc.iso8601
        rescue ArgumentError
          text
        end
      end

      # 将邮件摘要与正文/附件展平为 normalize 可识别的字段
      def flatten_message(summary, recipient_email, text_payload, html_payload, att_payload)
        # 优先读 "text" 键（/text 端点实际返回键），回退兜底 "text/plain"（防御性编程）
        text_content = text_from_payload(text_payload, "text")
        text_content = text_from_payload(text_payload, "text/plain") if text_content.empty?
        # 优先读 "html" 键，回退兜底 "text/html"
        html_content = text_from_payload(html_payload, "html")
        html_content = text_from_payload(html_payload, "text/html") if html_content.empty?

        {
          "id" => (summary["id"] || summary["messageId"] || "").to_s,
          "from" => first_non_empty(summary["from"], summary["origfrom"]),
          "to" => summary["to"] || recipient_email,
          "subject" => summary["subject"] || "",
          "text" => text_content,
          "html" => html_content,
          "date" => to_iso_time(summary["time"] || summary["date"]),
          "seen" => false,
          "attachments" => build_attachments(att_payload)
        }
      end

      # 展平附件列表
      def build_attachments(att_payload)
        list = att_payload.is_a?(Hash) ? att_payload["attachments"] : nil
        return [] unless list.is_a?(Array)

        list.filter_map do |att|
          next unless att.is_a?(Hash)

          {
            "filename" => att["name"] || att["filename"] || "",
            "size" => att["size"] || att["filesize"],
            "contentType" => att["contentType"] || att["content_type"] ||
              att["mimeType"] || att["mime_type"],
            "url" => attachment_url(att["downloadUrl"] || att["url"])
          }
        end
      end

      # 返回首个非空字符串
      def first_non_empty(*values)
        values.each do |v|
          next if v.nil?

          t = v.to_s
          return t unless t.empty?
        end
        ""
      end
    end
  end
end
