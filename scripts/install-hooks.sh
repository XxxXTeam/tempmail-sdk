#!/usr/bin/env bash
# install-hooks.sh：将 git hooks 安装到当前仓库
# 用法: ./scripts/install-hooks.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
HOOKS_DIR="$PROJECT_ROOT/.git/hooks"

if [[ ! -d "$PROJECT_ROOT/.git" ]]; then
    echo "错误: 未找到 .git 目录，请确保在项目根目录运行此脚本"
    exit 1
fi

# 备份现有 hook（如果存在且不是我们的脚本）
for hook in pre-commit; do
    src="$SCRIPT_DIR/hooks/$hook"
    dst="$HOOKS_DIR/$hook"

    if [[ -f "$dst" ]]; then
        if grep -q "pre-commit hook" "$dst" 2>/dev/null; then
            echo "  已存在 $hook，跳过"
            continue
        fi
        backup="$dst.backup.$(date +%Y%m%d%H%M%S)"
        cp "$dst" "$backup"
        echo "  已备份现有 $hook → $backup"
    fi

    cp "$src" "$dst"
    chmod +x "$dst"
    echo "  已安装 $hook"
done

echo ""
echo "git hooks 安装完成"
echo "如需卸载，手动删除以下文件即可:"
echo "  $HOOKS_DIR/pre-commit"
