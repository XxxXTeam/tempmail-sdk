# frozen_string_literal: true

require "json"
require "uri"

module TempmailSdk
  module Providers
    # 24mail.chacuo.net 渠道实现
    # POST / 同一接口用于注册和收信，HTTP only
    module TwentyfourmailChacuo
      CHANNEL = "24mail-chacuo"
      BASE_URL = "http://24mail.chacuo.net"
      DOMAINS = %w[chacuo.net 027168.com].freeze

      HEADERS = {
        "Accept" => "application/json, text/javascript, */*; q=0.01",
        "Accept-Language" => "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
        "Content-Type" => "application/x-www-form-urlencoded; charset=UTF-8",
        "Origin" => BASE_URL,
        "Referer" => "#{BASE_URL}/",
        "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " \
                        "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
        "x-requested-with" => "XMLHttpRequest"
      }.freeze

      module_function

      def random_local(length = 10)
        chars = ("a".."z").to_a + ("0".."9").to_a
        Array.new(length) { chars.sample }.join
      end

      # 创建 24mail-chacuo 临时邮箱
      # @return [EmailInfo]
      def generate_email
        name = random_local(10)
        domain = DOMAINS.sample
        body = "data=#{URI.encode_www_form_component("#{name}@#{domain}")}&type=refresh&arg="
        resp = Http.post(BASE_URL + "/", headers: HEADERS, body: body, timeout: 15)
        resp.raise_for_status
        data = JSON.parse(resp.body)
        raise "24mail-chacuo: 创建失败 status=#{data["status"]} info=#{data["info"]}" unless data["status"] == 1

        email = "#{name}@#{domain}"
        EmailInfo.new(channel: CHANNEL, email: email, token: email)
      end

      # 获取 24mail-chacuo 邮件列表
      # @param email [String]
      # @return [Array<Email>]
      def get_emails(email)
        addr = email.to_s.strip
        raise "24mail-chacuo: 邮箱地址为空" if addr.empty?

        at_idx = addr.index("@")
        name = at_idx ? addr[0...at_idx] : addr
        domain = at_idx ? addr[at_idx + 1..] : DOMAINS[0]

        body = "data=#{URI.encode_www_form_component("#{name}@#{domain}")}&type=refresh&arg="
        resp = Http.post(BASE_URL + "/", headers: HEADERS, body: body, timeout: 15)
        resp.raise_for_status
        data = JSON.parse(resp.body)
        return [] unless data["status"] == 1 && data["data"].is_a?(Array) && !data["data"].empty?

        list = data["data"][0]["list"]
        return [] unless list.is_a?(Array)

        list.map do |item|
          flat = {
            "id" => item["MID"],
            "from" => item["FROM"],
            "to" => addr,
            "subject" => item["SUBJECT"],
            "body" => item["CONTENT"],
            "date" => item["TIME"],
            "isRead" => false
          }
          Normalize.normalize_email(flat, addr)
        end
      end
    end
  end
end
