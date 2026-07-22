# frozen_string_literal: true

require "uri"
require "json"
require "securerandom"
require "base64"
require "net/http"

module TempmailSdk
  module Providers
    # mail.zhujump.com 固定域渠道
    # 通过注册账号、登录会话后创建固定域邮箱，再通过邮箱 ID 拉取邮件列表
    module Zhujump
      BASE_URL = "https://mail.zhujump.com"
      TOKEN_PREFIX = "zhj1:"
      DEFAULT_EXPIRY_TIME = 60 * 60 * 1000
      HEADERS = {
        "Accept" => "application/json",
        "Origin" => BASE_URL,
        "Referer" => "#{BASE_URL}/zh-CN/login",
        "User-Agent" => "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36"
      }.freeze

      module_function

      def random_value(prefix, size)
        chars = ("a".."z").to_a + ("0".."9").to_a
        "#{prefix}#{Array.new(size) { chars.sample }.join}"
      end

      def random_password
        "Sdk!#{random_value('', 12)}A1"
      end

      def login_referer(base_url)
        "#{base_url}/zh-CN/login"
      end

      def make_headers(base_url)
        HEADERS.merge("Origin" => base_url, "Referer" => login_referer(base_url))
      end

      # 将会话信息编码为 token（base64 JSON）
      def encode_token(cookie, email_id, base_url)
        raw = JSON.generate({ "c" => cookie, "i" => email_id, "b" => base_url })
        TOKEN_PREFIX + Base64.strict_encode64(raw)
      end

      # 从 token 解码出会话信息
      def decode_token(token)
        raise "zhujump: invalid session token" unless token.to_s.start_with?(TOKEN_PREFIX)

        payload = token[TOKEN_PREFIX.length..]
        data = JSON.parse(Base64.strict_decode64(payload))
        cookie = data["c"].to_s.strip
        email_id = data["i"].to_s.strip
        raise "zhujump: invalid session token" if cookie.empty? || email_id.empty?

        base_url = (data["b"] || BASE_URL).to_s.strip.chomp("/")
        { "cookie" => cookie, "email_id" => email_id, "base_url" => base_url }
      rescue ArgumentError, JSON::ParserError => e
        raise "zhujump: invalid session token (#{e.message})"
      end

      # 使用标准库 Net::HTTP 实现带 cookie jar 的会话请求
      # 此 provider 需要在注册/登录/创建邮箱的多步流程中保持 cookie
      class Session
        attr_reader :cookies

        def initialize(base_url)
          @base_url = base_url
          @cookies = {}
          config = Config.get_config
          @timeout = config.timeout || 15
          @proxy = config.proxy
          @insecure = config.insecure
          @extra_headers = config.headers || {}
        end

        def get(path, extra_headers: {})
          do_request(:get, path, extra_headers: extra_headers)
        end

        def post(path, json: nil, form: nil, extra_headers: {})
          do_request(:post, path, json: json, form: form, extra_headers: extra_headers)
        end

        private

        def do_request(method, path, json: nil, form: nil, extra_headers: {})
          uri = URI.parse("#{@base_url}#{path}")
          http = build_http(uri)
          req = build_req(method, uri, json, form, extra_headers)
          resp = http.request(req)
          # 收集 cookies
          (resp.get_fields("set-cookie") || []).each do |sc|
            kv = sc.split(";").first.to_s.strip.split("=", 2)
            @cookies[kv[0]] = kv[1] if kv.length == 2
          end
          resp
        end

        def build_http(uri)
          http = if @proxy && !@proxy.empty?
                   p = URI.parse(@proxy)
                   Net::HTTP.new(uri.host, uri.port, p.host, p.port, p.user, p.password)
                 else
                   Net::HTTP.new(uri.host, uri.port)
                 end
          http.use_ssl = (uri.scheme == "https")
          http.verify_mode = OpenSSL::SSL::VERIFY_NONE if @insecure && http.use_ssl?
          http.open_timeout = @timeout
          http.read_timeout = @timeout
          http
        end

        def build_req(method, uri, json, form, extra_headers)
          klass = method == :post ? Net::HTTP::Post : Net::HTTP::Get
          req = klass.new(uri.request_uri)
          # 设置 headers
          hdrs = Zhujump.make_headers(@base_url)
          @extra_headers.each { |k, v| req[k] = v }
          hdrs.each { |k, v| req[k] = v }
          extra_headers.each { |k, v| req[k] = v }
          # Cookie
          req["Cookie"] = cookie_header unless @cookies.empty?
          # Body
          if json
            req.body = JSON.generate(json)
            req["Content-Type"] = "application/json"
          elsif form
            req.body = URI.encode_www_form(form)
            req["Content-Type"] = "application/x-www-form-urlencoded"
          end
          req
        end

        def cookie_header
          @cookies.map { |k, v| "#{k}=#{v}" }.join("; ")
        end
      end

      # @param domain [String] 邮箱域名
      # @param channel [String] 渠道标识
      # @return [EmailInfo]
      def generate_email(domain, channel)
        generate_email_for_instance(BASE_URL, domain, channel, DEFAULT_EXPIRY_TIME)
      end

      # @param base_url [String] 实例基础 URL
      # @param domain [String] 邮箱域名
      # @param channel [String] 渠道标识
      # @param expiry_time [Integer, nil] 过期时间（毫秒）
      # @return [EmailInfo]
      def generate_email_for_instance(base_url, domain, channel, expiry_time = DEFAULT_EXPIRY_TIME)
        base_url = base_url.chomp("/")
        sess = Session.new(base_url)
        username = random_value("sdk", 10)
        password = random_password

        # 注册
        reg = sess.post("/api/auth/register",
                        json: { "username" => username, "password" => password, "turnstileToken" => "" })
        raise "zhujump: register failed (#{reg.code})" unless reg.is_a?(Net::HTTPSuccess)

        # 获取 CSRF
        csrf_resp = sess.get("/api/auth/csrf")
        raise "zhujump: csrf request failed" unless csrf_resp.is_a?(Net::HTTPSuccess)

        csrf_data = JSON.parse(csrf_resp.body || "{}")
        csrf = csrf_data["csrfToken"].to_s.strip
        raise "zhujump: csrf token missing" if csrf.empty?

        # 登录
        login = sess.post("/api/auth/callback/credentials?",
                          form: {
                            "username" => username, "password" => password,
                            "turnstileToken" => "", "redirect" => "false",
                            "csrfToken" => csrf, "callbackUrl" => login_referer(base_url)
                          },
                          extra_headers: { "x-auth-return-redirect" => "1" })
        # 允许 3xx 重定向也视为成功
        code = login.code.to_i
        raise "zhujump: login failed (#{code})" unless code >= 200 && code < 400

        # 验证 session
        session_resp = sess.get("/api/auth/session")
        raise "zhujump: session check failed" unless session_resp.is_a?(Net::HTTPSuccess)

        session_json = JSON.parse(session_resp.body || "{}")
        got_user = (session_json.dig("user", "username") || "").strip
        raise "zhujump: login verification failed" unless got_user == username

        # 创建邮箱
        local = random_value("sdk", 10)
        body = { "name" => local, "domain" => domain }
        body["expiryTime"] = expiry_time unless expiry_time.nil?
        created = sess.post("/api/emails/generate", json: body)
        raise "zhujump: generate email failed (#{created.code})" unless created.is_a?(Net::HTTPSuccess)

        created_json = JSON.parse(created.body || "{}")
        email_addr = created_json["email"].to_s.strip
        email_id = created_json["id"].to_s.strip
        raise "zhujump: invalid generate response" if email_addr.empty? || email_id.empty?

        cookie = sess.cookies.map { |k, v| "#{k}=#{v}" }.join("; ")
        EmailInfo.new(channel: channel, email: email_addr,
                      token: encode_token(cookie, email_id, base_url))
      end

      # @param token [String] 编码的会话 token
      # @param email [String] 邮箱地址
      # @return [Array<Email>]
      def get_emails(token, email)
        session = decode_token(token)
        base_url = session["base_url"]
        hdrs = make_headers(base_url).merge("Cookie" => session["cookie"])

        resp = Http.get("#{base_url}/api/emails/#{session['email_id']}", headers: hdrs)
        resp.raise_for_status
        data = resp.json
        rows = data.is_a?(Hash) ? data["messages"] : nil
        return [] unless rows.is_a?(Array)

        rows.filter_map do |item|
          next unless item.is_a?(Hash)

          source = item
          message_id = item["id"].to_s.strip
          if !message_id.empty? && item["content"].to_s.strip.empty? && item["html"].to_s.strip.empty?
            begin
              d = Http.get("#{base_url}/api/emails/#{session['email_id']}/#{message_id}", headers: hdrs)
              if d.ok?
                detail_data = d.json
                msg = detail_data.is_a?(Hash) ? detail_data["message"] : nil
                source = item.merge(msg) if msg.is_a?(Hash)
              end
            rescue StandardError
              # 忽略详情失败
            end
          end
          Normalize.normalize_email({
                                      "id" => source["id"] || "",
                                      "from_address" => source["from_address"] || "",
                                      "to_address" => source["to_address"] || email,
                                      "subject" => source["subject"] || "",
                                      "content" => source["content"] || "",
                                      "html" => source["html"] || "",
                                      "received_at" => source["received_at"] || source["sent_at"],
                                      "isRead" => false
                                    }, email)
        end
      end
    end
  end
end
