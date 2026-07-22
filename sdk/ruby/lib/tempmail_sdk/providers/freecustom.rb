# frozen_string_literal: true

module TempmailSdk
  module Providers
    # freecustom.email 渠道实现
    # 免注册临时邮箱，任意 local part @ 可用域名即时可收信
    module Freecustom
      CHANNEL = "freecustom"
      SITE_URL = "https://www.freecustom.email"
      DOMAINS_URL = "https://api2.freecustom.email/domains"
      REFERER = "https://www.freecustom.email/en"
      DEFAULT_UA = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " \
                   "(KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36"

      module_function

      def get_ua
        cfg = Config.get_config
        (cfg.headers || {})["User-Agent"] || DEFAULT_UA
      end

      def random_local(n = 10)
        chars = ("a".."z").to_a + ("0".."9").to_a
        Array.new(n) { chars.sample }.join
      end

      def first_val(msg, *keys)
        keys.each do |key|
          val = msg[key]
          return val.to_s if !val.nil? && val != ""
        end
        ""
      end

      # 挑选一个可收信域名
      def pick_domain
        resp = Http.get(
          DOMAINS_URL,
          headers: { "User-Agent" => get_ua, "Accept" => "application/json", "Referer" => REFERER }
        )
        resp.raise_for_status
        data = resp.json
        lst = data.is_a?(Hash) ? data["data"] : nil
        raise "freecustom: domain list empty" unless lst.is_a?(Array) && !lst.empty?

        usable = lst.select do |d|
          d.is_a?(Hash) && d["tier"] == "free" && d["expiring_soon"] != true && !d["domain"].to_s.empty?
        end
        pool = usable.empty? ? lst.select { |d| d.is_a?(Hash) && !d["domain"].to_s.empty? } : usable
        raise "freecustom: no usable domain" if pool.empty?

        pool.sample["domain"].to_s
      end

      # 获取匿名访问令牌
      def fetch_auth_token
        resp = Http.post(
          "#{SITE_URL}/api/auth",
          headers: {
            "User-Agent" => get_ua, "Accept" => "application/json",
            "Content-Type" => "application/json", "Referer" => REFERER
          }
        )
        resp.raise_for_status
        data = resp.json
        raise "freecustom: invalid token response" unless data.is_a?(Hash) && !data["token"].to_s.empty?

        data["token"].to_s
      end

      # @return [EmailInfo]
      def generate_email
        domain = pick_domain
        email = "#{random_local}@#{domain}"
        EmailInfo.new(channel: CHANNEL, email: email, token: email)
      end

      # @param email [String]
      # @return [Array<Email>]
      def get_emails(email)
        addr = email.to_s.strip
        raise "freecustom: missing email" if addr.empty?

        jwt = fetch_auth_token
        auth_headers = {
          "User-Agent" => get_ua, "Accept" => "application/json",
          "Referer" => REFERER, "Authorization" => "Bearer #{jwt}",
          "x-fce-client" => "web-client"
        }

        list_resp = Http.get(
          "#{SITE_URL}/api/public-mailbox?fullMailboxId=#{URI.encode_www_form_component(addr)}",
          headers: auth_headers
        )
        list_resp.raise_for_status
        list_data = list_resp.json
        return [] unless list_data.is_a?(Hash) && list_data["success"]

        items = list_data["data"]
        return [] unless items.is_a?(Array)

        items.select { |i| i.is_a?(Hash) && i["id"] }.map do |item|
          msg_id = item["id"].to_s
          full = item
          begin
            msg_resp = Http.get(
              "#{SITE_URL}/api/public-mailbox?fullMailboxId=#{URI.encode_www_form_component(addr)}" \
              "&messageId=#{URI.encode_www_form_component(msg_id)}",
              headers: auth_headers
            )
            if msg_resp.ok?
              msg_data = msg_resp.json
              if msg_data.is_a?(Hash) && msg_data["success"] && msg_data["data"].is_a?(Hash)
                full = msg_data["data"]
              end
            end
          rescue StandardError
            # 正文补全失败时退回列表元数据
          end

          row = {
            "id" => first_val(full, "id"),
            "from" => first_val(full, "from"),
            "to" => first_val(full, "to").empty? ? addr : first_val(full, "to"),
            "subject" => first_val(full, "subject"),
            "text" => first_val(full, "text"),
            "html" => first_val(full, "html"),
            "date" => first_val(full, "date")
          }
          Normalize.normalize_email(row, addr)
        end
      end
    end
  end
end
