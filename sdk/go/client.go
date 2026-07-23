package tempemail

import (
	"fmt"
	"math/rand"
	"strings"
	"time"

	prov "github.com/XxxXTeam/tempmail-sdk/sdk/go/provider"
)

/*
 * ChannelInfo 渠道信息，包含渠道标识、显示名称和对应网站
 */
func fixedDomain(domain string) *string { return &domain }

/*
 * genMoaktVariant 生成 moakt“同后端、多固定域名”变体的邮箱
 * moakt 及其 14 个固定域名变体（drmail.in / teml.net 等）共用同一后端，
 * 底层 MoaktGenerate 统一返回基准渠道 "moakt"，此处将 Channel 回填为具体变体渠道，
 * 保证返回的 EmailInfo.Channel 与用户请求的渠道标识一致
 */
func genMoaktVariant(domain string, channel Channel) (*EmailInfo, error) {
	info, err := fromMailbox(prov.MoaktGenerate(fixedDomain(domain)))
	if err != nil {
		return nil, err
	}
	info.Channel = channel
	return info, nil
}

/*
 * genTenminuteVariant 生成 10minute-one“同后端、多固定域名”变体的邮箱
 * TenminuteOneGenerate 仅接受 domain 参数、返回基准渠道 "10minute-one"，
 * 此处将 Channel 回填为具体变体渠道（xghff-com / oqqaj-com 等），
 * 保证返回的 EmailInfo.Channel 与用户请求的渠道标识一致
 */
func genTenminuteVariant(domain string, channel Channel) (*EmailInfo, error) {
	info, err := fromMailbox(prov.TenminuteOneGenerate(fixedDomain(domain)))
	if err != nil {
		return nil, err
	}
	info.Channel = channel
	return info, nil
}

type ChannelInfo struct {
	/* 渠道标识 */
	Channel Channel
	/* 渠道显示名称 */
	Name string
	/* 对应的临时邮箱服务网站 */
	Website string
}

/*
 * ListChannels 获取所有支持的渠道列表
 * 返回所有渠道的信息数组，包含渠道名称和对应网站
 */
func ListChannels() []ChannelInfo {
	result := make([]ChannelInfo, len(channelRegistry))
	for i := range channelRegistry {
		result[i] = ChannelInfo{
			Channel: channelRegistry[i].Channel,
			Name:    channelRegistry[i].Name,
			Website: channelRegistry[i].Website,
		}
	}
	return result
}

/*
 * GetChannelInfo 获取指定渠道的详细信息
 * 返回渠道信息和是否存在的标记
 */
func GetChannelInfo(channel Channel) (ChannelInfo, bool) {
	spec, ok := channelRegistryMap[channel]
	if !ok {
		return ChannelInfo{}, false
	}
	return ChannelInfo{Channel: spec.Channel, Name: spec.Name, Website: spec.Website}, true
}

/*
 * GenerateEmail 创建临时邮箱
 *
 * 错误处理策略:
 * - 指定渠道失败时，自动尝试其他可用渠道（打乱顺序逐个尝试）
 * - 未指定渠道时，打乱全部渠道逐个尝试，直到成功
 * - 所有渠道均不可用时返回 nil（不返回 error）
 *
 * 示例:
 *   info, _ := GenerateEmail(&GenerateEmailOptions{Channel: ChannelMailTm})
 *   if info != nil { fmt.Println(info.Email) }
 */
