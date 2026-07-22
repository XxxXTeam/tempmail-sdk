# frozen_string_literal: true

require "base64"
require "json"
require "securerandom"
require "time"

module TempmailSdk
  module Providers
    # dropmail-me 渠道 — https://dropmail.me
    # GraphQL 临时邮箱，需从页面提取 data-k 生成认证 token
    module DropmailMe
      CHANNEL = "dropmail-me"
      BASE_URL = "https://dropmail.me"

      HEADERS = {
        "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " \
                        "(KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36",
        "Accept" => "application/json",
        "Content-Type" => "application/json"
      }.freeze

      module_function

      # FNV-1a 变体哈希
      def fnv_hash(s)
        h = 2_166_136_261
        s.each_char do |c|
          h ^= c.ord
          h += (h << 1) + (h << 4) + (h << 7) + (h << 8) + (h << 24)
          h &= 0xFFFFFFFF
        end
        h.to_s(16)
      end

      # 生成 dropmail.me API 认证 token
      def generate_auth_token
        resp = Http.get("#{BASE_URL}/en/", headers: {
          "User-Agent" => HEADERS["User-Agent"],
          "Accept" => "text/html"
        }, timeout: 15)
        resp.raise_for_status

        match = resp.body.match(/<meta\s+name="app-config"\s+data-k="([^"]+)"/)
        raise "dropmail-me: 无法从页面提取 data-k" unless match

        data_k = match[1]
        # 反转 + base64 解码得到 secret
        _secret = Base64.decode64(data_k.reverse)

        # 生成随机部分
        date_str = Time.now.utc.strftime("%Y%m%d")
        random_part = date_str + SecureRandom.alphanumeric(16)

        # 计算哈希
        hash_input = random_part + _secret
        h = fnv_hash(hash_input)

        "website_#{random_part}_#{h}"
      end

      # 创建 dropmail.me 临时邮箱
      # @return [EmailInfo]
      def generate_email
        auth_token = generate_auth_token

        query = "mutation { introduceSession { id expiresAt addresses { address } } }"
        resp = Http.post(
          "#{BASE_URL}/api/graphql/#{auth_token}",
          headers: HEADERS,
          json: { "query" => query },
          timeout: 15
        )
        resp.raise_for_status
        data = resp.json

        session_data = (data["data"] || {})[" introduceSession"] || (data["data"] || {})["introduceSession"]
        raise "dropmail-me: 创建 session 失败" unless session_data

        session_id = session_data["id"] || ""
        addresses = session_data["addresses"] || []
        raise "dropmail-me: 响应中缺少 session ID 或地址" if session_id.empty? || addresses.empty?

        address = addresses[0]["address"] || ""
        raise "dropmail-me: 地址为空" if address.empty?

        # Token 序列化为 JSON
        composite_token = JSON.generate({
          "session_id" => session_id,
          "auth_token" => auth_token
        })

        # 解析过期时间
        expires_at = nil
        expires_str = session_data["expiresAt"]
        if expires_str
          begin
            dt = Time.parse(expires_str.sub("Z", "+00:00"))
            expires_at = (dt.to_f * 1000).to_i
          rescue ArgumentError, TypeError
            # 忽略解析失败
          end
        end

        EmailInfo.new(channel: CHANNEL, email: address, token: composite_token, expires_at: expires_at)
      end

      # 获取邮件列表
      # @param token [String] JSON {"session_id":"...","auth_token":"..."}
      # @param email [String]
      # @return [Array<Email>]
      def get_emails(token, email)
        raise "dropmail-me: token 不能为空" if token.nil? || token.to_s.empty?

        addr = (email || "").strip
        raise "dropmail-me: 邮箱地址不能为空" if addr.empty?

        # 解析复合 token
        begin
          token_data = JSON.parse(token)
        rescue JSON::ParserError
          raise "dropmail-me: token 格式无效"
        end

        session_id = token_data["session_id"] || ""
        auth_token = token_data["auth_token"] || ""
        raise "dropmail-me: token 中缺少 session_id 或 auth_token" if session_id.empty? || auth_token.empty?

        # 查询邮件
        query = "{ session(id:\"#{session_id}\") { mails { id headerFrom headerSubject text html receivedAt } } }"
        resp = Http.post(
          "#{BASE_URL}/api/graphql/#{auth_token}",
          headers: HEADERS,
          json: { "query" => query },
          timeout: 15
        )
        resp.raise_for_status
        data = resp.json

        session_resp = (data["data"] || {})["session"]
        return [] unless session_resp

        mails = session_resp["mails"]
        return [] unless mails.is_a?(Array) && !mails.empty?

        mails.filter_map do |msg|
          next unless msg.is_a?(Hash)

          raw = {
            "id" => msg["id"] || "",
            "from" => msg["headerFrom"] || "",
            "to" => addr,
            "subject" => msg["headerSubject"] || "",
            "text" => msg["text"] || "",
            "html" => msg["html"] || "",
            "date" => msg["receivedAt"] || ""
          }
          Normalize.normalize_email(raw, addr)
        end
      end
    end
  end
end
