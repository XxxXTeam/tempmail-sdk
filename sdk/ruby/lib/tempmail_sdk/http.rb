# frozen_string_literal: true

require "net/http"
require "uri"
require "json"
require "securerandom"

module TempmailSdk
  # 共享 HTTP 客户端
  # 所有 provider 通过此模块发起 HTTP 请求，自动应用全局配置（代理、超时、SSL 等）
  # 基于标准库 Net::HTTP 实现，无第三方依赖
  module Http
    # HTTP 响应的轻量封装，统一暴露状态码/正文/JSON/cookie 等
    class Response
      # @return [Integer] HTTP 状态码
      attr_reader :status_code
      # @return [String] 响应正文
      attr_reader :body
      # @return [Net::HTTPResponse] 原始响应对象
      attr_reader :raw

      def initialize(raw)
        @raw = raw
        @status_code = raw.code.to_i
        @body = raw.body || ""
      end

      # @return [Boolean] 状态码是否 2xx
      def ok?
        @status_code >= 200 && @status_code < 300
      end

      # 状态码非 2xx 时抛出异常（错误消息含状态码，便于重试判定）
      def raise_for_status
        return self if ok?

        raise "HTTP request failed: #{@status_code}"
      end

      # 解析 JSON 正文
      # @return [Object]
      def json
        JSON.parse(@body)
      end

      # 获取单个响应头（大小写不敏感）
      # @param name [String]
      # @return [String, nil]
      def header(name)
        @raw[name]
      end

      # 获取全部 Set-Cookie 头
      # @return [Array<String>]
      def set_cookies
        @raw.get_fields("set-cookie") || []
      end

      # 从 Set-Cookie 中提取指定名称的 cookie 值
      # @param name [String]
      # @return [String, nil]
      def cookie_value(name)
        set_cookies.each do |line|
          line.split(";").first.to_s.strip.split("=", 2).then do |k, v|
            return v if k == name
          end
        end
        nil
      end
    end

    # 复用持久连接时，陈旧连接被对端关闭会抛出的错误类型。
    # 原实现每次新建连接，故对这些错误重建连接重试一次，语义等价且幂等。
    CONN_RESET_ERRORS = [EOFError, Errno::ECONNRESET, Errno::EPIPE,
                         Errno::ECONNABORTED, IOError].freeze

    # 持久连接池：key => { http: Net::HTTP|nil, mutex: Mutex }
    # 按 host/port/scheme/proxy/insecure 归类，复用 keep-alive 连接，避免每次请求重建 TCP+TLS。
    @pool = {}
    @pool_mutex = Mutex.new
    @pool_config_version = -1

    module_function

    # 生成随机 IPv4 地址（每段 1-254），用于伪造来源 IP 请求头
    def random_ipv4
      Array.new(4) { rand(1..254) }.join(".")
    end

    # 生成一组伪造来源 IP 的请求头，所有头使用同一个随机 IP
    def spoof_ip_headers
      ip = random_ipv4
      {
        "X-Forwarded-For" => ip,
        "X-Real-IP" => ip,
        "X-Originating-IP" => ip
      }
    end

    # 将伪造来源 IP 请求头合并到用户指定的 headers 中（每次请求生成新的随机 IP）
    def merge_spoof_headers(headers)
      merged = spoof_ip_headers
      merged.merge!(headers) if headers
      merged
    end

    # 带全局配置的 GET 请求（自动注入伪造来源 IP 请求头）
    # @param url [String]
    # @param headers [Hash, nil]
    # @param timeout [Integer, nil]
    # @return [Response]
    def get(url, headers: nil, timeout: nil)
      request(:get, url, headers: headers, timeout: timeout)
    end

    # 带全局配置的 POST 请求
    # @param json [Object, nil] 作为 JSON 序列化的请求体
    # @param body [String, nil] 原始请求体
    def post(url, headers: nil, timeout: nil, json: nil, body: nil)
      request(:post, url, headers: headers, timeout: timeout, json: json, body: body)
    end

    # 通用请求执行（复用持久连接池）
    def request(method, url, headers: nil, timeout: nil, json: nil, body: nil)
      config = Config.get_config
      uri = URI.parse(url)
      effective_timeout = timeout || config.timeout || 15
      req = build_request(method, uri, config, headers, json, body)

      entry = acquire_entry(uri, config)
      # 同一连接同一时刻仅允许一个请求，保证 socket 读写不交错
      entry[:mutex].synchronize do
        perform_with_reuse(entry, uri, config, effective_timeout, req)
      end
    end

    # 获取（或初始化）目标主机的池条目；配置变更时整体重建连接池
    def acquire_entry(uri, config)
      key = pool_key(uri)
      @pool_mutex.synchronize do
        version = Config.config_version
        if version != @pool_config_version
          @pool.each_value { |e| safe_finish(e[:http]) }
          @pool.clear
          @pool_config_version = version
        end
        @pool[key] ||= { http: nil, mutex: Mutex.new }
      end
    end

    # 使用/建立持久连接执行请求；陈旧连接被对端关闭时重建一次
    def perform_with_reuse(entry, uri, config, timeout, req)
      http = entry[:http]
      if http.nil? || !http.started?
        http = start_http(uri, config, timeout)
        entry[:http] = http
      else
        http.open_timeout = timeout
        http.read_timeout = timeout
      end

      begin
        Response.new(http.request(req))
      rescue *CONN_RESET_ERRORS
        safe_finish(http)
        http = start_http(uri, config, timeout)
        entry[:http] = http
        Response.new(http.request(req))
      end
    end

    # 池键：区分协议/主机/端口即可（配置变更整体清池）
    def pool_key(uri)
      "#{uri.scheme}://#{uri.host}:#{uri.port}"
    end

    # 建立并启动持久连接（启用 keep-alive）
    def start_http(uri, config, timeout)
      http = build_http(uri, config, timeout)
      http.keep_alive_timeout = 30
      http.start
      http
    end

    # 安全关闭连接，忽略关闭期异常
    def safe_finish(http)
      http&.finish if http&.started?
    rescue StandardError
      nil
    end

    # 构造 Net::HTTP 实例（应用代理、TLS、超时）
    def build_http(uri, config, timeout)
      http = if config.proxy && !config.proxy.empty?
               p = URI.parse(config.proxy)
               Net::HTTP.new(uri.host, uri.port, p.host, p.port, p.user, p.password)
             else
               Net::HTTP.new(uri.host, uri.port)
             end
      http.use_ssl = (uri.scheme == "https")
      http.verify_mode = OpenSSL::SSL::VERIFY_NONE if config.insecure && http.use_ssl?
      http.open_timeout = timeout
      http.read_timeout = timeout
      http
    end

    # 构造请求对象并写入请求头/请求体
    def build_request(method, uri, config, headers, json, body)
      klass = method == :post ? Net::HTTP::Post : Net::HTTP::Get
      req = klass.new(uri.request_uri)

      merged = merge_spoof_headers(headers)
      config.headers&.each { |k, v| req[k] = v }
      merged.each { |k, v| req[k] = v }

      if json
        req.body = JSON.generate(json)
        req["Content-Type"] ||= "application/json"
      elsif body
        req.body = body
      end
      req
    end
  end
end
