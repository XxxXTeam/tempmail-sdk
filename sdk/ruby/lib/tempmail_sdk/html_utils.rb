# frozen_string_literal: true

module TempmailSdk
  # HTML 文本提取辅助
  module HtmlUtils
    module_function

    # 将 HTML 粗略转换为纯文本：剥离 script/style，去标签，压缩空白
    # @param value [String, nil]
    # @return [String]
    def html_to_text(value)
      return "" if value.nil? || value.empty?

      s = value.gsub(%r{<script\b[^>]*>.*?</script>}mi, " ")
      s = s.gsub(%r{<style\b[^>]*>.*?</style>}mi, " ")
      s = s.gsub(/<[^>]+>/, " ")
      s = unescape_entities(s)
      s.split.join(" ")
    end

    # 转义 HTML 特殊字符
    # @param text [String]
    # @return [String]
    def escape(text)
      text.to_s
          .gsub("&", "&amp;")
          .gsub("<", "&lt;")
          .gsub(">", "&gt;")
          .gsub('"', "&quot;")
    end

    # 反转义常见 HTML 实体
    def unescape_entities(text)
      text.gsub("&amp;", "&")
          .gsub("&lt;", "<")
          .gsub("&gt;", ">")
          .gsub("&quot;", '"')
          .gsub("&#39;", "'")
          .gsub("&nbsp;", " ")
    end
  end
end
