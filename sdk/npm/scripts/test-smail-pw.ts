/**
 * smail-pw 端到端诊断：生成邮箱 → 轮询收件箱 → 打印原始 RSC 与解析结果
 * 用法: npx ts-node --transpile-only scripts/test-smail-pw.ts
 */
import * as smail from '../src/providers/smail-pw';
import { fetchWithTimeout } from '../src/retry';

const ROOT = 'https://smail.pw/_root.data';
const POLL_MS = 5000;
const MAX_ROUNDS = 24;

async function fetchRawInbox(token: string): Promise<string> {
  const res = await fetchWithTimeout(ROOT, {
    method: 'GET',
    headers: {
      Accept: '*/*',
      referer: 'https://smail.pw/',
      Cookie: token,
    },
  });
  if (!res.ok) throw new Error(`GET ${res.status}`);
  return res.text();
}

async function fetchEmailBody(token: string, id: string): Promise<unknown> {
  const res = await fetchWithTimeout(`https://smail.pw/api/email/${encodeURIComponent(id)}`, {
    headers: { Accept: 'application/json', referer: 'https://smail.pw/', Cookie: token },
  });
  const text = await res.text();
  try {
    return JSON.parse(text);
  } catch {
    return { raw: text.slice(0, 500), status: res.status };
  }
}

async function main(): Promise<void> {
  console.log('=== smail-pw E2E ===\n');
  const mailbox = await smail.generateEmail();
  console.log('邮箱:', mailbox.email);
  console.log('Token:', mailbox.token.slice(0, 60) + '...\n');
  console.log('请向该地址发送一封测试邮件（任意发件方式）。');
  console.log(`探针主题建议: smail-sdk-probe-${Date.now()}\n`);

  for (let i = 1; i <= MAX_ROUNDS; i++) {
    const raw = await fetchRawInbox(mailbox.token);
    const parsed = await smail.getEmails(mailbox.token, mailbox.email);
    const hasEmailsKey = raw.includes('"emails"') && !/"emails",\[\]/.test(raw);
    console.log(
      `[${i}/${MAX_ROUNDS}] raw=${raw.length}B emails_key_nonempty=${hasEmailsKey} parsed=${parsed.length}`,
    );
    if (parsed.length > 0) {
      console.log('\n--- 解析到的邮件 ---');
      for (const e of parsed) {
        console.log(JSON.stringify(e, null, 2));
        const id = e.id;
        if (id && !id.startsWith('__smail_')) {
          const detail = await fetchEmailBody(mailbox.token, id);
          console.log('\n--- /api/email/' + id + ' ---');
          console.log(JSON.stringify(detail, null, 2).slice(0, 800));
        }
      }
      console.log('\n完成。');
      return;
    }
    if (hasEmailsKey) {
      console.log('\n--- 原始响应片段（含 emails 非空）---');
      const idx = raw.indexOf('"emails"');
      console.log(raw.slice(Math.max(0, idx - 80), idx + 400));
    }
    await new Promise((r) => setTimeout(r, POLL_MS));
  }
  console.log('\n超时：未收到邮件或未解析成功。');
  console.log('最后一次 raw 前 600 字符:\n', (await fetchRawInbox(mailbox.token)).slice(0, 600));
}

main().catch((e) => {
  console.error(e);
  process.exit(1);
});
