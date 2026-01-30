package main

import (
	"fmt"
	"log"

	tempemail "github.com/temp-email/sdk-go"
)

// To run this example, first update go.mod with:
// replace github.com/temp-email/sdk-go => ../

func main() {
	fmt.Println("=== Test Generate Email ===")
	fmt.Println()

	channels := []tempemail.Channel{
		tempemail.ChannelTempmail,
		tempemail.ChannelLinshiEmail,
		tempemail.ChannelTempmailLol,
		tempemail.ChannelChatgptOrgUk,
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
}
