package main

import (
	"fmt"
	"log"

	tempemail "github.com/XxxXTeam/tempmail-sdk/sdk/go"
)

func main() {
	fmt.Println("=== Test Generate Email ===")
	fmt.Println()

	channels := []tempemail.Channel{
		tempemail.ChannelTempmail,
		tempemail.ChannelLinshiEmail,
		tempemail.ChannelTempmailLol,
		tempemail.ChannelChatgptOrgUk,
		tempemail.ChannelTempmailLa,
		tempemail.ChannelTempMailIO,
		tempemail.ChannelAwamail,
		tempemail.ChannelMailTm,
		tempemail.ChannelDropmail,
	}

	for _, channel := range channels {
		fmt.Printf("Testing channel: %s\n", channel)
		emailInfo, err := tempemail.GenerateEmail(&tempemail.GenerateEmailOptions{
			Channel: channel,
		})
		if err != nil {
			fmt.Printf("  Error: %v\n\n", err)
			continue
		}

		fmt.Printf("  Channel: %s\n", emailInfo.Channel)
		fmt.Printf("  Email: %s\n", emailInfo.Email)
		if emailInfo.Token != "" {
			fmt.Printf("  Token: %s\n", emailInfo.Token)
		}
		if emailInfo.ExpiresAt != nil {
			fmt.Printf("  Expires: %v\n", emailInfo.ExpiresAt)
		}
		if emailInfo.CreatedAt != "" {
			fmt.Printf("  Created: %s\n", emailInfo.CreatedAt)
		}
		fmt.Println()
	}

	fmt.Println("=== Test Get Emails ===")
	fmt.Println()

	client := tempemail.NewClient()

	emailInfo, err := client.Generate(&tempemail.GenerateEmailOptions{
		Channel: tempemail.ChannelTempmail,
	})
	if err != nil {
		log.Fatalf("Failed to generate email: %v", err)
	}

	fmt.Printf("Generated: %s - %s\n", emailInfo.Channel, emailInfo.Email)

	result, err := client.GetEmails()
	if err != nil {
		log.Fatalf("Failed to get emails: %v", err)
	}

	fmt.Printf("Emails count: %d\n", len(result.Emails))

	// 展示标准化的邮件格式
	for _, email := range result.Emails {
		fmt.Printf("  ID: %s\n", email.ID)
		fmt.Printf("  From: %s\n", email.From)
		fmt.Printf("  To: %s\n", email.To)
		fmt.Printf("  Subject: %s\n", email.Subject)
		fmt.Printf("  Date: %s\n", email.Date)
		fmt.Printf("  IsRead: %v\n", email.IsRead)
		text := email.Text
		if len(text) > 100 {
			text = text[:100]
		}
		fmt.Printf("  Text: %s\n", text)
		html := email.HTML
		if len(html) > 100 {
			html = html[:100]
		}
		fmt.Printf("  HTML: %s\n", html)
		fmt.Printf("  Attachments: %d\n", len(email.Attachments))
		fmt.Println()
	}
}
