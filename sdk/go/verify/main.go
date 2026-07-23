package main

import (
	"encoding/json"
	"flag"
	"fmt"
	"math/rand"
	"os"
	"sort"
	"strings"
	"sync"
	"time"

	tempemail "github.com/XxxXTeam/tempmail-sdk/sdk/go"
)

/*
 * kindResult 单个渠道针对单一载荷类型的验证结果
 */
type kindResult struct {
	Kind       payloadKind `json:"kind"`
	Sent       bool        `json:"sent"` // 是否成功发信
	SendErr    string      `json:"sendErr,omitempty"`
	Received   bool        `json:"received"` // 是否按 token 收到该封
	HasText    bool        `json:"hasText"`  // 归一化后 text 非空
	HasHTML    bool        `json:"hasHTML"`  // 归一化后 html 非空
	HasCode    bool        `json:"hasCode"`  // text 或 html 中包含验证码
	HTMLLen    int         `json:"htmlLen"`  // 收到的 html 长度
	SentHTML   int         `json:"sentHTMLLen"`
	SentinelOK bool        `json:"sentinelOK"` // 大 HTML 末尾哨兵是否存在（未截断）
	HasAttach  bool        `json:"hasAttach"`  // 附件是否被归一化出
	Note       string      `json:"note,omitempty"`
}

/*
 * channelResult 单个渠道的完整验证结果
 */
type channelResult struct {
	Channel string       `json:"channel"`
	Website string       `json:"website"`
	GenOK   bool         `json:"genOK"` // 邮箱创建成功且渠道一致
	Email   string       `json:"email"`
	GenErr  string       `json:"genErr,omitempty"`
	MXHost  string       `json:"mxHost,omitempty"`
	Kinds   []kindResult `json:"kinds"`
	Verdict string       `json:"verdict"` // ok / no-body / truncated / partial / no-receive / gen-failed / skipped
	Elapsed string       `json:"elapsed"`
}

func main() {
	var (
		only         = flag.String("channels", "", "仅测试逗号分隔的指定渠道（调试用）")
		concurrency  = flag.Int("c", 4, "并发渠道数")
		pollTimeout  = flag.Duration("poll", 100*time.Second, "每渠道收信轮询上限")
		pollInterval = flag.Duration("interval", 5*time.Second, "轮询间隔")
		outJSON      = flag.String("json", "verify_result.json", "JSON 结果输出路径")
		outMD        = flag.String("md", "verify_report.md", "Markdown 报告输出路径")
		limit        = flag.Int("limit", 0, "只测前 N 个渠道（0=全部）")
		kindsFlag    = flag.String("kinds", "text,html,attach", "测试的载荷类型")
		htmlRowsF    = flag.Int("htmlrows", 400, "大HTML表格行数(控制体积,400≈112KB,80≈25KB)")
	)
	flag.Parse()
	htmlRows = *htmlRowsF

	// 关闭遥测，避免验证流量污染统计
	f := false
	tempemail.SetConfig(tempemail.SDKConfig{TelemetryEnabled: &f})

	kinds := parseKinds(*kindsFlag)

	all := tempemail.ListChannels()
	var targets []tempemail.ChannelInfo
	if *only != "" {
		want := map[string]bool{}
		for _, c := range strings.Split(*only, ",") {
			want[strings.TrimSpace(c)] = true
		}
		for _, ci := range all {
			if want[string(ci.Channel)] {
				targets = append(targets, ci)
			}
		}
	} else {
		targets = all
	}
	if *limit > 0 && *limit < len(targets) {
		targets = targets[:*limit]
	}

	fmt.Fprintf(os.Stderr, "共 %d 个渠道待验证，并发 %d，每渠道轮询上限 %v\n", len(targets), *concurrency, *pollTimeout)

	results := make([]channelResult, len(targets))
	sem := make(chan struct{}, *concurrency)
	var wg sync.WaitGroup
	var doneCount int
	var mu sync.Mutex

	for i, ci := range targets {
		wg.Add(1)
		sem <- struct{}{}
		go func(idx int, ci tempemail.ChannelInfo) {
			defer wg.Done()
			defer func() { <-sem }()
			rng := rand.New(rand.NewSource(int64(idx)*7919 + time.Now().UnixNano()))
			res := verifyChannel(ci, kinds, *pollTimeout, *pollInterval, rng)
			results[idx] = res
			mu.Lock()
			doneCount++
			n := doneCount
			mu.Unlock()
			fmt.Fprintf(os.Stderr, "[%d/%d] %-28s %s\n", n, len(targets), ci.Channel, res.Verdict)
		}(i, ci)
	}
	wg.Wait()

	// 写 JSON
	if data, err := json.MarshalIndent(results, "", "  "); err == nil {
		_ = os.WriteFile(*outJSON, data, 0644)
	}
	// 写 Markdown
	writeMarkdown(*outMD, results, kinds)

	// 控制台汇总
	printSummary(results)
	fmt.Fprintf(os.Stderr, "\nJSON: %s\nMarkdown: %s\n", *outJSON, *outMD)
}

