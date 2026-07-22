<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail;

/**
 * 邮件数据标准化模块
 *
 * 不同渠道的 API 返回字段名各不相同，本类通过多候选字段策略将其统一映射为
 * 标准的 Email 结构，保证 SDK 输出一致性。
 */
final class Normalize
{
    /** @var string[] 正文纯文本字段候选（按优先级），提为常量避免每次调用重复分配数组 */
    private const TEXT_KEYS = ['text', 'text_body', 'preview_text', 'mail_body_text', 'body', 'content', 'body_text', 'text_content', 'description'];

    /** @var string[] 正文 HTML 字段候选 */
    private const HTML_KEYS = ['html', 'html_body', 'html_content', 'body_html', 'mail_body_html'];

    /** @var string[] 邮件 ID 字段候选 */
    private const ID_KEYS = ['id', 'eid', '_id', 'mailboxId', 'messageId', 'mail_id'];

    /** @var string[] 发件人字段候选 */
    private const FROM_KEYS = ['from_addr', 'from_address', 'fromAddress', 'mail_sender', 'sender', 'address_from', 'from_email', 'from', 'messageFrom'];

    /** @var string[] 收件人字段候选 */
    private const TO_KEYS = ['to', 'to_address', 'toAddress', 'name_to', 'email_address', 'address'];

    /** @var string[] 主题字段候选 */
    private const SUBJECT_KEYS = ['subject', 'e_subject', 'mail_title'];

    /**
     * 将各提供商返回的原始邮件数据标准化为统一的 Email 格式。
     *
     * @param array<mixed> $raw            原始邮件数据
     * @param string       $recipientEmail 收件人地址（无匹配字段时回退）
     */
    public static function email(array $raw, string $recipientEmail = ''): Email
    {
        $text = self::pick($raw, self::TEXT_KEYS);
        $html = self::pick($raw, self::HTML_KEYS);

        // 修正 text/html 错配
        if ($text !== '' && $html === '' && self::isHtml($text)) {
            $html = $text;
            $text = '';
        }
        if ($text === '' && $html !== '') {
            $text = self::htmlToText($html);
        }
        if ($text !== '' && $html === '') {
            $html = '<html><body><pre>' . htmlspecialchars($text, ENT_QUOTES) . '</pre></body></html>';
        }

        return new Email(
            id: self::pick($raw, self::ID_KEYS),
            fromAddr: self::pick($raw, self::FROM_KEYS),
            to: self::pick($raw, self::TO_KEYS) ?: $recipientEmail,
            subject: self::pick($raw, self::SUBJECT_KEYS),
            text: $text,
            html: $html,
            date: self::normalizeDate($raw),
            isRead: self::normalizeIsRead($raw),
            attachments: self::normalizeAttachments($raw['attachments'] ?? null),
        );
    }

    /**
     * 从数组中按优先级顺序提取字符串值。
     *
     * @param array<mixed> $raw
     * @param string[]     $keys
     */
    private static function pick(array $raw, array $keys): string
    {
        foreach ($keys as $key) {
            if (array_key_exists($key, $raw) && $raw[$key] !== null) {
                $val = $raw[$key];
                if (is_string($val)) {
                    return $val;
                }
                if (is_scalar($val)) {
                    return (string) $val;
                }
            }
        }
        return '';
    }

    /**
     * 检测内容是否为 HTML。
     */
    private static function isHtml(string $content): bool
    {
        $prefix = strtolower(ltrim(substr($content, 0, 200)));
        if (str_starts_with($prefix, '<!doctype html') || str_starts_with($prefix, '<html') || str_starts_with($prefix, '<body')) {
            return true;
        }
        $trimmed = strtolower(trim($content));
        if (str_contains($trimmed, '<div') && str_contains($trimmed, '</div>')) {
            return true;
        }
        if (str_contains($trimmed, '<table') && str_contains($trimmed, '</table>')) {
            return true;
        }
        if (str_contains($trimmed, '<p') && str_contains($trimmed, '</p>') && str_contains($trimmed, '<')) {
            return true;
        }
        return false;
    }

    /**
     * 将 HTML 粗略转换为纯文本。
     */
    private static function htmlToText(string $html): string
    {
        $s = preg_replace('/<(script|style)[^>]*>.*?<\/\1>/is', '', $html) ?? $html;
        $s = preg_replace('/<br\s*\/?>/i', "\n", $s) ?? $s;
        $s = preg_replace('/<\/(p|div|tr|li|h[1-6])>/i', "\n", $s) ?? $s;
        $s = strip_tags($s);
        $s = html_entity_decode($s, ENT_QUOTES | ENT_HTML5);
        $s = preg_replace('/[ \t]+/', ' ', $s) ?? $s;
        $s = preg_replace('/\n{3,}/', "\n\n", $s) ?? $s;
        return trim($s);
    }

    /**
     * 提取并统一日期格式为 ISO 8601，支持字符串日期与秒/毫秒时间戳。
     *
     * @param array<mixed> $raw
     */
    private static function normalizeDate(array $raw): string
    {
        foreach (['received_at', 'receivedAt', 'created_at', 'createdAt', 'date'] as $key) {
            $val = $raw[$key] ?? null;
            if ($val === null) {
                continue;
            }
            if (is_string($val) && $val !== '') {
                $ts = strtotime($val);
                if ($ts !== false) {
                    return date('c', $ts);
                }
                return $val;
            }
            if ((is_int($val) || is_float($val)) && $val > 0) {
                $seconds = $val > 1e12 ? (int) ($val / 1000) : (int) $val;
                return date('c', $seconds);
            }
        }

        foreach (['timestamp', 'e_date'] as $key) {
            $val = $raw[$key] ?? null;
            if ((is_int($val) || is_float($val)) && $val > 0) {
                if ($key === 'timestamp' && $val < 1e12) {
                    return date('c', (int) $val);
                }
                return date('c', (int) ($val / 1000));
            }
        }

        return '';
    }

    /**
     * 提取已读状态，支持 bool / int(0|1) / str('0'|'1')。
     *
     * @param array<mixed> $raw
     */
    private static function normalizeIsRead(array $raw): bool
    {
        foreach (['seen', 'read', 'isRead'] as $key) {
            $val = $raw[$key] ?? null;
            if (is_bool($val)) {
                return $val;
            }
        }
        foreach (['is_read', 'is_seen'] as $key) {
            $val = $raw[$key] ?? null;
            if ($val === null) {
                continue;
            }
            if (is_bool($val)) {
                return $val;
            }
            if (is_int($val) || is_float($val)) {
                return (int) $val !== 0;
            }
            if (is_string($val)) {
                return $val === '1';
            }
        }
        return false;
    }

    /**
     * 提取并标准化附件列表。
     *
     * @param mixed $attachments
     * @return EmailAttachment[]
     */
    private static function normalizeAttachments(mixed $attachments): array
    {
        if (!is_array($attachments)) {
            return [];
        }
        $result = [];
        foreach ($attachments as $a) {
            if (!is_array($a)) {
                continue;
            }
            $size = $a['size'] ?? $a['filesize'] ?? null;
            $result[] = new EmailAttachment(
                filename: (string) ($a['filename'] ?? $a['name'] ?? ''),
                size: is_numeric($size) ? (int) $size : null,
                contentType: $a['contentType'] ?? $a['content_type'] ?? $a['mimeType'] ?? $a['mime_type'] ?? null,
                url: $a['url'] ?? $a['download_url'] ?? $a['downloadUrl'] ?? null,
            );
        }
        return $result;
    }
}
