# frozen_string_literal: true

require "json"
require "securerandom"

module TempmailSdk
  module Providers
    # maildrop.cc GraphQL API 渠道 — https://maildrop.cc
    # 无认证，公共邮箱
    module MaildropCc
      CHANNEL = "maildrop-cc"
      DOMAIN = "maildrop.cc"
      GRAPHQL_URL = "https://api.maildrop.cc/graphql"

      HEADERS = {
        "Accept" => "application/json",
        "Content-Type" => "application/json",
        "Origin" => "https://maildrop.cc",
        "Referer" => "https://maildrop.cc/",
        "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " \
                        "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"
      }.freeze

      LOCAL_CHARS = ("a".."z").to_a + ("0".."9").to_a

      module_function

      def random_local(n = 10)
        Array.new(n) { LOCAL_CHARS.sample }.join
      end

      def mailbox(email)
        e = (email || "").strip
        idx = e.index("@")
        idx ? e[0...idx] : e
      end

      def do_graphql(query)
        resp = Http.post(GRAPHQL_URL, headers: HEADERS, json: { "query" => query })
        resp.raise_for_status
        resp.json
      end

      def inbox_query(mb)
        mb_s = JSON.generate(mb)
        "query { inbox(mailbox: #{mb_s}) { id headerfrom subject date } }"
      end

      def message_query(mb, mid)
        mb_s = JSON.generate(mb)
        mid_s = JSON.generate(mid)
        "query { message(mailbox: #{mb_s}, id: #{mid_s}) { id headerfrom subject date html } }"
      end

      # 创建 maildrop.cc 临时邮箱
      # @return [EmailInfo]
      def generate_email
        email = "#{random_local(10)}@#{DOMAIN}"
        EmailInfo.new(channel: CHANNEL, email: email, token: email)
      end

      # 获取邮件列表
      # @param email [String]
      # @param token [String] （不使用，公共邮箱无需认证）
      # @return [Array<Email>]
      def get_emails(email, token = "")
        mb = mailbox(email)
        raise "maildrop-cc: 邮箱地址为空" if mb.empty?

        inbox_resp = do_graphql(inbox_query(mb))
        items = ((inbox_resp || {})["data"] || {})["inbox"]
        return [] unless items.is_a?(Array) && !items.empty?

        items.filter_map do |item|
          next unless item.is_a?(Hash)

          mid = (item["id"] || "").to_s

          raw = {
            "id" => mid,
            "from" => item["headerfrom"] || "",
            "subject" => item["subject"] || "",
            "date" => item["date"] || ""
          }

          # 查询单封详情补全 html 正文
          unless mid.empty?
            begin
              msg_resp = do_graphql(message_query(mb, mid))
              msg = ((msg_resp || {})["data"] || {})["message"]
              if msg.is_a?(Hash)
                raw = {
                  "id" => msg["id"] || mid,
                  "from" => msg["headerfrom"] || "",
                  "subject" => msg["subject"] || "",
                  "date" => msg["date"] || "",
                  "html" => msg["html"] || ""
                }
              end
            rescue StandardError
              # 回退使用列表元信息
            end
          end

          Normalize.normalize_email(raw, email)
        end
      end
    end
  end
end
