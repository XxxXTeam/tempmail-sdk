# frozen_string_literal: true

require "securerandom"
require "uri"

module TempmailSdk
  module Providers
    # GetNada 渠道实现 -- https://getnada.net
    # 匿名 JSON API：拉取公开域名 -> 开信箱换取 token -> 拉取邮件列表/详情
    module Getnada
      CHANNEL = "getnada"
      API_BASE = "https://getnada.net/api"
      HEADERS_JSON = { "Accept" => "application/json" }.freeze
      HEADERS_POST = { "Accept" => "application/json", "Content-Type" => "application/json" }.freeze
      # 单个域名标签校验（大小写不敏感）
      DOMAIN_LABEL_RE = /\A[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?\z/i.freeze

      module_function

      # 生成随机本地部分：sdk + 16 位小写字母数字
      def random_local
        chars = ("a".."z").to_a + ("0".."9").to_a
        "sdk" + Array.new(16) { chars.sample }.join
      end

      # 清洗并校验域名，非法返回空串
      def clean_domain(value)
        domain = value.to_s.strip.downcase
        return "" if domain.empty? || domain.length > 253 || domain.include?("..")

        labels = domain.split(".")
        return "" if labels.length < 2
        return "" unless labels.all? { |label| DOMAIN_LABEL_RE.match?(label) }

        domain
      end

      # 拉取公开域名列表，按偏好挑选，缺省优先 getnada.net
      # @param preferred [String, nil]
      # @return [String]
      def pick_domain(preferred = nil)
        resp = Http.get("#{API_BASE}/public/domains", headers: HEADERS_JSON, timeout: 15)
        resp.raise_for_status
        data = resp.json
        domains = data.is_a?(Hash) ? data["domains"] : []
        domains = [] unless domains.is_a?(Array)
        cleaned = domains.map { |d| clean_domain(d) }.reject(&:empty?)

        wanted = preferred.to_s.strip.sub(/\A@/, "").downcase
        unless wanted.empty?
          found = cleaned.find { |d| d == wanted }
          return found if found

          raise "getnada: domain not available: #{wanted}"
        end

        default = cleaned.find { |d| d == "getnada.net" }
        return default if default
        return cleaned.first unless cleaned.empty?

        raise "getnada: no domain available"
      end

      # 将原始邮件对象展平为 normalize 可识别的字段
      def flatten_message(raw, recipient)
        out = raw.dup
        out["id"] = raw["id"]
        out["from"] = first_non_empty(raw["from_addr"], raw["from"])
        out["to"] = first_non_empty(raw["to"]).empty? ? recipient : raw["to"]
        out["text"] = first_non_empty(raw["text_plain"], raw["text"])
        out["html"] = first_non_empty(raw["html_sanitized"], raw["html"])
        out["read"] = !raw["read_at"].to_s.empty? if raw.key?("read_at")
        out
      end

      def first_non_empty(*values)
        values.each do |v|
          next if v.nil?

          t = v.to_s
          return t unless t.empty?
        end
        ""
      end

      # 生成临时邮箱：选域名 -> 开信箱 -> 取 token/recipient
      # @param domain [String, nil] 指定域名（多渠道别名派生用）
      # @param channel [String] 渠道标识
      # @return [EmailInfo]
      def generate_email(domain = nil, channel = CHANNEL)
        selected_domain = pick_domain(domain)
        requested = "#{random_local}@#{selected_domain}"
        resp = Http.post("#{API_BASE}/inbox/open", headers: HEADERS_POST,
                                                   json: { "email" => requested }, timeout: 15)
        resp.raise_for_status
        data = resp.json
        raise "getnada: invalid open response" unless data.is_a?(Hash)

        token = data["token"].to_s.strip
        email = (data["recipient"] || requested).to_s.strip
        raise "getnada: invalid open response" if token.empty? || email.empty? || !email.include?("@")

        EmailInfo.new(channel: channel, email: email, token: token, expires_at: data["activeUntil"])
      end

      # 拉取单封邮件详情，失败返回 nil
      def fetch_detail(token, message_id)
        url = "#{API_BASE}/inbox/message?id=#{URI.encode_www_form_component(message_id)}" \
              "&token=#{URI.encode_www_form_component(token)}"
        resp = Http.get(url, headers: HEADERS_JSON, timeout: 15)
        return nil unless resp.ok?

        data = resp.json
        return data["message"] if data.is_a?(Hash) && data["message"].is_a?(Hash)

        nil
      end

      # 拉取邮件列表并逐封获取详情
      # @param token [String]
      # @param email [String]
      # @return [Array<Email>]
      def get_emails(token, email)
        auth = token.to_s.strip
        address = email.to_s.strip
        raise "getnada: empty token" if auth.empty?
        raise "getnada: empty email" if address.empty?

        resp = Http.get("#{API_BASE}/inbox/messages?token=#{URI.encode_www_form_component(auth)}",
                        headers: HEADERS_JSON, timeout: 15)
        resp.raise_for_status
        data = resp.json
        rows = data.is_a?(Hash) ? data["messages"] : []
        return [] unless rows.is_a?(Array)

        rows.filter_map do |item|
          next unless item.is_a?(Hash)

          message_id = item["id"].to_s.strip
          detail = message_id.empty? ? nil : fetch_detail(auth, message_id)
          Normalize.normalize_email(flatten_message(detail || item, address), address)
        end
      end
    end
  end
end
