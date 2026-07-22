#!/usr/bin/env python3
"""从 webui/ 模板生成 C# 内联 HTML 常量文件 WebUIHtml.cs。

读取 index.html，将 <!-- PLACEHOLDER_SCRIPT --> 替换为 app.js.html 的内容，
再转义为 C# verbatim 字符串（" -> ""），写入 src/WebUIHtml.cs。
与其它端保持同一份前端资产。请勿手工编辑生成产物。
"""
import pathlib
import sys

# 定位仓库根：scripts/ -> csharp/ -> sdk/ -> repo
REPO = pathlib.Path(__file__).resolve().parents[3]
INDEX = REPO / "webui" / "index.html"
APPJS = REPO / "webui" / "app.js.html"
OUT = REPO / "sdk" / "csharp" / "src" / "WebUIHtml.cs"
PLACEHOLDER = "<!-- PLACEHOLDER_SCRIPT -->"


def main() -> int:
    index_html = INDEX.read_text(encoding="utf-8")
    app_js = APPJS.read_text(encoding="utf-8")
    if PLACEHOLDER not in index_html:
        print(f"未在 index.html 找到占位符: {PLACEHOLDER}", file=sys.stderr)
        return 1
    page = index_html.replace(PLACEHOLDER, app_js, 1)
    # C# verbatim 字符串仅需转义双引号
    escaped = page.replace('"', '""')
    lines = [
        "namespace XxxXTeam.TempMail;",
        "",
        "/// <summary>",
        "/// WebUI 前端页面常量（构建期由 scripts/gen_webui_html.py 从 webui/ 模板生成）。",
        "/// index.html 占位处已内联 app.js.html，请勿手工编辑本文件。",
        "/// </summary>",
        "internal static class WebUIHtml",
        "{",
        "    /// <summary>完整前端 HTML 页面</summary>",
        '    internal const string Page = @"' + escaped + '";',
        "}",
        "",
    ]
    OUT.write_text("\n".join(lines), encoding="utf-8")
    print(f"已生成 {OUT} ({len(page)} 字符)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
