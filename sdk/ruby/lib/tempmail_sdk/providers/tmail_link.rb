# frozen_string_literal: true

require "uri"

module TempmailSdk
  module Providers
    # tmail.link 临时邮箱渠道（Django CSRF）
    # GET / 获取首页 HTML，正则提取邮箱地址
    # GET /inbox/{email}/ 获取 csrftoken cookie
    # POST /inbox/{email}/ 获取邮件列表（form: format=json + csrfmiddlewaretoken）
    module TmailLink
      CHANNEL = "tmail-link"
      BASE_URL = "https://tmail.link"

      EMAIL_RE = /([a-zA-Z0-9._%+\-]+@tmail\.link)/

      UA = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"

      module_function

      def browser_headers
        {
          "User-Agent" => UA,
          "Accept" => "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8",
          "Accept-Language" => "zh-CN,zh;q=0.9,en;q=0.8"
        }
      end

      def post_headers(email, token)
        {
          "User-Agent" => UA,
          "Accept" => "application/json, text/javascript, */*; q=0.01",
          "Accept-Language" => "zh-CN,zh;q=0.9,en;q=0.8",
          "X-Requested-With" => "XMLHttpRequest",
          "Content-Type" => "application/x-www-form-urlencoded; charset=UTF-8",
          "Origin" => BASE_URL,
          "Referer" => "#{BASE_URL}/inbox/#{email}/",
          "Cookie" => "csrftoken=#{token}"
        }
      end

      # @return [EmailInfo]
      def generate_email
        resp = Http.get("#{BASE_URL}/", headers: browser_headers)
        resp.raise_for_status

        m = resp.body.match(EMAIL_RE)
        raise "tmail-link: 未能从首页提取邮箱地址" unless m

        email = m[1].strip
        raise "tmail-link: 提取的邮箱地址为空" if email.empty?

        resp2 = Http.get("#{BASE_URL}/inbox/#{email}/", headers: browser_headers)
        resp2.raise_for_status

        token = resp2.cookie_value("csrftoken")
        raise "tmail-link: 未能获取 csrftoken" if token.nil? || token.empty?

        EmailInfo.new(channel: CHANNEL, email: email, token: token)
      end

      # @param email [String]
      # @param token [String]
      # @return [Array<Email>]
      def get_emails(email, token)
        addr = (email || "").strip
        raise "tmail-link: 邮箱地址为空" if addr.empty?

        csrf = (token || "").strip
        raise "tmail-link: csrftoken 为空" if csrf.empty?

        body = "format=json&csrfmiddlewaretoken=#{URI.encode_www_form_component(csrf)}"
        resp = Http.post("#{BASE_URL}/inbox/#{addr}/",
                         headers: post_headers(addr, csrf), body: body)
        resp.raise_for_status

        data = resp.json
        raise "tmail-link: 邮件列表响应格式异常" unless data.is_a?(Hash)

        messages = data["messages"]
        return [] unless messages.is_a?(Array) && !messages.empty?

        messages.filter_map do |item|
          next unless item.is_a?(Hash)

          raw = {
            "id" => (item["key"] || "").to_s,
            "from" => (item["sender"] || "").to_s,
            "to" => addr,
            "subject" => (item["subject"] || "").to_s,
            "date" => (item["date"] || "").to_s
          }
          Normalize.normalize_email(raw, addr)
        end
      end
    end
  end
end
