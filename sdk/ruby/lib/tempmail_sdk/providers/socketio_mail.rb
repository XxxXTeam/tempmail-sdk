# frozen_string_literal: true

module TempmailSdk
  module Providers
    # socketio-mail 渠道桩实现
    #
    # 该渠道依赖 WebSocket/Socket.IO 协议，Ruby 标准 HTTP 客户端无法实现。
    # 当前注册为桩，调用时抛出 NotImplementedProviderError。
    module SocketioMail
      CHANNEL = "socketio-mail"

      module_function

      # @raise [NotImplementedError]
      def generate_email
        raise NotImplementedProviderError, "channel not implemented yet: #{CHANNEL}"
      end

      # @raise [NotImplementedError]
      def get_emails(_email, _token)
        raise NotImplementedProviderError, "channel not implemented yet: #{CHANNEL}"
      end
    end
  end
end
