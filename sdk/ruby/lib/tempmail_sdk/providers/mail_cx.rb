# frozen_string_literal: true

require "securerandom"
require "time"

module TempmailSdk
  module Providers
    # mail.cx 渠道实现
    # 匿名 Web API: https://mail.cx/v1
    module MailCx
      CHANNEL = "mail-cx"
      BASE_URL = "https://mail.cx"
      UA = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36"

      module_function

      def random_string(length)
        chars = ("a".."z").to_a + ("0".."9").to_a
        Array.new(length) { chars.sample }.join
      end

      def client_id
        "tempmail-sdk-#{random_string(16)}"
      end

      def headers(cid)
        {
          "Accept" => "application/json", "Origin" => BASE_URL, "Referer" => "#{BASE_URL}/",
          "User-Agent" => UA, "X-Client-ID" => cid
        }
      end

      def fetch_config(cid)
        resp = Http.get("#{BASE_URL}/v1/config", headers: headers(cid), timeout: 15)
        resp.raise_for_status
        data = resp.json
        data.is_a?(Hash) ? data : {}
      end

      def pick_domain(config, preferred)
        items = config["system_domains"].is_a?(Array) ? config["system_domains"] : []
        domains = items.select { |i| i.is_a?(Hash) && !i["domain"].to_s.strip.empty? }
                       .map { |i| i["domain"].to_s.strip }
        raise "mail-cx: no system domains" if domains.empty?

        pref = preferred.to_s.strip.sub(/\A@/, "").downcase
        unless pref.empty?
          found = domains.find { |d| d.downcase == pref }
          return found if found
        end
        default = items.find { |i| i.is_a?(Hash) && i["default"] && !i["domain"].to_s.strip.empty? }
        default ? default["domain"].to_s.strip : domains.first
      end

      def first_non_empty(*values)
        values.each do |v|
          next if v.nil?

          t = v.to_s.strip
          return t unless t.empty?
        end
        ""
      end

      # @param preferred_domain [String, nil]
      # @return [EmailInfo]
      def generate_email(preferred_domain = nil)
        cid = client_id
        config = fetch_config(cid)
        domain = pick_domain(config, preferred_domain)
        created = Time.now.utc
        expires_at = nil
        ttl = config["ttl_seconds"]
        expires_at = ((created + ttl).to_f * 1000).to_i if ttl.is_a?(Numeric) && ttl.positive?
        EmailInfo.new(channel: CHANNEL, email: "#{random_string(12)}@#{domain}",
                      token: cid, expires_at: expires_at, created_at: created.iso8601)
      end

      def fetch_detail(cid, message_id)
        resp = Http.get("#{BASE_URL}/v1/email/#{URI.encode_www_form_component(message_id)}",
                        headers: headers(cid), timeout: 15)
        return nil unless resp.ok?

        data = resp.json
        data.is_a?(Hash) ? data : nil
      end

      # @param email [String]
      # @param client_id [String]
      # @return [Array<Email>]
      def get_emails(email, client_id)
        resp = Http.get("#{BASE_URL}/v1/inbox/#{URI.encode_www_form_component(email)}",
                        headers: headers(client_id), timeout: 30)
        return [] if resp.status_code == 204

        resp.raise_for_status
        data = resp.json
        rows = data.is_a?(Hash) ? data["emails"] : nil
        return [] unless rows.is_a?(Array)

        rows.map do |row|
          unless row.is_a?(Hash)
            next Normalize.normalize_email({}, email)
          end

          raw = {
            "id" => row["id"], "from" => first_non_empty(row["from_email"], row["from_name"]),
            "to" => email, "subject" => row["subject"] || "", "text" => row["preview_text"] || "",
            "created_at" => row["created_at"], "attachments" => row["attachments"]
          }
          mid = row["id"].to_s.strip
          unless mid.empty?
            begin
              detail = fetch_detail(client_id, mid)
              if detail
                raw = {
                  "id" => detail["id"], "from" => first_non_empty(detail["from_email"], detail["from_name"]),
                  "to" => email, "subject" => detail["subject"] || "",
                  "text" => first_non_empty(detail["text_body"], detail["preview_text"]),
                  "html" => detail["html_body"] || "", "created_at" => detail["created_at"],
                  "attachments" => detail["attachments"]
                }
              end
            rescue StandardError
              # 忽略详情失败
            end
          end
          Normalize.normalize_email(raw, email)
        end
      end
    end
  end
end
