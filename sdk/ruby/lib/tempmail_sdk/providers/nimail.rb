# frozen_string_literal: true

module TempmailSdk
  module Providers
    # nimail 渠道实现
    # API: https://www.nimail.cn
    # POST 表单 API，无需认证，token 即邮箱地址本身
    module Nimail
      CHANNEL = "nimail"
      BASE_URL = "https://www.nimail.cn"

      module_function

      # 生成随机邮箱前缀
      def random_local(n = 10)
        chars = ("a".."z").to_a + ("0".."9").to_a
        Array.new(n) { chars.sample }.join
      end

      # 获取 User-Agent
      def user_agent
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36"
      end

      # 按优先级从消息字典中提取首个非空值
      def first(msg, *keys)
        keys.each do |key|
          val = msg[key]
          return val.to_s if !val.nil? && val.to_s != ""
        end
        ""
      end

      # 创建 nimail.cn 临时邮箱
      # @return [EmailInfo]
      def generate_email
        email = "#{random_local(10)}@nimail.cn"

        resp = Http.post(
          "#{BASE_URL}/api/applymail",
          headers: {
            "User-Agent" => user_agent,
            "Content-Type" => "application/x-www-form-urlencoded",
            "Origin" => BASE_URL,
            "Referer" => "#{BASE_URL}/"
          },
          body: "mail=#{URI.encode_www_form_component(email)}"
        )
        resp.raise_for_status

        data = resp.json
        unless data.is_a?(Hash) && data["success"] == "true" && data["user"]
          raise "nimail: 创建邮箱失败 #{data.inspect}"
        end

        user = data["user"].to_s
        EmailInfo.new(channel: CHANNEL, email: user, token: user)
      end

      # 获取邮件列表
      # @param email [String]
      # @return [Array<Email>]
      def get_emails(email)
        addr = email.to_s.strip
        raise "nimail: 缺少邮箱地址" if addr.empty?

        resp = Http.post(
          "#{BASE_URL}/api/getmails",
          headers: {
            "User-Agent" => user_agent,
            "Content-Type" => "application/x-www-form-urlencoded",
            "Origin" => BASE_URL,
            "Referer" => "#{BASE_URL}/"
          },
          body: "mail=#{URI.encode_www_form_component(addr)}&time=0"
        )
        resp.raise_for_status

        data = resp.json
        return [] unless data.is_a?(Hash) && data["success"] == "true"

        mail = data["mail"]
        return [] unless mail.is_a?(Array)

        mail.select { |msg| msg.is_a?(Hash) }.map do |msg|
          row = {
            "id" => first(msg, "id", "time"),
            "from" => first(msg, "from", "sender"),
            "to" => addr,
            "subject" => first(msg, "subject", "title"),
            "text" => first(msg, "text", "content"),
            "html" => first(msg, "html", "content"),
            "date" => first(msg, "date", "time")
          }
          Normalize.normalize_email(row, addr)
        end
      end
    end
  end
end
