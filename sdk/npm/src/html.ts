const ENTITY_RE = /&(#x[0-9a-fA-F]+|#\d+|nbsp|amp|lt|gt|quot|apos|#39);/g;

export function decodeHtmlEntitiesOnce(input: string): string {
  return String(input || '').replace(ENTITY_RE, (entity, body: string) => {
    if (body === 'nbsp') return ' ';
    if (body === 'amp') return '&';
    if (body === 'lt') return '<';
    if (body === 'gt') return '>';
    if (body === 'quot') return '"';
    if (body === 'apos' || body === '#39') return "'";
    try {
      const codePoint = body.startsWith('#x')
        ? parseInt(body.slice(2), 16)
        : parseInt(body.slice(1), 10);
      return Number.isFinite(codePoint) ? String.fromCodePoint(codePoint) : entity;
    } catch {
      return entity;
    }
  });
}

export function htmlToText(input: string): string {
  const source = String(input || '');
  const lower = source.toLowerCase();
  let out = '';
  let i = 0;

  while (i < source.length) {
    if (source[i] !== '<') {
      out += source[i];
      i++;
      continue;
    }

    if (lower.startsWith('<script', i) || lower.startsWith('<style', i)) {
      const tag = lower.startsWith('<script', i) ? 'script' : 'style';
      const endTag = `</${tag}`;
      const end = lower.indexOf(endTag, i + tag.length + 1);
      if (end < 0) break;
      const close = source.indexOf('>', end + endTag.length);
      i = close < 0 ? source.length : close + 1;
      out += ' ';
      continue;
    }

    const close = source.indexOf('>', i + 1);
    if (close < 0) {
      out += source[i];
      i++;
      continue;
    }
    out += ' ';
    i = close + 1;
  }

  return decodeHtmlEntitiesOnce(out).replace(/\s+/g, ' ').trim();
}
