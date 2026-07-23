# frozen_string_literal: true

require "uri"

module TempmailSdk
  module Providers
    # altmails.com 渠道实现
    #
    # 流程：
    #   1. GET / 建立 session，从 HTML inline script 提取 CSRF token（'_token': 'xxx'）
    #   2. GET /random-email-address 获取随机邮箱地址（纯文本响应）
    #   3. 收信：重新建立 session 获取新 CSRF，POST /fetch-emails/{email} 获取邮件列表
    #   4. GET /view/{id} 获取每封邮件正文 HTML
    module Altmails
      CHANNEL = "altmails"
      BASE_URL = "https://tempmail.altmails.com"

      BROWSER_HEADERS = {
        "Accept" => "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8",
        "Accept-Language" => "en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7",
        "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36"
      }.freeze

      CSRF_RE = /'_token'\s*:\s*'([^']+)'/.freeze

      module_function

      # 访问首页提取 CSRF token
      # @return [String]
      def fetch_csrf
        resp = Http.get("#{BASE_URL}/", headers: BROWSER_HEADERS)
        resp.raise_for_status
        m = resp.body.match(CSRF_RE)
        raise "altmails: 未能从首页提取 CSRF token" unless m

        m[1]
      end

      # 创建临时邮箱
      # @return [EmailInfo]
      def generate_email
        csrf = fetch_csrf
        resp = Http.get("#{BASE_URL}/random-email-address",
                        headers: BROWSER_HEADERS.merge("Referer" => "#{BASE_URL}/"))
        resp.raise_for_status
        email = resp.body.strip
        raise "altmails: 邮箱地址无效: #{email.inspect}" if email.empty? || !email.include?("@")

        EmailInfo.new(channel: CHANNEL, email: email, token: csrf)
      end

      # 获取邮件列表
      # @param email [String]
      # @param token [String] CSRF token（收信时重新获取新 session）
      # @return [Array<Email>]
      def get_emails(email, token)
        _ = token
        csrf = fetch_csrf
        fetch_url = "#{BASE_URL}/fetch-emails/#{URI.encode_www_form_component(email)}"
        form = "_token=#{URI.encode_www_form_component(csrf)}"
        resp = Http.post(fetch_url,
                         headers: {
                           "Accept" => "application/json, text/plain, */*",
                           "Accept-Language" => "en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7",
                           "Content-Type" => "application/x-www-form-urlencoded",
                           "Origin" => BASE_URL,
                           "Referer" => "#{BASE_URL}/",
                           "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36",
                           "X-Requested-With" => "XMLHttpRequest"
                         },
                         body: form)
        resp.raise_for_status
        items = begin
          resp.json
        rescue StandardError
          []
        end
        return [] unless items.is_a?(Array)

        items.filter_map do |item|
          next unless item.is_a?(Hash)

          item["to"] = email
          mail_id = item["id"].to_s
          unless mail_id.empty?
            begin
              vr = Http.get("#{BASE_URL}/view/#{URI.encode_www_form_component(mail_id)}",
                            headers: BROWSER_HEADERS.merge("Referer" => "#{BASE_URL}/"))
              item["html_body"] = vr.body if vr.ok?
            rescue StandardError
              # 忽略正文获取失败
            end
          end
          Normalize.normalize_email(item, email)
        end
      end
    end
  end
end
