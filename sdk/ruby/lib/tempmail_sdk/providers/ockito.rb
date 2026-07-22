# frozen_string_literal: true

require "json"

module TempmailSdk
  module Providers
    # ockito 渠道实现
    # API: https://ockito.com/web-api
    # 流程: 获取 gtoken -> 获取邮箱 -> 用 Bearer token 收信
    module Ockito
      CHANNEL = "ockito"
      BASE_URL = "https://ockito.com/web-api"

      DEFAULT_HEADERS = {
        "Accept" => "application/json",
        "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36"
      }.freeze

      module_function

      # 请求并解析 JSON
      def fetch_json(path, method: :get, headers: nil, payload: nil)
        request_headers = DEFAULT_HEADERS.dup
        request_headers.merge!(headers) if headers

        resp = if method == :post
                 Http.post("#{BASE_URL}#{path}", headers: request_headers, json: payload || {})
               else
                 Http.get("#{BASE_URL}#{path}", headers: request_headers)
               end

        text = resp.body || ""
        data = text.empty? ? {} : JSON.parse(text)
        raise "ockito http #{resp.status_code}" unless resp.ok?

        [resp.status_code, data.is_a?(Hash) ? data : {}]
      end

      # 打包 token
      def pack_token(access_token, refresh_token)
        JSON.generate({ "access_token" => access_token, "refresh_token" => refresh_token })
      end

      # 解包 token
      def unpack_token(token)
        value = token.to_s.strip
        raise "ockito: 无效的会话 token" if value.empty? || !value.start_with?("{")

        data = JSON.parse(value)
        raise "ockito: 无效的会话 token" unless data.is_a?(Hash)

        access = data["access_token"].to_s.strip
        refresh = data["refresh_token"].to_s.strip
        raise "ockito: 无效的会话 token" if access.empty? || refresh.empty?

        [access, refresh]
      end

      # 刷新 access token
      def refresh_access_token(refresh_token)
        _, data = fetch_json("/grefresh", method: :post,
                                          headers: { "Authorization" => "Bearer #{refresh_token}", "X-PASSTHROUGH" => "Y" })
        access = data["access_token"].to_s.strip
        raise "ockito: 刷新 token 失败" if access.empty?

        access
      end

      # 带 Bearer 认证的请求（401 时自动刷新重试）
      def fetch_bearer_json(path, access_token, refresh_token)
        _, data = fetch_json(path, headers: { "Authorization" => "Bearer #{access_token}" })
        data
      rescue RuntimeError => e
        raise unless e.message.include?("401")

        refreshed = refresh_access_token(refresh_token)
        _, data = fetch_json(path, headers: { "Authorization" => "Bearer #{refreshed}" })
        data
      end

      # 扁平化收件箱行
      def flatten_inbox_row(raw, recipient)
        {
          "id" => (raw["uid"] || "").to_s,
          "from" => (raw["sender"] || "").to_s,
          "to" => recipient,
          "subject" => (raw["subject"] || "").to_s,
          "text" => (raw["snippet"] || "").to_s,
          "html" => (raw["html"] || "").to_s,
          "date" => (raw["timestamp"] || "").to_s,
          "is_read" => false,
          "attachments" => []
        }
      end

      # 扁平化邮件详情
      def flatten_message(raw, recipient)
        obj = raw["obj"].is_a?(Hash) ? raw["obj"] : raw
        {
          "id" => (raw["uid"] || "").to_s,
          "from" => (obj["sender_email"] || obj["SenderEmail"] || obj["from_"] || obj["From"] || obj["from"] || obj["sender_name"] || obj["SenderName"] || "").to_s,
          "to" => (obj["to"] || obj["To"] || recipient).to_s,
          "subject" => (obj["subject"] || obj["Subject"] || "").to_s,
          "text" => (obj["text"] || "").to_s,
          "html" => (obj["html"] || "").to_s,
          "date" => (obj["date"] || obj["Date"] || "").to_s,
          "is_read" => false,
          "attachments" => []
        }
      end

      # 创建临时邮箱
      # @return [EmailInfo]
      def generate_email
        _, login = fetch_json("/gtoken", method: :post, payload: {})
        access_token = login["access_token"].to_s.strip
        refresh_token = login["refresh_token"].to_s.strip
        raise "ockito: 无效的 token 响应" if access_token.empty? || refresh_token.empty?

        _, email_data = fetch_json("/email", headers: { "Authorization" => "Bearer #{access_token}" })
        email_value = email_data["email"]
        email = if email_value.is_a?(String)
                  email_value.strip
                elsif email_value.is_a?(Hash)
                  (email_value["email"] || "").to_s.strip
                else
                  ""
                end
        raise "ockito: 无效的邮箱响应" if email.empty? || !email.include?("@")

        EmailInfo.new(channel: CHANNEL, email: email, token: pack_token(access_token, refresh_token))
      end

      # 获取邮件列表
      # @param token [String] JSON 编码的 access+refresh token
      # @param email [String]
      # @return [Array<Email>]
      def get_emails(token, email)
        access_token, refresh_token = unpack_token(token)
        address = email.to_s.strip
        raise "ockito: 邮箱地址为空" if address.empty?

        data = fetch_bearer_json("/retrieve/#{URI.encode_www_form_component(address)}/emails", access_token, refresh_token)
        rows = data["inbox"]
        return [] unless rows.is_a?(Array)

        rows.select { |row| row.is_a?(Hash) }.map do |row|
          uid = row["uid"].to_s.strip
          if uid.empty?
            Normalize.normalize_email(flatten_inbox_row(row, address), address)
          else
            begin
              detail = fetch_bearer_json("/retrieve/#{URI.encode_www_form_component(address)}/#{URI.encode_www_form_component(uid)}", access_token, refresh_token)
              Normalize.normalize_email(flatten_message(detail, address), address)
            rescue StandardError
              Normalize.normalize_email(flatten_inbox_row(row, address), address)
            end
          end
        end
      end
    end
  end
end
