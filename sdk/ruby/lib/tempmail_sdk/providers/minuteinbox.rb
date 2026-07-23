# frozen_string_literal: true

require "json"
require "uri"

module TempmailSdk
  module Providers
    # minuteinbox.com 渠道实现（PHP session + CSRF token）
    #
    # 流程：GET / 获取 PHPSESSID cookie 和 CSRF token（const CSRF="xxx"）
    #       GET /index/index?csrf_token={csrf} 创建邮箱，返回 {"email":"user@minafter.com"}
    #       GET /index/refresh 获取邮件列表
    #       POST /index/email body: id=X 获取邮件详情
    # token 存储 JSON {"phpsessid":"...","csrf":"..."}
    module Minuteinbox
      CHANNEL = "minuteinbox"
      BASE_URL = "https://www.minuteinbox.com"

      module_function

      # 从 HTML 中提取 CSRF token（格式: const CSRF="xxx"）
      # @param html [String]
      # @return [String]
      def extract_csrf(html)
        marker = 'CSRF="'
        idx = html.index(marker)
        return "" unless idx

        sub = html[(idx + marker.length)..]
        end_idx = sub.index('"')
        end_idx ? sub[0...end_idx] : ""
      end

      # 将 session 序列化为 JSON token
      # @param phpsessid [String]
      # @param csrf [String]
      # @return [String]
      def encode_session(phpsessid, csrf)
        JSON.generate("phpsessid" => phpsessid, "csrf" => csrf)
      end

      # 从 JSON token 反序列化 session
      # @param token [String]
      # @return [Array<String>] [phpsessid, csrf]
      def decode_session(token)
        data = JSON.parse(token)
        phpsessid = data["phpsessid"].to_s.strip
        csrf = data["csrf"].to_s.strip
        raise "minuteinbox: session token 缺少必要字段" if phpsessid.empty? || csrf.empty?

        [phpsessid, csrf]
      rescue JSON::ParserError
        raise "minuteinbox: 无效的 session token"
      end

      # 创建 minuteinbox.com 临时邮箱
      # @return [EmailInfo]
      def generate_email
        hdrs = {
          "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " \
                          "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
          "Accept" => "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8"
        }
        r1 = Http.get(BASE_URL + "/", headers: hdrs, timeout: 15)
        r1.raise_for_status

        csrf = extract_csrf(r1.body)
        raise "minuteinbox: 未能从首页提取 CSRF token" if csrf.empty?

        phpsessid = ""
        r1.set_cookies.each do |line|
          pair = line.split(";", 2).first.to_s.strip
          next unless pair.start_with?("PHPSESSID=")

          phpsessid = pair.split("=", 2).last.to_s.strip
          break
        end
        raise "minuteinbox: 未获取到 PHPSESSID cookie" if phpsessid.empty?

        create_url = "#{BASE_URL}/index/index?csrf_token=#{URI.encode_uri_component(csrf)}"
        r2 = Http.get(
          create_url,
          headers: hdrs.merge(
            "X-Requested-With" => "XMLHttpRequest",
            "Cookie" => "PHPSESSID=#{phpsessid}"
          ),
          timeout: 15
        )
        r2.raise_for_status

        data = r2.json
        email = data.is_a?(Hash) ? data["email"].to_s.strip : ""
        raise "minuteinbox: 获取到的邮箱地址无效" if email.empty? || !email.include?("@")

        EmailInfo.new(channel: CHANNEL, email: email, token: encode_session(phpsessid, csrf))
      end

      # 获取 minuteinbox.com 邮件列表
      # @param token [String] JSON session token
      # @param email [String]
      # @return [Array<Email>]
      def get_emails(token, email)
        phpsessid, _csrf = decode_session(token)
        cookie_hdr = "PHPSESSID=#{phpsessid}"
        hdrs = {
          "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " \
                          "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
          "X-Requested-With" => "XMLHttpRequest",
          "Cookie" => cookie_hdr
        }

        r = Http.get(BASE_URL + "/index/refresh", headers: hdrs, timeout: 15)
        r.raise_for_status

        trimmed = r.body.to_s.strip
        return [] if trimmed == "0" || trimmed.empty? || trimmed == "[]"

        mail_list = r.json
        return [] unless mail_list.is_a?(Array) && !mail_list.empty?

        mail_list.filter_map do |item|
          next unless item.is_a?(Hash)

          mail_id = item["id"].to_s
          detail = fetch_detail(cookie_hdr, mail_id) unless mail_id.empty?

          is_read = item["precteno"] != "new"
          from = detail&.dig("od") || item["od"] || ""
          subject = detail&.dig("predmet") || item["predmet"] || ""
          html_body = detail&.dig("telo") || ""
          to = detail&.dig("komu") || email

          flat = {
            "id" => mail_id,
            "from" => from,
            "to" => to,
            "subject" => subject,
            "html" => html_body,
            "date" => item["kdy"] || "",
            "isRead" => is_read
          }
          Normalize.normalize_email(flat, email)
        end
      end

      # 获取单封邮件详情
      # @param cookie_hdr [String]
      # @param mail_id [String]
      # @return [Hash, nil]
      def fetch_detail(cookie_hdr, mail_id)
        return nil if mail_id.empty?

        hdrs = {
          "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " \
                          "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
          "X-Requested-With" => "XMLHttpRequest",
          "Content-Type" => "application/x-www-form-urlencoded",
          "Cookie" => cookie_hdr
        }
        resp = Http.post(
          BASE_URL + "/index/email",
          headers: hdrs,
          body: "id=#{URI.encode_uri_component(mail_id)}",
          timeout: 15
        )
        return nil unless resp.ok?

        resp.json
      rescue StandardError
        nil
      end
    end
  end
end