func GenerateEmail(opts *GenerateEmailOptions) (*EmailInfo, error) {
	if opts == nil {
		opts = &GenerateEmailOptions{}
	}

	tryOrder := buildChannelOrder(opts.Channel)

	// 解析域名筛选条件
	var targetDomains []string
	if opts.Suffix != "" {
		s := opts.Suffix
		if strings.HasPrefix(s, "@") {
			s = s[1:]
		}
		targetDomains = append(targetDomains, s)
	}
	targetDomains = append(targetDomains, opts.Domains...)
	// 按域名筛选渠道
	if len(targetDomains) > 0 {
		tryOrder = filterChannelsByDomain(tryOrder, targetDomains)
	}

	maxChannels := opts.MaxChannelsTried
	if maxChannels <= 0 {
		maxChannels = 20
	}
	totalTimeout := opts.TotalTimeout
	if totalTimeout <= 0 {
		totalTimeout = 60 * time.Second
	}
	startTime := time.Now()
	failedBackends := make(map[string]bool)

	channelsTried := 0
	var lastErrMsg string
	for _, ch := range tryOrder {
		if channelsTried >= maxChannels {
			sdkLogger.Warn("已尝试最大渠道数，停止", "max", maxChannels)
			break
		}
		if time.Since(startTime) >= totalTimeout {
			sdkLogger.Warn("整体超时，停止尝试")
			break
		}

		backend := channelToBackend[ch]

		if backend != "" {
			if failedBackends[backend] {
				sdkLogger.Debug("跳过渠道，同后端已失败", "channel", string(ch), "backend", backend)
				continue
			}
			if !isBackendOpen(backend) {
				sdkLogger.Debug("跳过渠道，后端熔断中", "channel", string(ch), "backend", backend)
				continue
			}
		}

		channelsTried++
		sdkLogger.Info("创建临时邮箱", "channel", string(ch))
		result, attempts, err := withRetryAndAttempts(func() (*EmailInfo, error) {
			return generateEmailOnce(ch, opts)
		}, opts.Retry)
		if err == nil && result != nil {
			sdkLogger.Info("邮箱创建成功", "channel", string(ch), "email", result.Email)
			reportTelemetry("generate_email", string(ch), true, attempts, channelsTried, "")
			if backend != "" {
				recordBackendSuccess(backend)
			}
			return result, nil
		}
		errMsg := "unknown error"
		if err != nil {
			errMsg = err.Error()
			lastErrMsg = errMsg
		}
		sdkLogger.Warn("渠道不可用，尝试下一个", "channel", string(ch), "error", errMsg)
		if backend != "" {
			failedBackends[backend] = true
			recordBackendFailure(backend)
		}
	}

	sdkLogger.Error("所有渠道均不可用，创建邮箱失败")
	reportTelemetry("generate_email", "", false, 0, channelsTried, lastErrMsg)
	if lastErrMsg == "" {
		lastErrMsg = "所有渠道均不可用"
	}
	return nil, fmt.Errorf("创建临时邮箱失败：已尝试 %d 个渠道，最后错误：%s", channelsTried, lastErrMsg)
}

/*
 * buildChannelOrder 构建渠道尝试顺序
 * 指定渠道时优先尝试该渠道，其余渠道打乱追加
 * 未指定时打乱全部渠道
 */
func buildChannelOrder(preferred Channel) []Channel {
	shuffled := make([]Channel, len(allChannels))
	copy(shuffled, allChannels)
	rand.Shuffle(len(shuffled), func(i, j int) {
		shuffled[i], shuffled[j] = shuffled[j], shuffled[i]
	})
	if preferred == "" {
		return shuffled
	}
	/* 预分配容量，避免从容量 1 起追加数百个渠道时的多次扩容与拷贝 */
	result := make([]Channel, 0, len(shuffled))
	result = append(result, preferred)
	for _, ch := range shuffled {
		if ch != preferred {
			result = append(result, ch)
		}
	}
	return result
}

/*
 * generateEmailOnce 单次创建邮箱（不含重试逻辑）
 * 根据渠道类型分发到对应的 provider 实现
 */
func generateEmailOnce(channel Channel, opts *GenerateEmailOptions) (*EmailInfo, error) {
	spec, ok := channelRegistryMap[channel]
	if !ok || spec.Generate == nil {
		return nil, fmt.Errorf("unknown channel: %s", channel)
	}
	return spec.Generate(opts)
}

/*
 * GetEmails 获取邮件列表
 * Channel/Email/Token 等信息由 SDK 从 EmailInfo 中自动获取，用户无需手动传递
 *
 * 错误处理策略:
 * - 网络错误、超时、服务端 5xx 错误 → 自动重试（默认 2 次）
 * - 重试耗尽后返回 { Success: false, Emails: [] }，不返回 error
 * - 参数校验错误（缺少 EmailInfo）直接返回 error
 *
 * 这种设计让调用方在轮询场景下不会因网络波动而中断整个流程，
 * 只需检查 Success 字段即可判断本次请求是否成功。
 *
 * 示例:
 *   info, _ := GenerateEmail(&GenerateEmailOptions{Channel: ChannelMailTm})
 *   result, _ := GetEmails(info, nil)
 */
