<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail;

/**
 * Socket.IO 协议辅助层（Engine.IO v3/v4）
 *
 * 在 WebSocket 之上封装 Socket.IO 握手与事件收发。
 * 仅支持短时同步操作（generate / 阻塞式收信）。
 */
final class SocketIo
{
    private WebSocket $ws;
    private bool $handshook = false;

    /** @var int[] 尝试的 Engine.IO 版本 */
    private const EIO_VERSIONS = [4, 3];

    private const USER_AGENT = 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0';

    /**
     * 连接到 Socket.IO 服务端。
     *
     * @param string $host 主机名（如 mjj.cm）
     * @param int $timeout 超时秒数
     */
    public static function connect(string $host, int $timeout = 15): self
    {
        $lastErr = null;
        foreach (self::EIO_VERSIONS as $eio) {
            $url = "wss://{$host}/socket.io/?EIO={$eio}&transport=websocket";
            $headers = [
                'User-Agent' => self::USER_AGENT,
                'Accept-Language' => 'zh-CN,zh;q=0.9,en;q=0.8',
                'Cache-Control' => 'no-cache',
                'DNT' => '1',
                'Pragma' => 'no-cache',
                'Origin' => "https://{$host}",
            ];
            try {
                $ws = new WebSocket();
                $ws->connect($url, $headers, $timeout);

                // 等待 Engine.IO open 包（以 "0" 开头）
                $packet = $ws->receive($timeout * 1000);
                if ($packet === null || !str_starts_with($packet, '0')) {
                    $ws->close();
                    throw new \RuntimeException("unexpected open packet for EIO={$eio}");
                }

                // 发送 Socket.IO connect
                $ws->send('40');

                $instance = new self();
                $instance->ws = $ws;
                $instance->handshook = true;
                return $instance;
            } catch (\Throwable $e) {
                $lastErr = $e;
            }
        }
        throw new \RuntimeException('SocketIO: handshake failed: ' . ($lastErr?->getMessage() ?? 'unknown'));
    }

    /**
     * 发送 Socket.IO 事件。
     *
     * @param string $event 事件名
     * @param mixed $payload 载荷
     */
    public function emit(string $event, mixed $payload): void
    {
        $packet = '42' . json_encode([$event, $payload], JSON_UNESCAPED_UNICODE | JSON_UNESCAPED_SLASHES);
        $this->ws->send($packet);
    }

    /**
     * 接收下一个 Socket.IO 事件（自动处理 ping/pong）。
     * 超时返回 null。
     *
     * @return array{0:string,1:mixed}|null [event, payload] 或 null
     */
    public function receiveEvent(int $timeoutMs = 15000): ?array
    {
        $deadline = microtime(true) + ($timeoutMs / 1000.0);

        while (true) {
            $remaining = (int) (($deadline - microtime(true)) * 1000);
            if ($remaining <= 0) {
                return null;
            }

            $packet = $this->ws->receive($remaining);
            if ($packet === null) {
                return null;
            }

            // Engine.IO ping
            if ($packet === '2') {
                $this->ws->send('3');
                continue;
            }

            // Socket.IO connect ack
            if (str_starts_with($packet, '40')) {
                continue;
            }

            // Socket.IO event
            if (str_starts_with($packet, '42')) {
                $json = substr($packet, 2);
                $decoded = json_decode($json, true);
                if (is_array($decoded) && !empty($decoded) && is_string($decoded[0])) {
                    return [$decoded[0], $decoded[1] ?? null];
                }
                continue;
            }

            // 其他包类型忽略
        }
    }

    /**
     * 等待指定事件名的响应。
     *
     * @return mixed 事件载荷
     */
    public function waitForEvent(string $eventName, int $timeoutMs = 15000): mixed
    {
        $deadline = microtime(true) + ($timeoutMs / 1000.0);
        while (true) {
            $remaining = (int) (($deadline - microtime(true)) * 1000);
            if ($remaining <= 0) {
                throw new \RuntimeException("SocketIO: timeout waiting for event '{$eventName}'");
            }
            $event = $this->receiveEvent($remaining);
            if ($event === null) {
                throw new \RuntimeException("SocketIO: connection closed waiting for '{$eventName}'");
            }
            if ($event[0] === $eventName) {
                return $event[1];
            }
        }
    }

    /**
     * 关闭连接。
     */
    public function close(): void
    {
        $this->ws->close();
        $this->handshook = false;
    }
}