func parseKinds(s string) []payloadKind {
	var ks []payloadKind
	for _, p := range strings.Split(s, ",") {
		switch strings.TrimSpace(p) {
		case "text":
			ks = append(ks, kindText)
		case "html":
			ks = append(ks, kindHTML)
		case "attach":
			ks = append(ks, kindAttach)
		}
	}
	if len(ks) == 0 {
		ks = []payloadKind{kindText, kindHTML, kindAttach}
	}
	return ks
}

/*
 * verifyChannel 验证单个渠道：创建邮箱 → 发信 → 轮询 → 校验
 */
func verifyChannel(ci tempemail.ChannelInfo, kinds []payloadKind, pollTimeout, pollInterval time.Duration, rng *rand.Rand) channelResult {
	start := time.Now()
	res := channelResult{Channel: string(ci.Channel), Website: ci.Website}
	res.MXHost = lookupMX(ci.Website)

	// 1. 创建邮箱，强制单渠道，禁止 fallback 污染
	info, err := tempemail.GenerateEmail(&tempemail.GenerateEmailOptions{
		Channel:          ci.Channel,
		MaxChannelsTried: 1,
	})
	if err != nil || info == nil {
		res.GenErr = "创建返回 nil"
		if err != nil {
			res.GenErr = err.Error()
		}
		res.Verdict = "gen-failed"
		res.Elapsed = time.Since(start).Round(time.Second).String()
		return res
	}
	if info.Channel != ci.Channel {
		// fallback 换了渠道，视为该渠道创建失败
		res.GenErr = fmt.Sprintf("fallback 到 %s", info.Channel)
		res.Verdict = "gen-failed"
		res.Email = info.Email
		res.Elapsed = time.Since(start).Round(time.Second).String()
		return res
	}
	res.GenOK = true
	res.Email = info.Email

	// 2. 逐类型发信，记录 token
	msgs := make([]builtMessage, 0, len(kinds))
	for _, k := range kinds {
		m := buildMessage(info.Email, k, rng)
		kr := kindResult{Kind: k, SentHTML: m.sentHTMLLen}
		if err := sendMail(info.Email, m.rawMIME); err != nil {
			kr.SendErr = err.Error()
		} else {
			kr.Sent = true
			msgs = append(msgs, m)
		}
		res.Kinds = append(res.Kinds, kr)
		// 发信之间轻微间隔，缓解 SMTP 限流
		time.Sleep(300 * time.Millisecond)
	}

	// 若全部发信失败，直接结束
	anySent := false
	for _, kr := range res.Kinds {
		if kr.Sent {
			anySent = true
		}
	}
	if !anySent {
		res.Verdict = "send-failed"
		res.Elapsed = time.Since(start).Round(time.Second).String()
		return res
	}

	// 3. 轮询收信，按 token 匹配
	deadline := time.Now().Add(pollTimeout)
	matched := map[string]tempemail.Email{} // token -> email
	for time.Now().Before(deadline) {
		result, e := tempemail.GetEmails(info, nil)
		if e == nil && result != nil && result.Success {
			for _, em := range result.Emails {
				for _, m := range msgs {
					if _, ok := matched[m.token]; ok {
						continue
					}
					if emailMatchesToken(em, m) {
						matched[m.token] = em
					}
				}
			}
		}
		if len(matched) >= len(msgs) {
			break
		}
		time.Sleep(pollInterval)
	}

	// 4. 校验每类型
	for i := range res.Kinds {
		kr := &res.Kinds[i]
		if !kr.Sent {
			continue
		}
		var m builtMessage
		for _, mm := range msgs {
			if mm.kind == kr.Kind {
				m = mm
			}
		}
		em, ok := matched[m.token]
		if !ok {
			kr.Received = false
			continue
		}
		kr.Received = true
		kr.HasText = strings.TrimSpace(em.Text) != ""
		kr.HasHTML = strings.TrimSpace(em.HTML) != ""
		kr.HTMLLen = len(em.HTML)
		kr.HasCode = strings.Contains(em.Text, m.code) || strings.Contains(em.HTML, m.code)
		if m.kind == kindHTML {
			kr.SentinelOK = strings.Contains(em.HTML, m.sentinel)
			/*
			 * 截断判据（以长度比例为主、哨兵为辅）：
			 * - 哨兵存在 → 完整
			 * - 哨兵缺失但收到长度 ≥ 发送 85% → 视为完整（哨兵可能被 sanitize 清洗，但正文基本保留）
			 * - 哨兵缺失且长度 < 85% → 判定截断，比例越低越严重
			 */
			ratio := 0.0
			if m.sentHTMLLen > 0 {
				ratio = float64(kr.HTMLLen) / float64(m.sentHTMLLen)
			}
			if !kr.SentinelOK {
				if ratio >= 0.85 {
					kr.Note = fmt.Sprintf("哨兵缺失但长度充足(%.0f%%,疑被sanitize清洗)", ratio*100)
				} else {
					kr.Note = fmt.Sprintf("正文截断 %d/%d(%.0f%%)", kr.HTMLLen, m.sentHTMLLen, ratio*100)
				}
			}
		}
		if m.kind == kindAttach {
			kr.HasAttach = len(em.Attachments) > 0
		}
	}

	res.Verdict = deriveVerdict(res.Kinds)
	res.Elapsed = time.Since(start).Round(time.Second).String()
	return res
}

