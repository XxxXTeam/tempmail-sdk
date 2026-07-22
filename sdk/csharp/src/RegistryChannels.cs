using System.Runtime.CompilerServices;
using XxxXTeam.TempMail.Providers;

namespace XxxXTeam.TempMail;

/// <summary>
/// 全部 279 个渠道的注册入口（严格按 baseline 顺序）。
/// 使用 ModuleInitializer 在程序集加载时自动填充注册表，
/// ListChannels() 输出顺序与 .baseline_channels.txt 逐行一致。
/// generate/getEmails 委托绑定到对应 Provider（纯 JSON API 渠道为真实现，复杂渠道为占位桩）。
/// </summary>
internal static class RegistryChannels
{
    private static int _done;

    /// <summary>程序集加载时自动注册全部渠道（幂等）</summary>
    [ModuleInitializer]
    internal static void Init()
    {
        if (System.Threading.Interlocked.Exchange(ref _done, 1) != 0) return;
        Registry.Register(new ChannelSpec
        {
            Channel = "tempmail",
            Name = "TempMail",
            Website = "tempmail.ing",
            Generate = o => Tempmail.Generate(o.Duration),
            GetEmails = (e, t) => Tempmail.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "tempmail-cn",
            Name = "TempMail CN",
            Website = "tempmail.cn",
            Generate = o => TempmailCn.Generate(),
            GetEmails = (e, t) => TempmailCn.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "ta-easy",
            Name = "TA Easy",
            Website = "ta-easy.com",
            Generate = o => TaEasy.Generate(),
            GetEmails = (e, t) => TaEasy.GetEmails(e, t),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "10minute-one",
            Name = "10 Minute Email",
            Website = "10minutemail.one",
            Generate = o => TenMinuteOne.Generate(o.Domain),
            GetEmails = (e, t) => TenMinuteOne.GetEmails(e, t),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "xghff-com",
            Name = "xghff.com",
            Website = "10minutemail.one",
            Generate = o => TenMinuteOne.Generate("xghff.com", "xghff-com"),
            GetEmails = (e, t) => TenMinuteOne.GetEmails(e, t),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "oqqaj-com",
            Name = "oqqaj.com",
            Website = "10minutemail.one",
            Generate = o => TenMinuteOne.Generate("oqqaj.com", "oqqaj-com"),
            GetEmails = (e, t) => TenMinuteOne.GetEmails(e, t),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "psovv-com",
            Name = "psovv.com",
            Website = "10minutemail.one",
            Generate = o => TenMinuteOne.Generate("psovv.com", "psovv-com"),
            GetEmails = (e, t) => TenMinuteOne.GetEmails(e, t),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "dbwot-com",
            Name = "dbwot.com",
            Website = "10minutemail.one",
            Generate = o => TenMinuteOne.Generate("dbwot.com", "dbwot-com"),
            GetEmails = (e, t) => TenMinuteOne.GetEmails(e, t),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "ygwpr-com",
            Name = "ygwpr.com",
            Website = "10minutemail.one",
            Generate = o => TenMinuteOne.Generate("ygwpr.com", "ygwpr-com"),
            GetEmails = (e, t) => TenMinuteOne.GetEmails(e, t),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "imxwe-com",
            Name = "imxwe.com",
            Website = "10minutemail.one",
            Generate = o => TenMinuteOne.Generate("imxwe.com", "imxwe-com"),
            GetEmails = (e, t) => TenMinuteOne.GetEmails(e, t),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "linshiyou",
            Name = "临时邮",
            Website = "linshiyou.com",
            Generate = o => Linshiyou.Generate(),
            GetEmails = (e, t) => Linshiyou.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "mffac",
            Name = "MFFAC",
            Website = "mffac.com",
            Generate = o => Mffac.Generate(),
            GetEmails = (e, t) => Mffac.GetEmails(e, t),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "tempmail-lol",
            Name = "TempMail LOL",
            Website = "tempmail.lol",
            Generate = o => TempMailLol.Generate(o.Domain),
            GetEmails = (e, t) => TempMailLol.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "chatgpt-org-uk",
            Name = "ChatGPT Mail",
            Website = "mail.chatgpt.org.uk",
            Generate = o => ChatgptOrgUk.Generate(),
            GetEmails = (e, t) => ChatgptOrgUk.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "temp-mail-io",
            Name = "Temp-Mail.io",
            Website = "temp-mail.io",
            Generate = o => TempMailIo.Generate(),
            GetEmails = (e, t) => TempMailIo.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "mail-cx",
            Name = "Mail.cx",
            Website = "mail.cx",
            Generate = o => MailCx.Generate("mail-cx", o.Domain),
            GetEmails = (e, t) => MailCx.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "ddker-com",
            Name = "ddker.com",
            Website = "mail.cx",
            Generate = o => MailCx.Generate("ddker-com", "ddker.com"),
            GetEmails = (e, t) => MailCx.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "catchmail",
            Name = "Catchmail",
            Website = "catchmail.io",
            Generate = o => Catchmail.Generate("catchmail", o.Domain),
            GetEmails = (e, t) => Catchmail.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "catchmail-mailistry",
            Name = "Catchmail Mailistry",
            Website = "mailistry.com",
            Generate = o => Catchmail.Generate("catchmail-mailistry", "mailistry.com"),
            GetEmails = (e, t) => Catchmail.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "catchmail-zeppost",
            Name = "Catchmail Zeppost",
            Website = "zeppost.com",
            Generate = o => Catchmail.Generate("catchmail-zeppost", "zeppost.com"),
            GetEmails = (e, t) => Catchmail.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "mailforspam",
            Name = "MailForSpam",
            Website = "mailforspam.com",
            Generate = o => MailForSpam.Generate("mailforspam", o.Domain),
            GetEmails = (e, t) => MailForSpam.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "mailforspam-tempmail-io",
            Name = "MailForSpam TempMail.io",
            Website = "tempmail.io",
            Generate = o => MailForSpam.Generate("mailforspam-tempmail-io", "tempmail.io"),
            GetEmails = (e, t) => MailForSpam.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "mailforspam-disposable",
            Name = "MailForSpam Disposable",
            Website = "disposable.email",
            Generate = o => MailForSpam.Generate("mailforspam-disposable", "disposable.email"),
            GetEmails = (e, t) => MailForSpam.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "tempmailc",
            Name = "TempMailC",
            Website = "tempmailc.com",
            Generate = o => Tempmailc.Generate(),
            GetEmails = (e, t) => Tempmailc.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "mailnesia",
            Name = "Mailnesia",
            Website = "mailnesia.com",
            Generate = o => Mailnesia.Generate(),
            GetEmails = (e, t) => Mailnesia.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "throwawaymail",
            Name = "ThrowawayMail",
            Website = "throwawaymail.app",
            Generate = o => ThrowawayMail.Generate(),
            GetEmails = (e, t) => ThrowawayMail.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "tempmail-fish",
            Name = "TempMail Fish",
            Website = "tempmail.fish",
            Generate = o => TempMailFish.Generate(),
            GetEmails = (e, t) => TempMailFish.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "neighbours-sh",
            Name = "Neighbours",
            Website = "neighbours.sh",
            Generate = o => Neighbours.GenerateSh(),
            GetEmails = (e, t) => Neighbours.GetEmailsSh(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "shitty-email",
            Name = "shitty.email",
            Website = "shitty.email",
            Generate = o => ShittyEmail.Generate(),
            GetEmails = (e, t) => ShittyEmail.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "tempmailpro",
            Name = "TempMail Pro",
            Website = "tempmailpro.us",
            Generate = o => TempMailPro.Generate(),
            GetEmails = (e, t) => TempMailPro.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "devmail-uk",
            Name = "DevMail UK",
            Website = "devmail.uk",
            Generate = o => DevMailUk.Generate(),
            GetEmails = (e, t) => DevMailUk.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "inboxkitten",
            Name = "InboxKitten",
            Website = "inboxkitten.com",
            Generate = o => InboxKitten.Generate(),
            GetEmails = (e, t) => InboxKitten.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "cleantempmail",
            Name = "CleanTempMail",
            Website = "cleantempmail.com",
            Generate = o => CleanTempMail.Generate(),
            GetEmails = (e, t) => CleanTempMail.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "getnada",
            Name = "GetNada",
            Website = "getnada.net",
            Generate = o => Getnada.Generate(o.Domain, "getnada"),
            GetEmails = (e, t) => Getnada.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "1vpn-net",
            Name = "1vpn.net",
            Website = "getnada.net",
            Generate = o => Getnada.Generate("1vpn.net", "1vpn-net"),
            GetEmails = (e, t) => Getnada.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "abematv-com",
            Name = "abematv.com",
            Website = "getnada.net",
            Generate = o => Getnada.Generate("abematv.com", "abematv-com"),
            GetEmails = (e, t) => Getnada.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "abematv-net",
            Name = "abematv.net",
            Website = "getnada.net",
            Generate = o => Getnada.Generate("abematv.net", "abematv-net"),
            GetEmails = (e, t) => Getnada.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "abematv-org",
            Name = "abematv.org",
            Website = "getnada.net",
            Generate = o => Getnada.Generate("abematv.org", "abematv-org"),
            GetEmails = (e, t) => Getnada.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "aceh-cc",
            Name = "aceh.cc",
            Website = "getnada.net",
            Generate = o => Getnada.Generate("aceh.cc", "aceh-cc"),
            GetEmails = (e, t) => Getnada.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "bangkabelitung-net",
            Name = "bangkabelitung.net",
            Website = "getnada.net",
            Generate = o => Getnada.Generate("bangkabelitung.net", "bangkabelitung-net"),
            GetEmails = (e, t) => Getnada.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "cctruyen-com",
            Name = "cctruyen.com",
            Website = "getnada.net",
            Generate = o => Getnada.Generate("cctruyen.com", "cctruyen-com"),
            GetEmails = (e, t) => Getnada.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "getnada-com",
            Name = "getnada.com",
            Website = "getnada.net",
            Generate = o => Getnada.Generate("getnada.com", "getnada-com"),
            GetEmails = (e, t) => Getnada.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "getnada-email",
            Name = "getnada.email",
            Website = "getnada.net",
            Generate = o => Getnada.Generate("getnada.email", "getnada-email"),
            GetEmails = (e, t) => Getnada.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "getnada-net",
            Name = "getnada.net",
            Website = "getnada.net",
            Generate = o => Getnada.Generate("getnada.net", "getnada-net"),
            GetEmails = (e, t) => Getnada.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "jawatengah-net",
            Name = "jawatengah.net",
            Website = "getnada.net",
            Generate = o => Getnada.Generate("jawatengah.net", "jawatengah-net"),
            GetEmails = (e, t) => Getnada.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "jawatimur-net",
            Name = "jawatimur.net",
            Website = "getnada.net",
            Generate = o => Getnada.Generate("jawatimur.net", "jawatimur-net"),
            GetEmails = (e, t) => Getnada.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "kalimantanbarat-net",
            Name = "kalimantanbarat.net",
            Website = "getnada.net",
            Generate = o => Getnada.Generate("kalimantanbarat.net", "kalimantanbarat-net"),
            GetEmails = (e, t) => Getnada.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "kalimantanselatan-net",
            Name = "kalimantanselatan.net",
            Website = "getnada.net",
            Generate = o => Getnada.Generate("kalimantanselatan.net", "kalimantanselatan-net"),
            GetEmails = (e, t) => Getnada.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "kalimantantengah-net",
            Name = "kalimantantengah.net",
            Website = "getnada.net",
            Generate = o => Getnada.Generate("kalimantantengah.net", "kalimantantengah-net"),
            GetEmails = (e, t) => Getnada.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "kalimantantimur-net",
            Name = "kalimantantimur.net",
            Website = "getnada.net",
            Generate = o => Getnada.Generate("kalimantantimur.net", "kalimantantimur-net"),
            GetEmails = (e, t) => Getnada.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "kalimantanutara-net",
            Name = "kalimantanutara.net",
            Website = "getnada.net",
            Generate = o => Getnada.Generate("kalimantanutara.net", "kalimantanutara-net"),
            GetEmails = (e, t) => Getnada.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "kepulauanriau-net",
            Name = "kepulauanriau.net",
            Website = "getnada.net",
            Generate = o => Getnada.Generate("kepulauanriau.net", "kepulauanriau-net"),
            GetEmails = (e, t) => Getnada.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "luxury345-com",
            Name = "luxury345.com",
            Website = "getnada.net",
            Generate = o => Getnada.Generate("luxury345.com", "luxury345-com"),
            GetEmails = (e, t) => Getnada.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "malukuutara-net",
            Name = "malukuutara.net",
            Website = "getnada.net",
            Generate = o => Getnada.Generate("malukuutara.net", "malukuutara-net"),
            GetEmails = (e, t) => Getnada.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "nusatenggarabarat-net",
            Name = "nusatenggarabarat.net",
            Website = "getnada.net",
            Generate = o => Getnada.Generate("nusatenggarabarat.net", "nusatenggarabarat-net"),
            GetEmails = (e, t) => Getnada.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "nusatenggaratimur-net",
            Name = "nusatenggaratimur.net",
            Website = "getnada.net",
            Generate = o => Getnada.Generate("nusatenggaratimur.net", "nusatenggaratimur-net"),
            GetEmails = (e, t) => Getnada.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "papuabarat-net",
            Name = "papuabarat.net",
            Website = "getnada.net",
            Generate = o => Getnada.Generate("papuabarat.net", "papuabarat-net"),
            GetEmails = (e, t) => Getnada.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "papuabaratdaya-net",
            Name = "papuabaratdaya.net",
            Website = "getnada.net",
            Generate = o => Getnada.Generate("papuabaratdaya.net", "papuabaratdaya-net"),
            GetEmails = (e, t) => Getnada.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "papuaselatan-net",
            Name = "papuaselatan.net",
            Website = "getnada.net",
            Generate = o => Getnada.Generate("papuaselatan.net", "papuaselatan-net"),
            GetEmails = (e, t) => Getnada.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "pehol-com",
            Name = "pehol.com",
            Website = "getnada.net",
            Generate = o => Getnada.Generate("pehol.com", "pehol-com"),
            GetEmails = (e, t) => Getnada.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "ptruyen-com",
            Name = "ptruyen.com",
            Website = "getnada.net",
            Generate = o => Getnada.Generate("ptruyen.com", "ptruyen-com"),
            GetEmails = (e, t) => Getnada.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "pulaubali-net",
            Name = "pulaubali.net",
            Website = "getnada.net",
            Generate = o => Getnada.Generate("pulaubali.net", "pulaubali-net"),
            GetEmails = (e, t) => Getnada.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "riau-net",
            Name = "riau.net",
            Website = "getnada.net",
            Generate = o => Getnada.Generate("riau.net", "riau-net"),
            GetEmails = (e, t) => Getnada.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "seokey-org",
            Name = "seokey.org",
            Website = "getnada.net",
            Generate = o => Getnada.Generate("seokey.org", "seokey-org"),
            GetEmails = (e, t) => Getnada.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "sulawesibarat-net",
            Name = "sulawesibarat.net",
            Website = "getnada.net",
            Generate = o => Getnada.Generate("sulawesibarat.net", "sulawesibarat-net"),
            GetEmails = (e, t) => Getnada.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "sulawesiselatan-net",
            Name = "sulawesiselatan.net",
            Website = "getnada.net",
            Generate = o => Getnada.Generate("sulawesiselatan.net", "sulawesiselatan-net"),
            GetEmails = (e, t) => Getnada.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "sulawesitengah-net",
            Name = "sulawesitengah.net",
            Website = "getnada.net",
            Generate = o => Getnada.Generate("sulawesitengah.net", "sulawesitengah-net"),
            GetEmails = (e, t) => Getnada.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "sulawesitenggara-net",
            Name = "sulawesitenggara.net",
            Website = "getnada.net",
            Generate = o => Getnada.Generate("sulawesitenggara.net", "sulawesitenggara-net"),
            GetEmails = (e, t) => Getnada.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "sumaterabarat-net",
            Name = "sumaterabarat.net",
            Website = "getnada.net",
            Generate = o => Getnada.Generate("sumaterabarat.net", "sumaterabarat-net"),
            GetEmails = (e, t) => Getnada.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "sumateraselatan-net",
            Name = "sumateraselatan.net",
            Website = "getnada.net",
            Generate = o => Getnada.Generate("sumateraselatan.net", "sumateraselatan-net"),
            GetEmails = (e, t) => Getnada.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "sumaterautara-net",
            Name = "sumaterautara.net",
            Website = "getnada.net",
            Generate = o => Getnada.Generate("sumaterautara.net", "sumaterautara-net"),
            GetEmails = (e, t) => Getnada.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "villatogel-com",
            Name = "villatogel.com",
            Website = "getnada.net",
            Generate = o => Getnada.Generate("villatogel.com", "villatogel-com"),
            GetEmails = (e, t) => Getnada.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "mail123",
            Name = "Mail123",
            Website = "mail123.fr",
            Generate = o => Mail123.Generate(),
            GetEmails = (e, t) => Mail123.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "mail10s",
            Name = "Mail10s",
            Website = "mail10s.com",
            Generate = o => Mail10s.Generate(),
            GetEmails = (e, t) => Mail10s.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "webmailtemp",
            Name = "WebMailTemp",
            Website = "webmailtemp.com",
            Generate = o => WebMailTemp.Generate(),
            GetEmails = (e, t) => WebMailTemp.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "tempfastmail",
            Name = "TempFastMail",
            Website = "tempfastmail.com",
            Generate = o => TempFastMail.Generate(),
            GetEmails = (e, t) => TempFastMail.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "1sec-mail",
            Name = "1SecMail",
            Website = "1sec-mail.com",
            Generate = o => OneSecMail.Generate(),
            GetEmails = (e, t) => OneSecMail.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "fakemail",
            Name = "FakeMail",
            Website = "fakemail.net",
            Generate = o => Fakemail.Generate(),
            GetEmails = (e, t) => Fakemail.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "openinbox",
            Name = "OpenInbox",
            Website = "openinbox.io",
            Generate = o => OpenInbox.Generate(),
            GetEmails = (e, t) => OpenInbox.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "inboxes",
            Name = "Inboxes",
            Website = "inboxes.com",
            Generate = o => Inboxes.Generate(o.Domain),
            GetEmails = (e, t) => Inboxes.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "uncorreotemporal",
            Name = "UnCorreoTemporal",
            Website = "uncorreotemporal.com",
            Generate = o => UnCorreoTemporal.Generate(),
            GetEmails = (e, t) => UnCorreoTemporal.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "awamail",
            Name = "AwaMail",
            Website = "awamail.com",
            Generate = o => Awamail.Generate(),
            GetEmails = (e, t) => Awamail.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "mail-tm",
            Name = "Mail.tm",
            Website = "mail.tm",
            Generate = o => MailTm.Generate("mail-tm", "https://api.mail.tm"),
            GetEmails = (e, t) => MailTm.GetEmails("https://api.mail.tm", t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "web-library-net",
            Name = "web-library.net",
            Website = "mail.tm",
            Generate = o => MailTm.Generate("web-library-net", "https://api.mail.tm"),
            GetEmails = (e, t) => MailTm.GetEmails("https://api.mail.tm", t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "dropmail",
            Name = "DropMail",
            Website = "dropmail.me",
            Generate = o => DropMail.Generate(),
            GetEmails = (e, t) => DropMail.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "guerrillamail",
            Name = "Guerrilla Mail",
            Website = "guerrillamail.com",
            Generate = o => GuerrillaMail.Generate("https://api.guerrillamail.com/ajax.php", "guerrillamail"),
            GetEmails = (e, t) => GuerrillaMail.GetEmails("https://api.guerrillamail.com/ajax.php", t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "guerrillamail-com",
            Name = "GuerrillaMail Root",
            Website = "guerrillamail.com",
            Generate = o => GuerrillaMail.Generate("https://guerrillamail.com/ajax.php", "guerrillamail-com"),
            GetEmails = (e, t) => GuerrillaMail.GetEmails("https://guerrillamail.com/ajax.php", t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "maildrop",
            Name = "Maildrop",
            Website = "maildrop.cx",
            Generate = o => Maildrop.Generate(o.Domain),
            GetEmails = (e, t) => Maildrop.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "smail-pw",
            Name = "Smail.pw",
            Website = "smail.pw",
            Generate = o => SmailPw.Generate(),
            GetEmails = (e, t) => SmailPw.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "vip-215",
            Name = "VIP 215",
            Website = "vip.215.im",
            Generate = o => Vip215.Generate(),
            GetEmails = (e, t) => Vip215.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "fake-legal",
            Name = "Fake Legal",
            Website = "fake.legal",
            Generate = o => FakeLegal.Generate("fake-legal", o.Domain),
            GetEmails = (e, t) => FakeLegal.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "imgui-de",
            Name = "imgui.de",
            Website = "fake.legal",
            Generate = o => FakeLegal.Generate("imgui-de", "imgui.de"),
            GetEmails = (e, t) => FakeLegal.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "pulsewebmenu-de",
            Name = "pulsewebmenu.de",
            Website = "fake.legal",
            Generate = o => FakeLegal.Generate("pulsewebmenu-de", "pulsewebmenu.de"),
            GetEmails = (e, t) => FakeLegal.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "moakt",
            Name = "Moakt",
            Website = "moakt.com",
            Generate = o => Moakt.Generate(o.Domain),
            GetEmails = (e, t) => Moakt.GetEmails(e, t),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "drmail-in",
            Name = "drmail.in",
            Website = "moakt.com",
            Generate = o => Moakt.Generate("drmail.in", "drmail-in"),
            GetEmails = (e, t) => Moakt.GetEmails(e, t),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "teml-net",
            Name = "teml.net",
            Website = "moakt.com",
            Generate = o => Moakt.Generate("teml.net", "teml-net"),
            GetEmails = (e, t) => Moakt.GetEmails(e, t),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "tmpeml-com",
            Name = "tmpeml.com",
            Website = "moakt.com",
            Generate = o => Moakt.Generate("tmpeml.com", "tmpeml-com"),
            GetEmails = (e, t) => Moakt.GetEmails(e, t),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "tmpbox-net",
            Name = "tmpbox.net",
            Website = "moakt.com",
            Generate = o => Moakt.Generate("tmpbox.net", "tmpbox-net"),
            GetEmails = (e, t) => Moakt.GetEmails(e, t),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "moakt-cc",
            Name = "moakt.cc",
            Website = "moakt.com",
            Generate = o => Moakt.Generate("moakt.cc", "moakt-cc"),
            GetEmails = (e, t) => Moakt.GetEmails(e, t),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "disbox-net",
            Name = "disbox.net",
            Website = "moakt.com",
            Generate = o => Moakt.Generate("disbox.net", "disbox-net"),
            GetEmails = (e, t) => Moakt.GetEmails(e, t),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "tmpmail-org",
            Name = "tmpmail.org",
            Website = "moakt.com",
            Generate = o => Moakt.Generate("tmpmail.org", "tmpmail-org"),
            GetEmails = (e, t) => Moakt.GetEmails(e, t),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "tmpmail-net",
            Name = "tmpmail.net",
            Website = "moakt.com",
            Generate = o => Moakt.Generate("tmpmail.net", "tmpmail-net"),
            GetEmails = (e, t) => Moakt.GetEmails(e, t),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "tmails-net",
            Name = "tmails.net",
            Website = "moakt.com",
            Generate = o => Moakt.Generate("tmails.net", "tmails-net"),
            GetEmails = (e, t) => Moakt.GetEmails(e, t),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "disbox-org",
            Name = "disbox.org",
            Website = "moakt.com",
            Generate = o => Moakt.Generate("disbox.org", "disbox-org"),
            GetEmails = (e, t) => Moakt.GetEmails(e, t),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "moakt-co",
            Name = "moakt.co",
            Website = "moakt.com",
            Generate = o => Moakt.Generate("moakt.co", "moakt-co"),
            GetEmails = (e, t) => Moakt.GetEmails(e, t),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "moakt-ws",
            Name = "moakt.ws",
            Website = "moakt.com",
            Generate = o => Moakt.Generate("moakt.ws", "moakt-ws"),
            GetEmails = (e, t) => Moakt.GetEmails(e, t),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "tmail-ws",
            Name = "tmail.ws",
            Website = "moakt.com",
            Generate = o => Moakt.Generate("tmail.ws", "tmail-ws"),
            GetEmails = (e, t) => Moakt.GetEmails(e, t),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "bareed-ws",
            Name = "bareed.ws",
            Website = "moakt.com",
            Generate = o => Moakt.Generate("bareed.ws", "bareed-ws"),
            GetEmails = (e, t) => Moakt.GetEmails(e, t),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "email10min",
            Name = "Email10Min",
            Website = "email10min.com",
            Generate = o => Email10Min.Generate(),
            GetEmails = (e, t) => Email10Min.GetEmails(e, t),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "mjj-cm",
            Name = "MJJ Mail",
            Website = "mjj.cm",
            Generate = o => SocketIoMail.GenerateMjjCm(),
            GetEmails = (e, t) => SocketIoMail.GetEmailsMjjCm(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "linshi-co",
            Name = "临时Co",
            Website = "linshi.co",
            Generate = o => SocketIoMail.GenerateLinshiCo(),
            GetEmails = (e, t) => SocketIoMail.GetEmailsLinshiCo(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "harakirimail",
            Name = "HarakiriMail",
            Website = "harakirimail.com",
            Generate = o => HarakiriMail.Generate(),
            GetEmails = (e, t) => HarakiriMail.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "jqkjqk-xyz",
            Name = "jqkjqk.xyz",
            Website = "mail.zhujump.com",
            Generate = o => Zhujump.Generate("jqkjqk.xyz", "jqkjqk-xyz"),
            GetEmails = (e, t) => Zhujump.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "lyhlevi-com",
            Name = "LyhLevi MoeMail",
            Website = "lyhlevi.com",
            Generate = o => Zhujump.GenerateForInstance("https://lyhlevi.com", "lyhlevi.com", "lyhlevi-com", 24L * 60 * 60 * 1000),
            GetEmails = (e, t) => Zhujump.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "tempmail-plus",
            Name = "TempMail Plus",
            Website = "tempmail.plus",
            Generate = o => TempMailPlus.Generate(),
            GetEmails = (e, t) => TempMailPlus.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "fexpost-com",
            Name = "fexpost.com",
            Website = "tempmail.plus",
            Generate = o => TempMailPlus.Generate("fexpost.com", "fexpost-com"),
            GetEmails = (e, t) => TempMailPlus.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "fexbox-org",
            Name = "fexbox.org",
            Website = "tempmail.plus",
            Generate = o => TempMailPlus.Generate("fexbox.org", "fexbox-org"),
            GetEmails = (e, t) => TempMailPlus.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "mailbox-in-ua",
            Name = "mailbox.in.ua",
            Website = "tempmail.plus",
            Generate = o => TempMailPlus.Generate("mailbox.in.ua", "mailbox-in-ua"),
            GetEmails = (e, t) => TempMailPlus.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "rover-info",
            Name = "rover.info",
            Website = "tempmail.plus",
            Generate = o => TempMailPlus.Generate("rover.info", "rover-info"),
            GetEmails = (e, t) => TempMailPlus.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "chitthi-in",
            Name = "chitthi.in",
            Website = "tempmail.plus",
            Generate = o => TempMailPlus.Generate("chitthi.in", "chitthi-in"),
            GetEmails = (e, t) => TempMailPlus.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "fextemp-com",
            Name = "fextemp.com",
            Website = "tempmail.plus",
            Generate = o => TempMailPlus.Generate("fextemp.com", "fextemp-com"),
            GetEmails = (e, t) => TempMailPlus.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "any-pink",
            Name = "any.pink",
            Website = "tempmail.plus",
            Generate = o => TempMailPlus.Generate("any.pink", "any-pink"),
            GetEmails = (e, t) => TempMailPlus.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "merepost-com",
            Name = "merepost.com",
            Website = "tempmail.plus",
            Generate = o => TempMailPlus.Generate("merepost.com", "merepost-com"),
            GetEmails = (e, t) => TempMailPlus.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "tempmail-lol-v2",
            Name = "TempMail LOL V2",
            Website = "tempmail.lol",
            Generate = o => TempMailLolV2.Generate(),
            GetEmails = (e, t) => TempMailLolV2.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "tempgbox",
            Name = "TempGBox",
            Website = "tempgbox.net",
            Generate = o => TempGBox.Generate(),
            GetEmails = (e, t) => TempGBox.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "emailnator",
            Name = "Emailnator",
            Website = "emailnator.com",
            Generate = o => Emailnator.Generate(),
            GetEmails = (e, t) => Emailnator.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "temporam",
            Name = "Temporam",
            Website = "temporam.com",
            Generate = o => Temporam.Generate(o.Domain),
            GetEmails = (e, t) => Temporam.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "neighbours",
            Name = "Neighbours",
            Website = "neighbours.sh",
            Generate = o => Neighbours.Generate(o.Domain),
            GetEmails = (e, t) => Neighbours.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "sharklasers",
            Name = "SharkLasers",
            Website = "sharklasers.com",
            Generate = o => GuerrillaMail.Generate("https://www.sharklasers.com/ajax.php", "sharklasers"),
            GetEmails = (e, t) => GuerrillaMail.GetEmails("https://www.sharklasers.com/ajax.php", t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "sharklasers-com",
            Name = "SharkLasers Root",
            Website = "sharklasers.com",
            Generate = o => GuerrillaMail.Generate("https://sharklasers.com/ajax.php", "sharklasers-com"),
            GetEmails = (e, t) => GuerrillaMail.GetEmails("https://sharklasers.com/ajax.php", t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "grr-la",
            Name = "Grr.la",
            Website = "grr.la",
            Generate = o => GuerrillaMail.Generate("https://www.grr.la/ajax.php", "grr-la"),
            GetEmails = (e, t) => GuerrillaMail.GetEmails("https://www.grr.la/ajax.php", t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "grr-la-com",
            Name = "Grr.la Root",
            Website = "grr.la",
            Generate = o => GuerrillaMail.Generate("https://grr.la/ajax.php", "grr-la-com"),
            GetEmails = (e, t) => GuerrillaMail.GetEmails("https://grr.la/ajax.php", t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "guerrillamail-info",
            Name = "GuerrillaMail Info",
            Website = "guerrillamail.info",
            Generate = o => GuerrillaMail.Generate("https://www.guerrillamail.info/ajax.php", "guerrillamail-info"),
            GetEmails = (e, t) => GuerrillaMail.GetEmails("https://www.guerrillamail.info/ajax.php", t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "spam4me",
            Name = "Spam4.me",
            Website = "spam4.me",
            Generate = o => GuerrillaMail.Generate("https://www.spam4.me/ajax.php", "spam4me"),
            GetEmails = (e, t) => GuerrillaMail.GetEmails("https://www.spam4.me/ajax.php", t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "guerrillamail-net",
            Name = "GuerrillaMail Net",
            Website = "guerrillamail.net",
            Generate = o => GuerrillaMail.Generate("https://www.guerrillamail.net/ajax.php", "guerrillamail-net"),
            GetEmails = (e, t) => GuerrillaMail.GetEmails("https://www.guerrillamail.net/ajax.php", t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "guerrillamail-org",
            Name = "GuerrillaMail Org",
            Website = "guerrillamail.org",
            Generate = o => GuerrillaMail.Generate("https://www.guerrillamail.org/ajax.php", "guerrillamail-org"),
            GetEmails = (e, t) => GuerrillaMail.GetEmails("https://www.guerrillamail.org/ajax.php", t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "guerrillamailblock",
            Name = "GuerrillaMailBlock",
            Website = "guerrillamailblock.com",
            Generate = o => GuerrillaMail.Generate("https://www.guerrillamailblock.com/ajax.php", "guerrillamailblock"),
            GetEmails = (e, t) => GuerrillaMail.GetEmails("https://www.guerrillamailblock.com/ajax.php", t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "guerrillamail-com-www",
            Name = "GuerrillaMail WWW",
            Website = "guerrillamail.com",
            Generate = o => GuerrillaMail.Generate("https://www.guerrillamail.com/ajax.php", "guerrillamail-com-www"),
            GetEmails = (e, t) => GuerrillaMail.GetEmails("https://www.guerrillamail.com/ajax.php", t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "m2u",
            Name = "MailToYou",
            Website = "m2u.io",
            Generate = o => M2u.Generate(),
            GetEmails = (e, t) => M2u.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "tempy-email",
            Name = "Tempy Email",
            Website = "tempy.email",
            Generate = o => TempyEmail.Generate(o.Domain),
            GetEmails = (e, t) => TempyEmail.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "fmail",
            Name = "FMail",
            Website = "fmail.men",
            Generate = o => Fmail.Generate(o.Domain),
            GetEmails = (e, t) => Fmail.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "ockito",
            Name = "Ockito",
            Website = "ockito.com",
            Generate = o => Ockito.Generate(),
            GetEmails = (e, t) => Ockito.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "anonbox",
            Name = "Anonbox",
            Website = "anonbox.net",
            Generate = o => AnonBox.Generate(),
            GetEmails = (e, t) => AnonBox.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "duckmail",
            Name = "DuckMail",
            Website = "duckmail.sbs",
            Generate = o => MailTm.Generate("duckmail", "https://api.duckmail.sbs"),
            GetEmails = (e, t) => MailTm.GetEmails("https://api.duckmail.sbs", t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "mailinator",
            Name = "Mailinator",
            Website = "mailinator.com",
            Generate = o => Mailinator.Generate(),
            GetEmails = (e, t) => Mailinator.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "tempmail365",
            Name = "Tempmail365",
            Website = "https://tempmail365.cn",
            Generate = o => Tempmail365.Generate(o.Domain),
            GetEmails = (e, t) => Tempmail365.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "tempinbox",
            Name = "TempInbox",
            Website = "https://www.tempinbox.xyz",
            Generate = o => Tempinbox.Generate(o.Domain),
            GetEmails = (e, t) => Tempinbox.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "byom",
            Name = "Byom",
            Website = "byom.de",
            Generate = o => Byom.Generate(),
            GetEmails = (e, t) => Byom.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "anonymmail",
            Name = "AnonymMail",
            Website = "anonymmail.net",
            Generate = o => Anonymmail.Generate(),
            GetEmails = (e, t) => Anonymmail.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "eyepaste",
            Name = "EyePaste",
            Website = "eyepaste.com",
            Generate = o => Eyepaste.Generate(),
            GetEmails = (e, t) => Eyepaste.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "mail-sunls",
            Name = "Mail Sunls",
            Website = "mail.sunls.de",
            Generate = o => MailSunls.Generate(),
            GetEmails = (e, t) => MailSunls.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "expressinboxhub",
            Name = "ExpressInboxHub",
            Website = "expressinboxhub.com",
            Generate = o => ExpressInboxHub.Generate(),
            GetEmails = (e, t) => ExpressInboxHub.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "lroid",
            Name = "Lroid",
            Website = "lroid.com",
            Generate = o => Lroid.Generate(),
            GetEmails = (e, t) => Lroid.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "haribu",
            Name = "Haribu",
            Website = "haribu.net",
            Generate = o => Haribu.Generate(),
            GetEmails = (e, t) => Haribu.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "rootsh",
            Name = "Rootsh(BccTo)",
            Website = "rootsh.com",
            Generate = o => Rootsh.Generate(o.Domain),
            GetEmails = (e, t) => Rootsh.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "fake-email-site",
            Name = "FakeEmailSite",
            Website = "fake-email.site",
            Generate = o => FakeEmailSite.Generate(),
            GetEmails = (e, t) => FakeEmailSite.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "mohmal",
            Name = "Mohmal",
            Website = "mohmal.com",
            Generate = o => Mohmal.Generate(),
            GetEmails = (e, t) => Mohmal.GetEmails(e, t),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "mailgolem",
            Name = "MailGolem",
            Website = "mailgolem.com",
            Generate = o => Mailgolem.Generate(),
            GetEmails = (e, t) => Mailgolem.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "best-temp-mail",
            Name = "BestTempMail",
            Website = "best-temp-mail.com",
            Generate = o => BestTempMail.Generate(),
            GetEmails = (e, t) => BestTempMail.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "disposablemail-app",
            Name = "DisposableMail",
            Website = "disposablemail.app",
            Generate = o => DisposablemailApp.Generate(),
            GetEmails = (e, t) => DisposablemailApp.GetEmails(e, t),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "mailtemp-cc",
            Name = "MailTemp.cc",
            Website = "mailtemp.cc",
            Generate = o => MailtempCc.Generate(),
            GetEmails = (e, t) => MailtempCc.GetEmails(e, t),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "minuteinbox",
            Name = "MinuteInbox",
            Website = "minuteinbox.com",
            Generate = o => Minuteinbox.Generate(),
            GetEmails = (e, t) => Minuteinbox.GetEmails(e, t),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "mailcatch",
            Name = "MailCatch",
            Website = "mailcatch.com",
            Generate = o => MailCatch.Generate(),
            GetEmails = (e, t) => MailCatch.GetEmails(e, t),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "tempemail-co",
            Name = "TempEmail.co",
            Website = "tempemail.co",
            Generate = o => TempEmailCo.Generate(),
            GetEmails = (e, t) => TempEmailCo.GetEmails(e, t),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "tempemails-net",
            Name = "TempEmails.net",
            Website = "tempemails.net",
            Generate = o => TempEmailsNet.Generate(),
            GetEmails = (e, t) => TempEmailsNet.GetEmails(e, t),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "altmails",
            Name = "AltMails",
            Website = "tempmail.altmails.com",
            Generate = o => Altmails.Generate(),
            GetEmails = (e, t) => Altmails.GetEmails(e, t),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "tempemail-info",
            Name = "TempEmailInfo",
            Website = "tempemail.info",
            Generate = o => TempEmailInfo.Generate(),
            GetEmails = (e, t) => TempEmailInfo.GetEmails(e, t),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "smailpro",
            Name = "SmailPro",
            Website = "smailpro.com",
            Generate = o => Smailpro.Generate(),
            GetEmails = (e, t) => Smailpro.GetEmails(e, t),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "tempmailten",
            Name = "TempMailTen",
            Website = "tempmailten.com",
            Generate = o => TempMailTen.Generate(),
            GetEmails = (e, t) => TempMailTen.GetEmails(e, t),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "maildrop-cc",
            Name = "MailDrop.cc",
            Website = "maildrop.cc",
            Generate = o => MailDropCc.Generate(),
            GetEmails = (e, t) => MailDropCc.GetEmails(e, t),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "10minutemail-net",
            Name = "10MinuteMail.net",
            Website = "10minutemail.net",
            Generate = o => TenMinuteMailNet.Generate(),
            GetEmails = (e, t) => TenMinuteMailNet.GetEmails(e, t),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "linshiyouxiang-net",
            Name = "临时邮箱(linshiyouxiang)",
            Website = "linshiyouxiang.net",
            Generate = o => LinshiyouxiangNet.Generate(),
            GetEmails = (e, t) => LinshiyouxiangNet.GetEmails(e, t),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "tempmail-fyi",
            Name = "Temp-Mail.fyi",
            Website = "temp-mail.fyi",
            Generate = o => TempMailFyi.Generate(),
            GetEmails = (e, t) => TempMailFyi.GetEmails(e, t),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "disposablemail-com",
            Name = "DisposableMail.com",
            Website = "disposablemail.com",
            Generate = o => DisposablemailCom.Generate(),
            GetEmails = (e, t) => DisposablemailCom.GetEmails(e, t),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "tempp-mails",
            Name = "TemppMails",
            Website = "tempp-mails.com",
            Generate = o => TemppMails.Generate(),
            GetEmails = (e, t) => TemppMails.GetEmails(e, t),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "emailtemp-org",
            Name = "EmailTemp",
            Website = "emailtemp.org",
            Generate = o => EmailtempOrg.Generate(),
            GetEmails = (e, t) => EmailtempOrg.GetEmails(e, t),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "mytempmail-cc",
            Name = "MyTempMail.cc",
            Website = "mytempmail.cc",
            Generate = o => MytempmailCc.Generate(),
            GetEmails = (e, t) => MytempmailCc.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "temp-mail-now",
            Name = "TempMailNow",
            Website = "temp-mail.now",
            Generate = o => TempMailNow.Generate(),
            GetEmails = (e, t) => TempMailNow.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "mail-td",
            Name = "Mail.td",
            Website = "mail.td",
            Generate = o => MailTd.Generate(),
            GetEmails = (e, t) => MailTd.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "mailhole-de",
            Name = "Mailhole.de",
            Website = "mailhole.de",
            Generate = o => MailholeDe.Generate(),
            GetEmails = (e, t) => MailholeDe.GetEmails(string.IsNullOrEmpty(t) ? e : t),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "tmail-link",
            Name = "TMail.link",
            Website = "tmail.link",
            Generate = o => TmailLink.Generate(),
            GetEmails = (e, t) => TmailLink.GetEmails(e, t),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "24mail-chacuo",
            Name = "24Mail Chacuo",
            Website = "24mail.chacuo.net",
            Generate = o => ChacuoMail.Generate(o.Domain),
            GetEmails = (e, t) => ChacuoMail.GetEmails(e, t),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "nimail",
            Name = "NiMail",
            Website = "nimail.cn",
            Generate = o => Nimail.Generate(),
            GetEmails = (e, t) => Nimail.GetEmails(string.IsNullOrEmpty(t) ? e : t),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "freecustom",
            Name = "FreeCustom.Email",
            Website = "freecustom.email",
            Generate = o => FreeCustom.Generate(),
            GetEmails = (e, t) => FreeCustom.GetEmails(string.IsNullOrEmpty(t) ? e : t),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "16888888-cyou",
            Name = "Mailmomy (16888888.cyou)",
            Website = "mailmomy.com",
            Generate = o => Mailmomy.Generate("16888888-cyou", "16888888.cyou"),
            GetEmails = (e, t) => Mailmomy.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "17666688-shop",
            Name = "Mailmomy (17666688.shop)",
            Website = "mailmomy.com",
            Generate = o => Mailmomy.Generate("17666688-shop", "17666688.shop"),
            GetEmails = (e, t) => Mailmomy.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "282mail-com",
            Name = "Mailmomy (282mail.com)",
            Website = "mailmomy.com",
            Generate = o => Mailmomy.Generate("282mail-com", "282mail.com"),
            GetEmails = (e, t) => Mailmomy.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "blackhole-djurby-se",
            Name = "Mailinator (blackhole.djurby.se)",
            Website = "mailinator.com",
            Generate = o => Mailinator.Generate("blackhole-djurby-se", "blackhole.djurby.se"),
            GetEmails = (e, t) => Mailinator.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "block-bdea-cc",
            Name = "Mailinator (block.bdea.cc)",
            Website = "mailinator.com",
            Generate = o => Mailinator.Generate("block-bdea-cc", "block.bdea.cc"),
            GetEmails = (e, t) => Mailinator.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "bsdu32-buzz",
            Name = "Mailmomy (bsdu32.buzz)",
            Website = "mailmomy.com",
            Generate = o => Mailmomy.Generate("bsdu32-buzz", "bsdu32.buzz"),
            GetEmails = (e, t) => Mailmomy.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "b-smelly-cc",
            Name = "Mailinator (b.smelly.cc)",
            Website = "mailinator.com",
            Generate = o => Mailinator.Generate("b-smelly-cc", "b.smelly.cc"),
            GetEmails = (e, t) => Mailinator.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "carlton183-changeip-net",
            Name = "Mailinator (183carlton.changeip.net)",
            Website = "mailinator.com",
            Generate = o => Mailinator.Generate("carlton183-changeip-net", "183carlton.changeip.net"),
            GetEmails = (e, t) => Mailinator.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "dea-soon-it",
            Name = "Mailinator (dea.soon.it)",
            Website = "mailinator.com",
            Generate = o => Mailinator.Generate("dea-soon-it", "dea.soon.it"),
            GetEmails = (e, t) => Mailinator.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "disposable-al-sudani-com",
            Name = "Mailinator (disposable.al-sudani.com)",
            Website = "mailinator.com",
            Generate = o => Mailinator.Generate("disposable-al-sudani-com", "disposable.al-sudani.com"),
            GetEmails = (e, t) => Mailinator.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "disposable-nogonad-nl",
            Name = "Mailinator (disposable.nogonad.nl)",
            Website = "mailinator.com",
            Generate = o => Mailinator.Generate("disposable-nogonad-nl", "disposable.nogonad.nl"),
            GetEmails = (e, t) => Mailinator.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "doxu243-buzz",
            Name = "Mailmomy (doxu243.buzz)",
            Website = "mailmomy.com",
            Generate = o => Mailmomy.Generate("doxu243-buzz", "doxu243.buzz"),
            GetEmails = (e, t) => Mailmomy.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "easyme-pro",
            Name = "Mailmomy (easyme.pro)",
            Website = "mailmomy.com",
            Generate = o => Mailmomy.Generate("easyme-pro", "easyme.pro"),
            GetEmails = (e, t) => Mailmomy.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "ebs-com-ar",
            Name = "Mailinator (ebs.com.ar)",
            Website = "mailinator.com",
            Generate = o => Mailinator.Generate("ebs-com-ar", "ebs.com.ar"),
            GetEmails = (e, t) => Mailinator.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "etgdev-de",
            Name = "Mailinator (etgdev.de)",
            Website = "mailinator.com",
            Generate = o => Mailinator.Generate("etgdev-de", "etgdev.de"),
            GetEmails = (e, t) => Mailinator.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "evergreenco-shop",
            Name = "Mailmomy (evergreenco.shop)",
            Website = "mailmomy.com",
            Generate = o => Mailmomy.Generate("evergreenco-shop", "evergreenco.shop"),
            GetEmails = (e, t) => Mailmomy.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "fwd2m-eszett-es",
            Name = "Mailinator (fwd2m.eszett.es)",
            Website = "mailinator.com",
            Generate = o => Mailinator.Generate("fwd2m-eszett-es", "fwd2m.eszett.es"),
            GetEmails = (e, t) => Mailinator.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "jama-trenet-eu",
            Name = "Mailinator (jama.trenet.eu)",
            Website = "mailinator.com",
            Generate = o => Mailinator.Generate("jama-trenet-eu", "jama.trenet.eu"),
            GetEmails = (e, t) => Mailinator.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "j-fairuse-org",
            Name = "Mailinator (j.fairuse.org)",
            Website = "mailinator.com",
            Generate = o => Mailinator.Generate("j-fairuse-org", "j.fairuse.org"),
            GetEmails = (e, t) => Mailinator.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "layueming-pics",
            Name = "Mailmomy (layueming.pics)",
            Website = "mailmomy.com",
            Generate = o => Mailmomy.Generate("layueming-pics", "layueming.pics"),
            GetEmails = (e, t) => Mailmomy.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "m-887-at",
            Name = "Mailinator (m.887.at)",
            Website = "mailinator.com",
            Generate = o => Mailinator.Generate("m-887-at", "m.887.at"),
            GetEmails = (e, t) => Mailinator.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "m8r-davidfuhr-de",
            Name = "Mailinator (m8r.davidfuhr.de)",
            Website = "mailinator.com",
            Generate = o => Mailinator.Generate("m8r-davidfuhr-de", "m8r.davidfuhr.de"),
            GetEmails = (e, t) => Mailinator.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "m8r-mcasal-com",
            Name = "Mailinator (m8r.mcasal.com)",
            Website = "mailinator.com",
            Generate = o => Mailinator.Generate("m8r-mcasal-com", "m8r.mcasal.com"),
            GetEmails = (e, t) => Mailinator.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "mail-bentrask-com",
            Name = "Mailinator (mail.bentrask.com)",
            Website = "mailinator.com",
            Generate = o => Mailinator.Generate("mail-bentrask-com", "mail.bentrask.com"),
            GetEmails = (e, t) => Mailinator.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "mail-fsmash-org",
            Name = "Mailinator (mail.fsmash.org)",
            Website = "mailinator.com",
            Generate = o => Mailinator.Generate("mail-fsmash-org", "mail.fsmash.org"),
            GetEmails = (e, t) => Mailinator.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "mailinatorzz-mooo-com",
            Name = "Mailinator (mailinatorzz.mooo.com)",
            Website = "mailinator.com",
            Generate = o => Mailinator.Generate("mailinatorzz-mooo-com", "mailinatorzz.mooo.com"),
            GetEmails = (e, t) => Mailinator.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "mi-meon-be",
            Name = "Mailinator (mi.meon.be)",
            Website = "mailinator.com",
            Generate = o => Mailinator.Generate("mi-meon-be", "mi.meon.be"),
            GetEmails = (e, t) => Mailinator.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "mingyuekeji-online",
            Name = "Mailmomy (mingyuekeji.online)",
            Website = "mailmomy.com",
            Generate = o => Mailmomy.Generate("mingyuekeji-online", "mingyuekeji.online"),
            GetEmails = (e, t) => Mailmomy.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "mingyueming-click",
            Name = "Mailmomy (mingyueming.click)",
            Website = "mailmomy.com",
            Generate = o => Mailmomy.Generate("mingyueming-click", "mingyueming.click"),
            GetEmails = (e, t) => Mailmomy.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "mingyueming-shop",
            Name = "Mailmomy (mingyueming.shop)",
            Website = "mailmomy.com",
            Generate = o => Mailmomy.Generate("mingyueming-shop", "mingyueming.shop"),
            GetEmails = (e, t) => Mailmomy.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "mingyukeji-lol",
            Name = "Mailmomy (mingyukeji.lol)",
            Website = "mailmomy.com",
            Generate = o => Mailmomy.Generate("mingyukeji-lol", "mingyukeji.lol"),
            GetEmails = (e, t) => Mailmomy.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "mn-curppa-com",
            Name = "Mailinator (mn.curppa.com)",
            Website = "mailinator.com",
            Generate = o => Mailinator.Generate("mn-curppa-com", "mn.curppa.com"),
            GetEmails = (e, t) => Mailinator.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "m-nik-me",
            Name = "Mailinator (m.nik.me)",
            Website = "mailinator.com",
            Generate = o => Mailinator.Generate("m-nik-me", "m.nik.me"),
            GetEmails = (e, t) => Mailinator.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "mtmdev-com",
            Name = "Mailinator (mtmdev.com)",
            Website = "mailinator.com",
            Generate = o => Mailinator.Generate("mtmdev-com", "mtmdev.com"),
            GetEmails = (e, t) => Mailinator.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "nospam-thurstons-us",
            Name = "Mailinator (nospam.thurstons.us)",
            Website = "mailinator.com",
            Generate = o => Mailinator.Generate("nospam-thurstons-us", "nospam.thurstons.us"),
            GetEmails = (e, t) => Mailinator.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "notfond-404-mn",
            Name = "Mailinator (notfond.404.mn)",
            Website = "mailinator.com",
            Generate = o => Mailinator.Generate("notfond-404-mn", "notfond.404.mn"),
            GetEmails = (e, t) => Mailinator.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "null-k3vin-net",
            Name = "Mailinator (null.k3vin.net)",
            Website = "mailinator.com",
            Generate = o => Mailinator.Generate("null-k3vin-net", "null.k3vin.net"),
            GetEmails = (e, t) => Mailinator.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "nuxh62-space",
            Name = "Mailmomy (nuxh62.space)",
            Website = "mailmomy.com",
            Generate = o => Mailmomy.Generate("nuxh62-space", "nuxh62.space"),
            GetEmails = (e, t) => Mailmomy.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "proid-cloud-ip-cc",
            Name = "Mailmomy (proid.cloud-ip.cc)",
            Website = "mailmomy.com",
            Generate = o => Mailmomy.Generate("proid-cloud-ip-cc", "proid.cloud-ip.cc"),
            GetEmails = (e, t) => Mailmomy.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "ramjane-mooo-com",
            Name = "Mailinator (ramjane.mooo.com)",
            Website = "mailinator.com",
            Generate = o => Mailinator.Generate("ramjane-mooo-com", "ramjane.mooo.com"),
            GetEmails = (e, t) => Mailinator.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "rauxa-seny-cat",
            Name = "Mailinator (rauxa.seny.cat)",
            Website = "mailinator.com",
            Generate = o => Mailinator.Generate("rauxa-seny-cat", "rauxa.seny.cat"),
            GetEmails = (e, t) => Mailinator.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "really-istrash-com",
            Name = "Mailinator (really.istrash.com)",
            Website = "mailinator.com",
            Generate = o => Mailinator.Generate("really-istrash-com", "really.istrash.com"),
            GetEmails = (e, t) => Mailinator.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "sbook-pics",
            Name = "Mailmomy (sbook.pics)",
            Website = "mailmomy.com",
            Generate = o => Mailmomy.Generate("sbook-pics", "sbook.pics"),
            GetEmails = (e, t) => Mailmomy.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "spam-hortuk-ovh",
            Name = "Mailinator (spam.hortuk.ovh)",
            Website = "mailinator.com",
            Generate = o => Mailinator.Generate("spam-hortuk-ovh", "spam.hortuk.ovh"),
            GetEmails = (e, t) => Mailinator.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "sp-woot-at",
            Name = "Mailinator (sp.woot.at)",
            Website = "mailinator.com",
            Generate = o => Mailinator.Generate("sp-woot-at", "sp.woot.at"),
            GetEmails = (e, t) => Mailinator.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "test-unergie-com",
            Name = "Mailinator (test.unergie.com)",
            Website = "mailinator.com",
            Generate = o => Mailinator.Generate("test-unergie-com", "test.unergie.com"),
            GetEmails = (e, t) => Mailinator.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "torch-yi-org",
            Name = "Mailinator (torch.yi.org)",
            Website = "mailinator.com",
            Generate = o => Mailinator.Generate("torch-yi-org", "torch.yi.org"),
            GetEmails = (e, t) => Mailinator.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "t-zibet-net",
            Name = "Mailinator (t.zibet.net)",
            Website = "mailinator.com",
            Generate = o => Mailinator.Generate("t-zibet-net", "t.zibet.net"),
            GetEmails = (e, t) => Mailinator.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "xue32-buzz",
            Name = "Mailmomy (xue32.buzz)",
            Website = "mailmomy.com",
            Generate = o => Mailmomy.Generate("xue32-buzz", "xue32.buzz"),
            GetEmails = (e, t) => Mailmomy.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "apihz",
            Name = "ApiHz TempMail",
            Website = "apihz.cn",
            Generate = o => Apihz.Generate(),
            GetEmails = (e, t) => Apihz.GetEmails(t),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "sogetthis-com",
            Name = "Mailinator (sogetthis.com)",
            Website = "mailinator.com",
            Generate = o => Mailinator.Generate("sogetthis-com", "sogetthis.com"),
            GetEmails = (e, t) => Mailinator.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "bobmail-info",
            Name = "Mailinator (bobmail.info)",
            Website = "mailinator.com",
            Generate = o => Mailinator.Generate("bobmail-info", "bobmail.info"),
            GetEmails = (e, t) => Mailinator.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "suremail-info",
            Name = "Mailinator (suremail.info)",
            Website = "mailinator.com",
            Generate = o => Mailinator.Generate("suremail-info", "suremail.info"),
            GetEmails = (e, t) => Mailinator.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "binkmail-com",
            Name = "Mailinator (binkmail.com)",
            Website = "mailinator.com",
            Generate = o => Mailinator.Generate("binkmail-com", "binkmail.com"),
            GetEmails = (e, t) => Mailinator.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "veryrealemail-com",
            Name = "Mailinator (veryrealemail.com)",
            Website = "mailinator.com",
            Generate = o => Mailinator.Generate("veryrealemail-com", "veryrealemail.com"),
            GetEmails = (e, t) => Mailinator.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "mailmomy",
            Name = "Mailmomy",
            Website = "mailmomy.com",
            Generate = o => Mailmomy.Generate(),
            GetEmails = (e, t) => Mailmomy.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "chammy-info",
            Name = "Mailinator (chammy.info)",
            Website = "mailinator.com",
            Generate = o => Mailinator.Generate("chammy-info", "chammy.info"),
            GetEmails = (e, t) => Mailinator.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "thisisnotmyrealemail-com",
            Name = "Mailinator (thisisnotmyrealemail.com)",
            Website = "mailinator.com",
            Generate = o => Mailinator.Generate("thisisnotmyrealemail-com", "thisisnotmyrealemail.com"),
            GetEmails = (e, t) => Mailinator.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "notmailinator-com",
            Name = "Mailinator (notmailinator.com)",
            Website = "mailinator.com",
            Generate = o => Mailinator.Generate("notmailinator-com", "notmailinator.com"),
            GetEmails = (e, t) => Mailinator.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "spamhereplease-com",
            Name = "Mailinator (spamhereplease.com)",
            Website = "mailinator.com",
            Generate = o => Mailinator.Generate("spamhereplease-com", "spamhereplease.com"),
            GetEmails = (e, t) => Mailinator.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "sendspamhere-com",
            Name = "Mailinator (sendspamhere.com)",
            Website = "mailinator.com",
            Generate = o => Mailinator.Generate("sendspamhere-com", "sendspamhere.com"),
            GetEmails = (e, t) => Mailinator.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "sendfree-org",
            Name = "Mailinator (sendfree.org)",
            Website = "mailinator.com",
            Generate = o => Mailinator.Generate("sendfree-org", "sendfree.org"),
            GetEmails = (e, t) => Mailinator.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "junk-beats-org",
            Name = "Mailinator (junk.beats.org)",
            Website = "mailinator.com",
            Generate = o => Mailinator.Generate("junk-beats-org", "junk.beats.org"),
            GetEmails = (e, t) => Mailinator.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "junk-ihmehl-com",
            Name = "Mailinator (junk.ihmehl.com)",
            Website = "mailinator.com",
            Generate = o => Mailinator.Generate("junk-ihmehl-com", "junk.ihmehl.com"),
            GetEmails = (e, t) => Mailinator.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "junk-noplay-org",
            Name = "Mailinator (junk.noplay.org)",
            Website = "mailinator.com",
            Generate = o => Mailinator.Generate("junk-noplay-org", "junk.noplay.org"),
            GetEmails = (e, t) => Mailinator.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "junk-vanillasystem-com",
            Name = "Mailinator (junk.vanillasystem.com)",
            Website = "mailinator.com",
            Generate = o => Mailinator.Generate("junk-vanillasystem-com", "junk.vanillasystem.com"),
            GetEmails = (e, t) => Mailinator.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "spam-jasonpearce-com",
            Name = "Mailinator (spam.jasonpearce.com)",
            Website = "mailinator.com",
            Generate = o => Mailinator.Generate("spam-jasonpearce-com", "spam.jasonpearce.com"),
            GetEmails = (e, t) => Mailinator.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "fish-skytale-net",
            Name = "Mailinator (fish.skytale.net)",
            Website = "mailinator.com",
            Generate = o => Mailinator.Generate("fish-skytale-net", "fish.skytale.net"),
            GetEmails = (e, t) => Mailinator.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "spam-mccrew-com",
            Name = "Mailinator (spam.mccrew.com)",
            Website = "mailinator.com",
            Generate = o => Mailinator.Generate("spam-mccrew-com", "spam.mccrew.com"),
            GetEmails = (e, t) => Mailinator.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "dropmail-click",
            Name = "DropMail.click",
            Website = "dropmail.click",
            Generate = o => DropMailClick.Generate(),
            GetEmails = (e, t) => DropMailClick.GetEmails(string.IsNullOrEmpty(t) ? e : t),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "spam-coroiu-com",
            Name = "Mailinator (spam.coroiu.com)",
            Website = "mailinator.com",
            Generate = o => Mailinator.Generate("spam-coroiu-com", "spam.coroiu.com"),
            GetEmails = (e, t) => Mailinator.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "spam-deluser-net",
            Name = "Mailinator (spam.deluser.net)",
            Website = "mailinator.com",
            Generate = o => Mailinator.Generate("spam-deluser-net", "spam.deluser.net"),
            GetEmails = (e, t) => Mailinator.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "spam-dhsf-net",
            Name = "Mailinator (spam.dhsf.net)",
            Website = "mailinator.com",
            Generate = o => Mailinator.Generate("spam-dhsf-net", "spam.dhsf.net"),
            GetEmails = (e, t) => Mailinator.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "spam-lucatnt-com",
            Name = "Mailinator (spam.lucatnt.com)",
            Website = "mailinator.com",
            Generate = o => Mailinator.Generate("spam-lucatnt-com", "spam.lucatnt.com"),
            GetEmails = (e, t) => Mailinator.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "spam-lyceum-life-com-ru",
            Name = "Mailinator (spam.lyceum-life.com.ru)",
            Website = "mailinator.com",
            Generate = o => Mailinator.Generate("spam-lyceum-life-com-ru", "spam.lyceum-life.com.ru"),
            GetEmails = (e, t) => Mailinator.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "spam-netpirates-net",
            Name = "Mailinator (spam.netpirates.net)",
            Website = "mailinator.com",
            Generate = o => Mailinator.Generate("spam-netpirates-net", "spam.netpirates.net"),
            GetEmails = (e, t) => Mailinator.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "spam-no-ip-net",
            Name = "Mailinator (spam.no-ip.net)",
            Website = "mailinator.com",
            Generate = o => Mailinator.Generate("spam-no-ip-net", "spam.no-ip.net"),
            GetEmails = (e, t) => Mailinator.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "spam-ozh-org",
            Name = "Mailinator (spam.ozh.org)",
            Website = "mailinator.com",
            Generate = o => Mailinator.Generate("spam-ozh-org", "spam.ozh.org"),
            GetEmails = (e, t) => Mailinator.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "spam-pyphus-org",
            Name = "Mailinator (spam.pyphus.org)",
            Website = "mailinator.com",
            Generate = o => Mailinator.Generate("spam-pyphus-org", "spam.pyphus.org"),
            GetEmails = (e, t) => Mailinator.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "spam-shep-pw",
            Name = "Mailinator (spam.shep.pw)",
            Website = "mailinator.com",
            Generate = o => Mailinator.Generate("spam-shep-pw", "spam.shep.pw"),
            GetEmails = (e, t) => Mailinator.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "spam-wtf-at",
            Name = "Mailinator (spam.wtf.at)",
            Website = "mailinator.com",
            Generate = o => Mailinator.Generate("spam-wtf-at", "spam.wtf.at"),
            GetEmails = (e, t) => Mailinator.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "spam-wulczer-org",
            Name = "Mailinator (spam.wulczer.org)",
            Website = "mailinator.com",
            Generate = o => Mailinator.Generate("spam-wulczer-org", "spam.wulczer.org"),
            GetEmails = (e, t) => Mailinator.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "crap-kakadua-net",
            Name = "Mailinator (crap.kakadua.net)",
            Website = "mailinator.com",
            Generate = o => Mailinator.Generate("crap-kakadua-net", "crap.kakadua.net"),
            GetEmails = (e, t) => Mailinator.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "spam-janlugt-nl",
            Name = "Mailinator (spam.janlugt.nl)",
            Website = "mailinator.com",
            Generate = o => Mailinator.Generate("spam-janlugt-nl", "spam.janlugt.nl"),
            GetEmails = (e, t) => Mailinator.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "min-burningfish-net",
            Name = "Mailinator (min.burningfish.net)",
            Website = "mailinator.com",
            Generate = o => Mailinator.Generate("min-burningfish-net", "min.burningfish.net"),
            GetEmails = (e, t) => Mailinator.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "sink-fblay-com",
            Name = "Mailinator (sink.fblay.com)",
            Website = "mailinator.com",
            Generate = o => Mailinator.Generate("sink-fblay-com", "sink.fblay.com"),
            GetEmails = (e, t) => Mailinator.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "tempgmailer",
            Name = "TempGmailer",
            Website = "tempgmailer.com",
            Generate = o => TempGmailer.Generate(),
            GetEmails = (e, t) => TempGmailer.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "temp-mail-org",
            Name = "Temp-Mail.org",
            Website = "temp-mail.org",
            Generate = o => TempMailOrg.Generate(),
            GetEmails = (e, t) => TempMailOrg.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "xkx-me",
            Name = "XKX.me",
            Website = "xkx.me",
            Generate = o => XkxMe.Generate(),
            GetEmails = (e, t) => XkxMe.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "gonebox-email",
            Name = "Gonebox Email",
            Website = "gonebox.email",
            Generate = o => GoneboxEmail.Generate(),
            GetEmails = (e, t) => GoneboxEmail.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "mailcat-ai",
            Name = "Mailcat AI",
            Website = "mailcat.ai",
            Generate = o => MailcatAi.Generate(),
            GetEmails = (e, t) => MailcatAi.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "tempgo-email",
            Name = "TempGo Email",
            Website = "tempgo.email",
            Generate = o => TempGoEmail.Generate(),
            GetEmails = (e, t) => TempGoEmail.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "restmail-net",
            Name = "Restmail.net",
            Website = "restmail.net",
            Generate = o => RestmailNet.Generate(),
            GetEmails = (e, t) => RestmailNet.GetEmails(e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "dropmail-me",
            Name = "DropMail.me",
            Website = "dropmail.me",
            Generate = o => DropMailMe.Generate(),
            GetEmails = (e, t) => DropMailMe.GetEmails(t, e),
        });
        Registry.Register(new ChannelSpec
        {
            Channel = "ten-minute-mail-net",
            Name = "10MinuteMail.net",
            Website = "10minutemail.net",
            Generate = o => TenMinuteMailNetJson.Generate(),
            GetEmails = (e, t) => TenMinuteMailNetJson.GetEmails(t, e),
        });
    }
}