func GetEmails(info *EmailInfo, opts *GetEmailsOptions) (*GetEmailsResult, error) {
	if info == nil {
		reportTelemetry("get_emails", "", false, 0, 0, "EmailInfo is required, call GenerateEmail() first")
		return nil, fmt.Errorf("EmailInfo is required, call GenerateEmail() first")
	}
	if info.Channel == "" {
		reportTelemetry("get_emails", "", false, 0, 0, "channel is required")
		return nil, fmt.Errorf("channel is required")
	}
	if info.Email == "" && info.Channel != ChannelTempmailLol {
		reportTelemetry("get_emails", string(info.Channel), false, 0, 0, "email is required")
		return nil, fmt.Errorf("email is required")
	}

	var retry *RetryOptions
	if opts != nil {
		retry = opts.Retry
	}

	sdkLogger.Debug("获取邮件", "channel", string(info.Channel), "email", info.Email)
	emails, attempts, err := withRetryAndAttempts(func() ([]Email, error) {
		return getEmailsOnce(info.Channel, info.Email, info.token)
	}, retry)

	if err != nil {
		reportTelemetry("get_emails", string(info.Channel), false, attempts, 0, err.Error())
		/*
		 * 重试耗尽后仍然失败 → 返回空结果而非 error
		 * 这样调用方在轮询场景下不会因为一次网络波动而中断整个流程
		 */
		sdkLogger.Error("获取邮件失败", "channel", string(info.Channel), "error", err.Error())
		return &GetEmailsResult{
			Channel: info.Channel,
			Email:   info.Email,
			Emails:  []Email{},
			Success: false,
		}, nil
	}

	if len(emails) > 0 {
		sdkLogger.Info("获取到邮件", "channel", string(info.Channel), "count", len(emails))
	} else {
		sdkLogger.Debug("暂无邮件", "channel", string(info.Channel))
	}
	reportTelemetry("get_emails", string(info.Channel), true, attempts, 0, "")

	return &GetEmailsResult{
		Channel: info.Channel,
		Email:   info.Email,
		Emails:  emails,
		Success: true,
	}, nil
}

/*
 * getEmailsOnce 单次获取邮件（不含重试逻辑）
 * 根据渠道类型分发到对应的 provider 实现
 * token 由 SDK 内部从 EmailInfo 中获取，用户无感知
 */
func getEmailsOnce(channel Channel, email string, token string) ([]Email, error) {
	spec, ok := channelRegistryMap[channel]
	if !ok || spec.GetEmails == nil {
		return nil, fmt.Errorf("unsupported channel: %s", channel)
	}
	return spec.GetEmails(email, token)
}

/*
 * EmailInfo.GetEmails 获取当前邮箱的邮件列表
 * Token 等内部信息由 SDK 自动维护，用户无需关心
 *
 * 示例:
 *   info, _ := GenerateEmail(&GenerateEmailOptions{Channel: ChannelMailTm})
 *   result, _ := info.GetEmails(nil)
 */
func (info *EmailInfo) GetEmails(opts *GetEmailsOptions) (*GetEmailsResult, error) {
	return GetEmails(info, opts)
}

/*
 * Client 临时邮箱客户端
 * 封装了邮箱创建和邮件获取的完整流程，自动管理邮箱信息和认证令牌
 *
 * 示例:
 *   client := NewClient()
 *   info, _ := client.Generate(&GenerateEmailOptions{Channel: ChannelMailTm})
 *   result, _ := client.GetEmails(nil)
 *   if result.Success {
 *       fmt.Println("邮件数:", len(result.Emails))
 *   }
 */
type Client struct {
	emailInfo *EmailInfo
}

/* NewClient 创建临时邮箱客户端实例 */
func NewClient() *Client {
	return &Client{}
}

/*
 * Generate 创建临时邮箱并缓存邮箱信息
 * 后续调用 GetEmails() 时自动使用此邮箱的渠道、地址和令牌
 */
func (c *Client) Generate(opts *GenerateEmailOptions) (*EmailInfo, error) {
	info, err := GenerateEmail(opts)
	if err != nil {
		return nil, err
	}
	c.emailInfo = info
	return info, nil
}

/*
 * GetEmails 获取当前邮箱的邮件列表
 * 必须先调用 Generate() 创建邮箱
 */
func (c *Client) GetEmails(opts *GetEmailsOptions) (*GetEmailsResult, error) {
	if c.emailInfo == nil {
		reportTelemetry("get_emails", "", false, 0, 0, "no email generated. Call Generate() first")
		return nil, fmt.Errorf("no email generated. Call Generate() first")
	}

	return GetEmails(c.emailInfo, opts)
}

/* GetEmailInfo 获取当前缓存的邮箱信息，未调用 Generate() 时返回 nil */
func (c *Client) GetEmailInfo() *EmailInfo {
	return c.emailInfo
}