/*
 * emailMatchesToken 判断收到的邮件是否为指定 token 的测试邮件
 * 优先匹配 subject（含 token），回退匹配正文中的 token
 */
func emailMatchesToken(em tempemail.Email, m builtMessage) bool {
	if strings.Contains(em.Subject, m.token) {
		return true
	}
	if strings.Contains(em.Text, m.token) || strings.Contains(em.HTML, m.token) {
		return true
	}
	return false
}

/*
 * deriveVerdict 依据各类型结果综合判定渠道收信质量
 */
func deriveVerdict(kinds []kindResult) string {
	var received, total int
	var bodyMissing, truncated, codeMissing, attachMissing bool
	var attachTested bool
	for _, kr := range kinds {
		if !kr.Sent {
			continue
		}
		total++
		if !kr.Received {
			continue
		}
		received++
		if kr.Kind == kindText {
			if !kr.HasText && !kr.HasHTML {
				bodyMissing = true
			}
			if !kr.HasCode {
				codeMissing = true
			}
		}
		if kr.Kind == kindHTML {
			if !kr.HasHTML {
				bodyMissing = true
			} else if strings.Contains(kr.Note, "正文截断") {
				truncated = true
			}
		}
		if kr.Kind == kindAttach {
			attachTested = true
			if !kr.HasAttach {
				attachMissing = true
			}
		}
	}
	if total == 0 {
		return "send-failed"
	}
	if received == 0 {
		return "no-receive"
	}
	if bodyMissing {
		return "no-body"
	}
	if truncated {
		return "truncated"
	}
	if received < total || codeMissing || (attachTested && attachMissing) {
		return "partial"
	}
	return "ok"
}

/*
 * writeMarkdown 输出人类可读的 Markdown 报告
 */
