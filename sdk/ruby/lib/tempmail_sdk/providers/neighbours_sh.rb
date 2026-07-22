# frozen_string_literal: true

module TempmailSdk
  module Providers
    # neighbours-sh 渠道实现
    # 公共收件箱模式，任意用户名即可收信，无需注册
    # API: https://neighbours.sh/api/v1
    module NeighboursSh
      CHANNEL = "neighbours-sh"
      API_BASE = "https://neighbours.sh/api/v1"
      DOMAIN = "neighbours.sh"
      HEADERS = {
        "Accept" => "application/json",
        "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36"
      }.freeze

      module_function

      # 生成随机用户名：前缀 sdk + 10 位小写字母数字
      def random_local
        chars = ("a".."z").to_a + ("0".."9").to_a
        "sdk" + Array.new(10) { chars.sample }.join
      end

      # 从 from/to 结构中提取首个可用地址
      def first_address(value)
        return "" if value.nil?
        return value.strip if value.is_a?(String)

        if value.is_a?(Array)
          value.each do |item|
            hit = first_address(item)
            return hit unless hit.empty?
          end
          return ""
        end

        if value.is_a?(Hash)
          addr = value["address"].to_s.strip
          return addr unless addr.empty?

          text = value["text"].to_s.strip
          return text if text.include?("@")

          return first_address(value["value"])
        end

        value.to_s.strip
      end

      # 请求 JSON，allow_404 时 404 返回 nil
      def request_json(path, allow_404: false)
        resp = Http.get("#{API_BASE}#{path}", headers: HEADERS, timeout: 15)
        return nil if allow_404 && resp.status_code == 404

        resp.raise_for_status
        data = resp.json
        data.is_a?(Hash) ? data : nil
      end

      # 将邮件详情映射为标准化中间格式
      def flatten_message(detail, recipient)
        {
          "id" => (detail["uid"] || "").to_s,
          "from" => first_address(detail["from"]),
          "to" => first_address(detail["to"]).then { |v| v.empty? ? recipient : v },
          "subject" => (detail["subject"] || "").to_s,
          "text" => (detail["text"] || "").to_s,
          "html" => (detail["html"] || "").to_s,
          "date" => (detail["date"] || "").to_s,
          "is_read" => false,
          "attachments" => detail["attachments"] || []
        }
      end

      # 获取单封邮件详情
      def fetch_detail(address, uid)
        data = request_json("/inbox/#{URI.encode_www_form_component(address)}/#{URI.encode_www_form_component(uid)}", allow_404: true)
        return nil unless data.is_a?(Hash)

        detail = data["data"]
        detail.is_a?(Hash) ? detail : nil
      end

      # 创建临时邮箱（公共收件箱，本地生成地址）
      # @return [EmailInfo]
      def generate_email
        email = "#{random_local}@#{DOMAIN}"
        EmailInfo.new(channel: CHANNEL, email: email, token: email)
      end

      # 获取邮件列表
      # @param email [String]
      # @return [Array<Email>]
      def get_emails(email)
        address = email.to_s.strip
        raise "neighbours-sh: 缺少邮箱地址" if address.empty?

        data = request_json("/inbox/#{URI.encode_www_form_component(address)}", allow_404: true)
        return [] unless data

        rows = data["data"]
        return [] unless rows.is_a?(Array)

        rows.select { |item| item.is_a?(Hash) }.filter_map do |item|
          uid = item["uid"].to_s.strip
          next if uid.empty?

          detail = fetch_detail(address, uid)
          next unless detail

          Normalize.normalize_email(flatten_message(detail, address), address)
        end
      end
    end
  end
end
