# frozen_string_literal: true

require_relative "channels_data"

# provider 模块懒加载映射：模块名 => 相对文件路径
# 通过 Ruby autoload 延迟到渠道首次真正调用时才加载对应 provider 文件，
# 避免加载 SDK 时一次性 require 全部 provider（及其级联依赖），显著降低启动开销与常驻内存。
module TempmailSdk
  module Providers
    # 每个 provider 文件恰好定义一个同名子模块，映射一一对应（零歧义）。
    PROVIDER_AUTOLOADS = {
      Tempmail: "tempmail", MailTm: "mail_tm", Duckmail: "duckmail",
      Guerrillamail: "guerrillamail", GuerrillamailMirrors: "guerrillamail_mirrors",
      TempMailIo: "temp_mail_io", TempmailLol: "tempmail_lol", TempmailPlus: "tempmail_plus",
      RestmailNet: "restmail_net", Inboxkitten: "inboxkitten", Maildrop: "maildrop",
      MailCx: "mail_cx", OneSecMail: "one_sec_mail", Moakt: "moakt", Getnada: "getnada",
      Mailinator: "mailinator", Mailmomy: "mailmomy", Catchmail: "catchmail",
      Mailforspam: "mailforspam", FakeLegal: "fake_legal", TenminuteOne: "tenminute_one",
      Zhujump: "zhujump", Anonymmail: "anonymmail", Byom: "byom", Cleantempmail: "cleantempmail",
      DevmailUk: "devmail_uk", DropmailClick: "dropmail_click", FakeEmailSite: "fake_email_site",
      Fmail: "fmail", Freecustom: "freecustom", GoneboxEmail: "gonebox_email",
      Harakirimail: "harakirimail", Inboxes: "inboxes", M2u: "m2u", Mail10s: "mail10s",
      Mail123: "mail123", Mffac: "mffac", Tempmailc: "tempmailc", NeighboursSh: "neighbours_sh",
      Temporam: "temporam", Neighbours: "neighbours", TempyEmail: "tempy_email", Ockito: "ockito",
      Tempmail365: "tempmail365", Tempinbox: "tempinbox", MailSunls: "mail_sunls",
      Smailpro: "smailpro", MailholeDe: "mailhole_de", Nimail: "nimail", Rootsh: "rootsh",
      ShittyEmail: "shitty_email", TaEasy: "ta_easy", TempMailNow: "temp_mail_now",
      TempMailOrg: "temp_mail_org", TempemailCo: "tempemail_co", TempemailInfo: "tempemail_info",
      TempemailsNet: "tempemails_net", Tempfastmail: "tempfastmail", Tempgmailer: "tempgmailer",
      TempgoEmail: "tempgo_email", TempmailFish: "tempmail_fish", TempmailFyi: "tempmail_fyi",
      TempmailLolV2: "tempmail_lol_v2", Tempmailpro: "tempmailpro", Tempmailten: "tempmailten",
      TempMails: "tempp_mails", TenMinuteMailNet: "ten_minute_mail_net",
      Throwawaymail: "throwawaymail", TmailLink: "tmail_link",
      Uncorreotemporal: "uncorreotemporal", Webmailtemp: "webmailtemp", XkxMe: "xkx_me",
      Altmails: "altmails", Apihz: "apihz", Awamail: "awamail", BestTempMail: "best_temp_mail",
      ChatgptOrgUk: "chatgpt_org_uk", Disposablemail: "disposablemail",
      DisposablemailApp: "disposablemail_app", Email10min: "email10min",
      Emailnator: "emailnator", EmailtempOrg: "emailtemp_org",
      Expressinboxhub: "expressinboxhub", Fakemail: "fakemail",
      LinshiyouxiangNet: "linshiyouxiang_net", MailTd: "mail_td",
      TempmailCn: "tempmail_cn", Linshiyou: "linshiyou", Mailnesia: "mailnesia",
      Dropmail: "dropmail", SmailPw: "smail_pw", Vip215: "vip_215",
      Tempgbox: "tempgbox", Anonbox: "anonbox", Eyepaste: "eyepaste",
      Lroid: "lroid", Haribu: "haribu", Mohmal: "mohmal", Mailgolem: "mailgolem",
      MytempmailCc: "mytempmail_cc", MailtempCc: "mailtemp_cc", Minuteinbox: "minuteinbox",
      Mailcatch: "mailcatch", MaildropCc: "maildrop_cc", TenminutemailNet: "tenminutemail_net",
      Openinbox: "openinbox", MailcatAi: "mailcat_ai", DropmailMe: "dropmail_me",
      TwentyfourmailChacuo: "twentyfourmail_chacuo"
    }.freeze

    PROVIDER_AUTOLOADS.each do |mod_name, file|
      autoload mod_name, File.join(__dir__, "providers", file)
    end
  end
end

