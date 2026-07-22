# frozen_string_literal: true

require "json"
require "uri"

module TempmailSdk
  module Providers
    # dropmail.me GraphQL 渠道 — https://dropmail.me
    # 使用 af_ 令牌访问 GraphQL API 创建 session 和查询邮件
    module Dropmail
      CHANNEL = "dropmail"
      TOKEN_GENERATE_URL = "https://dropmail.me/api/token/generate"
      TOKEN_RENEW_URL = "https://dropmail.me/api/token/renew"

      CREATE_SESSION_QUERY = "mutation {introduceSession {id, expiresAt, addresses{id, address}}}"
      GET_MAILS_QUERY = 'query ($id: ID!) { session(id:$id) { mails { id, rawSize, fromAddr, toAddr, receivedAt, text, headerFrom, headerSubject, html } } }'

      TOKEN_HEADERS = {
        "Accept" => "application/json",
        "Content-Type" => "application/json",
        "Origin" => "https://dropmail.me",
        "Referer" => "https://dropmail.me/api/"
      }.freeze

      AUTO_CACHE_SEC = 50 * 60
      RENEW_BEFORE_SEC = 10 * 60

      @lock = Mutex.new
      @cached_af = nil # [token_string, expiry_monotonic]

      module_function

      # 从配置或环境变量中获取显式 token
      def explicit_af_token
        ENV["DROPMAIL_AUTH_TOKEN"].to_s.strip.then do |t|
          return t unless t.empty?
        end
        ENV["DROPMAIL_API_TOKEN"].to_s.strip
      end

      # 从 API 获取新 af_ token
      def fetch_af_token
        resp = Http.post(TOKEN_GENERATE_URL, headers: TOKEN_HEADERS, json: { "type" => "af", "lifetime" => "1h" })
        resp.raise_for_status
        body = resp.json
        tok = (body["token"] || "").to_s.strip
        raise "DropMail token/generate 未返回有效 af_ 令牌" if tok.empty? || !tok.start_with?("af_")

        tok
      end

      # 解析 af_ token（优先显式配置，其次自动获取并缓存）
      def resolve_af_token
        ex = explicit_af_token
        return ex unless ex.empty?

        now = Process.clock_gettime(Process::CLOCK_MONOTONIC)
        @lock.synchronize do
          if @cached_af
            tok, exp = @cached_af
            return tok if now < exp - RENEW_BEFORE_SEC
          end
          @cached_af = nil
          new_tok = fetch_af_token
          @cached_af = [new_tok, Process.clock_gettime(Process::CLOCK_MONOTONIC) + AUTO_CACHE_SEC]
          new_tok
        end
      end

      # 构造 GraphQL URL
      def graphql_url
        af = resolve_af_token
        "https://dropmail.me/api/graphql/#{URI.encode_www_form_component(af)}"
      end

      # 发送 GraphQL 请求
      def graphql_request(query, variables = nil)
        data = { "query" => query }
        data["variables"] = JSON.generate(variables) if variables

        resp = Http.post(
          graphql_url,
          headers: { "Content-Type" => "application/x-www-form-urlencoded" },
          body: URI.encode_www_form(data)
        )
        resp.raise_for_status
        result = resp.json
        if result["errors"]&.any?
          raise "GraphQL error: #{result['errors'].first['message'] || 'Unknown'}"
        end

        result["data"] || {}
      end

      # 创建临时邮箱
      # @return [EmailInfo]
      def generate_email
        data = graphql_request(CREATE_SESSION_QUERY)
        session = data["introduceSession"]
        raise "Failed to generate email" unless session && session["addresses"]&.any?

        EmailInfo.new(
          channel: CHANNEL,
          email: session["addresses"][0]["address"],
          token: session["id"],
          expires_at: session["expiresAt"]
        )
      end

      # 获取邮件列表
      # @param token [String] session ID
      # @param email [String]
      # @return [Array<Email>]
      def get_emails(token, email = "")
        data = graphql_request(GET_MAILS_QUERY, { "id" => token })
        mails = ((data["session"] || {})["mails"]) || []
        mails.map do |m|
          raw = {
            "id" => m["id"] || "",
            "from" => m["fromAddr"] || "",
            "to" => m["toAddr"] || email,
            "subject" => m["headerSubject"] || "",
            "text" => m["text"] || "",
            "html" => m["html"] || "",
            "received_at" => m["receivedAt"] || "",
            "attachments" => []
          }
          Normalize.normalize_email(raw, email)
        end
      end
    end
  end
end
