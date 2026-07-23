# frozen_string_literal: true

require "uri"

module TempmailSdk
  module Providers
    # mailtemp.cc 渠道实现（PHP POST form-urlencoded API）
    #
    # 流程：POST api.php body: action=inbox 创建邮箱（返回 JSON 字符串用户名）
    #       POST api.php body: action=fetch&inbox={username}&last_id=0 获取邮件列表
    #       POST api.php body: action=view&id={id}&inbox={username} 获取邮件详情
    # token 存储 username（@前面的部分），域名固定 neocea.com
    module MailtempCc
      CHANNEL = "mailtemp-cc"
      API_URL = "https://mailtemp.cc/api.php"

      HEADERS = {
        "Content-Type" => "application/x-www-form-urlencoded",
        "Accept" => "application/json, text/plain, */*",
        "Accept-Language" => "zh-CN,zh;q=0.9,en;q=0.8",
        "Referer" => "https://mailtemp.cc/",
        "Origin" => "https://mailtemp.cc",
        "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " \
                        "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"
      }.freeze

      module_function

      # 创建 mailtemp.cc 临时邮箱
      # @return [EmailInfo]
      def generate_email
        resp = Http.post(API_URL, headers: HEADERS, body: "action=inbox", timeout: 15)
        resp.raise_for_status
        # 返回值为 JSON 字符串格式（带引号），如 "vindictiverate"
        username = resp.json
        username = username.to_s.strip
        raise "mailtemp-cc: 返回的用户名为空" if username.empty?

        EmailInfo.new(channel: CHANNEL, email: "#{username}@neocea.com", token: username)
      end

      # 获取 mailtemp.cc 邮件列表
      # @param email [String]
      # @param token [String] username
      # @return [Array<Email>]
      def get_emails(email, token)
        token = token.to_s.strip
        raise "mailtemp-cc: token 为空" if token.empty?

        form = URI.encode_www_form("action" => "fetch", "inbox" => token, "last_id" => "0")
        resp = Http.post(API_URL, headers: HEADERS, body: form, timeout: 15)
        resp.raise_for_status
        items = resp.json
        return [] unless items.is_a?(Array)
        return [] if items.empty?

        items.filter_map do |item|
          next unless item.is_a?(Hash)

          mail_id = item["id"].to_s
          unless mail_id.empty?
            detail = view_email(token, mail_id)
            if detail.is_a?(Hash)
              item["html"] = detail["body_html"] if detail["body_html"]
              item["body_html"] = detail["body_html"] if detail["body_html"]
            end
          end

          item["from"] ||= item["sender_email"] || item["sender"]
          item["date"] ||= item["received_at"]
          item["to"] = email
          Normalize.normalize_email(item, email)
        end
      end

      # 获取单封邮件详情
      # @param inbox [String] username
      # @param mail_id [String]
      # @return [Hash, nil]
      def view_email(inbox, mail_id)
        form = URI.encode_www_form("action" => "view", "id" => mail_id, "inbox" => inbox)
        resp = Http.post(API_URL, headers: HEADERS, body: form, timeout: 15)
        return nil unless resp.ok?

        resp.json
      rescue StandardError
        nil
      end
    end
  end
end
