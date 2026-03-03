package main

import (
	"encoding/json"
	"fmt"
	"os"
	"os/signal"
	"strings"
	"syscall"
	"time"

	tempemail "github.com/XxxXTeam/tempmail-sdk/sdk/go"
)

func printJson(label string, data any) {
	fmt.Printf("\n%s:\n", label)
	jsonData, _ := json.MarshalIndent(data, "", "  ")
	fmt.Println(string(jsonData))
}

// 配置
const (
	Channel      = tempemail.ChannelTempmail // 指定渠道
	PollInterval = 5 * time.Second           // 轮询间隔
	MaxPollCount = 60                        // 最大轮询次数
)

func printEmail(email tempemail.Email, index int) {
	fmt.Printf("\n邮件 #%d\n", index+1)
	fmt.Println(strings.Repeat("━", 50))
	fmt.Printf("ID: %s\n", email.ID)
	fmt.Printf("发件人: %s\n", getOrDefault(email.From, "未知"))
	fmt.Printf("收件人: %s\n", getOrDefault(email.To, "未知"))
	fmt.Printf("主题: %s\n", getOrDefault(email.Subject, "无主题"))
	fmt.Printf("时间: %s\n", getOrDefault(email.Date, "未知"))
	fmt.Printf("已读: %v\n", email.IsRead)
	if email.Text != "" {
		text := email.Text
		if len(text) > 200 {
			text = text[:200]
		}
		fmt.Printf("内容: %s\n", text)
	}
	if email.HTML != "" {
		html := email.HTML
		if len(html) > 100 {
			html = html[:100] + "..."
		}
		fmt.Printf("HTML: %s\n", html)
	}
	if len(email.Attachments) > 0 {
		names := make([]string, len(email.Attachments))
		for i, a := range email.Attachments {
			names[i] = a.Filename
		}
		fmt.Printf("附件: %s\n", strings.Join(names, ", "))
	}
	fmt.Println(strings.Repeat("━", 50))
}

func pollEmails(emailInfo *tempemail.EmailInfo) {
	fmt.Printf("\n开始轮询邮件...（每 %v 检查一次）\n", PollInterval)
	fmt.Println("按 Ctrl+C 停止轮询")
	fmt.Println()

	// 处理 Ctrl+C 信号
	sigChan := make(chan os.Signal, 1)
	signal.Notify(sigChan, syscall.SIGINT, syscall.SIGTERM)

	ticker := time.NewTicker(PollInterval)
	defer ticker.Stop()

	pollCount := 0

	for {
		select {
		case <-sigChan:
			fmt.Println("\n\n收到停止信号，退出轮询")
			return
		case <-ticker.C:
			pollCount++
			if pollCount > MaxPollCount {
				fmt.Println("\n\n已达到最大轮询次数，停止轮询")
				return
			}

			result, err := emailInfo.GetEmails(nil)

			timestamp := time.Now().Format("15:04:05")

			if err != nil {
				fmt.Printf("\n轮询出错: %v\n", err)
				continue
			}

			if len(result.Emails) > 0 {
				fmt.Printf("\n[%s] 🎉 收到 %d 封邮件！\n", timestamp, len(result.Emails))

				// 打印原始返回数据
				printJson("返回数据 (GetEmailsResult)", result)

				for i, email := range result.Emails {
					printEmail(email, i)
				}
				// 收到邮件后可以选择继续轮询或退出
				// return
			} else {
				fmt.Printf("\r[%s] 第 %d/%d 次检查，暂无新邮件...", timestamp, pollCount, MaxPollCount)
			}
		}
	}
}

func getOrDefault(s, def string) string {
	if s == "" {
		return def
	}
	return s
}

func main() {
	fmt.Println(strings.Repeat("═", 50))
	fmt.Println("  临时邮箱 Demo - 获取邮箱并轮询邮件")
	fmt.Println(strings.Repeat("═", 50))

	// 1. 列出所有支持的渠道
	fmt.Println("\n[1] 列出所有支持的渠道...")
	channels := tempemail.ListChannels()
	printJson("支持的渠道列表", channels)

	// 2. 获取指定渠道信息
	fmt.Printf("\n[2] 获取指定渠道 \"%s\" 信息...\n", Channel)
	channelInfo, _ := tempemail.GetChannelInfo(Channel)
	printJson("渠道信息", channelInfo)

	// 3. 从指定渠道获取邮箱
	fmt.Printf("\n[3] 从 %s 渠道获取临时邮箱...\n", Channel)

	emailInfo, err := tempemail.GenerateEmail(&tempemail.GenerateEmailOptions{
		Channel: Channel,
	})
	if err != nil {
		fmt.Printf("\n❌ 获取邮箱失败: %v\n", err)
		os.Exit(1)
	}

	fmt.Println("\n✅ 获取邮箱成功！")
	printJson("返回数据 (EmailInfo)", emailInfo)

	fmt.Println("\n📋 邮箱信息:")
	fmt.Printf("   渠道: %s\n", emailInfo.Channel)
	fmt.Printf("   邮箱: %s\n", emailInfo.Email)
	if emailInfo.ExpiresAt != nil {
		fmt.Printf("   过期时间: %v\n", emailInfo.ExpiresAt)
	}
	if emailInfo.CreatedAt != "" {
		fmt.Printf("   创建时间: %s\n", emailInfo.CreatedAt)
	}

	fmt.Println("\n📧 请发送邮件到以上邮箱地址进行测试")

	// 4. 轮询获取邮件
	pollEmails(emailInfo)
}
