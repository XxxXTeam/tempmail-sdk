# frozen_string_literal: true

require_relative "lib/tempmail_sdk/version"

Gem::Specification.new do |spec|
  spec.name = "tempmail_sdk"
  spec.version = TempmailSdk::VERSION
  spec.authors = ["tempmail-sdk"]
  spec.summary = "临时邮箱 SDK，聚合 279 个第三方临时邮箱渠道，返回统一标准化格式"
  spec.description = "五端（Go/npm/Rust/Python/C）之外的 Ruby 版临时邮箱 SDK，" \
                     "共享相同的渠道标识与顺序，提供统一的 Email 结构、渠道 fallback、代理/超时/TLS/遥测配置。"
  spec.homepage = "https://github.com/tempmail-sdk/tempmail-sdk"
  spec.license = "GPL-3.0-only"
  spec.required_ruby_version = ">= 3.0"

  spec.files = Dir.glob("lib/**/*.rb") + ["README.md"].select { |f| File.exist?(f) }
  spec.require_paths = ["lib"]

  spec.metadata["rubygems_mfa_required"] = "false"
end
