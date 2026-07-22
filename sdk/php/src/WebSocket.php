<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail;

/**
 * 最小化 WebSocket 客户端（同步阻塞）
 *
 * 仅用于 Socket.IO 渠道的短时连接（generate / 阻塞式 getEmails）。
 * 支持 wss:// (TLS) 和 ws:// 两种协议。
 */
final class WebSocket
{
    /** @var resource|null */
    private $stream = null;

    private bool $connected = false;

    /**
     * 连接到 WebSocket 服务端并完成握手。
     *
     * @param string $url 完整 WebSocket URL（wss://host/path?query）
     * @param array<string,string> $headers 附加请求头
     * @param int $timeout 连接与读写超时（秒）
     */
    public function connect(string $url, array $headers = [], int $timeout = 15): void
    {
        $parsed = parse_url($url);
        if ($parsed === false || empty($parsed['host'])) {
            throw new \RuntimeException('WebSocket: invalid URL');
        }

        $scheme = $parsed['scheme'] ?? 'wss';
        $host = $parsed['host'];
        $port = $parsed['port'] ?? ($scheme === 'wss' ? 443 : 80);
        $path = ($parsed['path'] ?? '/') . (isset($parsed['query']) ? '?' . $parsed['query'] : '');

        $transport = ($scheme === 'wss') ? 'ssl' : 'tcp';
        $address = $transport . '://' . $host . ':' . $port;

        $ctx = stream_context_create([
            'ssl' => [
                'verify_peer' => !ConfigStore::get()->insecure,
                'verify_peer_name' => !ConfigStore::get()->insecure,
            ],
        ]);

        $errno = 0;
        $errstr = '';
        $this->stream = @stream_socket_client($address, $errno, $errstr, $timeout, STREAM_CLIENT_CONNECT, $ctx);
        if ($this->stream === false) {
            throw new \RuntimeException("WebSocket: connect failed: $errstr ($errno)");
        }
        stream_set_timeout($this->stream, $timeout);

        // WebSocket 升级握手
        $key = base64_encode(random_bytes(16));
        $headerLines = [
            "GET $path HTTP/1.1",
            "Host: $host",
            "Upgrade: websocket",
            "Connection: Upgrade",
            "Sec-WebSocket-Key: $key",
            "Sec-WebSocket-Version: 13",
            "Origin: https://$host",
        ];
        foreach ($headers as $k => $v) {
            $headerLines[] = "$k: $v";
        }
        $headerLines[] = '';
        $headerLines[] = '';

        fwrite($this->stream, implode("\r\n", $headerLines));

        // 读取握手响应
        $response = '';
        while (!str_contains($response, "\r\n\r\n")) {
            $chunk = fread($this->stream, 4096);
            if ($chunk === false || $chunk === '') {
                throw new \RuntimeException('WebSocket: handshake read failed');
            }
            $response .= $chunk;
        }

        if (!str_contains($response, '101')) {
            throw new \RuntimeException('WebSocket: upgrade rejected: ' . substr($response, 0, 200));
        }

        $this->connected = true;
    }

    /**
     * 发送文本帧。
     */
    public function send(string $data): void
    {
        if (!$this->connected || $this->stream === null) {
            throw new \RuntimeException('WebSocket: not connected');
        }
        $frame = $this->encodeFrame($data, 0x81); // 0x81 = final text frame
        fwrite($this->stream, $frame);
    }

    /**
     * 接收一条文本消息（阻塞）。
     * 超时返回 null。自动处理 ping/pong 和 close 帧。
     */
    public function receive(?int $timeoutMs = null): ?string
    {
        if (!$this->connected || $this->stream === null) {
            return null;
        }

        if ($timeoutMs !== null) {
            $sec = intdiv($timeoutMs, 1000);
            $usec = ($timeoutMs % 1000) * 1000;
            stream_set_timeout($this->stream, $sec, $usec);
        }

        while (true) {
            $header = $this->readExact(2);
            if ($header === null) {
                return null; // 超时或 EOF
            }

            $byte1 = ord($header[0]);
            $byte2 = ord($header[1]);
            $opcode = $byte1 & 0x0F;
            $masked = ($byte2 & 0x80) !== 0;
            $payloadLen = $byte2 & 0x7F;

            if ($payloadLen === 126) {
                $ext = $this->readExact(2);
                if ($ext === null) {
                    return null;
                }
                $payloadLen = unpack('n', $ext)[1];
            } elseif ($payloadLen === 127) {
                $ext = $this->readExact(8);
                if ($ext === null) {
                    return null;
                }
                $payloadLen = unpack('J', $ext)[1];
            }

            $maskKey = null;
            if ($masked) {
                $maskKey = $this->readExact(4);
                if ($maskKey === null) {
                    return null;
                }
            }

            $payload = '';
            if ($payloadLen > 0) {
                $payload = $this->readExact($payloadLen);
                if ($payload === null) {
                    return null;
                }
                if ($maskKey !== null) {
                    for ($i = 0; $i < $payloadLen; $i++) {
                        $payload[$i] = chr(ord($payload[$i]) ^ ord($maskKey[$i % 4]));
                    }
                }
            }

            switch ($opcode) {
                case 0x01: // text
                    return $payload;
                case 0x02: // binary
                    return $payload;
                case 0x08: // close
                    $this->close();
                    return null;
                case 0x09: // ping -> pong
                    $this->sendPong($payload);
                    continue 2;
                case 0x0A: // pong (ignore)
                    continue 2;
                default:
                    continue 2;
            }
        }
    }

    /**
     * 关闭连接。
     */
    public function close(): void
    {
        if ($this->stream !== null) {
            @fclose($this->stream);
            $this->stream = null;
        }
        $this->connected = false;
    }

    public function isConnected(): bool
    {
        return $this->connected;
    }

    private function sendPong(string $payload): void
    {
        if ($this->stream !== null) {
            fwrite($this->stream, $this->encodeFrame($payload, 0x8A)); // final pong
        }
    }

    /**
     * 编码 WebSocket 帧（客户端必须 mask）。
     */
    private function encodeFrame(string $data, int $firstByte): string
    {
        $len = strlen($data);
        $frame = chr($firstByte);

        if ($len < 126) {
            $frame .= chr($len | 0x80); // mask bit set
        } elseif ($len < 65536) {
            $frame .= chr(126 | 0x80) . pack('n', $len);
        } else {
            $frame .= chr(127 | 0x80) . pack('J', $len);
        }

        // mask key
        $mask = random_bytes(4);
        $frame .= $mask;

        // masked payload
        for ($i = 0; $i < $len; $i++) {
            $frame .= chr(ord($data[$i]) ^ ord($mask[$i % 4]));
        }

        return $frame;
    }

    /**
     * 精确读取指定字节数。超时或 EOF 返回 null。
     */
    private function readExact(int $length): ?string
    {
        $data = '';
        $remaining = $length;
        while ($remaining > 0) {
            $chunk = @fread($this->stream, $remaining);
            if ($chunk === false || $chunk === '') {
                $meta = stream_get_meta_data($this->stream);
                if ($meta['timed_out'] || $meta['eof']) {
                    return null;
                }
                return null;
            }
            $data .= $chunk;
            $remaining -= strlen($chunk);
        }
        return $data;
    }
}
