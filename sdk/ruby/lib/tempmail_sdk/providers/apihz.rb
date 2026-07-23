# frozen_string_literal: true

require "base64"
require "json"
require "uri"

module TempmailSdk
  module Providers
    # apihz（接口盒子）渠道实现
    #
    # 需 id/key 凭据，邮箱有效期 10 分钟。读信须携带创建时的 pwd，
    # 故 token 用 base64(JSON{mail,pwd}) 编码保存。
    module Apihz
      CHANNEL = "apihz"
      BASE_URL = "https://cn.apihz.cn"
      TOK_PREFIX = "apihz1:"
      # 官方公共账号（共享频次），未配置自有 id/key 时使用
      PUBLIC_ID = "88888888"
      PUBLIC_KEY = "88888888"
      DOMAINS = %w[apimail.email apimail.vip].freeze

      HEADERS = {
        "Accept" => "application/json",
        "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " \
                       "(KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36"
      }.freeze

      module_function

      # 读取 id/key：优先环境变量 APIHZ_ID/APIHZ_KEY，回退官方公共账号
      def credentials
        id = ENV["APIHZ_ID"].to_s.strip
        key = ENV["APIHZ_KEY"].to_s.strip
        id = PUBLIC_ID if id.empty?
        key = PUBLIC_KEY if key.empty?
        [id, key]
      end

      # 生成随机本地部分
      def random_local(length = 10)
        chars = ("a".."z").to_a + ("0".."9").to_a
        Array.new(length) { chars.sample }.join
      end

      # 生成 12 位随机密码
      def random_password
        chars = ("A".."Z").to_a + ("a".."z").to_a + ("0".."9").to_a
        Array.new(12) { chars.sample }.join
      end

      # token 编码：前缀 + base64(JSON{mail,pwd})
      def encode_token(mail, pwd)
        raw = JSON.generate("mail" => mail, "pwd" => pwd)
        TOK_PREFIX + Base64.strict_encode64(raw)
      end

      # token 解码
      def decode_token(token)
        raise "apihz: 无效 token" unless token.to_s.start_with?(TOK_PREFIX)

        raw = Base64.strict_decode64(token[TOK_PREFIX.length..])
        o = JSON.parse(raw)
        mail = o["mail"].to_s.strip
        pwd = o["pwd"].to_s.strip
        raise "apihz: 无效 token" if mail.empty? || pwd.empty?

        [mail, pwd]
      rescue ArgumentError, JSON::ParserError
        raise "apihz: 无效 token"
      end

      # 创建临时邮箱
      # GET /api/mail/mailcache.php?id=&key=&domain=&name=&pwd=&buytype=0
      # @return [EmailInfo]
      def generate_email
        id, key = credentials
        domain = DOMAINS.sample
        name = random_local
        pwd = random_password

        url = "#{BASE_URL}/api/mail/mailcache.php?" \
              "id=#{URI.encode_www_form_component(id)}&key=#{URI.encode_www_form_component(key)}" \
              "&domain=#{URI.encode_www_form_component(domain)}" \
              "&name=#{URI.encode_www_form_component(name)}" \
              "&pwd=#{URI.encode_www_form_component(pwd)}&buytype=0"

        resp = Http.get(url, headers: HEADERS)
        resp.raise_for_status
        data = resp.json
        raise "apihz: 创建邮箱失败 #{data['msg']}" if data["code"] != 200 || data["mail"].to_s.empty?

        final_pwd = data["pwd"].to_s
        final_pwd = pwd if final_pwd.empty?

        EmailInfo.new(channel: CHANNEL, email: data["mail"], token: encode_token(data["mail"], final_pwd))
      end

      # 获取邮件列表
      # GET /api/mail/mailgetlist.php?id=&key=&mail=&pwd=&page=1
      # @param token [String]
      # @return [Array<Email>]
      def get_emails(token)
        mail, pwd = decode_token(token)
        id, key = credentials

        url = "#{BASE_URL}/api/mail/mailgetlist.php?" \
              "id=#{URI.encode_www_form_component(id)}&key=#{URI.encode_www_form_component(key)}" \
              "&mail=#{URI.encode_www_form_component(mail)}" \
              "&pwd=#{URI.encode_www_form_component(pwd)}&page=1"

        resp = Http.get(url, headers: HEADERS)
        resp.raise_for_status
        data = resp.json
        return [] if data["code"] != 200

        list = data["data"]
        return [] unless list.is_a?(Array) && !list.empty?

        list.map do |msg|
          from_addr = msg["frommail"].to_s
          from_addr = msg["fromname"].to_s if from_addr.empty?
          date_val = msg["time2"].to_s
          date_val = msg["time1"].to_s if date_val.empty?
          Normalize.normalize_email({
            "id" => msg["time1"].to_s,
            "from" => from_addr,
            "to" => mail,
            "subject" => msg["subject"].to_s,
            "text" => msg["content"].to_s,
            "html" => msg["content"].to_s,
            "date" => date_val,
            "isRead" => false
          }, mail)
        end
      end
    end
  end
end
