#!/usr/bin/env python3
"""生成 WebUI 内嵌 HTML 头文件。

从 webui/index.html 读取模板，将末尾 `<!-- PLACEHOLDER_SCRIPT -->`
占位符替换为 webui/app.js.html 的内容，再把结果转义为可被 C 编译器
识别的相邻字符串字面量，写入 src/webui_html.h。
"""
import os

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
INDEX = os.path.join(ROOT, "webui", "index.html")
APPJS = os.path.join(ROOT, "webui", "app.js.html")
OUT = os.path.abspath(os.path.join(HERE, "..", "src", "webui_html.h"))

PLACEHOLDER = "<!-- PLACEHOLDER_SCRIPT -->"


def main():
    with open(INDEX, "r", encoding="utf-8") as f:
        html = f.read()
    with open(APPJS, "r", encoding="utf-8") as f:
        appjs = f.read()
    merged = html.replace(PLACEHOLDER, appjs)

    lines = merged.split("\n")
    out = [
        "/* 本文件由 scripts/gen_webui_html.py 自动生成，请勿手动编辑 */",
        "#ifndef TEMPMAIL_WEBUI_HTML_H",
        "#define TEMPMAIL_WEBUI_HTML_H",
        "",
        "static const char TM_WEBUI_HTML[] =",
    ]
    for i, line in enumerate(lines):
        esc = line.replace("\\", "\\\\").replace('"', '\\"')
        nl = "" if i == len(lines) - 1 else "\\n"
        out.append('    "%s%s"' % (esc, nl))
    out.append("    ;")
    out.append("")
    out.append("#endif /* TEMPMAIL_WEBUI_HTML_H */")
    with open(OUT, "w", encoding="utf-8") as f:
        f.write("\n".join(out) + "\n")
    print("生成 %s（%d 字节，%d 行）" % (OUT, len(merged), len(lines)))


if __name__ == "__main__":
    main()
