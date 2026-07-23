# frozen_string_literal: true

require "digest"
require "json"

module TempmailSdk
  module Providers
    # mail.td 渠道实现（基于 SHA-256 Proof-of-Work 的临时邮箱服务）
    #
    # 流程：
    #   1. GET /api/domains 获取可用域名（过滤 pro_only）
    #   2. 求解 PoW: SHA-256(address + timestamp + nonce) 需满足 difficulty 个前导零位
    #   3. POST /api/accounts 携带 PoW 创建账户 → 返回 JWT + ID
    #   4. GET /api/accounts/{id}/messages?page=1 携带 Bearer JWT 获取邮件
    # token 格式: JSON {"jwt":"...","id":"..."}
    module MailTd
      CHANNEL = "mail-td"
      API_BASE = "https://api.mail.td/api"
      RAND_CHARS = ("a".."z").to_a + ("0".."9").to_a

      HEADERS = {
        "Accept" => "application/json",
        "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " \
                       "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36"
      }.freeze

      module_function

      # 生成随机小写字母 + 数字字符串
      def random_string(len)
        Array.new(len) { RAND_CHARS.sample }.join
      end

      # 检查哈希是否满足 difficulty 个前导零位
      def leading_zero_bits?(hash_bytes, difficulty)
        full = difficulty / 8
        remain = difficulty % 8
        (0...full).each { |i| return false if hash_bytes.getbyte(i) != 0 }
        if remain.positive? && full < hash_bytes.bytesize
          mask = (0xFF << (8 - remain)) & 0xFF
          return false if (hash_bytes.getbyte(full) & mask) != 0
        end
        true
      end

      # 求解 Proof-of-Work
      # 目标：SHA-256(address + timestamp + nonce) 前导零位 >= difficulty
      def solve_pow(address, timestamp, difficulty)
        base = "#{address}#{timestamp}"
        nonce = 0
        while nonce < 100_000_000
          hash = Digest::SHA256.digest("#{base}#{nonce}")
          return nonce.to_s if leading_zero_bits?(hash, difficulty)

          nonce += 1
        end
        raise "mail-td: PoW 求解超时"
      end

      # 获取可用域名列表（过滤 pro_only）
      def fetch_domains
        resp = Http.get("#{API_BASE}/domains", headers: HEADERS)
        raise "mail-td: 域名请求 HTTP #{resp.status_code}" unless resp.ok?

        data = resp.json
        domains = data["domains"] || []
        free = domains.select { |d| d["pro_only"] == false && !d["domain"].to_s.empty? }
                      .map { |d| d["domain"] }
        raise "mail-td: 无可用免费域名" if free.empty?

        free
      end

      # 创建临时邮箱
      # @return [EmailInfo]
      def generate_email
        domain = fetch_domains.sample
        username = random_string(10)
        address = "#{username}@#{domain}"
        password = random_string(20)
        auth_key = Digest::SHA256.hexdigest(password)

        addr_lower = address.strip.downcase
        difficulty = 15
        pow_token = ""

        4.times do
          timestamp = Time.now.to_i
          nonce = solve_pow(addr_lower, timestamp, difficulty)
          pow_obj = { "t" => timestamp, "n" => nonce, "d" => difficulty }
          pow_obj["token"] = pow_token unless pow_token.empty?

          resp = Http.post("#{API_BASE}/accounts",
                           headers: HEADERS.merge("Content-Type" => "application/json"),
                           json: { "address" => address, "auth_key" => auth_key, "pow" => pow_obj })

          result = begin
            resp.json
          rescue StandardError
            {}
          end

          if result["status"] == "retry"
            rd = result["required_difficulty"]
            difficulty = rd.is_a?(Numeric) ? rd.to_i : difficulty + 2
            pow_token = result["token"].to_s
            next
          end

          raise "mail-td: 创建账户 HTTP #{resp.status_code}: #{result['error']}" unless resp.ok?

          result_addr = result["address"].to_s
          result_token = result["token"].to_s
          result_id = result["id"].to_s
          raise "mail-td: 响应缺少必要字段" if result_addr.empty? || result_token.empty? || result_id.empty?

          tok = JSON.generate("jwt" => result_token, "id" => result_id)
          return EmailInfo.new(channel: CHANNEL, email: result_addr, token: tok)
        end

        raise "mail-td: PoW 重试次数超限"
      end

      # 获取邮件列表
      # @param token [String] JSON {"jwt":"...","id":"..."}
      # @param email [String]
      # @return [Array<Email>]
      def get_emails(token, email)
        raise "mail-td: token 为空" if token.to_s.empty?

        parsed = JSON.parse(token)
        jwt = parsed["jwt"].to_s
        id = parsed["id"].to_s
        raise "mail-td: token 缺少 jwt 或 id" if jwt.empty? || id.empty?

        resp = Http.get("#{API_BASE}/accounts/#{id}/messages?page=1",
                        headers: HEADERS.merge("Authorization" => "Bearer #{jwt}"))
        raise "mail-td: 邮件请求 HTTP #{resp.status_code}" unless resp.ok?

        data = resp.json
        messages = data["messages"]
        return [] unless messages.is_a?(Array) && !messages.empty?

        messages.map do |msg|
          from_addr = msg.dig("from", "address").to_s
          Normalize.normalize_email({
            "id" => msg["id"].to_s,
            "from" => from_addr,
            "to" => email,
            "subject" => msg["subject"].to_s,
            "text" => msg["text"].to_s,
            "html" => msg["html"].to_s,
            "created_at" => msg["created_at"].to_s
          }, email)
        end
      rescue JSON::ParserError
        raise "mail-td: token 格式无效"
      end
    end
  end
end
