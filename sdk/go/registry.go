package tempemail

import "fmt"

/*
 * ChannelSpec 单个渠道的注册规格
 * 每新增一个渠道只需在注册文件里追加一处 registerChannel(ChannelSpec{...})，
 * 渠道列表（allChannels）、信息映射表（channelInfoMap）、创建/收信分发逻辑
 * 全部由该结构自动派生，无需再手动同步多处平行结构。
 */
type ChannelSpec struct {
	/* 渠道标识 */
	Channel Channel
	/* 渠道显示名称 */
	Name string
	/* 对应的临时邮箱服务网站 */
	Website string
	/* 创建邮箱的实现（对应原 generateEmailOnce 中该渠道的 case 体） */
	Generate func(opts *GenerateEmailOptions) (*EmailInfo, error)
	/* 获取邮件的实现（对应原 getEmailsOnce 中该渠道的 case 体） */
	GetEmails func(email, token string) ([]Email, error)
}

/* 有序渠道注册表，注册顺序即枚举顺序（硬约束，五端一致） */
var channelRegistry []*ChannelSpec

/* 渠道标识到注册规格的映射，便于 O(1) 查找分发 */
var channelRegistryMap = map[Channel]*ChannelSpec{}

/*
 * allChannels 所有支持的渠道标识（有序），用于随机选择和遍历。
 * 由 registerChannel 增量填充，注册顺序即枚举顺序，无需再手动维护平行列表。
 */
var allChannels []Channel

/*
 * registerChannel 注册一个渠道
 * 追加到有序切片、建立映射并同步 allChannels；
 * 重复注册同一 Channel 视为编程错误，直接 panic。
 */
func registerChannel(spec ChannelSpec) {
	if _, exists := channelRegistryMap[spec.Channel]; exists {
		panic(fmt.Sprintf("duplicate channel registration: %s", spec.Channel))
	}
	stored := spec
	channelRegistry = append(channelRegistry, &stored)
	channelRegistryMap[spec.Channel] = &stored
	allChannels = append(allChannels, spec.Channel)
}
