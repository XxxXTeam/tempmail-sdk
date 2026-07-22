# frozen_string_literal: true

module TempmailSdk
  module Providers
    # InboxKitten 渠道 — https://inboxkitten.com
    module Inboxkitten
      CHANNEL = "inboxkitten"
      API_BASE = "https://inboxkitten.com/api/v1/mail"
      DOMAIN = "inboxkitten.com"
      HEADERS_JSON = { "Accept" => "application/json" }.freeze
      HEADERS_HTML = { "Accept" => "text/html,*/*" }.freeze

      module_function

      def random_local
        chars = "abcdefghijklmnopqrstuvwxyz0123456789".chars
        "sdk" + Array.new(16) { chars.sample }.join
      end

      # 无需请求服务端，随机本地部分即可
      # @return [EmailInfo]
      def generate_email
        EmailInfo.new(channel: CHANNEL, email: "#{random_local}@#{DOMAIN}")
      end

      def fetch_meta(region, key)
        resp = Http.get(
          "#{API_BASE}/getKey?region=#{URI.encode_www_form_component(region)}&key=#{URI.encode_www_form_component(key)}",
          headers: HEADERS_JSON, timeout: 15
        )
        resp.raise_for_status
        data = resp.json
        data.is_a?(Hash) ? data : {}
      end

      def fetch_html(region, key)
        resp = Http.get(
          "#{API_BASE}/getHtml?region=#{URI.encode_www_form_component(region)}&key=#{URI.encode_www_form_component(key)}",
          headers: HEADERS_HTML, timeout: 15
        )
        resp.raise_for_status
        resp.body
      end

      def detail_raw(row, recipient)
        storage = row["storage"].is_a?(Hash) ? row["storage"] : {}
        message = row["message"].is_a?(Hash) ? row["message"] : {}
        headers = message["headers"].is_a?(Hash) ? message["headers"] : {}
        envelope = row["envelope"].is_a?(Hash) ? row["envelope"] : {}
        region = storage["region"].to_s.strip
        key = storage["key"].to_s.strip
        raw = {
          "id" => key, "messageId" => key,
          "from" => headers["from"] || envelope["sender"] || "",
          "to" => row["recipient"] || envelope["targets"] || recipient,
          "subject" => headers["subject"] || "", "timestamp" => row["timestamp"]
        }
        return raw if region.empty? || key.empty?

        begin
          meta = fetch_meta(region, key)
          html = fetch_html(region, key)
          raw["from"] = meta["name"] || raw["from"]
          raw["to"] = meta["recipients"] || raw["to"]
          raw["subject"] = meta["subject"] || raw["subject"]
          raw["text"] = HtmlUtils.html_to_text(html)
          raw["html"] = html
        rescue StandardError
          # 忽略详情失败
        end
        raw
      end

      # @param email [String]
      # @return [Array<Email>]
      def get_emails(email)
        local = email.to_s.strip.split("@").first.to_s
        raise "inboxkitten: empty email" if local.empty?

        resp = Http.get("#{API_BASE}/list?recipient=#{URI.encode_www_form_component(local)}",
                        headers: HEADERS_JSON, timeout: 15)
        resp.raise_for_status
        rows = resp.json
        return [] unless rows.is_a?(Array)

        rows.select { |r| r.is_a?(Hash) }.map { |item| Normalize.normalize_email(detail_raw(item, email), email) }
      end
    end
  end
end
