# frozen_string_literal: true

require "uri"

module TempmailSdk
  module Providers
    # mailmomy 渠道实现 -- https://mailmomy.com
    #
    # 免注册、无鉴权、无 CAPTCHA 的纯 GET JSON API 临时邮箱，Token 即邮箱地址本身。
    # 流程:
    #   1. GET /api/domains/active -> JSON 字符串数组，随机选一个域名（失败回退 mailmomy.com）
    #   2. 本地随机 10 位 [a-z0-9] local part，拼接为 <local>@<域名>
    #   3. GET /api/mail/messages?to=<email>&page=1&limit=20 -> {"emails": [...], ...}
    #
    # 16 个域名变体渠道（如 16888888.cyou、sbook.pics 等）复用同一后端，
    # 仅在 generate_email(domain) 传入固定域名即可，收信逻辑完全一致。
    module Mailmomy
      CHANNEL = "mailmomy"
      BASE_URL = "https://mailmomy.com"
      DEFAULT_DOMAIN = "mailmomy.com"
      DEFAULT_UA = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " \
                   "(KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36"

      module_function

      # 获取 User-Agent，优先使用配置中的自定义 UA
      def user_agent
        (Config.get_config.headers || {})["User-Agent"] || DEFAULT_UA
      end

      # 生成 n 位小写字母数字随机邮箱前缀
      def random_local(length = 10)
        chars = ("a".."z").to_a + ("0".."9").to_a
        Array.new(length) { chars.sample }.join
      end

      # 按优先级从消息 Hash 中提取首个非空值并转为字符串
      def first_value(msg, *keys)
        keys.each do |key|
          val = msg[key]
          return val.to_s unless val.nil? || val == ""
        end
        ""
      end

      # 拉取 mailmomy 当前可用域名池并随机选取一个
      # GET /api/domains/active -> JSON 字符串数组；请求或解析失败时回退到默认 mailmomy.com
      def pick_domain
        resp = Http.get("#{BASE_URL}/api/domains/active",
                        headers: { "User-Agent" => user_agent, "Accept" => "application/json" })
        resp.raise_for_status
        data = resp.json
        return DEFAULT_DOMAIN unless data.is_a?(Array)

        domains = data.select { |d| d.is_a?(String) && !d.empty? }
        domains.empty? ? DEFAULT_DOMAIN : domains.sample
      rescue StandardError
        DEFAULT_DOMAIN
      end

      # 创建临时邮箱；服务免注册，直接构造 <local>@<域名> 即可收信
      # @param domain [String, nil] 指定固定域名（域名变体渠道用）；为空则从可用域名池随机选取
      # @return [EmailInfo]
      def generate_email(domain = nil)
        selected = domain.to_s.strip
        selected = pick_domain if selected.empty?
        email = "#{random_local(10)}@#{selected}"
        EmailInfo.new(channel: CHANNEL, email: email, token: email)
      end

      # 获取邮件列表
      # GET /api/mail/messages?to=<email>&page=1&limit=20
      # 返回 {"emails": [{id,recipient,from,subject,message,bodyText,receivedAt}], ...}
      # @param email [String]
      # @return [Array<Email>]
      def get_emails(email)
        addr = email.to_s.strip
        raise "mailmomy: 缺少邮箱地址" if addr.empty?

        url = "#{BASE_URL}/api/mail/messages?to=#{URI.encode_www_form_component(addr)}&page=1&limit=20"
        resp = Http.get(url, headers: { "User-Agent" => user_agent, "Accept" => "application/json" })
        resp.raise_for_status

        data = resp.json
        return [] unless data.is_a?(Hash)

        emails = data["emails"]
        return [] unless emails.is_a?(Array)

        emails.filter_map do |msg|
          next unless msg.is_a?(Hash)

          message = first_value(msg, "message")
          row = {
            "id" => first_value(msg, "id"),
            "from" => first_value(msg, "from"),
            "to" => first_value(msg, "recipient").empty? ? addr : first_value(msg, "recipient"),
            "subject" => first_value(msg, "subject"),
            "text" => first_value(msg, "bodyText").empty? ? message : first_value(msg, "bodyText"),
            "html" => message,
            "date" => first_value(msg, "receivedAt")
          }
          Normalize.normalize_email(row, addr)
        end
      end
    end
  end
end
