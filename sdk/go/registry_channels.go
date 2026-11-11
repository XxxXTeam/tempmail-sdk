package tempemail

import (
	"fmt"

	prov "github.com/XxxXTeam/tempmail-sdk/sdk/go/provider"
)

// 本文件由渠道注册元数据自动组织：每个渠道一处 registerChannel 调用，
// 注册顺序即为 allChannels 的枚举顺序（硬约束，五端一致）。
// 闭包内的 Generate / GetEmails 调用与原 dispatch switch 逐一对应，语义不变。

func init() {
	registerChannel(ChannelSpec{
		Channel: ChannelTempmail,
		Name:    "TempMail",
		Website: "tempmail.ing",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			duration := opts.Duration
			if duration <= 0 {
				duration = 30
			}
			return fromMailbox(prov.TempmailGenerate(duration))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.TempmailGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelTempmailCn,
		Name:    "TempMail CN",
		Website: "tempmail.cn",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.TempmailCNGenerate(opts.Domain))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.TempmailCNGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelTaEasy,
		Name:    "TA Easy",
		Website: "ta-easy.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.TaEasyGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for ta-easy channel")
			}
			return normEmailsResult(prov.TaEasyGetEmails(email, token))
		},
	})

	registerChannel(ChannelSpec{
		Channel: Channel10minuteOne,
		Name:    "10 Minute Email",
		Website: "10minutemail.one",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.TenminuteOneGenerate(opts.Domain))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for 10minute-one channel")
			}
			return normEmailsResult(prov.TenminuteOneGetEmails(email, token))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelXghffCom,
		Name:    "xghff.com",
		Website: "10minutemail.one",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return genTenminuteVariant("xghff.com", ChannelXghffCom)
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for %s channel", ChannelXghffCom)
			}
			return normEmailsResult(prov.TenminuteOneGetEmails(email, token))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelOqqajCom,
		Name:    "oqqaj.com",
		Website: "10minutemail.one",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return genTenminuteVariant("oqqaj.com", ChannelOqqajCom)
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for %s channel", ChannelOqqajCom)
			}
			return normEmailsResult(prov.TenminuteOneGetEmails(email, token))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelPsovvCom,
		Name:    "psovv.com",
		Website: "10minutemail.one",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return genTenminuteVariant("psovv.com", ChannelPsovvCom)
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for %s channel", ChannelPsovvCom)
			}
			return normEmailsResult(prov.TenminuteOneGetEmails(email, token))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelDbwotCom,
		Name:    "dbwot.com",
		Website: "10minutemail.one",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return genTenminuteVariant("dbwot.com", ChannelDbwotCom)
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for %s channel", ChannelDbwotCom)
			}
			return normEmailsResult(prov.TenminuteOneGetEmails(email, token))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelYgwprCom,
		Name:    "ygwpr.com",
		Website: "10minutemail.one",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return genTenminuteVariant("ygwpr.com", ChannelYgwprCom)
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for %s channel", ChannelYgwprCom)
			}
			return normEmailsResult(prov.TenminuteOneGetEmails(email, token))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelImxweCom,
		Name:    "imxwe.com",
		Website: "10minutemail.one",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return genTenminuteVariant("imxwe.com", ChannelImxweCom)
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for %s channel", ChannelImxweCom)
			}
			return normEmailsResult(prov.TenminuteOneGetEmails(email, token))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelLinshiyou,
		Name:    "临时邮",
		Website: "linshiyou.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.LinshiyouGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for linshiyou channel")
			}
			return normEmailsResult(prov.LinshiyouGetEmails(token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelTempmailLol,
		Name:    "TempMail LOL",
		Website: "tempmail.lol",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.TempmailLolGenerate(opts.Domain))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for tempmail-lol channel")
			}
			return normEmailsResult(prov.TempmailLolGetEmails(token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelChatgptOrgUk,
		Name:    "ChatGPT Mail",
		Website: "mail.chatgpt.org.uk",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.ChatgptOrgUkGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for chatgpt-org-uk channel")
			}
			return normEmailsResult(prov.ChatgptOrgUkGetEmails(email, token))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelTempMailIo,
		Name:    "Temp-Mail.io",
		Website: "temp-mail.io",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.TempMailIoGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.TempMailIoGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelMailCx,
		Name:    "Mail.cx",
		Website: "mail.cx",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.MailCxGenerate(opts.Domain))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for mail-cx channel")
			}
			return normEmailsResult(prov.MailCxGetEmails(token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelDdkerCom,
		Name:    "ddker.com",
		Website: "mail.cx",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			info, err := fromMailbox(prov.MailCxGenerate(fixedDomain("ddker.com")))
			if err != nil {
				return nil, err
			}
			info.Channel = ChannelDdkerCom
			return info, nil
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for ddker-com channel")
			}
			return normEmailsResult(prov.MailCxGetEmails(token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelCatchmail,
		Name:    "Catchmail",
		Website: "catchmail.io",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.CatchmailGenerate(opts.Domain))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.CatchmailGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelCatchmailMailistry,
		Name:    "Catchmail Mailistry",
		Website: "mailistry.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.CatchmailGenerate(fixedDomain("mailistry.com"), string(ChannelCatchmailMailistry)))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.CatchmailGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelCatchmailZeppost,
		Name:    "Catchmail Zeppost",
		Website: "zeppost.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.CatchmailGenerate(fixedDomain("zeppost.com"), string(ChannelCatchmailZeppost)))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.CatchmailGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelMailforspam,
		Name:    "MailForSpam",
		Website: "mailforspam.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.MailforspamGenerate(opts.Domain))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.MailforspamGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelMailforspamTempmailIo,
		Name:    "MailForSpam TempMail.io",
		Website: "tempmail.io",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.MailforspamGenerate(fixedDomain("tempmail.io"), string(ChannelMailforspamTempmailIo)))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.MailforspamGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelMailforspamDisposable,
		Name:    "MailForSpam Disposable",
		Website: "disposable.email",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.MailforspamGenerate(fixedDomain("disposable.email"), string(ChannelMailforspamDisposable)))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.MailforspamGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelTempmailc,
		Name:    "TempMailC",
		Website: "tempmailc.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.TempmailcGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.TempmailcGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelMailnesia,
		Name:    "Mailnesia",
		Website: "mailnesia.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.MailnesiaGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.MailnesiaGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelMailticking,
		Name:    "MailTicking",
		Website: "mailticking.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.MailtickingGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for mailticking channel")
			}
			return normEmailsResult(prov.MailtickingGetEmails(email, token))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelThrowawaymail,
		Name:    "ThrowawayMail",
		Website: "throwawaymail.app",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.ThrowawaymailGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for throwawaymail channel")
			}
			return normEmailsResult(prov.ThrowawaymailGetEmails(token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelTempmailFish,
		Name:    "TempMail Fish",
		Website: "tempmail.fish",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.TempmailFishGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for tempmail-fish channel")
			}
			return normEmailsResult(prov.TempmailFishGetEmails(token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelNeighboursSh,
		Name:    "Neighbours",
		Website: "neighbours.sh",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.NeighboursShGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.NeighboursShGetEmails(token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelShittyEmail,
		Name:    "shitty.email",
		Website: "shitty.email",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.ShittyEmailGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for shitty-email channel")
			}
			return normEmailsResult(prov.ShittyEmailGetEmails(token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelTempmailpro,
		Name:    "TempMail Pro",
		Website: "tempmailpro.us",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.TempmailproGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for tempmailpro channel")
			}
			return normEmailsResult(prov.TempmailproGetEmails(token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelDevmailUk,
		Name:    "DevMail UK",
		Website: "devmail.uk",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.DevmailUkGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.DevmailUkGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelInboxkitten,
		Name:    "InboxKitten",
		Website: "inboxkitten.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.InboxkittenGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.InboxkittenGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelCleanTempMail,
		Name:    "CleanTempMail",
		Website: "cleantempmail.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.CleanTempMailGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.CleanTempMailGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelGetnada,
		Name:    "GetNada",
		Website: "getnada.net",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.GetnadaGenerate(opts.Domain))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for getnada channel")
			}
			return normEmailsResult(prov.GetnadaGetEmails(token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelOneVpnNet,
		Name:    "1vpn.net",
		Website: "getnada.net",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.GetnadaGenerate(fixedDomain("1vpn.net"), string(ChannelOneVpnNet)))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for %s channel", ChannelOneVpnNet)
			}
			return normEmailsResult(prov.GetnadaGetEmails(token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelAbematvCom,
		Name:    "abematv.com",
		Website: "getnada.net",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.GetnadaGenerate(fixedDomain("abematv.com"), string(ChannelAbematvCom)))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for %s channel", ChannelAbematvCom)
			}
			return normEmailsResult(prov.GetnadaGetEmails(token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelAbematvNet,
		Name:    "abematv.net",
		Website: "getnada.net",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.GetnadaGenerate(fixedDomain("abematv.net"), string(ChannelAbematvNet)))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for %s channel", ChannelAbematvNet)
			}
			return normEmailsResult(prov.GetnadaGetEmails(token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelAbematvOrg,
		Name:    "abematv.org",
		Website: "getnada.net",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.GetnadaGenerate(fixedDomain("abematv.org"), string(ChannelAbematvOrg)))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for %s channel", ChannelAbematvOrg)
			}
			return normEmailsResult(prov.GetnadaGetEmails(token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelAcehCc,
		Name:    "aceh.cc",
		Website: "getnada.net",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.GetnadaGenerate(fixedDomain("aceh.cc"), string(ChannelAcehCc)))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for %s channel", ChannelAcehCc)
			}
			return normEmailsResult(prov.GetnadaGetEmails(token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelBangkabelitungNet,
		Name:    "bangkabelitung.net",
		Website: "getnada.net",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.GetnadaGenerate(fixedDomain("bangkabelitung.net"), string(ChannelBangkabelitungNet)))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for %s channel", ChannelBangkabelitungNet)
			}
			return normEmailsResult(prov.GetnadaGetEmails(token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelCctruyenCom,
		Name:    "cctruyen.com",
		Website: "getnada.net",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.GetnadaGenerate(fixedDomain("cctruyen.com"), string(ChannelCctruyenCom)))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for %s channel", ChannelCctruyenCom)
			}
			return normEmailsResult(prov.GetnadaGetEmails(token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelGetnadaCom,
		Name:    "getnada.com",
		Website: "getnada.net",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.GetnadaGenerate(fixedDomain("getnada.com"), string(ChannelGetnadaCom)))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for %s channel", ChannelGetnadaCom)
			}
			return normEmailsResult(prov.GetnadaGetEmails(token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelGetnadaEmail,
		Name:    "getnada.email",
		Website: "getnada.net",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.GetnadaGenerate(fixedDomain("getnada.email"), string(ChannelGetnadaEmail)))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for %s channel", ChannelGetnadaEmail)
			}
			return normEmailsResult(prov.GetnadaGetEmails(token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelGetnadaNet,
		Name:    "getnada.net",
		Website: "getnada.net",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.GetnadaGenerate(fixedDomain("getnada.net"), string(ChannelGetnadaNet)))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for %s channel", ChannelGetnadaNet)
			}
			return normEmailsResult(prov.GetnadaGetEmails(token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelJawatengahNet,
		Name:    "jawatengah.net",
		Website: "getnada.net",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.GetnadaGenerate(fixedDomain("jawatengah.net"), string(ChannelJawatengahNet)))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for %s channel", ChannelJawatengahNet)
			}
			return normEmailsResult(prov.GetnadaGetEmails(token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelJawatimurNet,
		Name:    "jawatimur.net",
		Website: "getnada.net",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.GetnadaGenerate(fixedDomain("jawatimur.net"), string(ChannelJawatimurNet)))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for %s channel", ChannelJawatimurNet)
			}
			return normEmailsResult(prov.GetnadaGetEmails(token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelKalimantantimurNet,
		Name:    "kalimantantimur.net",
		Website: "getnada.net",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.GetnadaGenerate(fixedDomain("kalimantantimur.net"), string(ChannelKalimantantimurNet)))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for %s channel", ChannelKalimantantimurNet)
			}
			return normEmailsResult(prov.GetnadaGetEmails(token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelLuxury345Com,
		Name:    "luxury345.com",
		Website: "getnada.net",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.GetnadaGenerate(fixedDomain("luxury345.com"), string(ChannelLuxury345Com)))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for %s channel", ChannelLuxury345Com)
			}
			return normEmailsResult(prov.GetnadaGetEmails(token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelPeholCom,
		Name:    "pehol.com",
		Website: "getnada.net",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.GetnadaGenerate(fixedDomain("pehol.com"), string(ChannelPeholCom)))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for %s channel", ChannelPeholCom)
			}
			return normEmailsResult(prov.GetnadaGetEmails(token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelPtruyenCom,
		Name:    "ptruyen.com",
		Website: "getnada.net",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.GetnadaGenerate(fixedDomain("ptruyen.com"), string(ChannelPtruyenCom)))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for %s channel", ChannelPtruyenCom)
			}
			return normEmailsResult(prov.GetnadaGetEmails(token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelRiauNet,
		Name:    "riau.net",
		Website: "getnada.net",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.GetnadaGenerate(fixedDomain("riau.net"), string(ChannelRiauNet)))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for %s channel", ChannelRiauNet)
			}
			return normEmailsResult(prov.GetnadaGetEmails(token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelSeokeyOrg,
		Name:    "seokey.org",
		Website: "getnada.net",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.GetnadaGenerate(fixedDomain("seokey.org"), string(ChannelSeokeyOrg)))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for %s channel", ChannelSeokeyOrg)
			}
			return normEmailsResult(prov.GetnadaGetEmails(token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelSulawesiselatanNet,
		Name:    "sulawesiselatan.net",
		Website: "getnada.net",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.GetnadaGenerate(fixedDomain("sulawesiselatan.net"), string(ChannelSulawesiselatanNet)))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for %s channel", ChannelSulawesiselatanNet)
			}
			return normEmailsResult(prov.GetnadaGetEmails(token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelSumaterabaratNet,
		Name:    "sumaterabarat.net",
		Website: "getnada.net",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.GetnadaGenerate(fixedDomain("sumaterabarat.net"), string(ChannelSumaterabaratNet)))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for %s channel", ChannelSumaterabaratNet)
			}
			return normEmailsResult(prov.GetnadaGetEmails(token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelSumaterautaraNet,
		Name:    "sumaterautara.net",
		Website: "getnada.net",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.GetnadaGenerate(fixedDomain("sumaterautara.net"), string(ChannelSumaterautaraNet)))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for %s channel", ChannelSumaterautaraNet)
			}
			return normEmailsResult(prov.GetnadaGetEmails(token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelVillatogelCom,
		Name:    "villatogel.com",
		Website: "getnada.net",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.GetnadaGenerate(fixedDomain("villatogel.com"), string(ChannelVillatogelCom)))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for %s channel", ChannelVillatogelCom)
			}
			return normEmailsResult(prov.GetnadaGetEmails(token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelMail123,
		Name:    "Mail123",
		Website: "mail123.fr",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.Mail123Generate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.Mail123GetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelMail10s,
		Name:    "Mail10s",
		Website: "mail10s.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.Mail10sGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.Mail10sGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelTempfastmail,
		Name:    "TempFastMail",
		Website: "tempfastmail.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.TempfastmailGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for tempfastmail channel")
			}
			return normEmailsResult(prov.TempfastmailGetEmails(token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelOneSecMail,
		Name:    "1SecMail",
		Website: "1sec-mail.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.OneSecMailGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for 1sec-mail channel")
			}
			return normEmailsResult(prov.OneSecMailGetEmails(token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelFakemail,
		Name:    "FakeMail",
		Website: "fakemail.net",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.FakemailGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for fakemail channel")
			}
			return normEmailsResult(prov.FakemailGetEmails(token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelOpeninbox,
		Name:    "OpenInbox",
		Website: "openinbox.io",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.OpeninboxGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for openinbox channel")
			}
			return normEmailsResult(prov.OpeninboxGetEmails(token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelInboxes,
		Name:    "Inboxes",
		Website: "inboxes.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.InboxesGenerate(opts.Domain))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.InboxesGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelUncorreotemporal,
		Name:    "UnCorreoTemporal",
		Website: "uncorreotemporal.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.UncorreotemporalGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for uncorreotemporal channel")
			}
			return normEmailsResult(prov.UncorreotemporalGetEmails(token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelAwamail,
		Name:    "AwaMail",
		Website: "awamail.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.AwamailGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for awamail channel")
			}
			return normEmailsResult(prov.AwamailGetEmails(token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelMailTm,
		Name:    "Mail.tm",
		Website: "mail.tm",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.MailTmGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for mail-tm channel")
			}
			return normEmailsResult(prov.MailTmGetEmails(token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelWebLibraryNet,
		Name:    "web-library.net",
		Website: "mail.tm",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			info, err := fromMailbox(prov.MailTmGenerate())
			if err != nil {
				return nil, err
			}
			info.Channel = ChannelWebLibraryNet
			return info, nil
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for web-library-net channel")
			}
			return normEmailsResult(prov.MailTmGetEmails(token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelDropmail,
		Name:    "DropMail",
		Website: "dropmail.me",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.DropmailGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for dropmail channel")
			}
			return normEmailsResult(prov.DropmailGetEmails(token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelGuerrillaMail,
		Name:    "Guerrilla Mail",
		Website: "guerrillamail.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.GuerrillaMailGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for guerrillamail channel")
			}
			return normEmailsResult(prov.GuerrillaMailGetEmails(token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelGuerrillamailCom,
		Name:    "GuerrillaMail Root",
		Website: "guerrillamail.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.GuerrillamailMirrorGenerate("guerrillamail-com", "https://guerrillamail.com/ajax.php"))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for guerrillamail-com channel")
			}
			return normEmailsResult(prov.GuerrillamailMirrorGetEmails("https://guerrillamail.com/ajax.php", token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelMaildrop,
		Name:    "Maildrop",
		Website: "maildrop.cx",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.MaildropGenerate(opts.Domain))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for maildrop channel")
			}
			return normEmailsResult(prov.MaildropGetEmails(token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelSmailPw,
		Name:    "Smail.pw",
		Website: "smail.pw",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.SmailPwGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for smail-pw channel")
			}
			return normEmailsResult(prov.SmailPwGetEmails(token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelVip215,
		Name:    "VIP 215",
		Website: "vip.215.im",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.MailVip215Generate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for vip-215 channel")
			}
			return normEmailsResult(prov.MailVip215GetEmails(token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelFakeLegal,
		Name:    "Fake Legal",
		Website: "fake.legal",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.FakeLegalGenerate(opts.Domain))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.FakeLegalGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelImguiDe,
		Name:    "imgui.de",
		Website: "fake.legal",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.FakeLegalGenerate(fixedDomain("imgui.de"), string(ChannelImguiDe)))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.FakeLegalGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelPulsewebmenuDe,
		Name:    "pulsewebmenu.de",
		Website: "fake.legal",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.FakeLegalGenerate(fixedDomain("pulsewebmenu.de"), string(ChannelPulsewebmenuDe)))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.FakeLegalGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelMoakt,
		Name:    "Moakt",
		Website: "moakt.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.MoaktGenerate(opts.Domain))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for moakt channel")
			}
			return normEmailsResult(prov.MoaktGetEmails(email, token))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelDrmailIn,
		Name:    "drmail.in",
		Website: "moakt.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return genMoaktVariant("drmail.in", ChannelDrmailIn)
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for %s channel", ChannelDrmailIn)
			}
			return normEmailsResult(prov.MoaktGetEmails(email, token))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelTemlNet,
		Name:    "teml.net",
		Website: "moakt.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return genMoaktVariant("teml.net", ChannelTemlNet)
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for %s channel", ChannelTemlNet)
			}
			return normEmailsResult(prov.MoaktGetEmails(email, token))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelTmpemlCom,
		Name:    "tmpeml.com",
		Website: "moakt.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return genMoaktVariant("tmpeml.com", ChannelTmpemlCom)
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for %s channel", ChannelTmpemlCom)
			}
			return normEmailsResult(prov.MoaktGetEmails(email, token))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelTmpboxNet,
		Name:    "tmpbox.net",
		Website: "moakt.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return genMoaktVariant("tmpbox.net", ChannelTmpboxNet)
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for %s channel", ChannelTmpboxNet)
			}
			return normEmailsResult(prov.MoaktGetEmails(email, token))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelMoaktCc,
		Name:    "moakt.cc",
		Website: "moakt.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return genMoaktVariant("moakt.cc", ChannelMoaktCc)
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for %s channel", ChannelMoaktCc)
			}
			return normEmailsResult(prov.MoaktGetEmails(email, token))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelDisboxNet,
		Name:    "disbox.net",
		Website: "moakt.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return genMoaktVariant("disbox.net", ChannelDisboxNet)
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for %s channel", ChannelDisboxNet)
			}
			return normEmailsResult(prov.MoaktGetEmails(email, token))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelTmpmailOrg,
		Name:    "tmpmail.org",
		Website: "moakt.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return genMoaktVariant("tmpmail.org", ChannelTmpmailOrg)
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for %s channel", ChannelTmpmailOrg)
			}
			return normEmailsResult(prov.MoaktGetEmails(email, token))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelTmpmailNet,
		Name:    "tmpmail.net",
		Website: "moakt.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return genMoaktVariant("tmpmail.net", ChannelTmpmailNet)
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for %s channel", ChannelTmpmailNet)
			}
			return normEmailsResult(prov.MoaktGetEmails(email, token))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelTmailsNet,
		Name:    "tmails.net",
		Website: "moakt.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return genMoaktVariant("tmails.net", ChannelTmailsNet)
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for %s channel", ChannelTmailsNet)
			}
			return normEmailsResult(prov.MoaktGetEmails(email, token))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelDisboxOrg,
		Name:    "disbox.org",
		Website: "moakt.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return genMoaktVariant("disbox.org", ChannelDisboxOrg)
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for %s channel", ChannelDisboxOrg)
			}
			return normEmailsResult(prov.MoaktGetEmails(email, token))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelMoaktCo,
		Name:    "moakt.co",
		Website: "moakt.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return genMoaktVariant("moakt.co", ChannelMoaktCo)
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for %s channel", ChannelMoaktCo)
			}
			return normEmailsResult(prov.MoaktGetEmails(email, token))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelMoaktWs,
		Name:    "moakt.ws",
		Website: "moakt.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return genMoaktVariant("moakt.ws", ChannelMoaktWs)
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for %s channel", ChannelMoaktWs)
			}
			return normEmailsResult(prov.MoaktGetEmails(email, token))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelTmailWs,
		Name:    "tmail.ws",
		Website: "moakt.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return genMoaktVariant("tmail.ws", ChannelTmailWs)
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for %s channel", ChannelTmailWs)
			}
			return normEmailsResult(prov.MoaktGetEmails(email, token))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelBareedWs,
		Name:    "bareed.ws",
		Website: "moakt.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return genMoaktVariant("bareed.ws", ChannelBareedWs)
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for %s channel", ChannelBareedWs)
			}
			return normEmailsResult(prov.MoaktGetEmails(email, token))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelEmail10min,
		Name:    "Email10Min",
		Website: "email10min.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.Email10minGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for email10min channel")
			}
			return normEmailsResult(prov.Email10minGetEmails(email, token))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelLinshiCo,
		Name:    "临时Co",
		Website: "linshi.co",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.LinshiCoGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.LinshiCoGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelHarakirimail,
		Name:    "HarakiriMail",
		Website: "harakirimail.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.HarakirimailGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.HarakirimailGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelJqkjqkXyz,
		Name:    "jqkjqk.xyz",
		Website: "mail.zhujump.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.ZhujumpGenerate("jqkjqk.xyz", string(ChannelJqkjqkXyz)))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for %s channel", ChannelJqkjqkXyz)
			}
			return normEmailsResult(prov.ZhujumpGetEmails(email, token))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelLyhleviCom,
		Name:    "LyhLevi MoeMail",
		Website: "lyhlevi.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.MoemailGenerate("https://lyhlevi.com", "lyhlevi.com", string(ChannelLyhleviCom), 24*60*60*1000))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for %s channel", ChannelLyhleviCom)
			}
			return normEmailsResult(prov.ZhujumpGetEmails(email, token))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelTempmailPlus,
		Name:    "TempMail Plus",
		Website: "tempmail.plus",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.TempmailPlusGenerate(opts.Domain))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.TempmailPlusGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelFexpostCom,
		Name:    "fexpost.com",
		Website: "tempmail.plus",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.TempmailPlusGenerate(fixedDomain("fexpost.com"), string(ChannelFexpostCom)))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.TempmailPlusGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelFexboxOrg,
		Name:    "fexbox.org",
		Website: "tempmail.plus",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.TempmailPlusGenerate(fixedDomain("fexbox.org"), string(ChannelFexboxOrg)))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.TempmailPlusGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelMailboxInUa,
		Name:    "mailbox.in.ua",
		Website: "tempmail.plus",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.TempmailPlusGenerate(fixedDomain("mailbox.in.ua"), string(ChannelMailboxInUa)))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.TempmailPlusGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelRoverInfo,
		Name:    "rover.info",
		Website: "tempmail.plus",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.TempmailPlusGenerate(fixedDomain("rover.info"), string(ChannelRoverInfo)))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.TempmailPlusGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelChitthiIn,
		Name:    "chitthi.in",
		Website: "tempmail.plus",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.TempmailPlusGenerate(fixedDomain("chitthi.in"), string(ChannelChitthiIn)))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.TempmailPlusGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelFextempCom,
		Name:    "fextemp.com",
		Website: "tempmail.plus",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.TempmailPlusGenerate(fixedDomain("fextemp.com"), string(ChannelFextempCom)))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.TempmailPlusGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelAnyPink,
		Name:    "any.pink",
		Website: "tempmail.plus",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.TempmailPlusGenerate(fixedDomain("any.pink"), string(ChannelAnyPink)))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.TempmailPlusGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelMerepostCom,
		Name:    "merepost.com",
		Website: "tempmail.plus",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.TempmailPlusGenerate(fixedDomain("merepost.com"), string(ChannelMerepostCom)))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.TempmailPlusGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelTempmailLolV2,
		Name:    "TempMail LOL V2",
		Website: "tempmail.lol",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.TempmailLolV2Generate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for tempmail-lol-v2 channel")
			}
			return normEmailsResult(prov.TempmailLolV2GetEmails(token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelTempgbox,
		Name:    "TempGBox",
		Website: "tempgbox.net",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.TempgboxGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for tempgbox channel")
			}
			return normEmailsResult(prov.TempgboxGetEmails(token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelEmailnator,
		Name:    "Emailnator",
		Website: "emailnator.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.EmailnatorGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for emailnator channel")
			}
			return normEmailsResult(prov.EmailnatorGetEmails(token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelTemporam,
		Name:    "Temporam",
		Website: "temporam.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.TemporamGenerate(opts.Domain))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.TemporamGetEmails(token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelNeighbours,
		Name:    "Neighbours",
		Website: "neighbours.sh",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.NeighboursGenerate(opts.Domain))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.NeighboursGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelSharklasers,
		Name:    "SharkLasers",
		Website: "sharklasers.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.GuerrillamailMirrorGenerate("sharklasers", "https://www.sharklasers.com/ajax.php"))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for sharklasers channel")
			}
			return normEmailsResult(prov.GuerrillamailMirrorGetEmails("https://www.sharklasers.com/ajax.php", token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelSharklasersCom,
		Name:    "SharkLasers Root",
		Website: "sharklasers.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.GuerrillamailMirrorGenerate("sharklasers-com", "https://sharklasers.com/ajax.php"))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for sharklasers-com channel")
			}
			return normEmailsResult(prov.GuerrillamailMirrorGetEmails("https://sharklasers.com/ajax.php", token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelGrrLa,
		Name:    "Grr.la",
		Website: "grr.la",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.GuerrillamailMirrorGenerate("grr-la", "https://www.grr.la/ajax.php"))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for grr-la channel")
			}
			return normEmailsResult(prov.GuerrillamailMirrorGetEmails("https://www.grr.la/ajax.php", token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelGrrLaCom,
		Name:    "Grr.la Root",
		Website: "grr.la",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.GuerrillamailMirrorGenerate("grr-la-com", "https://grr.la/ajax.php"))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for grr-la-com channel")
			}
			return normEmailsResult(prov.GuerrillamailMirrorGetEmails("https://grr.la/ajax.php", token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelGuerrillamailInfo,
		Name:    "GuerrillaMail Info",
		Website: "guerrillamail.info",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.GuerrillamailMirrorGenerate("guerrillamail-info", "https://www.guerrillamail.info/ajax.php"))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for guerrillamail-info channel")
			}
			return normEmailsResult(prov.GuerrillamailMirrorGetEmails("https://www.guerrillamail.info/ajax.php", token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelSpam4me,
		Name:    "Spam4.me",
		Website: "spam4.me",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.GuerrillamailMirrorGenerate("spam4me", "https://www.spam4.me/ajax.php"))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for spam4me channel")
			}
			return normEmailsResult(prov.GuerrillamailMirrorGetEmails("https://www.spam4.me/ajax.php", token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelGuerrillamailNet,
		Name:    "GuerrillaMail Net",
		Website: "guerrillamail.net",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.GuerrillamailMirrorGenerate("guerrillamail-net", "https://www.guerrillamail.net/ajax.php"))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for guerrillamail-net channel")
			}
			return normEmailsResult(prov.GuerrillamailMirrorGetEmails("https://www.guerrillamail.net/ajax.php", token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelGuerrillamailOrg,
		Name:    "GuerrillaMail Org",
		Website: "guerrillamail.org",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.GuerrillamailMirrorGenerate("guerrillamail-org", "https://www.guerrillamail.org/ajax.php"))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for guerrillamail-org channel")
			}
			return normEmailsResult(prov.GuerrillamailMirrorGetEmails("https://www.guerrillamail.org/ajax.php", token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelGuerrillamailBlock,
		Name:    "GuerrillaMailBlock",
		Website: "guerrillamailblock.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.GuerrillamailMirrorGenerate("guerrillamailblock", "https://www.guerrillamailblock.com/ajax.php"))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for guerrillamailblock channel")
			}
			return normEmailsResult(prov.GuerrillamailMirrorGetEmails("https://www.guerrillamailblock.com/ajax.php", token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelGuerrillamailComWww,
		Name:    "GuerrillaMail WWW",
		Website: "guerrillamail.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.GuerrillamailMirrorGenerate("guerrillamail-com-www", "https://www.guerrillamail.com/ajax.php"))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for guerrillamail-com-www channel")
			}
			return normEmailsResult(prov.GuerrillamailMirrorGetEmails("https://www.guerrillamail.com/ajax.php", token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelTempyEmail,
		Name:    "Tempy Email",
		Website: "tempy.email",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.TempyEmailGenerate(opts.Domain))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.TempyEmailGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelFmail,
		Name:    "FMail",
		Website: "fmail.men",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.FmailGenerate(opts.Domain))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.FmailGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelOckito,
		Name:    "Ockito",
		Website: "ockito.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.OckitoGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for ockito channel")
			}
			return normEmailsResult(prov.OckitoGetEmails(token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelAnonbox,
		Name:    "Anonbox",
		Website: "anonbox.net",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.AnonboxGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for anonbox channel")
			}
			return normEmailsResult(prov.AnonboxGetEmails(token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelDuckmail,
		Name:    "DuckMail",
		Website: "duckmail.sbs",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.DuckmailGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for duckmail channel")
			}
			return normEmailsResult(prov.DuckmailGetEmails(token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelMailinator,
		Name:    "Mailinator",
		Website: "mailinator.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.MailinatorGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.MailinatorGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelTempmail365,
		Name:    "Tempmail365",
		Website: "https://tempmail365.cn",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.Tempmail365Generate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.Tempmail365GetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelTempinbox,
		Name:    "TempInbox",
		Website: "https://www.tempinbox.xyz",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.TempinboxGenerate(opts.Domain))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.TempinboxGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelByom,
		Name:    "Byom",
		Website: "byom.de",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.ByomGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.ByomGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelAnonymmail,
		Name:    "AnonymMail",
		Website: "anonymmail.net",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.AnonymmailGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.AnonymmailGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelEyepaste,
		Name:    "EyePaste",
		Website: "eyepaste.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.EyepasteGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.EyepasteGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelMailSunls,
		Name:    "Mail Sunls",
		Website: "mail.sunls.de",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.MailSunlsGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.MailSunlsGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelExpressinboxhub,
		Name:    "ExpressInboxHub",
		Website: "expressinboxhub.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.ExpressinboxhubGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for expressinboxhub channel")
			}
			return normEmailsResult(prov.ExpressinboxhubGetEmails(email, token))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelLroid,
		Name:    "Lroid",
		Website: "lroid.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.LroidGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for lroid channel")
			}
			return normEmailsResult(prov.LroidGetEmails(token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelHaribu,
		Name:    "Haribu",
		Website: "haribu.net",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.HaribuGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for haribu channel")
			}
			return normEmailsResult(prov.HaribuGetEmails(email, token))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelRootsh,
		Name:    "Rootsh(BccTo)",
		Website: "rootsh.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.RootshGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for rootsh channel")
			}
			return normEmailsResult(prov.RootshGetEmails(email, token))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelFakeEmailSite,
		Name:    "FakeEmailSite",
		Website: "fake-email.site",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.FakeEmailSiteGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.FakeEmailSiteGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelMailgolem,
		Name:    "MailGolem",
		Website: "mailgolem.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.MailgolemGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for mailgolem channel")
			}
			return normEmailsResult(prov.MailgolemGetEmails(email, token))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelBestTempMail,
		Name:    "BestTempMail",
		Website: "best-temp-mail.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.BestTempMailGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for best-temp-mail channel")
			}
			return normEmailsResult(prov.BestTempMailGetEmails(email, token))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelDisposablemailApp,
		Name:    "DisposableMail",
		Website: "disposablemail.app",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.DisposablemailAppGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for disposablemail-app channel")
			}
			return normEmailsResult(prov.DisposablemailAppGetEmails(email, token))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelMailtempCc,
		Name:    "MailTemp.cc",
		Website: "mailtemp.cc",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.MailtempCcGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for mailtemp-cc channel")
			}
			return normEmailsResult(prov.MailtempCcGetEmails(email, token))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelMinuteinbox,
		Name:    "MinuteInbox",
		Website: "minuteinbox.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.MinuteinboxGenerate(opts.Duration, ""))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for minuteinbox channel")
			}
			return normEmailsResult(prov.MinuteinboxGetEmails(token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelMailcatch,
		Name:    "MailCatch",
		Website: "mailcatch.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.MailcatchGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.MailcatchGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelTempemailCo,
		Name:    "TempEmail.co",
		Website: "tempemail.co",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.TempemailCoGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for tempemail-co channel")
			}
			return normEmailsResult(prov.TempemailCoGetEmails(email, token))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelAltmails,
		Name:    "AltMails",
		Website: "tempmail.altmails.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.AltmailsGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for altmails channel")
			}
			return normEmailsResult(prov.AltmailsGetEmails(email, token))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelTempemailInfo,
		Name:    "TempEmailInfo",
		Website: "tempemail.info",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.TempemailInfoGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for tempemail-info channel")
			}
			return normEmailsResult(prov.TempemailInfoGetEmails(email, token))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelSmailpro,
		Name:    "SmailPro",
		Website: "smailpro.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.SmailproGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.SmailproGetEmails(email, token))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelTempmailten,
		Name:    "TempMailTen",
		Website: "tempmailten.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.TempmailtenGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for tempmailten channel")
			}
			return normEmailsResult(prov.TempmailtenGetEmails(email, token))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelMaildropCc,
		Name:    "MailDrop.cc",
		Website: "maildrop.cc",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.MaildropCcGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.MaildropCcGetEmails(email, token))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelTenminutemailNet,
		Name:    "10MinuteMail.net",
		Website: "10minutemail.net",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.TenminutemailNetGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for 10minutemail-net channel")
			}
			return normEmailsResult(prov.TenminutemailNetGetEmails(email, token))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelLinshiyouxiangNet,
		Name:    "临时邮箱(linshiyouxiang)",
		Website: "linshiyouxiang.net",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.LinshiyouxiangNetGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for linshiyouxiang-net channel")
			}
			return normEmailsResult(prov.LinshiyouxiangNetGetEmails(email, token))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelTempMailFyi,
		Name:    "Temp-Mail.fyi",
		Website: "temp-mail.fyi",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.TempMailFyiGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for tempmail-fyi channel")
			}
			return normEmailsResult(prov.TempMailFyiGetEmails(email, token))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelDisposablemailCom,
		Name:    "DisposableMail.com",
		Website: "disposablemail.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.DisposablemailGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for disposablemail-com channel")
			}
			return normEmailsResult(prov.DisposablemailGetEmails(email, token))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelTemppMails,
		Name:    "TemppMails",
		Website: "tempp-mails.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.TemppMailsGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for tempp-mails channel")
			}
			return normEmailsResult(prov.TemppMailsGetEmails(email, token))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelEmailtempOrg,
		Name:    "EmailTemp",
		Website: "emailtemp.org",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.EmailtempOrgGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for emailtemp-org channel")
			}
			return normEmailsResult(prov.EmailtempOrgGetEmails(email, token))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelMytempmailCc,
		Name:    "MyTempMail.cc",
		Website: "mytempmail.cc",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.MytempmailCcGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for mytempmail-cc channel")
			}
			return normEmailsResult(prov.MytempmailCcGetEmails(token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelMailTd,
		Name:    "Mail.td",
		Website: "mail.td",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.MailTdGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for mail-td channel")
			}
			return normEmailsResult(prov.MailTdGetEmails(token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelMailholeDe,
		Name:    "Mailhole.de",
		Website: "mailhole.de",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.MailholeDeGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for mailhole-de channel")
			}
			return normEmailsResult(prov.MailholeDeGetEmails(token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelTmailLink,
		Name:    "TMail.link",
		Website: "tmail.link",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.TmailLinkGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for tmail-link channel")
			}
			return normEmailsResult(prov.TmailLinkGetEmails(token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: Channel24mailChacuo,
		Name:    "24Mail Chacuo",
		Website: "24mail.chacuo.net",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.TwentyfourmailChacuoGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.TwentyfourmailChacuoGetEmails(token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelNimail,
		Name:    "NiMail",
		Website: "nimail.cn",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.NimailGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.NimailGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelFreecustom,
		Name:    "FreeCustom.Email",
		Website: "freecustom.email",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.FreecustomGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.FreecustomGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelN17666688Shop,
		Name:    "Mailmomy (17666688.shop)",
		Website: "mailmomy.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.N17666688ShopGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.N17666688ShopGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelN282mailCom,
		Name:    "Mailmomy (282mail.com)",
		Website: "mailmomy.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.N282mailComGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.N282mailComGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelBlackholeDjurbySe,
		Name:    "Mailinator (blackhole.djurby.se)",
		Website: "mailinator.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.BlackholeDjurbySeGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.BlackholeDjurbySeGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelBlockBdeaCc,
		Name:    "Mailinator (block.bdea.cc)",
		Website: "mailinator.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.BlockBdeaCcGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.BlockBdeaCcGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelBsdu32Buzz,
		Name:    "Mailmomy (bsdu32.buzz)",
		Website: "mailmomy.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.Bsdu32BuzzGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.Bsdu32BuzzGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelBSmellyCc,
		Name:    "Mailinator (b.smelly.cc)",
		Website: "mailinator.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.BSmellyCcGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.BSmellyCcGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelCarlton183ChangeipNet,
		Name:    "Mailinator (183carlton.changeip.net)",
		Website: "mailinator.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.Carlton183ChangeipNetGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.Carlton183ChangeipNetGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelDeaSoonIt,
		Name:    "Mailinator (dea.soon.it)",
		Website: "mailinator.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.DeaSoonItGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.DeaSoonItGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelDisposableAlSudaniCom,
		Name:    "Mailinator (disposable.al-sudani.com)",
		Website: "mailinator.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.DisposableAlSudaniComGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.DisposableAlSudaniComGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelDisposableNogonadNl,
		Name:    "Mailinator (disposable.nogonad.nl)",
		Website: "mailinator.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.DisposableNogonadNlGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.DisposableNogonadNlGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelDoxu243Buzz,
		Name:    "Mailmomy (doxu243.buzz)",
		Website: "mailmomy.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.Doxu243BuzzGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.Doxu243BuzzGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelEasymePro,
		Name:    "Mailmomy (easyme.pro)",
		Website: "mailmomy.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.EasymeProGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.EasymeProGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelEbsComAr,
		Name:    "Mailinator (ebs.com.ar)",
		Website: "mailinator.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.EbsComArGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.EbsComArGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelEtgdevDe,
		Name:    "Mailinator (etgdev.de)",
		Website: "mailinator.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.EtgdevDeGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.EtgdevDeGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelFwd2mEszettEs,
		Name:    "Mailinator (fwd2m.eszett.es)",
		Website: "mailinator.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.Fwd2mEszettEsGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.Fwd2mEszettEsGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelJamaTrenetEu,
		Name:    "Mailinator (jama.trenet.eu)",
		Website: "mailinator.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.JamaTrenetEuGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.JamaTrenetEuGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelJFairuseOrg,
		Name:    "Mailinator (j.fairuse.org)",
		Website: "mailinator.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.JFairuseOrgGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.JFairuseOrgGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelLayuemingPics,
		Name:    "Mailmomy (layueming.pics)",
		Website: "mailmomy.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.LayuemingPicsGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.LayuemingPicsGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelM887At,
		Name:    "Mailinator (m.887.at)",
		Website: "mailinator.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.M887AtGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.M887AtGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelM8rDavidfuhrDe,
		Name:    "Mailinator (m8r.davidfuhr.de)",
		Website: "mailinator.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.M8rDavidfuhrDeGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.M8rDavidfuhrDeGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelM8rMcasalCom,
		Name:    "Mailinator (m8r.mcasal.com)",
		Website: "mailinator.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.M8rMcasalComGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.M8rMcasalComGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelMailBentraskCom,
		Name:    "Mailinator (mail.bentrask.com)",
		Website: "mailinator.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.MailBentraskComGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.MailBentraskComGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelMailFsmashOrg,
		Name:    "Mailinator (mail.fsmash.org)",
		Website: "mailinator.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.MailFsmashOrgGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.MailFsmashOrgGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelMailinatorzzmoooCom,
		Name:    "Mailinator (mailinatorzz.mooo.com)",
		Website: "mailinator.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.MailinatorzzmoooComGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.MailinatorzzmoooComGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelMiMeonBe,
		Name:    "Mailinator (mi.meon.be)",
		Website: "mailinator.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.MiMeonBeGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.MiMeonBeGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelMingyuekejiOnline,
		Name:    "Mailmomy (mingyuekeji.online)",
		Website: "mailmomy.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.MingyuekejiOnlineGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.MingyuekejiOnlineGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelMingyuemingClick,
		Name:    "Mailmomy (mingyueming.click)",
		Website: "mailmomy.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.MingyuemingClickGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.MingyuemingClickGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelMingyuemingShop,
		Name:    "Mailmomy (mingyueming.shop)",
		Website: "mailmomy.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.MingyuemingShopGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.MingyuemingShopGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelMingyukejiLol,
		Name:    "Mailmomy (mingyukeji.lol)",
		Website: "mailmomy.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.MingyukejiLolGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.MingyukejiLolGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelMnCurppaCom,
		Name:    "Mailinator (mn.curppa.com)",
		Website: "mailinator.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.MnCurppaComGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.MnCurppaComGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelMNikMe,
		Name:    "Mailinator (m.nik.me)",
		Website: "mailinator.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.MNikMeGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.MNikMeGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelMtmdevCom,
		Name:    "Mailinator (mtmdev.com)",
		Website: "mailinator.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.MtmdevComGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.MtmdevComGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelNospamThurstonsUs,
		Name:    "Mailinator (nospam.thurstons.us)",
		Website: "mailinator.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.NospamThurstonsUsGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.NospamThurstonsUsGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelNotfond404Mn,
		Name:    "Mailinator (notfond.404.mn)",
		Website: "mailinator.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.Notfond404MnGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.Notfond404MnGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelNullK3vinNet,
		Name:    "Mailinator (null.k3vin.net)",
		Website: "mailinator.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.NullK3vinNetGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.NullK3vinNetGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelNuxh62Space,
		Name:    "Mailmomy (nuxh62.space)",
		Website: "mailmomy.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.Nuxh62SpaceGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.Nuxh62SpaceGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelProidCloudIpCc,
		Name:    "Mailmomy (proid.cloud-ip.cc)",
		Website: "mailmomy.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.ProidCloudIpCcGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.ProidCloudIpCcGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelRamjaneMoooCom,
		Name:    "Mailinator (ramjane.mooo.com)",
		Website: "mailinator.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.RamjaneMoooComGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.RamjaneMoooComGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelRauxaSenyCat,
		Name:    "Mailinator (rauxa.seny.cat)",
		Website: "mailinator.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.RauxaSenyCatGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.RauxaSenyCatGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelReallyIstrashCom,
		Name:    "Mailinator (really.istrash.com)",
		Website: "mailinator.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.ReallyIstrashComGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.ReallyIstrashComGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelSbookPics,
		Name:    "Mailmomy (sbook.pics)",
		Website: "mailmomy.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.SbookPicsGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.SbookPicsGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelSpamHortukOvh,
		Name:    "Mailinator (spam.hortuk.ovh)",
		Website: "mailinator.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.SpamHortukOvhGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.SpamHortukOvhGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelSpWootAt,
		Name:    "Mailinator (sp.woot.at)",
		Website: "mailinator.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.SpWootAtGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.SpWootAtGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelTestUnergieCom,
		Name:    "Mailinator (test.unergie.com)",
		Website: "mailinator.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.TestUnergieComGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.TestUnergieComGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelTorchYiOrg,
		Name:    "Mailinator (torch.yi.org)",
		Website: "mailinator.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.TorchYiOrgGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.TorchYiOrgGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelTZibetNet,
		Name:    "Mailinator (t.zibet.net)",
		Website: "mailinator.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.TZibetNetGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.TZibetNetGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelXue32Buzz,
		Name:    "Mailmomy (xue32.buzz)",
		Website: "mailmomy.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.Xue32BuzzGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.Xue32BuzzGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelApihz,
		Name:    "ApiHz TempMail",
		Website: "apihz.cn",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.ApihzGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for apihz channel")
			}
			return normEmailsResult(prov.ApihzGetEmails(token))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelSogetthisCom,
		Name:    "Mailinator (sogetthis.com)",
		Website: "mailinator.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.SogetthisComGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.SogetthisComGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelBobmailInfo,
		Name:    "Mailinator (bobmail.info)",
		Website: "mailinator.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.BobmailInfoGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.BobmailInfoGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelSuremailInfo,
		Name:    "Mailinator (suremail.info)",
		Website: "mailinator.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.SuremailInfoGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.SuremailInfoGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelBinkmailCom,
		Name:    "Mailinator (binkmail.com)",
		Website: "mailinator.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.BinkmailComGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.BinkmailComGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelVeryrealemailCom,
		Name:    "Mailinator (veryrealemail.com)",
		Website: "mailinator.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.VeryrealemailComGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.VeryrealemailComGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelMailmomy,
		Name:    "Mailmomy",
		Website: "mailmomy.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.MailmomyGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.MailmomyGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelChammyInfo,
		Name:    "Mailinator (chammy.info)",
		Website: "mailinator.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.ChammyInfoGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.ChammyInfoGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelThisisnotmyrealemailCom,
		Name:    "Mailinator (thisisnotmyrealemail.com)",
		Website: "mailinator.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.ThisisnotmyrealemailComGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.ThisisnotmyrealemailComGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelNotmailinatorCom,
		Name:    "Mailinator (notmailinator.com)",
		Website: "mailinator.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.NotmailinatorComGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.NotmailinatorComGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelSpamherepleaseCom,
		Name:    "Mailinator (spamhereplease.com)",
		Website: "mailinator.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.SpamherepleaseComGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.SpamherepleaseComGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelSendspamhereCom,
		Name:    "Mailinator (sendspamhere.com)",
		Website: "mailinator.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.SendspamhereComGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.SendspamhereComGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelSendfreeOrg,
		Name:    "Mailinator (sendfree.org)",
		Website: "mailinator.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.SendfreeOrgGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.SendfreeOrgGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelJunkBeatsOrg,
		Name:    "Mailinator (junk.beats.org)",
		Website: "mailinator.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.JunkBeatsOrgGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.JunkBeatsOrgGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelJunkIhmehlCom,
		Name:    "Mailinator (junk.ihmehl.com)",
		Website: "mailinator.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.JunkIhmehlComGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.JunkIhmehlComGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelJunkNoplayOrg,
		Name:    "Mailinator (junk.noplay.org)",
		Website: "mailinator.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.JunkNoplayOrgGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.JunkNoplayOrgGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelJunkVanillasystemCom,
		Name:    "Mailinator (junk.vanillasystem.com)",
		Website: "mailinator.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.JunkVanillasystemComGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.JunkVanillasystemComGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelSpamJasonpearceCom,
		Name:    "Mailinator (spam.jasonpearce.com)",
		Website: "mailinator.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.SpamJasonpearceComGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.SpamJasonpearceComGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelFishSkytaleNet,
		Name:    "Mailinator (fish.skytale.net)",
		Website: "mailinator.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.FishSkytaleNetGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.FishSkytaleNetGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelSpamMccrewCom,
		Name:    "Mailinator (spam.mccrew.com)",
		Website: "mailinator.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.SpamMccrewComGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.SpamMccrewComGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelDropmailClick,
		Name:    "DropMail.click",
		Website: "dropmail.click",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.DropmailClickGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.DropmailClickGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelSpamCoroiuCom,
		Name:    "Mailinator (spam.coroiu.com)",
		Website: "mailinator.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.SpamCoroiuComGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.SpamCoroiuComGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelSpamDeluserNet,
		Name:    "Mailinator (spam.deluser.net)",
		Website: "mailinator.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.SpamDeluserNetGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.SpamDeluserNetGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelSpamDhsfNet,
		Name:    "Mailinator (spam.dhsf.net)",
		Website: "mailinator.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.SpamDhsfNetGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.SpamDhsfNetGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelSpamLucatntCom,
		Name:    "Mailinator (spam.lucatnt.com)",
		Website: "mailinator.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.SpamLucatntComGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.SpamLucatntComGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelSpamLyceumLifeComRu,
		Name:    "Mailinator (spam.lyceum-life.com.ru)",
		Website: "mailinator.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.SpamLyceumLifeComRuGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.SpamLyceumLifeComRuGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelSpamNetpiratesNet,
		Name:    "Mailinator (spam.netpirates.net)",
		Website: "mailinator.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.SpamNetpiratesNetGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.SpamNetpiratesNetGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelSpamNoIpNet,
		Name:    "Mailinator (spam.no-ip.net)",
		Website: "mailinator.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.SpamNoIpNetGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.SpamNoIpNetGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelSpamOzhOrg,
		Name:    "Mailinator (spam.ozh.org)",
		Website: "mailinator.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.SpamOzhOrgGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.SpamOzhOrgGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelSpamPyphusOrg,
		Name:    "Mailinator (spam.pyphus.org)",
		Website: "mailinator.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.SpamPyphusOrgGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.SpamPyphusOrgGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelSpamShepPw,
		Name:    "Mailinator (spam.shep.pw)",
		Website: "mailinator.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.SpamShepPwGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.SpamShepPwGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelSpamWtfAt,
		Name:    "Mailinator (spam.wtf.at)",
		Website: "mailinator.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.SpamWtfAtGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.SpamWtfAtGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelSpamWulczerOrg,
		Name:    "Mailinator (spam.wulczer.org)",
		Website: "mailinator.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.SpamWulczerOrgGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.SpamWulczerOrgGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelCrapKakaduaNet,
		Name:    "Mailinator (crap.kakadua.net)",
		Website: "mailinator.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.CrapKakaduaNetGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.CrapKakaduaNetGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelSpamJanlugtNl,
		Name:    "Mailinator (spam.janlugt.nl)",
		Website: "mailinator.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.SpamJanlugtNlGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.SpamJanlugtNlGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelMinBurningfishNet,
		Name:    "Mailinator (min.burningfish.net)",
		Website: "mailinator.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.MinBurningfishNetGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.MinBurningfishNetGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelSinkFblayCom,
		Name:    "Mailinator (sink.fblay.com)",
		Website: "mailinator.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.SinkFblayComGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.SinkFblayComGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelTempgmailer,
		Name:    "TempGmailer",
		Website: "tempgmailer.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.TempgmailerGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for tempgmailer channel")
			}
			return normEmailsResult(prov.TempgmailerGetEmails(token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelTempMailOrg,
		Name:    "Temp-Mail.org",
		Website: "temp-mail.org",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.TempMailOrgGenerate(opts.Duration, ""))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for temp-mail-org channel")
			}
			return normEmailsResult(prov.TempMailOrgGetEmails(token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelXkxMe,
		Name:    "XKX.me",
		Website: "xkx.me",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.XkxMeGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for xkx-me channel")
			}
			return normEmailsResult(prov.XkxMeGetEmails(token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelMailcatAi,
		Name:    "Mailcat AI",
		Website: "mailcat.ai",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.MailcatAiGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for mailcat-ai channel")
			}
			return normEmailsResult(prov.MailcatAiGetEmails(token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelTempgoEmail,
		Name:    "TempGo Email",
		Website: "tempgo.email",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.TempgoEmailGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for tempgo-email channel")
			}
			return normEmailsResult(prov.TempgoEmailGetEmails(token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelRestmailNet,
		Name:    "Restmail.net",
		Website: "restmail.net",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.RestmailNetGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.RestmailNetGetEmails(token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelDropmailMe,
		Name:    "DropMail.me",
		Website: "dropmail.me",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			domain := ""
			if opts.Domain != nil {
				domain = *opts.Domain
			}
			return fromMailbox(prov.DropmailMeGenerate(opts.Duration, domain))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for dropmail-me channel")
			}
			return normEmailsResult(prov.DropmailMeGetEmails(token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelTenMinuteMailNet,
		Name:    "10MinuteMail.net",
		Website: "10minutemail.net",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			domain := ""
			if opts.Domain != nil {
				domain = *opts.Domain
			}
			return fromMailbox(prov.TenMinuteMailNetGenerate(opts.Duration, domain))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for ten-minute-mail-net channel")
			}
			return normEmailsResult(prov.TenMinuteMailNetGetEmails(token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelTempmailsIo,
		Name:    "TempMails.io",
		Website: "tempmails.io",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.TempmailsIoGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for tempmails-io channel")
			}
			return normEmailsResult(prov.TempmailsIoGetEmails(email, token))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelShitpostEmail,
		Name:    "ShitPost.email",
		Website: "shitpost.email",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.ShitpostEmailGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for shitpost-email channel")
			}
			return normEmailsResult(prov.ShitpostEmailGetEmails(email, token))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelSmails,
		Name:    "Smails.dev",
		Website: "smails.dev",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.SmailsGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for smails channel")
			}
			return normEmailsResult(prov.SmailsGetEmails(email, token))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelTempmailportal,
		Name:    "TempMail Portal",
		Website: "tempmailportal.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.TempmailportalGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for tempmailportal channel")
			}
			return normEmailsResult(prov.TempmailportalGetEmails(email, token))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelHuskmail,
		Name:    "HuskMail",
		Website: "huskmail.xyz",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.HuskmailGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for huskmail channel")
			}
			return normEmailsResult(prov.HuskmailGetEmails(email, token))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelZerodrop,
		Name:    "Zerodrop",
		Website: "zerodrop.dev",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.ZerodropGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for zerodrop channel")
			}
			return normEmailsResult(prov.ZerodropGetEmails(email, token))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelFiretempmail,
		Name:    "FireTempMail",
		Website: "firetempmail.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.FiretempmailGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for firetempmail channel")
			}
			return normEmailsResult(prov.FiretempmailGetEmails(email, token))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelNullmail,
		Name:    "NullMail",
		Website: "nullmail.cc",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.NullmailGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for nullmail channel")
			}
			return normEmailsResult(prov.NullmailGetEmails(email, token))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelTenminApp,
		Name:    "Tenmin.app",
		Website: "tenmin.app",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.TenminAppGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for tenmin-app channel")
			}
			return normEmailsResult(prov.TenminAppGetEmails(email, token))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelMtempmail,
		Name:    "MTempMail",
		Website: "mtempmail.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.MtempmailGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for mtempmail channel")
			}
			return normEmailsResult(prov.MtempmailGetEmails(email, token))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelTempmailEE,
		Name:    "TempMail.ee",
		Website: "tempmail.ee",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.TempmailEEGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for tempmail-ee channel")
			}
			return normEmailsResult(prov.TempmailEEGetEmails(email, token))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelTemporarymailCom,
		Name:    "Temporarymail.com",
		Website: "temporarymail.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.TemporarymailGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for temporarymail-com channel")
			}
			return normEmailsResult(prov.TemporarymailGetEmails(email, token))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelEmail30Min,
		Name:    "30minemail",
		Website: "30minemail.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.ThirtyMinEmailGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for 30minemail channel")
			}
			return normEmailsResult(prov.ThirtyMinEmailGetEmails(email, token))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelLinshiXYZ,
		Name:    "Linshi.xyz",
		Website: "linshi.xyz",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.LinshiXYZGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for linshi-xyz channel")
			}
			return normEmailsResult(prov.LinshiXYZGetEmails(email, token))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelCrazymailing,
		Name:    "CrazyMailing",
		Website: "crazymailing.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.CrazymailingGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for crazymailing channel")
			}
			return normEmailsResult(prov.CrazymailingGetEmails(email, token))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelNukemail,
		Name:    "NukeMail",
		Website: "nukemail.app",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.NukemailGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for nukemail channel")
			}
			return normEmailsResult(prov.NukemailGetEmails(email, token))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelShadowmail,
		Name:    "ShadowMail",
		Website: "shadowmail.win",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.ShadowmailGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for shadowmail channel")
			}
			return normEmailsResult(prov.ShadowmailGetEmails(email, token))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelFlybymail,
		Name:    "FlyByMail",
		Website: "flybymail.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.FlybymailGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for flybymail channel")
			}
			return normEmailsResult(prov.FlybymailGetEmails(email, token))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelNowtempmail,
		Name:    "NowTempMail",
		Website: "nowtempmail.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.NowtempMailGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for nowtempmail channel")
			}
			return normEmailsResult(prov.NowtempMailGetEmails(email, token))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelClawdemail,
		Name:    "ClawdEmail",
		Website: "clawdemail.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.ClawdEmailGenerate(""))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for clawdemail channel")
			}
			return normEmailsResult(prov.ClawdEmailGetEmails(email, token))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelTempmail100,
		Name:    "TempMail100",
		Website: "tempmail100.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.TempMail100Generate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for tempmail100 channel")
			}
			return normEmailsResult(prov.TempMail100GetEmails(email, token))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelTempmailto,
		Name:    "Tempmailto",
		Website: "tempmailto.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.TempmailtoGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for tempmailto channel")
			}
			return normEmailsResult(prov.TempmailtoGetEmails(email, token))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelTempMailGG,
		Name:    "TempMailGG",
		Website: "temp-mail.gg",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.TempMailGGGenerate(""))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for temp-mail-gg channel")
			}
			return normEmailsResult(prov.TempMailGGGetEmails(email, token))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelTmpkit,
		Name:    "Tmpkit",
		Website: "tmpkit.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.TmpkitGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for tmpkit channel")
			}
			return normEmailsResult(prov.TmpkitGetEmails(email, token))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelInternxt,
		Name:    "Internxt",
		Website: "internxt.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.InternxtGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for internxt channel")
			}
			return normEmailsResult(prov.InternxtGetEmails(email, token))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelGeneratorEmail,
		Name:    "GeneratorEmail",
		Website: "generator.email",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.GeneratorEmailGenerate())
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for generator-email channel")
			}
			return normEmailsResult(prov.GeneratorEmailGetEmails(email, token))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelBlondmailCom,
		Name:    "blondmail.com",
		Website: "inboxes.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return genInboxesVariant("blondmail.com", ChannelBlondmailCom)
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.InboxesGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelChapsmailCom,
		Name:    "chapsmail.com",
		Website: "inboxes.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return genInboxesVariant("chapsmail.com", ChannelChapsmailCom)
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.InboxesGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelClowmailCom,
		Name:    "clowmail.com",
		Website: "inboxes.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return genInboxesVariant("clowmail.com", ChannelClowmailCom)
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.InboxesGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelDropjarCom,
		Name:    "dropjar.com",
		Website: "inboxes.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return genInboxesVariant("dropjar.com", ChannelDropjarCom)
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.InboxesGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelFivermailCom,
		Name:    "fivermail.com",
		Website: "inboxes.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return genInboxesVariant("fivermail.com", ChannelFivermailCom)
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.InboxesGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelGetairmailCom,
		Name:    "getairmail.com",
		Website: "inboxes.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return genInboxesVariant("getairmail.com", ChannelGetairmailCom)
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.InboxesGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelGetmuleCom,
		Name:    "getmule.com",
		Website: "inboxes.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return genInboxesVariant("getmule.com", ChannelGetmuleCom)
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.InboxesGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelInboxbearCom,
		Name:    "inboxbear.com",
		Website: "inboxes.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return genInboxesVariant("inboxbear.com", ChannelInboxbearCom)
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.InboxesGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelReplyloopCom,
		Name:    "replyloop.com",
		Website: "inboxes.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return genInboxesVariant("replyloop.com", ChannelReplyloopCom)
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.InboxesGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelTemptamiCom,
		Name:    "temptami.com",
		Website: "inboxes.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return genInboxesVariant("temptami.com", ChannelTemptamiCom)
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.InboxesGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelGimpmailCom,
		Name:    "gimpmail.com",
		Website: "inboxes.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return genInboxesVariant("gimpmail.com", ChannelGimpmailCom)
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.InboxesGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelGivmailCom,
		Name:    "givmail.com",
		Website: "inboxes.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return genInboxesVariant("givmail.com", ChannelGivmailCom)
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.InboxesGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelGuysmailCom,
		Name:    "guysmail.com",
		Website: "inboxes.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return genInboxesVariant("guysmail.com", ChannelGuysmailCom)
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.InboxesGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelRobotMailCom,
		Name:    "robot-mail.com",
		Website: "inboxes.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return genInboxesVariant("robot-mail.com", ChannelRobotMailCom)
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.InboxesGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelTupmailCom,
		Name:    "tupmail.com",
		Website: "inboxes.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return genInboxesVariant("tupmail.com", ChannelTupmailCom)
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.InboxesGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelVomotoCom,
		Name:    "vomoto.com",
		Website: "inboxes.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return genInboxesVariant("vomoto.com", ChannelVomotoCom)
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.InboxesGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelUquMe,
		Name:    "uqu.me",
		Website: "mail.cx",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			info, err := fromMailbox(prov.MailCxGenerate(fixedDomain("uqu.me")))
			if err != nil {
				return nil, err
			}
			info.Channel = ChannelUquMe
			return info, nil
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for uqu-me channel")
			}
			return normEmailsResult(prov.MailCxGetEmails(token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelN9k3rCom,
		Name:    "9k3r.com",
		Website: "mail.cx",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			info, err := fromMailbox(prov.MailCxGenerate(fixedDomain("9k3r.com")))
			if err != nil {
				return nil, err
			}
			info.Channel = ChannelN9k3rCom
			return info, nil
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for 9k3r-com channel")
			}
			return normEmailsResult(prov.MailCxGetEmails(token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelN2famailCom,
		Name:    "2famail.com",
		Website: "mailmomy.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.MailmomyVariantGenerate("2famail.com", "2famail-com"))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.MailmomyGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelXikemailCom,
		Name:    "xikemail.com",
		Website: "mailmomy.com",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.MailmomyVariantGenerate("xikemail.com", "xikemail-com"))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			return normEmailsResult(prov.MailmomyGetEmails(email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelFldubaiCom,
		Name:    "fldubai.com",
		Website: "mail.tm",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.MailTmGenerateAtDomain("fldubai.com", "fldubai-com"))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for fldubai-com channel")
			}
			return normEmailsResult(prov.MailTmGetEmails(token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelNicegroundShop,
		Name:    "niceground.shop",
		Website: "duckmail.sbs",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.DuckmailGenerateAtDomain("niceground.shop", "niceground-shop"))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for niceground-shop channel")
			}
			return normEmailsResult(prov.DuckmailGetEmails(token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelGlasswhitehubCom,
		Name:    "glasswhitehub.com",
		Website: "duckmail.sbs",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.DuckmailGenerateAtDomain("glasswhitehub.com", "glasswhitehub-com"))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for glasswhitehub-com channel")
			}
			return normEmailsResult(prov.DuckmailGetEmails(token, email))
		},
	})

	registerChannel(ChannelSpec{
		Channel: ChannelHubaiclassOrg,
		Name:    "hubaiclass.org",
		Website: "duckmail.sbs",
		Generate: func(opts *GenerateEmailOptions) (*EmailInfo, error) {
			return fromMailbox(prov.DuckmailGenerateAtDomain("hubaiclass.org", "hubaiclass-org"))
		},
		GetEmails: func(email, token string) ([]Email, error) {
			if token == "" {
				return nil, fmt.Errorf("internal error: token missing for hubaiclass-org channel")
			}
			return normEmailsResult(prov.DuckmailGetEmails(token, email))
		},
	})

}
