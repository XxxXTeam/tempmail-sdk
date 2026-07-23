# frozen_string_literal: true

require "json"

module TempmailSdk
  module Providers
    # linshiyouxiang.net 渠道实现
    #
    # 流程：
    #   1. GET / 获取 temp_mail cookie，从 HTML 正则提取 tempMailGlobal（邮箱）和 mailCodeGlobal（校验 code）
    #   2. POST /get-messages {"email":"...","code":"..."} 获取邮件列表
    # token 存储 mailCodeGlobal 的值（HMAC 哈希，后续请求用于校验）
    module LinshiyouxiangNet
      CHANNEL = "linshiyouxiang-net"
      BASE_URL = "https://www.linshiyouxiang.net"

      EMAIL_RE = /tempMailGlobal\s*=\s*'([^']+)'/.freeze
      CODE_RE  = /mailCodeGlobal\s*=\s*'([^']+)'/.freeze

      BROWSER_HEADERS = {
        "Accept" => "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
        "Accept-Language" => "zh-CN,zh;q=0.9,en;q=0.8",
        "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " \
                       "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36"
      }.freeze

      AJAX_HEADERS = {
        "Accept" => "application/json, text/javascript, */*; q=0.01",
        "Accept-Language" => "zh-CN,zh;q=0.9,en;q=0.8",
        "Content-Type" => "application/json",
        "Origin" => BASE_URL,
        "Referer" => "#{BASE_URL}/",
        "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " \
                       "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36",
        "X-Requested-With" => "XMLHttpRequest"
      }.freeze

      module_function

      # 创建临时邮箱
      # @return [EmailInfo]
      def generate_email
        resp = Http.get("#{BASE_URL}/", headers: BROWSER_HEADERS)
        resp.raise_for_status
        html = resp.body

        em = html.match(EMAIL_RE)
        raise "linshiyouxiang-net: 未能从首页提取邮箱地址" unless em

        email = em[1].strip
        raise "linshiyouxiang-net: 提取的邮箱地址为空" if email.empty?

        code = ""
        if (cm = html.match(CODE_RE))
          code = cm[1].strip
        end

        EmailInfo.new(channel: CHANNEL, email: email, token: code)
      end

      # 获取邮件列表
      # POST /get-messages {"email":"...","code":"<token>"}
      # @param email [String]
      # @param token [String] mailCodeGlobal 值
      # @return [Array<Email>]
      def get_emails(email, token)
        raise "linshiyouxiang-net: 邮箱地址为空" if email.to_s.strip.empty?

        resp = Http.post("#{BASE_URL}/get-messages",
                         headers: AJAX_HEADERS,
                         json: { "email" => email, "code" => token.to_s })
        resp.raise_for_status
        data = resp.json
        emails_raw = data["emails"]
        return [] unless emails_raw.is_a?(Array) && !emails_raw.empty?

        emails_raw.filter_map do |raw|
          next unless raw.is_a?(Hash)

          Normalize.normalize_email(raw, email)
        end
      end
    end
  end
end
