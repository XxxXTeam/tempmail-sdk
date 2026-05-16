/** 用合成 / 真实 RSC payload 验证 smail-pw 解析器 */
import * as smail from '../src/providers/smail-pw';

async function parsePayload(text: string, email: string) {
  const orig = global.fetch;
  (global as any).fetch = async () => ({
    ok: true,
    text: async () => text,
    headers: new Headers(),
  });
  try {
    return await smail.getEmails('__session=x', email);
  } finally {
    global.fetch = orig;
  }
}

const RECIPIENT = 'ecstatic-mclaren-lf2ol1@smail.pw';

/** 来自 E2E 实测：subject 与 time 相邻、无内联 subject 字符串 */
const REAL_WITH_MAIL =
  '[{"_1":2,"_3":4,"_5":6},"root",{"_7":34},"routes/layout",{"_7":29},"home",{"_7":8},"data",{"_9":10,"_11":12,"_13":14,"_15":16},"addresses",[21],"emails",[17],"locale","en","renderedAt",1778969683205,{"_18":19,"_20":21,"_22":23,"_24":25,"_26":21,"_27":28},"id","iNVl7gw65lz8yHEpVbQYn","to_address","' +
  RECIPIENT +
  '","from_name","openel","from_address","openel@foxmail.com","subject","time",1778969599318,{"_13":14,"_30":31,"_32":33},"theme","light","renderedYear",2026,{"_30":31}]';

async function main(): Promise<void> {
  const cases: { name: string; json: string; expect: number }[] = [
    {
      name: 'real-rr7-no-inline-subject',
      json: REAL_WITH_MAIL,
      expect: 1,
    },
    {
      name: 'live-empty',
      json:
        '[{"_1":2},"root","data","addresses",[3],"emails",[],"locale","en","renderedAt",1,"test@smail.pw"]',
      expect: 0,
    },
  ];
  let failed = 0;
  for (const c of cases) {
    const emails = await parsePayload(c.json, RECIPIENT);
    const ok = emails.length === c.expect;
    console.log(
      `${ok ? 'OK' : 'FAIL'} ${c.name}: count=${emails.length} id=${emails[0]?.id ?? '-'} from=${emails[0]?.from ?? '-'}`,
    );
    if (!ok) failed++;
  }
  process.exit(failed > 0 ? 1 : 0);
}

main();