module TempmailSdk
  # 单个渠道的注册规格
  #
  # generate: 接收 GenerateEmailOptions，返回 EmailInfo 的 Proc
  # get_emails: 接收 (email, token)，返回 Array<Email> 的 Proc
  ChannelSpec = Struct.new(:channel, :name, :website, :generate, :get_emails, keyword_init: true)

  # 渠道注册表：单一事实来源
  #
  # 有序注册表数组 + 映射由同一处 register_channel 派生。
  # list_channels、渠道信息、generate/get_emails 两个 dispatch 全部从注册表派生。
  # 注册顺序即枚举顺序（硬约束，与 baseline 逐行一致）。
  module Registry
    # 有序渠道注册表，注册顺序即枚举顺序
    REGISTRY = []
    # 渠道标识到注册规格的映射，O(1) 查找分发
    REGISTRY_MAP = {}

    module_function

    # 注册一个渠道：追加到有序列表并建立映射；重复注册视为编程错误
    # @param spec [ChannelSpec]
    def register_channel(spec)
      raise "duplicate channel registration: #{spec.channel}" if REGISTRY_MAP.key?(spec.channel)

      REGISTRY << spec
      REGISTRY_MAP[spec.channel] = spec
    end

    # 校验 token 非空
    def require_token(token, channel)
      raise "token is required for #{channel} channel" if token.nil? || token.to_s.empty?

      token
    end

    # 覆写 EmailInfo 的渠道标识（用于同一 provider 派生多个渠道别名）
    def with_channel(info, channel)
      info.channel = channel
      info
    end

    # 尚未实现真实逻辑的渠道桩：可正常枚举，但调用时抛出 NotImplementedProviderError
    # @param channel [String]
    # @return [ChannelSpec]
    def stub_spec(channel, name, website)
      ChannelSpec.new(
        channel: channel, name: name, website: website,
        generate: ->(_opts) { raise NotImplementedProviderError, "channel not implemented yet: #{channel}" },
        get_emails: ->(_email, _token) { raise NotImplementedProviderError, "channel not implemented yet: #{channel}" }
      )
    end

    # 真实实现映射：slug => { generate: Proc(options)->EmailInfo, get_emails: Proc(email, token)->Array<Email> }
    # 未列入者按桩注册。新增真实实现只需在此追加一处。
    def real_implementations
      p = Providers
      {
        "moakt" => {
          generate: ->(o) { p::Moakt.generate_email(o.domain) },
          get_emails: ->(e, t) { p::Moakt.get_emails(e, require_token(t, "moakt")) }
        },
        "drmail-in" => {
          generate: ->(_o) { with_channel(p::Moakt.generate_email("drmail.in"), "drmail-in") },
          get_emails: ->(e, t) { p::Moakt.get_emails(e, require_token(t, "moakt")) }
        },
        "teml-net" => {
          generate: ->(_o) { with_channel(p::Moakt.generate_email("teml.net"), "teml-net") },
          get_emails: ->(e, t) { p::Moakt.get_emails(e, require_token(t, "moakt")) }
        },
        "tmpeml-com" => {
          generate: ->(_o) { with_channel(p::Moakt.generate_email("tmpeml.com"), "tmpeml-com") },
          get_emails: ->(e, t) { p::Moakt.get_emails(e, require_token(t, "moakt")) }
        },
        "tmpbox-net" => {
          generate: ->(_o) { with_channel(p::Moakt.generate_email("tmpbox.net"), "tmpbox-net") },
          get_emails: ->(e, t) { p::Moakt.get_emails(e, require_token(t, "moakt")) }
        },
        "moakt-cc" => {
          generate: ->(_o) { with_channel(p::Moakt.generate_email("moakt.cc"), "moakt-cc") },
          get_emails: ->(e, t) { p::Moakt.get_emails(e, require_token(t, "moakt")) }
        },
        "disbox-net" => {
          generate: ->(_o) { with_channel(p::Moakt.generate_email("disbox.net"), "disbox-net") },
          get_emails: ->(e, t) { p::Moakt.get_emails(e, require_token(t, "moakt")) }
        },
        "tmpmail-org" => {
          generate: ->(_o) { with_channel(p::Moakt.generate_email("tmpmail.org"), "tmpmail-org") },
          get_emails: ->(e, t) { p::Moakt.get_emails(e, require_token(t, "moakt")) }
        },
        "tmpmail-net" => {
          generate: ->(_o) { with_channel(p::Moakt.generate_email("tmpmail.net"), "tmpmail-net") },
          get_emails: ->(e, t) { p::Moakt.get_emails(e, require_token(t, "moakt")) }
        },
        "tmails-net" => {
          generate: ->(_o) { with_channel(p::Moakt.generate_email("tmails.net"), "tmails-net") },
          get_emails: ->(e, t) { p::Moakt.get_emails(e, require_token(t, "moakt")) }
        },
        "disbox-org" => {
          generate: ->(_o) { with_channel(p::Moakt.generate_email("disbox.org"), "disbox-org") },
          get_emails: ->(e, t) { p::Moakt.get_emails(e, require_token(t, "moakt")) }
        },
        "moakt-co" => {
          generate: ->(_o) { with_channel(p::Moakt.generate_email("moakt.co"), "moakt-co") },
          get_emails: ->(e, t) { p::Moakt.get_emails(e, require_token(t, "moakt")) }
        },
        "moakt-ws" => {
          generate: ->(_o) { with_channel(p::Moakt.generate_email("moakt.ws"), "moakt-ws") },
          get_emails: ->(e, t) { p::Moakt.get_emails(e, require_token(t, "moakt")) }
        },
        "tmail-ws" => {
          generate: ->(_o) { with_channel(p::Moakt.generate_email("tmail.ws"), "tmail-ws") },
          get_emails: ->(e, t) { p::Moakt.get_emails(e, require_token(t, "moakt")) }
        },
        "bareed-ws" => {
          generate: ->(_o) { with_channel(p::Moakt.generate_email("bareed.ws"), "bareed-ws") },
          get_emails: ->(e, t) { p::Moakt.get_emails(e, require_token(t, "moakt")) }
        },
        "tempmail" => {
          generate: ->(o) { p::Tempmail.generate_email(o.duration) },
          get_emails: ->(e, _t) { p::Tempmail.get_emails(e) }
        },
        "mail-tm" => {
          generate: ->(_o) { p::MailTm.generate_email },
          get_emails: ->(e, t) { p::MailTm.get_emails(e, require_token(t, "mail-tm")) }
        },
        "web-library-net" => {
          generate: ->(_o) { with_channel(p::MailTm.generate_email, "web-library-net") },
          get_emails: ->(e, t) { p::MailTm.get_emails(e, require_token(t, "mail-tm")) }
        },
        "duckmail" => {
          generate: ->(_o) { p::Duckmail.generate_email },
          get_emails: ->(e, t) { p::Duckmail.get_emails(e, require_token(t, "duckmail")) }
        },
        "guerrillamail" => {
          generate: ->(_o) { p::Guerrillamail.generate_email },
          get_emails: ->(e, t) { p::Guerrillamail.get_emails(e, require_token(t, "guerrillamail")) }
        },
        # guerrillamail 镜像家族：共用同一套 ajax.php API，仅固定 baseURL 不同
        "guerrillamail-com" => {
          generate: ->(_o) { p::GuerrillamailMirrors.generate_email("guerrillamail-com") },
          get_emails: ->(e, t) { p::GuerrillamailMirrors.get_emails("guerrillamail-com", e, require_token(t, "guerrillamail-com")) }
        },
        "sharklasers" => {
          generate: ->(_o) { p::GuerrillamailMirrors.generate_email("sharklasers") },
          get_emails: ->(e, t) { p::GuerrillamailMirrors.get_emails("sharklasers", e, require_token(t, "sharklasers")) }
        },
        "sharklasers-com" => {
          generate: ->(_o) { p::GuerrillamailMirrors.generate_email("sharklasers-com") },
          get_emails: ->(e, t) { p::GuerrillamailMirrors.get_emails("sharklasers-com", e, require_token(t, "sharklasers-com")) }
        },
        "grr-la" => {
          generate: ->(_o) { p::GuerrillamailMirrors.generate_email("grr-la") },
          get_emails: ->(e, t) { p::GuerrillamailMirrors.get_emails("grr-la", e, require_token(t, "grr-la")) }
        },
        "grr-la-com" => {
          generate: ->(_o) { p::GuerrillamailMirrors.generate_email("grr-la-com") },
          get_emails: ->(e, t) { p::GuerrillamailMirrors.get_emails("grr-la-com", e, require_token(t, "grr-la-com")) }
        },
        "guerrillamail-info" => {
          generate: ->(_o) { p::GuerrillamailMirrors.generate_email("guerrillamail-info") },
          get_emails: ->(e, t) { p::GuerrillamailMirrors.get_emails("guerrillamail-info", e, require_token(t, "guerrillamail-info")) }
        },
        "spam4me" => {
          generate: ->(_o) { p::GuerrillamailMirrors.generate_email("spam4me") },
          get_emails: ->(e, t) { p::GuerrillamailMirrors.get_emails("spam4me", e, require_token(t, "spam4me")) }
        },
        "guerrillamail-net" => {
          generate: ->(_o) { p::GuerrillamailMirrors.generate_email("guerrillamail-net") },
          get_emails: ->(e, t) { p::GuerrillamailMirrors.get_emails("guerrillamail-net", e, require_token(t, "guerrillamail-net")) }
        },
        "guerrillamail-org" => {
          generate: ->(_o) { p::GuerrillamailMirrors.generate_email("guerrillamail-org") },
          get_emails: ->(e, t) { p::GuerrillamailMirrors.get_emails("guerrillamail-org", e, require_token(t, "guerrillamail-org")) }
        },
        "guerrillamailblock" => {
          generate: ->(_o) { p::GuerrillamailMirrors.generate_email("guerrillamailblock") },
          get_emails: ->(e, t) { p::GuerrillamailMirrors.get_emails("guerrillamailblock", e, require_token(t, "guerrillamailblock")) }
        },
        "guerrillamail-com-www" => {
          generate: ->(_o) { p::GuerrillamailMirrors.generate_email("guerrillamail-com-www") },
          get_emails: ->(e, t) { p::GuerrillamailMirrors.get_emails("guerrillamail-com-www", e, require_token(t, "guerrillamail-com-www")) }
        },
        "temp-mail-io" => {
          generate: ->(_o) { p::TempMailIo.generate_email },
          get_emails: ->(e, _t) { p::TempMailIo.get_emails(e) }
        },
        "tempmail-lol" => {
          generate: ->(o) { p::TempmailLol.generate_email(o.domain) },
          get_emails: ->(e, t) { p::TempmailLol.get_emails(e, require_token(t, "tempmail-lol")) }
        },
        "tempmail-plus" => {
          generate: ->(_o) { p::TempmailPlus.generate_email },
          get_emails: ->(e, _t) { p::TempmailPlus.get_emails(e) }
        },
        # tempmail-plus 别名渠道：固定域名变体
        "fexpost-com" => {
          generate: ->(_o) { p::TempmailPlus.generate_email("fexpost.com", "fexpost-com") },
          get_emails: ->(e, _t) { p::TempmailPlus.get_emails(e) }
        },
        "fexbox-org" => {
          generate: ->(_o) { p::TempmailPlus.generate_email("fexbox.org", "fexbox-org") },
          get_emails: ->(e, _t) { p::TempmailPlus.get_emails(e) }
        },
        "mailbox-in-ua" => {
          generate: ->(_o) { p::TempmailPlus.generate_email("mailbox.in.ua", "mailbox-in-ua") },
          get_emails: ->(e, _t) { p::TempmailPlus.get_emails(e) }
        },
        "rover-info" => {
          generate: ->(_o) { p::TempmailPlus.generate_email("rover.info", "rover-info") },
          get_emails: ->(e, _t) { p::TempmailPlus.get_emails(e) }
        },
        "chitthi-in" => {
          generate: ->(_o) { p::TempmailPlus.generate_email("chitthi.in", "chitthi-in") },
          get_emails: ->(e, _t) { p::TempmailPlus.get_emails(e) }
        },
        "fextemp-com" => {
          generate: ->(_o) { p::TempmailPlus.generate_email("fextemp.com", "fextemp-com") },
          get_emails: ->(e, _t) { p::TempmailPlus.get_emails(e) }
        },
        "any-pink" => {
          generate: ->(_o) { p::TempmailPlus.generate_email("any.pink", "any-pink") },
          get_emails: ->(e, _t) { p::TempmailPlus.get_emails(e) }
        },
        "merepost-com" => {
          generate: ->(_o) { p::TempmailPlus.generate_email("merepost.com", "merepost-com") },
          get_emails: ->(e, _t) { p::TempmailPlus.get_emails(e) }
        },
        "restmail-net" => {
          generate: ->(_o) { p::RestmailNet.generate_email },
          get_emails: ->(e, _t) { p::RestmailNet.get_emails(e) }
        },
        "inboxkitten" => {
          generate: ->(_o) { p::Inboxkitten.generate_email },
          get_emails: ->(e, _t) { p::Inboxkitten.get_emails(e) }
        },
        "maildrop" => {
          generate: ->(o) { p::Maildrop.generate_email(o.domain) },
          get_emails: ->(e, t) { p::Maildrop.get_emails(e, t) }
        },
        "mail-cx" => {
          generate: ->(o) { p::MailCx.generate_email(o.domain) },
          get_emails: ->(e, t) { p::MailCx.get_emails(e, require_token(t, "mail-cx")) }
        },
        "ddker-com" => {
          generate: ->(_o) { with_channel(p::MailCx.generate_email("ddker.com"), "ddker-com") },
          get_emails: ->(e, t) { p::MailCx.get_emails(e, require_token(t, "mail-cx")) }
        },
        "1sec-mail" => {
          generate: ->(_o) { p::OneSecMail.generate_email },
          get_emails: ->(e, t) { p::OneSecMail.get_emails(e, require_token(t, "1sec-mail")) }
        },
        "getnada" => {
          generate: ->(o) { p::Getnada.generate_email(o.domain) },
          get_emails: ->(e, t) { p::Getnada.get_emails(require_token(t, "getnada"), e) }
        },
        "1vpn-net" => {
          generate: ->(_o) { p::Getnada.generate_email("1vpn.net", "1vpn-net") },
          get_emails: ->(e, t) { p::Getnada.get_emails(require_token(t, "getnada"), e) }
        },
        "abematv-com" => {
          generate: ->(_o) { p::Getnada.generate_email("abematv.com", "abematv-com") },
          get_emails: ->(e, t) { p::Getnada.get_emails(require_token(t, "getnada"), e) }
        },
        "abematv-net" => {
          generate: ->(_o) { p::Getnada.generate_email("abematv.net", "abematv-net") },
          get_emails: ->(e, t) { p::Getnada.get_emails(require_token(t, "getnada"), e) }
        },
        "abematv-org" => {
          generate: ->(_o) { p::Getnada.generate_email("abematv.org", "abematv-org") },
          get_emails: ->(e, t) { p::Getnada.get_emails(require_token(t, "getnada"), e) }
        },
        "aceh-cc" => {
          generate: ->(_o) { p::Getnada.generate_email("aceh.cc", "aceh-cc") },
          get_emails: ->(e, t) { p::Getnada.get_emails(require_token(t, "getnada"), e) }
        },
        "bangkabelitung-net" => {
          generate: ->(_o) { p::Getnada.generate_email("bangkabelitung.net", "bangkabelitung-net") },
          get_emails: ->(e, t) { p::Getnada.get_emails(require_token(t, "getnada"), e) }
        },
        "cctruyen-com" => {
          generate: ->(_o) { p::Getnada.generate_email("cctruyen.com", "cctruyen-com") },
          get_emails: ->(e, t) { p::Getnada.get_emails(require_token(t, "getnada"), e) }
        },
        "getnada-com" => {
          generate: ->(_o) { p::Getnada.generate_email("getnada.com", "getnada-com") },
          get_emails: ->(e, t) { p::Getnada.get_emails(require_token(t, "getnada"), e) }
        },
        "getnada-email" => {
          generate: ->(_o) { p::Getnada.generate_email("getnada.email", "getnada-email") },
          get_emails: ->(e, t) { p::Getnada.get_emails(require_token(t, "getnada"), e) }
        },
        "getnada-net" => {
          generate: ->(_o) { p::Getnada.generate_email("getnada.net", "getnada-net") },
          get_emails: ->(e, t) { p::Getnada.get_emails(require_token(t, "getnada"), e) }
        },
        "jawatengah-net" => {
          generate: ->(_o) { p::Getnada.generate_email("jawatengah.net", "jawatengah-net") },
          get_emails: ->(e, t) { p::Getnada.get_emails(require_token(t, "getnada"), e) }
        },
        "jawatimur-net" => {
          generate: ->(_o) { p::Getnada.generate_email("jawatimur.net", "jawatimur-net") },
          get_emails: ->(e, t) { p::Getnada.get_emails(require_token(t, "getnada"), e) }
        },
        "kalimantanbarat-net" => {
          generate: ->(_o) { p::Getnada.generate_email("kalimantanbarat.net", "kalimantanbarat-net") },
          get_emails: ->(e, t) { p::Getnada.get_emails(require_token(t, "getnada"), e) }
        },
        "kalimantanselatan-net" => {
          generate: ->(_o) { p::Getnada.generate_email("kalimantanselatan.net", "kalimantanselatan-net") },
          get_emails: ->(e, t) { p::Getnada.get_emails(require_token(t, "getnada"), e) }
        },
        "kalimantantengah-net" => {
          generate: ->(_o) { p::Getnada.generate_email("kalimantantengah.net", "kalimantantengah-net") },
          get_emails: ->(e, t) { p::Getnada.get_emails(require_token(t, "getnada"), e) }
        },
        "kalimantantimur-net" => {
          generate: ->(_o) { p::Getnada.generate_email("kalimantantimur.net", "kalimantantimur-net") },
          get_emails: ->(e, t) { p::Getnada.get_emails(require_token(t, "getnada"), e) }
        },
        "kalimantanutara-net" => {
          generate: ->(_o) { p::Getnada.generate_email("kalimantanutara.net", "kalimantanutara-net") },
          get_emails: ->(e, t) { p::Getnada.get_emails(require_token(t, "getnada"), e) }
        },
        "kepulauanriau-net" => {
          generate: ->(_o) { p::Getnada.generate_email("kepulauanriau.net", "kepulauanriau-net") },
          get_emails: ->(e, t) { p::Getnada.get_emails(require_token(t, "getnada"), e) }
        },
        "luxury345-com" => {
          generate: ->(_o) { p::Getnada.generate_email("luxury345.com", "luxury345-com") },
          get_emails: ->(e, t) { p::Getnada.get_emails(require_token(t, "getnada"), e) }
        },
        "malukuutara-net" => {
          generate: ->(_o) { p::Getnada.generate_email("malukuutara.net", "malukuutara-net") },
          get_emails: ->(e, t) { p::Getnada.get_emails(require_token(t, "getnada"), e) }
        },
        "nusatenggarabarat-net" => {
          generate: ->(_o) { p::Getnada.generate_email("nusatenggarabarat.net", "nusatenggarabarat-net") },
          get_emails: ->(e, t) { p::Getnada.get_emails(require_token(t, "getnada"), e) }
        },
        "nusatenggaratimur-net" => {
          generate: ->(_o) { p::Getnada.generate_email("nusatenggaratimur.net", "nusatenggaratimur-net") },
          get_emails: ->(e, t) { p::Getnada.get_emails(require_token(t, "getnada"), e) }
        },
        "papuabarat-net" => {
          generate: ->(_o) { p::Getnada.generate_email("papuabarat.net", "papuabarat-net") },
          get_emails: ->(e, t) { p::Getnada.get_emails(require_token(t, "getnada"), e) }
        },
        "papuabaratdaya-net" => {
          generate: ->(_o) { p::Getnada.generate_email("papuabaratdaya.net", "papuabaratdaya-net") },
          get_emails: ->(e, t) { p::Getnada.get_emails(require_token(t, "getnada"), e) }
        },
        "papuaselatan-net" => {
          generate: ->(_o) { p::Getnada.generate_email("papuaselatan.net", "papuaselatan-net") },
          get_emails: ->(e, t) { p::Getnada.get_emails(require_token(t, "getnada"), e) }
        },
        "pehol-com" => {
          generate: ->(_o) { p::Getnada.generate_email("pehol.com", "pehol-com") },
          get_emails: ->(e, t) { p::Getnada.get_emails(require_token(t, "getnada"), e) }
        },
        "ptruyen-com" => {
          generate: ->(_o) { p::Getnada.generate_email("ptruyen.com", "ptruyen-com") },
          get_emails: ->(e, t) { p::Getnada.get_emails(require_token(t, "getnada"), e) }
        },
        "pulaubali-net" => {
          generate: ->(_o) { p::Getnada.generate_email("pulaubali.net", "pulaubali-net") },
          get_emails: ->(e, t) { p::Getnada.get_emails(require_token(t, "getnada"), e) }
        },
        "riau-net" => {
          generate: ->(_o) { p::Getnada.generate_email("riau.net", "riau-net") },
          get_emails: ->(e, t) { p::Getnada.get_emails(require_token(t, "getnada"), e) }
        },
        "seokey-org" => {
          generate: ->(_o) { p::Getnada.generate_email("seokey.org", "seokey-org") },
          get_emails: ->(e, t) { p::Getnada.get_emails(require_token(t, "getnada"), e) }
        },
        "sulawesibarat-net" => {
          generate: ->(_o) { p::Getnada.generate_email("sulawesibarat.net", "sulawesibarat-net") },
          get_emails: ->(e, t) { p::Getnada.get_emails(require_token(t, "getnada"), e) }
        },
        "sulawesiselatan-net" => {
          generate: ->(_o) { p::Getnada.generate_email("sulawesiselatan.net", "sulawesiselatan-net") },
          get_emails: ->(e, t) { p::Getnada.get_emails(require_token(t, "getnada"), e) }
        },
        "sulawesitengah-net" => {
          generate: ->(_o) { p::Getnada.generate_email("sulawesitengah.net", "sulawesitengah-net") },
          get_emails: ->(e, t) { p::Getnada.get_emails(require_token(t, "getnada"), e) }
        },
        "sulawesitenggara-net" => {
          generate: ->(_o) { p::Getnada.generate_email("sulawesitenggara.net", "sulawesitenggara-net") },
          get_emails: ->(e, t) { p::Getnada.get_emails(require_token(t, "getnada"), e) }
        },
        "sumaterabarat-net" => {
          generate: ->(_o) { p::Getnada.generate_email("sumaterabarat.net", "sumaterabarat-net") },
          get_emails: ->(e, t) { p::Getnada.get_emails(require_token(t, "getnada"), e) }
        },
        "sumateraselatan-net" => {
          generate: ->(_o) { p::Getnada.generate_email("sumateraselatan.net", "sumateraselatan-net") },
          get_emails: ->(e, t) { p::Getnada.get_emails(require_token(t, "getnada"), e) }
        },
        "sumaterautara-net" => {
          generate: ->(_o) { p::Getnada.generate_email("sumaterautara.net", "sumaterautara-net") },
          get_emails: ->(e, t) { p::Getnada.get_emails(require_token(t, "getnada"), e) }
        },
        "villatogel-com" => {
          generate: ->(_o) { p::Getnada.generate_email("villatogel.com", "villatogel-com") },
          get_emails: ->(e, t) { p::Getnada.get_emails(require_token(t, "getnada"), e) }
        },
        # mailmomy 家族：mailmomy 主渠道从域名池随机选域，其余为固定域名变体（同后端 API）
        "16888888-cyou" => {
          generate: ->(_o) { with_channel(p::Mailmomy.generate_email("16888888.cyou"), "16888888-cyou") },
          get_emails: ->(e, _t) { p::Mailmomy.get_emails(e) }
        },
        "17666688-shop" => {
          generate: ->(_o) { with_channel(p::Mailmomy.generate_email("17666688.shop"), "17666688-shop") },
          get_emails: ->(e, _t) { p::Mailmomy.get_emails(e) }
        },
        "282mail-com" => {
          generate: ->(_o) { with_channel(p::Mailmomy.generate_email("282mail.com"), "282mail-com") },
          get_emails: ->(e, _t) { p::Mailmomy.get_emails(e) }
        },
        "bsdu32-buzz" => {
          generate: ->(_o) { with_channel(p::Mailmomy.generate_email("bsdu32.buzz"), "bsdu32-buzz") },
          get_emails: ->(e, _t) { p::Mailmomy.get_emails(e) }
        },
        "doxu243-buzz" => {
          generate: ->(_o) { with_channel(p::Mailmomy.generate_email("doxu243.buzz"), "doxu243-buzz") },
          get_emails: ->(e, _t) { p::Mailmomy.get_emails(e) }
        },
        "easyme-pro" => {
          generate: ->(_o) { with_channel(p::Mailmomy.generate_email("easyme.pro"), "easyme-pro") },
          get_emails: ->(e, _t) { p::Mailmomy.get_emails(e) }
        },
        "evergreenco-shop" => {
          generate: ->(_o) { with_channel(p::Mailmomy.generate_email("evergreenco.shop"), "evergreenco-shop") },
          get_emails: ->(e, _t) { p::Mailmomy.get_emails(e) }
        },
        "layueming-pics" => {
          generate: ->(_o) { with_channel(p::Mailmomy.generate_email("layueming.pics"), "layueming-pics") },
          get_emails: ->(e, _t) { p::Mailmomy.get_emails(e) }
        },
        "mingyuekeji-online" => {
          generate: ->(_o) { with_channel(p::Mailmomy.generate_email("mingyuekeji.online"), "mingyuekeji-online") },
          get_emails: ->(e, _t) { p::Mailmomy.get_emails(e) }
        },
        "mingyueming-click" => {
          generate: ->(_o) { with_channel(p::Mailmomy.generate_email("mingyueming.click"), "mingyueming-click") },
          get_emails: ->(e, _t) { p::Mailmomy.get_emails(e) }
        },
        "mingyueming-shop" => {
          generate: ->(_o) { with_channel(p::Mailmomy.generate_email("mingyueming.shop"), "mingyueming-shop") },
          get_emails: ->(e, _t) { p::Mailmomy.get_emails(e) }
        },
        "mingyukeji-lol" => {
          generate: ->(_o) { with_channel(p::Mailmomy.generate_email("mingyukeji.lol"), "mingyukeji-lol") },
          get_emails: ->(e, _t) { p::Mailmomy.get_emails(e) }
        },
        "nuxh62-space" => {
          generate: ->(_o) { with_channel(p::Mailmomy.generate_email("nuxh62.space"), "nuxh62-space") },
          get_emails: ->(e, _t) { p::Mailmomy.get_emails(e) }
        },
        "proid-cloud-ip-cc" => {
          generate: ->(_o) { with_channel(p::Mailmomy.generate_email("proid.cloud-ip.cc"), "proid-cloud-ip-cc") },
          get_emails: ->(e, _t) { p::Mailmomy.get_emails(e) }
        },
        "sbook-pics" => {
          generate: ->(_o) { with_channel(p::Mailmomy.generate_email("sbook.pics"), "sbook-pics") },
          get_emails: ->(e, _t) { p::Mailmomy.get_emails(e) }
        },
        "xue32-buzz" => {
          generate: ->(_o) { with_channel(p::Mailmomy.generate_email("xue32.buzz"), "xue32-buzz") },
          get_emails: ->(e, _t) { p::Mailmomy.get_emails(e) }
        },
        "mailmomy" => {
          generate: ->(_o) { p::Mailmomy.generate_email },
          get_emails: ->(e, _t) { p::Mailmomy.get_emails(e) }
        },
        # mailinator 家族：主渠道 mailinator.com，其余为姊妹域名别名（收信统一走 public 域名 API）
        "mailinator" => {
          generate: ->(_o) { p::Mailinator.generate_email },
          get_emails: ->(e, t) { p::Mailinator.get_emails(t.to_s, e) }
        },
        "blackhole-djurby-se" => {
          generate: ->(_o) { p::Mailinator.generate_email("blackhole.djurby.se", "blackhole-djurby-se") },
          get_emails: ->(e, t) { p::Mailinator.get_emails(t.to_s, e) }
        },
        "block-bdea-cc" => {
          generate: ->(_o) { p::Mailinator.generate_email("block.bdea.cc", "block-bdea-cc") },
          get_emails: ->(e, t) { p::Mailinator.get_emails(t.to_s, e) }
        },
        "b-smelly-cc" => {
          generate: ->(_o) { p::Mailinator.generate_email("b.smelly.cc", "b-smelly-cc") },
          get_emails: ->(e, t) { p::Mailinator.get_emails(t.to_s, e) }
        },
        "carlton183-changeip-net" => {
          generate: ->(_o) { p::Mailinator.generate_email("183carlton.changeip.net", "carlton183-changeip-net") },
          get_emails: ->(e, t) { p::Mailinator.get_emails(t.to_s, e) }
        },
        "dea-soon-it" => {
          generate: ->(_o) { p::Mailinator.generate_email("dea.soon.it", "dea-soon-it") },
          get_emails: ->(e, t) { p::Mailinator.get_emails(t.to_s, e) }
        },
        "disposable-al-sudani-com" => {
          generate: ->(_o) { p::Mailinator.generate_email("disposable.al-sudani.com", "disposable-al-sudani-com") },
          get_emails: ->(e, t) { p::Mailinator.get_emails(t.to_s, e) }
        },
        "disposable-nogonad-nl" => {
          generate: ->(_o) { p::Mailinator.generate_email("disposable.nogonad.nl", "disposable-nogonad-nl") },
          get_emails: ->(e, t) { p::Mailinator.get_emails(t.to_s, e) }
        },
        "ebs-com-ar" => {
          generate: ->(_o) { p::Mailinator.generate_email("ebs.com.ar", "ebs-com-ar") },
          get_emails: ->(e, t) { p::Mailinator.get_emails(t.to_s, e) }
        },
        "etgdev-de" => {
          generate: ->(_o) { p::Mailinator.generate_email("etgdev.de", "etgdev-de") },
          get_emails: ->(e, t) { p::Mailinator.get_emails(t.to_s, e) }
        },
        "fwd2m-eszett-es" => {
          generate: ->(_o) { p::Mailinator.generate_email("fwd2m.eszett.es", "fwd2m-eszett-es") },
          get_emails: ->(e, t) { p::Mailinator.get_emails(t.to_s, e) }
        },
        "jama-trenet-eu" => {
          generate: ->(_o) { p::Mailinator.generate_email("jama.trenet.eu", "jama-trenet-eu") },
          get_emails: ->(e, t) { p::Mailinator.get_emails(t.to_s, e) }
        },
        "j-fairuse-org" => {
          generate: ->(_o) { p::Mailinator.generate_email("j.fairuse.org", "j-fairuse-org") },
          get_emails: ->(e, t) { p::Mailinator.get_emails(t.to_s, e) }
        },
        "m-887-at" => {
          generate: ->(_o) { p::Mailinator.generate_email("m.887.at", "m-887-at") },
          get_emails: ->(e, t) { p::Mailinator.get_emails(t.to_s, e) }
        },
        "m8r-davidfuhr-de" => {
          generate: ->(_o) { p::Mailinator.generate_email("m8r.davidfuhr.de", "m8r-davidfuhr-de") },
          get_emails: ->(e, t) { p::Mailinator.get_emails(t.to_s, e) }
        },
        "m8r-mcasal-com" => {
          generate: ->(_o) { p::Mailinator.generate_email("m8r.mcasal.com", "m8r-mcasal-com") },
          get_emails: ->(e, t) { p::Mailinator.get_emails(t.to_s, e) }
        },
        "mail-bentrask-com" => {
          generate: ->(_o) { p::Mailinator.generate_email("mail.bentrask.com", "mail-bentrask-com") },
          get_emails: ->(e, t) { p::Mailinator.get_emails(t.to_s, e) }
        },
        "mail-fsmash-org" => {
          generate: ->(_o) { p::Mailinator.generate_email("mail.fsmash.org", "mail-fsmash-org") },
          get_emails: ->(e, t) { p::Mailinator.get_emails(t.to_s, e) }
        },
        "mailinatorzz-mooo-com" => {
          generate: ->(_o) { p::Mailinator.generate_email("mailinatorzz.mooo.com", "mailinatorzz-mooo-com") },
          get_emails: ->(e, t) { p::Mailinator.get_emails(t.to_s, e) }
        },
        "mi-meon-be" => {
          generate: ->(_o) { p::Mailinator.generate_email("mi.meon.be", "mi-meon-be") },
          get_emails: ->(e, t) { p::Mailinator.get_emails(t.to_s, e) }
        },
        "mn-curppa-com" => {
          generate: ->(_o) { p::Mailinator.generate_email("mn.curppa.com", "mn-curppa-com") },
          get_emails: ->(e, t) { p::Mailinator.get_emails(t.to_s, e) }
        },
        "m-nik-me" => {
          generate: ->(_o) { p::Mailinator.generate_email("m.nik.me", "m-nik-me") },
          get_emails: ->(e, t) { p::Mailinator.get_emails(t.to_s, e) }
        },
        "mtmdev-com" => {
          generate: ->(_o) { p::Mailinator.generate_email("mtmdev.com", "mtmdev-com") },
          get_emails: ->(e, t) { p::Mailinator.get_emails(t.to_s, e) }
        },
        "nospam-thurstons-us" => {
          generate: ->(_o) { p::Mailinator.generate_email("nospam.thurstons.us", "nospam-thurstons-us") },
          get_emails: ->(e, t) { p::Mailinator.get_emails(t.to_s, e) }
        },
        "notfond-404-mn" => {
          generate: ->(_o) { p::Mailinator.generate_email("notfond.404.mn", "notfond-404-mn") },
          get_emails: ->(e, t) { p::Mailinator.get_emails(t.to_s, e) }
        },
        "null-k3vin-net" => {
          generate: ->(_o) { p::Mailinator.generate_email("null.k3vin.net", "null-k3vin-net") },
          get_emails: ->(e, t) { p::Mailinator.get_emails(t.to_s, e) }
        },
        "ramjane-mooo-com" => {
          generate: ->(_o) { p::Mailinator.generate_email("ramjane.mooo.com", "ramjane-mooo-com") },
          get_emails: ->(e, t) { p::Mailinator.get_emails(t.to_s, e) }
        },
        "rauxa-seny-cat" => {
          generate: ->(_o) { p::Mailinator.generate_email("rauxa.seny.cat", "rauxa-seny-cat") },
          get_emails: ->(e, t) { p::Mailinator.get_emails(t.to_s, e) }
        },
        "really-istrash-com" => {
          generate: ->(_o) { p::Mailinator.generate_email("really.istrash.com", "really-istrash-com") },
          get_emails: ->(e, t) { p::Mailinator.get_emails(t.to_s, e) }
        },
        "spam-hortuk-ovh" => {
          generate: ->(_o) { p::Mailinator.generate_email("spam.hortuk.ovh", "spam-hortuk-ovh") },
          get_emails: ->(e, t) { p::Mailinator.get_emails(t.to_s, e) }
        },
        "sp-woot-at" => {
          generate: ->(_o) { p::Mailinator.generate_email("sp.woot.at", "sp-woot-at") },
          get_emails: ->(e, t) { p::Mailinator.get_emails(t.to_s, e) }
        },
        "test-unergie-com" => {
          generate: ->(_o) { p::Mailinator.generate_email("test.unergie.com", "test-unergie-com") },
          get_emails: ->(e, t) { p::Mailinator.get_emails(t.to_s, e) }
        },
        "torch-yi-org" => {
          generate: ->(_o) { p::Mailinator.generate_email("torch.yi.org", "torch-yi-org") },
          get_emails: ->(e, t) { p::Mailinator.get_emails(t.to_s, e) }
        },
        "t-zibet-net" => {
          generate: ->(_o) { p::Mailinator.generate_email("t.zibet.net", "t-zibet-net") },
          get_emails: ->(e, t) { p::Mailinator.get_emails(t.to_s, e) }
        },
        "sogetthis-com" => {
          generate: ->(_o) { p::Mailinator.generate_email("sogetthis.com", "sogetthis-com") },
          get_emails: ->(e, t) { p::Mailinator.get_emails(t.to_s, e) }
        },
        "bobmail-info" => {
          generate: ->(_o) { p::Mailinator.generate_email("bobmail.info", "bobmail-info") },
          get_emails: ->(e, t) { p::Mailinator.get_emails(t.to_s, e) }
        },
        "suremail-info" => {
          generate: ->(_o) { p::Mailinator.generate_email("suremail.info", "suremail-info") },
          get_emails: ->(e, t) { p::Mailinator.get_emails(t.to_s, e) }
        },
        "binkmail-com" => {
          generate: ->(_o) { p::Mailinator.generate_email("binkmail.com", "binkmail-com") },
          get_emails: ->(e, t) { p::Mailinator.get_emails(t.to_s, e) }
        },
        "veryrealemail-com" => {
          generate: ->(_o) { p::Mailinator.generate_email("veryrealemail.com", "veryrealemail-com") },
          get_emails: ->(e, t) { p::Mailinator.get_emails(t.to_s, e) }
        },
        "chammy-info" => {
          generate: ->(_o) { p::Mailinator.generate_email("chammy.info", "chammy-info") },
          get_emails: ->(e, t) { p::Mailinator.get_emails(t.to_s, e) }
        },
        "thisisnotmyrealemail-com" => {
          generate: ->(_o) { p::Mailinator.generate_email("thisisnotmyrealemail.com", "thisisnotmyrealemail-com") },
          get_emails: ->(e, t) { p::Mailinator.get_emails(t.to_s, e) }
        },
        "notmailinator-com" => {
          generate: ->(_o) { p::Mailinator.generate_email("notmailinator.com", "notmailinator-com") },
          get_emails: ->(e, t) { p::Mailinator.get_emails(t.to_s, e) }
        },
        "spamhereplease-com" => {
          generate: ->(_o) { p::Mailinator.generate_email("spamhereplease.com", "spamhereplease-com") },
          get_emails: ->(e, t) { p::Mailinator.get_emails(t.to_s, e) }
        },
        "sendspamhere-com" => {
          generate: ->(_o) { p::Mailinator.generate_email("sendspamhere.com", "sendspamhere-com") },
          get_emails: ->(e, t) { p::Mailinator.get_emails(t.to_s, e) }
        },
        "sendfree-org" => {
          generate: ->(_o) { p::Mailinator.generate_email("sendfree.org", "sendfree-org") },
          get_emails: ->(e, t) { p::Mailinator.get_emails(t.to_s, e) }
        },
        "junk-beats-org" => {
          generate: ->(_o) { p::Mailinator.generate_email("junk.beats.org", "junk-beats-org") },
          get_emails: ->(e, t) { p::Mailinator.get_emails(t.to_s, e) }
        },
        "junk-ihmehl-com" => {
          generate: ->(_o) { p::Mailinator.generate_email("junk.ihmehl.com", "junk-ihmehl-com") },
          get_emails: ->(e, t) { p::Mailinator.get_emails(t.to_s, e) }
        },
        "junk-noplay-org" => {
          generate: ->(_o) { p::Mailinator.generate_email("junk.noplay.org", "junk-noplay-org") },
          get_emails: ->(e, t) { p::Mailinator.get_emails(t.to_s, e) }
        },
        "junk-vanillasystem-com" => {
          generate: ->(_o) { p::Mailinator.generate_email("junk.vanillasystem.com", "junk-vanillasystem-com") },
          get_emails: ->(e, t) { p::Mailinator.get_emails(t.to_s, e) }
        },
        "spam-jasonpearce-com" => {
          generate: ->(_o) { p::Mailinator.generate_email("spam.jasonpearce.com", "spam-jasonpearce-com") },
          get_emails: ->(e, t) { p::Mailinator.get_emails(t.to_s, e) }
        },
        "fish-skytale-net" => {
          generate: ->(_o) { p::Mailinator.generate_email("fish.skytale.net", "fish-skytale-net") },
          get_emails: ->(e, t) { p::Mailinator.get_emails(t.to_s, e) }
        },
        "spam-mccrew-com" => {
          generate: ->(_o) { p::Mailinator.generate_email("spam.mccrew.com", "spam-mccrew-com") },
          get_emails: ->(e, t) { p::Mailinator.get_emails(t.to_s, e) }
        },
        "spam-coroiu-com" => {
          generate: ->(_o) { p::Mailinator.generate_email("spam.coroiu.com", "spam-coroiu-com") },
          get_emails: ->(e, t) { p::Mailinator.get_emails(t.to_s, e) }
        },
        "spam-deluser-net" => {
          generate: ->(_o) { p::Mailinator.generate_email("spam.deluser.net", "spam-deluser-net") },
          get_emails: ->(e, t) { p::Mailinator.get_emails(t.to_s, e) }
        },
        "spam-dhsf-net" => {
          generate: ->(_o) { p::Mailinator.generate_email("spam.dhsf.net", "spam-dhsf-net") },
          get_emails: ->(e, t) { p::Mailinator.get_emails(t.to_s, e) }
        },
        "spam-lucatnt-com" => {
          generate: ->(_o) { p::Mailinator.generate_email("spam.lucatnt.com", "spam-lucatnt-com") },
          get_emails: ->(e, t) { p::Mailinator.get_emails(t.to_s, e) }
        },
        "spam-lyceum-life-com-ru" => {
          generate: ->(_o) { p::Mailinator.generate_email("spam.lyceum-life.com.ru", "spam-lyceum-life-com-ru") },
          get_emails: ->(e, t) { p::Mailinator.get_emails(t.to_s, e) }
        },
        "spam-netpirates-net" => {
          generate: ->(_o) { p::Mailinator.generate_email("spam.netpirates.net", "spam-netpirates-net") },
          get_emails: ->(e, t) { p::Mailinator.get_emails(t.to_s, e) }
        },
        "spam-no-ip-net" => {
          generate: ->(_o) { p::Mailinator.generate_email("spam.no-ip.net", "spam-no-ip-net") },
          get_emails: ->(e, t) { p::Mailinator.get_emails(t.to_s, e) }
        },
        "spam-ozh-org" => {
          generate: ->(_o) { p::Mailinator.generate_email("spam.ozh.org", "spam-ozh-org") },
          get_emails: ->(e, t) { p::Mailinator.get_emails(t.to_s, e) }
        },
        "spam-pyphus-org" => {
          generate: ->(_o) { p::Mailinator.generate_email("spam.pyphus.org", "spam-pyphus-org") },
          get_emails: ->(e, t) { p::Mailinator.get_emails(t.to_s, e) }
        },
        "spam-shep-pw" => {
          generate: ->(_o) { p::Mailinator.generate_email("spam.shep.pw", "spam-shep-pw") },
          get_emails: ->(e, t) { p::Mailinator.get_emails(t.to_s, e) }
        },
        "spam-wtf-at" => {
          generate: ->(_o) { p::Mailinator.generate_email("spam.wtf.at", "spam-wtf-at") },
          get_emails: ->(e, t) { p::Mailinator.get_emails(t.to_s, e) }
        },
        "spam-wulczer-org" => {
          generate: ->(_o) { p::Mailinator.generate_email("spam.wulczer.org", "spam-wulczer-org") },
          get_emails: ->(e, t) { p::Mailinator.get_emails(t.to_s, e) }
        },
        "crap-kakadua-net" => {
          generate: ->(_o) { p::Mailinator.generate_email("crap.kakadua.net", "crap-kakadua-net") },
          get_emails: ->(e, t) { p::Mailinator.get_emails(t.to_s, e) }
        },
        "spam-janlugt-nl" => {
          generate: ->(_o) { p::Mailinator.generate_email("spam.janlugt.nl", "spam-janlugt-nl") },
          get_emails: ->(e, t) { p::Mailinator.get_emails(t.to_s, e) }
        },
        "min-burningfish-net" => {
          generate: ->(_o) { p::Mailinator.generate_email("min.burningfish.net", "min-burningfish-net") },
          get_emails: ->(e, t) { p::Mailinator.get_emails(t.to_s, e) }
        },
        "sink-fblay-com" => {
          generate: ->(_o) { p::Mailinator.generate_email("sink.fblay.com", "sink-fblay-com") },
          get_emails: ->(e, t) { p::Mailinator.get_emails(t.to_s, e) }
        },
        # catchmail 家族：主渠道随机域名，别名用固定域名
        "catchmail" => {
          generate: ->(o) { p::Catchmail.generate_email(o.domain) },
          get_emails: ->(e, _t) { p::Catchmail.get_emails(e) }
        },
        "catchmail-mailistry" => {
          generate: ->(_o) { with_channel(p::Catchmail.generate_email("mailistry.com"), "catchmail-mailistry") },
          get_emails: ->(e, _t) { p::Catchmail.get_emails(e) }
        },
        "catchmail-zeppost" => {
          generate: ->(_o) { with_channel(p::Catchmail.generate_email("zeppost.com"), "catchmail-zeppost") },
          get_emails: ->(e, _t) { p::Catchmail.get_emails(e) }
        },
        # mailforspam 家族：主渠道随机域名，别名用固定域名
        "mailforspam" => {
          generate: ->(o) { p::Mailforspam.generate_email(o.domain) },
          get_emails: ->(e, _t) { p::Mailforspam.get_emails(e) }
        },
        "mailforspam-tempmail-io" => {
          generate: ->(_o) { with_channel(p::Mailforspam.generate_email("tempmail.io"), "mailforspam-tempmail-io") },
          get_emails: ->(e, _t) { p::Mailforspam.get_emails(e) }
        },
        "mailforspam-disposable" => {
          generate: ->(_o) { with_channel(p::Mailforspam.generate_email("disposable.email"), "mailforspam-disposable") },
          get_emails: ->(e, _t) { p::Mailforspam.get_emails(e) }
        },
        # fake-legal 家族：主渠道随机域名，别名用 POST 自定义域
        "fake-legal" => {
          generate: ->(o) { p::FakeLegal.generate_email(o.domain) },
          get_emails: ->(e, _t) { p::FakeLegal.get_emails(e) }
        },
        "imgui-de" => {
          generate: ->(_o) { p::FakeLegal.generate_email("imgui.de", "imgui-de") },
          get_emails: ->(e, _t) { p::FakeLegal.get_emails(e) }
        },
        "pulsewebmenu-de" => {
          generate: ->(_o) { p::FakeLegal.generate_email("pulsewebmenu.de", "pulsewebmenu-de") },
          get_emails: ->(e, _t) { p::FakeLegal.get_emails(e) }
        },
        # 10minute-one 家族：主渠道从 SSR 获取 JWT + 域名池，别名用固定域名
        "10minute-one" => {
          generate: ->(o) { p::TenminuteOne.generate_email(o.domain) },
          get_emails: ->(e, t) { p::TenminuteOne.get_emails(e, require_token(t, "10minute-one")) }
        },
        "xghff-com" => {
          generate: ->(_o) { with_channel(p::TenminuteOne.generate_email("xghff.com"), "xghff-com") },
          get_emails: ->(e, t) { p::TenminuteOne.get_emails(e, require_token(t, "10minute-one")) }
        },
        "oqqaj-com" => {
          generate: ->(_o) { with_channel(p::TenminuteOne.generate_email("oqqaj.com"), "oqqaj-com") },
          get_emails: ->(e, t) { p::TenminuteOne.get_emails(e, require_token(t, "10minute-one")) }
        },
        "psovv-com" => {
          generate: ->(_o) { with_channel(p::TenminuteOne.generate_email("psovv.com"), "psovv-com") },
          get_emails: ->(e, t) { p::TenminuteOne.get_emails(e, require_token(t, "10minute-one")) }
        },
        "dbwot-com" => {
          generate: ->(_o) { with_channel(p::TenminuteOne.generate_email("dbwot.com"), "dbwot-com") },
          get_emails: ->(e, t) { p::TenminuteOne.get_emails(e, require_token(t, "10minute-one")) }
        },
        "ygwpr-com" => {
          generate: ->(_o) { with_channel(p::TenminuteOne.generate_email("ygwpr.com"), "ygwpr-com") },
          get_emails: ->(e, t) { p::TenminuteOne.get_emails(e, require_token(t, "10minute-one")) }
        },
        "imxwe-com" => {
          generate: ->(_o) { with_channel(p::TenminuteOne.generate_email("imxwe.com"), "imxwe-com") },
          get_emails: ->(e, t) { p::TenminuteOne.get_emails(e, require_token(t, "10minute-one")) }
        },
        # zhujump 家族：通过注册账号、登录后创建邮箱
        "jqkjqk-xyz" => {
          generate: ->(_o) { p::Zhujump.generate_email("jqkjqk.xyz", "jqkjqk-xyz") },
          get_emails: ->(e, t) { p::Zhujump.get_emails(require_token(t, "jqkjqk-xyz"), e) }
        },
        "lyhlevi-com" => {
          generate: ->(_o) { p::Zhujump.generate_email_for_instance("https://lyhlevi.com", "lyhlevi.com", "lyhlevi-com", 24 * 60 * 60 * 1000) },
          get_emails: ->(e, t) { p::Zhujump.get_emails(require_token(t, "lyhlevi-com"), e) }
        },
        # shard-10-independent-simple-a 渠道绑定
        "anonymmail" => {
          generate: ->(_o) { p::Anonymmail.generate_email },
          get_emails: ->(e, _t) { p::Anonymmail.get_emails(e) }
        },
        "byom" => {
          generate: ->(_o) { p::Byom.generate_email },
          get_emails: ->(e, _t) { p::Byom.get_emails(e) }
        },
        "cleantempmail" => {
          generate: ->(_o) { p::Cleantempmail.generate_email },
          get_emails: ->(e, _t) { p::Cleantempmail.get_emails(e) }
        },
        "devmail-uk" => {
          generate: ->(_o) { p::DevmailUk.generate_email },
          get_emails: ->(e, _t) { p::DevmailUk.get_emails(e) }
        },
        "dropmail-click" => {
          generate: ->(_o) { p::DropmailClick.generate_email },
          get_emails: ->(e, t) { p::DropmailClick.get_emails(t.to_s.empty? ? e : t) }
        },
        "fake-email-site" => {
          generate: ->(_o) { p::FakeEmailSite.generate_email },
          get_emails: ->(e, _t) { p::FakeEmailSite.get_emails(e) }
        },
        "fmail" => {
          generate: ->(o) { p::Fmail.generate_email(o.domain) },
          get_emails: ->(e, _t) { p::Fmail.get_emails(e) }
        },
        "freecustom" => {
          generate: ->(_o) { p::Freecustom.generate_email },
          get_emails: ->(e, t) { p::Freecustom.get_emails(t.to_s.empty? ? e : t) }
        },
        "gonebox-email" => {
          generate: ->(_o) { p::GoneboxEmail.generate_email },
          get_emails: ->(e, t) { p::GoneboxEmail.get_emails(t.to_s, e) }
        },
        "harakirimail" => {
          generate: ->(_o) { p::Harakirimail.generate_email },
          get_emails: ->(e, _t) { p::Harakirimail.get_emails(e) }
        },
        "inboxes" => {
          generate: ->(o) { p::Inboxes.generate_email(o.domain) },
          get_emails: ->(e, _t) { p::Inboxes.get_emails(e) }
        },
        "m2u" => {
          generate: ->(_o) { p::M2u.generate_email },
          get_emails: ->(e, t) { p::M2u.get_emails(t.to_s, e) }
        },
        "mail10s" => {
          generate: ->(_o) { p::Mail10s.generate_email },
          get_emails: ->(e, _t) { p::Mail10s.get_emails(e) }
        },
        "mail123" => {
          generate: ->(_o) { p::Mail123.generate_email },
          get_emails: ->(e, _t) { p::Mail123.get_emails(e) }
        },
        # shard-11-independent-simple-b 渠道绑定
        "mffac" => {
          generate: ->(_o) { p::Mffac.generate_email },
          get_emails: ->(e, t) { p::Mffac.get_emails(e, t || "") }
        },
        "tempmailc" => {
          generate: ->(_o) { p::Tempmailc.generate_email },
          get_emails: ->(e, _t) { p::Tempmailc.get_emails(e) }
        },
        "neighbours-sh" => {
          generate: ->(_o) { p::NeighboursSh.generate_email },
          get_emails: ->(e, _t) { p::NeighboursSh.get_emails(e) }
        },
        "temporam" => {
          generate: ->(o) { p::Temporam.generate_email(o.domain) },
          get_emails: ->(e, t) { p::Temporam.get_emails(t, e) }
        },
        "neighbours" => {
          generate: ->(o) { p::Neighbours.generate_email(o.domain) },
          get_emails: ->(e, _t) { p::Neighbours.get_emails(e) }
        },
        "tempy-email" => {
          generate: ->(o) { p::TempyEmail.generate_email(o.domain) },
          get_emails: ->(e, _t) { p::TempyEmail.get_emails(e) }
        },
        "ockito" => {
          generate: ->(_o) { p::Ockito.generate_email },
          get_emails: ->(e, t) { p::Ockito.get_emails(require_token(t, "ockito"), e) }
        },
        "tempmail365" => {
          generate: ->(o) { p::Tempmail365.generate_email(o.domain) },
          get_emails: ->(e, _t) { p::Tempmail365.get_emails(e) }
        },
        "tempinbox" => {
          generate: ->(o) { p::Tempinbox.generate_email(o.domain) },
          get_emails: ->(e, _t) { p::Tempinbox.get_emails(e) }
        },
        "mail-sunls" => {
          generate: ->(_o) { p::MailSunls.generate_email },
          get_emails: ->(e, _t) { p::MailSunls.get_emails(e) }
        },
        "smailpro" => {
          generate: ->(_o) { p::Smailpro.generate_email },
          get_emails: ->(e, t) { p::Smailpro.get_emails(e, t || "") }
        },
        "mailhole-de" => {
          generate: ->(_o) { p::MailholeDe.generate_email },
          get_emails: ->(e, t) { p::MailholeDe.get_emails(t || e) }
        },
        "nimail" => {
          generate: ->(_o) { p::Nimail.generate_email },
          get_emails: ->(e, t) { p::Nimail.get_emails(t || e) }
        },
        # shard-09-independent-session-b 渠道绑定
        "rootsh" => {
          generate: ->(o) { p::Rootsh.generate_email(o.domain) },
          get_emails: ->(e, t) { p::Rootsh.get_emails(require_token(t, "rootsh"), e) }
        },
        "shitty-email" => {
          generate: ->(_o) { p::ShittyEmail.generate_email },
          get_emails: ->(e, t) { p::ShittyEmail.get_emails(require_token(t, "shitty-email"), e) }
        },
        "ta-easy" => {
          generate: ->(_o) { p::TaEasy.generate_email },
          get_emails: ->(e, t) { p::TaEasy.get_emails(e, require_token(t, "ta-easy")) }
        },
        "temp-mail-now" => {
          generate: ->(_o) { p::TempMailNow.generate_email },
          get_emails: ->(e, t) { p::TempMailNow.get_emails(require_token(t, "temp-mail-now"), e) }
        },
        "temp-mail-org" => {
          generate: ->(_o) { p::TempMailOrg.generate_email },
          get_emails: ->(e, t) { p::TempMailOrg.get_emails(require_token(t, "temp-mail-org"), e) }
        },
        "tempemail-co" => {
          generate: ->(_o) { p::TempemailCo.generate_email },
          get_emails: ->(e, t) { p::TempemailCo.get_emails(e, require_token(t, "tempemail-co")) }
        },
        "tempemail-info" => {
          generate: ->(_o) { p::TempemailInfo.generate_email },
          get_emails: ->(e, t) { p::TempemailInfo.get_emails(e, require_token(t, "tempemail-info")) }
        },
        "tempemails-net" => {
          generate: ->(_o) { p::TempemailsNet.generate_email },
          get_emails: ->(e, t) { p::TempemailsNet.get_emails(e, require_token(t, "tempemails-net")) }
        },
        "tempfastmail" => {
          generate: ->(_o) { p::Tempfastmail.generate_email },
          get_emails: ->(e, t) { p::Tempfastmail.get_emails(require_token(t, "tempfastmail"), e) }
        },
        "tempgmailer" => {
          generate: ->(_o) { p::Tempgmailer.generate_email },
          get_emails: ->(e, t) { p::Tempgmailer.get_emails(require_token(t, "tempgmailer"), e) }
        },
        "tempgo-email" => {
          generate: ->(_o) { p::TempgoEmail.generate_email },
          get_emails: ->(e, t) { p::TempgoEmail.get_emails(require_token(t, "tempgo-email"), e) }
        },
        "tempmail-fish" => {
          generate: ->(_o) { p::TempmailFish.generate_email },
          get_emails: ->(e, t) { p::TempmailFish.get_emails(require_token(t, "tempmail-fish"), e) }
        },
        "tempmail-fyi" => {
          generate: ->(_o) { p::TempmailFyi.generate_email },
          get_emails: ->(e, t) { p::TempmailFyi.get_emails(e, require_token(t, "tempmail-fyi")) }
        },
        "tempmail-lol-v2" => {
          generate: ->(_o) { p::TempmailLolV2.generate_email },
          get_emails: ->(e, t) { p::TempmailLolV2.get_emails(require_token(t, "tempmail-lol-v2"), e) }
        },
        "tempmailpro" => {
          generate: ->(_o) { p::Tempmailpro.generate_email },
          get_emails: ->(e, t) { p::Tempmailpro.get_emails(require_token(t, "tempmailpro"), e) }
        },
        "tempmailten" => {
          generate: ->(_o) { p::Tempmailten.generate_email },
          get_emails: ->(e, t) { p::Tempmailten.get_emails(e, require_token(t, "tempmailten")) }
        },
        "tempp-mails" => {
          generate: ->(_o) { p::TempMails.generate_email },
          get_emails: ->(e, t) { p::TempMails.get_emails(e, require_token(t, "tempp-mails")) }
        },
        "ten-minute-mail-net" => {
          generate: ->(_o) { p::TenMinuteMailNet.generate_email },
          get_emails: ->(e, t) { p::TenMinuteMailNet.get_emails(require_token(t, "ten-minute-mail-net"), e) }
        },
        "throwawaymail" => {
          generate: ->(_o) { p::Throwawaymail.generate_email },
          get_emails: ->(e, t) { p::Throwawaymail.get_emails(require_token(t, "throwawaymail"), e) }
        },
        "tmail-link" => {
          generate: ->(_o) { p::TmailLink.generate_email },
          get_emails: ->(e, t) { p::TmailLink.get_emails(e, require_token(t, "tmail-link")) }
        },
        "uncorreotemporal" => {
          generate: ->(_o) { p::Uncorreotemporal.generate_email },
          get_emails: ->(e, t) { p::Uncorreotemporal.get_emails(require_token(t, "uncorreotemporal"), e) }
        },
        "webmailtemp" => {
          generate: ->(_o) { p::Webmailtemp.generate_email },
          get_emails: ->(e, t) { p::Webmailtemp.get_emails(require_token(t, "webmailtemp"), e) }
        },
        "xkx-me" => {
          generate: ->(_o) { p::XkxMe.generate_email },
          get_emails: ->(e, t) { p::XkxMe.get_emails(require_token(t, "xkx-me"), e) }
        },
        "tempmail-cn" => {
          generate: ->(o) { p::TempmailCn.generate_email(o.domain) },
          get_emails: ->(e, _t) { p::TempmailCn.get_emails(e) }
        },
        "linshiyou" => {
          generate: ->(_o) { p::Linshiyou.generate_email },
          get_emails: ->(e, t) { p::Linshiyou.get_emails(require_token(t, "linshiyou"), e) }
        },
        "mailnesia" => {
          generate: ->(_o) { p::Mailnesia.generate_email },
          get_emails: ->(e, _t) { p::Mailnesia.get_emails(e) }
        },
        "dropmail" => {
          generate: ->(_o) { p::Dropmail.generate_email },
          get_emails: ->(e, t) { p::Dropmail.get_emails(require_token(t, "dropmail"), e) }
        },
        "smail-pw" => {
          generate: ->(_o) { p::SmailPw.generate_email },
          get_emails: ->(e, t) { p::SmailPw.get_emails(require_token(t, "smail-pw"), e) }
        },
        "vip-215" => {
          generate: ->(_o) { p::Vip215.generate_email },
          get_emails: ->(e, t) { p::Vip215.get_emails(require_token(t, "vip-215"), e) }
        },
        "tempgbox" => {
          generate: ->(_o) { p::Tempgbox.generate_email },
          get_emails: ->(e, t) { p::Tempgbox.get_emails(require_token(t, "tempgbox"), e) }
        },
        "emailnator" => {
          generate: ->(_o) { p::Emailnator.generate_email },
          get_emails: ->(e, t) { p::Emailnator.get_emails(e, require_token(t, "emailnator")) }
        },
        "anonbox" => {
          generate: ->(_o) { p::Anonbox.generate_email },
          get_emails: ->(e, t) { p::Anonbox.get_emails(require_token(t, "anonbox"), e) }
        },
        "eyepaste" => {
          generate: ->(_o) { p::Eyepaste.generate_email },
          get_emails: ->(e, _t) { p::Eyepaste.get_emails(e) }
        },
        "lroid" => {
          generate: ->(_o) { p::Lroid.generate_email },
          get_emails: ->(e, t) { p::Lroid.get_emails(require_token(t, "lroid"), e) }
        },
        "haribu" => {
          generate: ->(_o) { p::Haribu.generate_email },
          get_emails: ->(e, t) { p::Haribu.get_emails(require_token(t, "haribu"), e) }
        },
        "mohmal" => {
          generate: ->(_o) { p::Mohmal.generate_email },
          get_emails: ->(e, t) { p::Mohmal.get_emails(e, require_token(t, "mohmal")) }
        },
        "mailgolem" => {
          generate: ->(_o) { p::Mailgolem.generate_email },
          get_emails: ->(e, t) { p::Mailgolem.get_emails(e, require_token(t, "mailgolem")) }
        },
        "best-temp-mail" => {
          generate: ->(_o) { p::BestTempMail.generate_email },
          get_emails: ->(e, t) { p::BestTempMail.get_emails(e, require_token(t, "best-temp-mail")) }
        },
        "disposablemail-app" => {
          generate: ->(_o) { p::DisposablemailApp.generate_email },
          get_emails: ->(e, t) { p::DisposablemailApp.get_emails(e, require_token(t, "disposablemail-app")) }
        },
        "mailtemp-cc" => {
          generate: ->(_o) { p::MailtempCc.generate_email },
          get_emails: ->(e, t) { p::MailtempCc.get_emails(e, require_token(t, "mailtemp-cc")) }
        },
        "minuteinbox" => {
          generate: ->(_o) { p::Minuteinbox.generate_email },
          get_emails: ->(e, t) { p::Minuteinbox.get_emails(require_token(t, "minuteinbox"), e) }
        },
        "mailcatch" => {
          generate: ->(_o) { p::Mailcatch.generate_email },
          get_emails: ->(e, t) { p::Mailcatch.get_emails(e, require_token(t, "mailcatch")) }
        },
        "altmails" => {
          generate: ->(_o) { p::Altmails.generate_email },
          get_emails: ->(e, t) { p::Altmails.get_emails(e, require_token(t, "altmails")) }
        },
        "maildrop-cc" => {
          generate: ->(_o) { p::MaildropCc.generate_email },
          get_emails: ->(e, t) { p::MaildropCc.get_emails(e, t || "") }
        },
        "10minutemail-net" => {
          generate: ->(_o) { p::TenminutemailNet.generate_email },
          get_emails: ->(e, t) { p::TenminutemailNet.get_emails(e, require_token(t, "10minutemail-net")) }
        },
        "linshiyouxiang-net" => {
          generate: ->(_o) { p::LinshiyouxiangNet.generate_email },
          get_emails: ->(e, t) { p::LinshiyouxiangNet.get_emails(e, require_token(t, "linshiyouxiang-net")) }
        },
        "disposablemail-com" => {
          generate: ->(_o) { p::Disposablemail.generate_email },
          get_emails: ->(e, t) { p::Disposablemail.get_emails(e, require_token(t, "disposablemail-com")) }
        },
        "emailtemp-org" => {
          generate: ->(_o) { p::EmailtempOrg.generate_email },
          get_emails: ->(e, t) { p::EmailtempOrg.get_emails(e, require_token(t, "emailtemp-org")) }
        },
        "mytempmail-cc" => {
          generate: ->(_o) { p::MytempmailCc.generate_email },
          get_emails: ->(e, t) { p::MytempmailCc.get_emails(require_token(t, "mytempmail-cc"), e) }
        },
        "mail-td" => {
          generate: ->(_o) { p::MailTd.generate_email },
          get_emails: ->(e, t) { p::MailTd.get_emails(require_token(t, "mail-td"), e) }
        },
        "24mail-chacuo" => {
          generate: ->(_o) { p::TwentyfourmailChacuo.generate_email },
          get_emails: ->(e, _t) { p::TwentyfourmailChacuo.get_emails(e) }
        },
        "apihz" => {
          generate: ->(_o) { p::Apihz.generate_email },
          get_emails: ->(_e, t) { p::Apihz.get_emails(require_token(t, "apihz")) }
        },
        "mailcat-ai" => {
          generate: ->(_o) { p::MailcatAi.generate_email },
          get_emails: ->(e, t) { p::MailcatAi.get_emails(require_token(t, "mailcat-ai"), e) }
        },
        "dropmail-me" => {
          generate: ->(_o) { p::DropmailMe.generate_email },
          get_emails: ->(e, t) { p::DropmailMe.get_emails(require_token(t, "dropmail-me"), e) }
        },
        "openinbox" => {
          generate: ->(_o) { p::Openinbox.generate_email },
          get_emails: ->(e, t) { p::Openinbox.get_emails(require_token(t, "openinbox"), e) }
        },
        "chatgpt-org-uk" => {
          generate: ->(_o) { p::ChatgptOrgUk.generate_email },
          get_emails: ->(e, t) { p::ChatgptOrgUk.get_emails(e, require_token(t, "chatgpt-org-uk")) }
        },
        "expressinboxhub" => {
          generate: ->(_o) { p::Expressinboxhub.generate_email },
          get_emails: ->(e, t) { p::Expressinboxhub.get_emails(e, require_token(t, "expressinboxhub")) }
        },
        "fakemail" => {
          generate: ->(_o) { p::Fakemail.generate_email },
          get_emails: ->(e, t) { p::Fakemail.get_emails(require_token(t, "fakemail"), e) }
        },
        "awamail" => {
          generate: ->(_o) { p::Awamail.generate_email },
          get_emails: ->(e, t) { p::Awamail.get_emails(require_token(t, "awamail"), e) }
        },
        "email10min" => {
          generate: ->(_o) { p::Email10min.generate_email },
          get_emails: ->(e, t) { p::Email10min.get_emails(e, require_token(t, "email10min")) }
        },
      }
    end

    # 依据有序渠道元数据构建注册表：有真实实现用真实实现，否则注册桩。
    # 顺序严格遵循 CHANNEL_DATA（= baseline），为硬约束。
    def build!
      return unless REGISTRY.empty?

      impls = real_implementations
      CHANNEL_DATA.each do |slug, name, website|
        impl = impls[slug]
        spec = if impl
                 ChannelSpec.new(channel: slug, name: name, website: website,
                                 generate: impl[:generate], get_emails: impl[:get_emails])
               else
                 stub_spec(slug, name, website)
               end
        register_channel(spec)
      end
    end
  end

  # 加载时立即构建注册表
  Registry.build!
end
