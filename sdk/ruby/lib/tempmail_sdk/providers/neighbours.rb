# frozen_string_literal: true

module TempmailSdk
  module Providers
    # Neighbours 渠道实现
    # API: https://neighbours.sh/api/v1
    # 支持多域名选择
    module Neighbours
      CHANNEL = "neighbours"
      API_BASE = "https://neighbours.sh/api/v1"
      HEADERS = {
        "Accept" => "application/json",
        "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36"
      }.freeze

      module_function

      # 生成随机用户名
      def random_local
        chars = ("a".."z").to_a + ("0".."9").to_a
        "sdk" + Array.new(16) { chars.sample }.join
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

      # 将邮件映射为标准化中间格式
      def flatten_message(raw, recipient)
        {
          "id" => (raw["uid"] || "").to_s,
          "from" => first_address(raw["from"]),
          "to" => first_address(raw["to"]).then { |v| v.empty? ? recipient : v },
          "subject" => (raw["subject"] || "").to_s,
          "text" => (raw["text"] || raw["snippet"] || "").to_s,
          "html" => (raw["html"] || "").to_s,
          "date" => (raw["date"] || raw["received_at"] || "").to_s,
          "is_read" => false,
          "attachments" => raw["attachments"] || []
        }
      end

      # 获取可用域名列表
      def list_domains
        data = request_json("/config/domains")
        domains = []
        if data.is_a?(Hash)
          nested = data["data"]
          if nested.is_a?(Hash)
            (nested["domains"] || []).each do |item|
              s = item.to_s.strip.downcase
              domains << s unless s.empty?
            end
          end
        end
        raise "neighbours: 域名列表为空" if domains.empty?

        domains
      end

      # 获取单封邮件详情
      def fetch_detail(address, uid)
        data = request_json("/inbox/#{URI.encode_www_form_component(address)}/#{URI.encode_www_form_component(uid)}", allow_404: true)
        return nil unless data.is_a?(Hash)

        detail = data["data"]
        detail.is_a?(Hash) ? detail : nil
      end

      # 创建临时邮箱
      # @param domain [String, nil]
      # @return [EmailInfo]
      def generate_email(domain = nil)
        domains = list_domains
        if domain && !domain.to_s.strip.empty?
          wanted = domain.strip.downcase
          raise "neighbours: 不支持的域名 #{domain}" unless domains.include?(wanted)

          selected = wanted
        else
          selected = domains.sample
        end
        email = "#{random_local}@#{selected}"
        EmailInfo.new(channel: CHANNEL, email: email, token: email)
      end

      # 获取邮件列表
      # @param email [String]
      # @return [Array<Email>]
      def get_emails(email)
        address = email.to_s.strip
        raise "neighbours: 邮箱地址为空" if address.empty?

        data = request_json("/inbox/#{URI.encode_www_form_component(address)}", allow_404: true)
        return [] unless data.is_a?(Hash)

        rows = data["data"]
        return [] unless rows.is_a?(Array)

        rows.select { |item| item.is_a?(Hash) }.map do |item|
          uid = item["uid"].to_s.strip
          detail = uid.empty? ? nil : fetch_detail(address, uid)
          Normalize.normalize_email(flatten_message(detail || item, address), address)
        end
      end
    end
  end
end
