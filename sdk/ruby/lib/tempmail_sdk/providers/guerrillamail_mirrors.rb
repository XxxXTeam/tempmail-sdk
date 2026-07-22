# frozen_string_literal: true

module TempmailSdk
  module Providers
    # Guerrillamail 镜像渠道实现
    # sharklasers.com / grr.la / guerrillamail.info / spam4.me 等共用同一套 ajax.php API，仅 baseURL 不同
    module GuerrillamailMirrors
      DEFAULT_HEADERS = {
        "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36"
      }.freeze

      # 各镜像渠道的固定 baseURL 表
      BASE_URLS = {
        "sharklasers" => "https://www.sharklasers.com/ajax.php",
        "grr-la" => "https://www.grr.la/ajax.php",
        "guerrillamail-info" => "https://www.guerrillamail.info/ajax.php",
        "spam4me" => "https://www.spam4.me/ajax.php",
        "guerrillamail-com" => "https://guerrillamail.com/ajax.php",
        "sharklasers-com" => "https://sharklasers.com/ajax.php",
        "grr-la-com" => "https://grr.la/ajax.php",
        "guerrillamail-net" => "https://www.guerrillamail.net/ajax.php",
        "guerrillamail-org" => "https://www.guerrillamail.org/ajax.php",
        "guerrillamailblock" => "https://www.guerrillamailblock.com/ajax.php",
        "guerrillamail-com-www" => "https://www.guerrillamail.com/ajax.php"
      }.freeze

      module_function

      # 创建临时邮箱
      # @param channel [String] 渠道标识
      # @return [EmailInfo]
      def generate_email(channel)
        base_url = BASE_URLS.fetch(channel)
        resp = Http.get("#{base_url}?f=get_email_address&lang=en", headers: DEFAULT_HEADERS)
        resp.raise_for_status
        data = resp.json
        if data["email_addr"].to_s.empty? || data["sid_token"].to_s.empty?
          raise "#{channel}: missing email_addr or sid_token"
        end

        expires_at = nil
        expires_at = (data["email_timestamp"] + 3600) * 1000 if data["email_timestamp"]
        EmailInfo.new(channel: channel, email: data["email_addr"], token: data["sid_token"], expires_at: expires_at)
      end

      # 获取邮件列表，对无正文的邮件调用 fetch_email 拉取完整正文
      # @param channel [String] 渠道标识
      # @param email [String]
      # @param token [String]
      # @return [Array<Email>]
      def get_emails(channel, email, token)
        base_url = BASE_URLS.fetch(channel)
        sid = URI.encode_www_form_component(token)
        resp = Http.get("#{base_url}?f=check_email&seq=0&sid_token=#{sid}", headers: DEFAULT_HEADERS)
        resp.raise_for_status
        list = resp.json["list"]
        return [] unless list.is_a?(Array)

        list.map do |item|
          body = item["mail_body"].to_s
          if body.empty? && item["mail_id"]
            begin
              mid = URI.encode_www_form_component(item["mail_id"].to_s)
              dr = Http.get("#{base_url}?f=fetch_email&sid_token=#{sid}&email_id=#{mid}", headers: DEFAULT_HEADERS)
              body = dr.json["mail_body"].to_s if dr.ok?
            rescue StandardError
              # 忽略单封详情失败
            end
          end
          text = body.empty? ? item["mail_excerpt"].to_s : body.gsub(/<[^>]+>/, " ")
          text = text.split.join(" ")
          Normalize.normalize_email({
                                      "id" => item["mail_id"], "from" => item["mail_from"], "to" => email,
                                      "subject" => item["mail_subject"], "text" => text, "html" => body,
                                      "date" => item["mail_date"] || "", "isRead" => item["mail_read"] == 1
                                    }, email)
        end
      end
    end
  end
end