func writeMarkdown(path string, results []channelResult, kinds []payloadKind) {
	var b strings.Builder
	b.WriteString("# 临时邮箱渠道真实收信验证报告\n\n")
	b.WriteString(fmt.Sprintf("生成时间: %s\n\n", time.Now().Format("2006-01-02 15:04:05")))

	// 分类统计
	byVerdict := map[string][]string{}
	for _, r := range results {
		byVerdict[r.Verdict] = append(byVerdict[r.Verdict], r.Channel)
	}
	b.WriteString("## 结论分布\n\n")
	order := []string{"ok", "partial", "truncated", "no-body", "no-receive", "send-failed", "gen-failed"}
	labels := map[string]string{
		"ok":          "✅ 正常（收信+正文+验证码+附件均完整）",
		"partial":     "⚠️ 部分（收到但验证码/附件/部分类型缺失）",
		"truncated":   "✂️ 正文截断（大HTML被截断/哨兵丢失）",
		"no-body":     "📭 有标题无正文",
		"no-receive":  "❌ 未收到测试邮件",
		"send-failed": "🚫 发信失败",
		"gen-failed":  "🔌 邮箱创建失败/fallback",
	}
	for _, v := range order {
		list := byVerdict[v]
		b.WriteString(fmt.Sprintf("- **%s**: %d 个\n", labels[v], len(list)))
	}
	b.WriteString("\n")

	// 重点问题清单
	b.WriteString("## 重点问题渠道\n\n")
	for _, v := range []string{"no-body", "truncated", "partial", "no-receive"} {
		list := byVerdict[v]
		if len(list) == 0 {
			continue
		}
		sort.Strings(list)
		b.WriteString(fmt.Sprintf("### %s\n\n%s\n\n", labels[v], strings.Join(list, ", ")))
	}

	// 明细表
	b.WriteString("## 明细\n\n")
	b.WriteString("| 渠道 | 结论 | 创建 | 文本 | 大HTML | 附件 | MX | 邮箱 |\n")
	b.WriteString("|---|---|---|---|---|---|---|---|\n")
	for _, r := range results {
		textCell, htmlCell, attachCell := "-", "-", "-"
		for _, kr := range r.Kinds {
			switch kr.Kind {
			case kindText:
				textCell = cellText(kr)
			case kindHTML:
				htmlCell = cellHTML(kr)
			case kindAttach:
				attachCell = cellAttach(kr)
			}
		}
		gen := "✓"
		if !r.GenOK {
			gen = "✗ " + r.GenErr
		}
		b.WriteString(fmt.Sprintf("| %s | %s | %s | %s | %s | %s | %s | %s |\n",
			r.Channel, r.Verdict, gen, textCell, htmlCell, attachCell, r.MXHost, r.Email))
	}

	_ = os.WriteFile(path, []byte(b.String()), 0644)
}

func cellText(kr kindResult) string {
	if !kr.Sent {
		return "发信失败"
	}
	if !kr.Received {
		return "未收到"
	}
	s := ""
	if kr.HasText || kr.HasHTML {
		s = "有正文"
	} else {
		s = "❌无正文"
	}
	if kr.HasCode {
		s += "/验证码✓"
	} else {
		s += "/验证码✗"
	}
	return s
}

func cellHTML(kr kindResult) string {
	if !kr.Sent {
		return "发信失败"
	}
	if !kr.Received {
		return "未收到"
	}
	if !kr.HasHTML {
		return "❌无HTML"
	}
	if strings.Contains(kr.Note, "正文截断") {
		return fmt.Sprintf("✂️截断 %d/%d", kr.HTMLLen, kr.SentHTML)
	}
	if !kr.SentinelOK {
		return fmt.Sprintf("✓完整(清洗) %d/%d", kr.HTMLLen, kr.SentHTML)
	}
	return fmt.Sprintf("✓完整 %d", kr.HTMLLen)
}

func cellAttach(kr kindResult) string {
	if !kr.Sent {
		return "发信失败"
	}
	if !kr.Received {
		return "未收到"
	}
	if kr.HasAttach {
		return "✓有附件"
	}
	return "✗无附件"
}

func printSummary(results []channelResult) {
	cnt := map[string]int{}
	for _, r := range results {
		cnt[r.Verdict]++
	}
	fmt.Fprintf(os.Stderr, "\n=== 汇总 ===\n")
	for _, v := range []string{"ok", "partial", "truncated", "no-body", "no-receive", "send-failed", "gen-failed"} {
		fmt.Fprintf(os.Stderr, "%-12s %d\n", v, cnt[v])
	}
}
