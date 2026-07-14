package tempemail

import (
	"fmt"
	"math/rand"

	prov "github.com/XxxXTeam/tempmail-sdk/sdk/go/provider"
)

/* 所有支持的渠道列表，用于随机选择和遍历 */
var allChannels = []Channel{
	ChannelTempmail,
	ChannelTempmailCn,
	ChannelTaEasy,
	Channel10minuteOne,
	ChannelXghffCom,
	ChannelOqqajCom,
	ChannelPsovvCom,
	ChannelDbwotCom,
	ChannelYgwprCom,
	ChannelImxweCom,
	ChannelLinshiyou,
	ChannelMffac,
	ChannelTempmailLol,
	ChannelChatgptOrgUk,
	ChannelTempMailIo,
	ChannelMailCx,
	ChannelDdkerCom,
	ChannelCatchmail,
	ChannelCatchmailMailistry,
	ChannelCatchmailZeppost,
	ChannelMailforspam,
	ChannelMailforspamTempmailIo,
	ChannelMailforspamDisposable,
	ChannelTempmailc,
	ChannelMailnesia,
	ChannelThrowawaymail,
	ChannelTempmailFish,
	ChannelNeighboursSh,
	ChannelShittyEmail,
	ChannelTempmailpro,
	ChannelDevmailUk,
	ChannelInboxkitten,
	ChannelCleanTempMail,
	ChannelGetnada,
	ChannelOneVpnNet,
	ChannelAbematvCom,
	ChannelAbematvNet,
	ChannelAbematvOrg,
	ChannelAcehCc,
	ChannelBangkabelitungNet,
	ChannelCctruyenCom,
	ChannelGetnadaCom,
	ChannelGetnadaEmail,
	ChannelGetnadaNet,
	ChannelJawatengahNet,
	ChannelJawatimurNet,
	ChannelKalimantanbaratNet,
	ChannelKalimantanselatanNet,
	ChannelKalimantantengahNet,
	ChannelKalimantantimurNet,
	ChannelKalimantanutaraNet,
	ChannelKepulauanriauNet,
	ChannelLuxury345Com,
	ChannelMalukuutaraNet,
	ChannelNusatenggarabaratNet,
	ChannelNusatenggaratimurNet,
	ChannelPapuabaratNet,
	ChannelPapuabaratdayaNet,
	ChannelPapuaselatanNet,
	ChannelPeholCom,
	ChannelPtruyenCom,
	ChannelPulaubaliNet,
	ChannelRiauNet,
	ChannelSeokeyOrg,
	ChannelSulawesibaratNet,
	ChannelSulawesiselatanNet,
	ChannelSulawesitengahNet,
	ChannelSulawesitenggaraNet,
	ChannelSumaterabaratNet,
	ChannelSumateraselatanNet,
	ChannelSumaterautaraNet,
	ChannelVillatogelCom,
	ChannelMail123,
	ChannelMail10s,
	ChannelWebmailtemp,
	ChannelTempfastmail,
	ChannelOneSecMail,
	ChannelFakemail,
	ChannelOpeninbox,
	ChannelInboxes,
	ChannelUncorreotemporal,
	ChannelAwamail,
	ChannelMailTm,
	ChannelWebLibraryNet,
	ChannelDropmail,
	ChannelGuerrillaMail,
	ChannelGuerrillamailCom,
	ChannelMaildrop,
	ChannelSmailPw,
	ChannelVip215,
	ChannelFakeLegal,
	ChannelImguiDe,
	ChannelPulsewebmenuDe,
	ChannelMoakt,
	ChannelDrmailIn,
	ChannelTemlNet,
	ChannelTmpemlCom,
	ChannelTmpboxNet,
	ChannelMoaktCc,
	ChannelDisboxNet,
	ChannelTmpmailOrg,
	ChannelTmpmailNet,
	ChannelTmailsNet,
	ChannelDisboxOrg,
	ChannelMoaktCo,
	ChannelMoaktWs,
	ChannelTmailWs,
	ChannelBareedWs,
	ChannelEmail10min,
	ChannelMjjCm,
	ChannelLinshiCo,
	ChannelHarakirimail,
	ChannelJqkjqkXyz,
	ChannelLyhleviCom,
	ChannelTempmailPlus,
	ChannelFexpostCom,
	ChannelFexboxOrg,
	ChannelMailboxInUa,
	ChannelRoverInfo,
	ChannelChitthiIn,
	ChannelFextempCom,
	ChannelAnyPink,
	ChannelMerepostCom,
	ChannelTempmailLolV2,
	ChannelTempgbox,
	ChannelEmailnator,
	ChannelTemporam,
	ChannelNeighbours,
	ChannelSharklasers,
	ChannelSharklasersCom,
	ChannelGrrLa,
	ChannelGrrLaCom,
	ChannelGuerrillamailInfo,
	ChannelSpam4me,
	ChannelGuerrillamailNet,
	ChannelGuerrillamailOrg,
	ChannelGuerrillamailBlock,
	ChannelGuerrillamailComWww,
	ChannelM2u,
	ChannelTempyEmail,
	ChannelFmail,
	ChannelOckito,
	ChannelAnonbox,
	ChannelDuckmail,
	ChannelMailinator,
	ChannelTempmail365,
	ChannelTempinbox,
	ChannelByom,
	ChannelAnonymmail,
	ChannelEyepaste,
	ChannelMailSunls,
	ChannelExpressinboxhub,
	ChannelLroid,
	ChannelHaribu,
	ChannelRootsh,
	ChannelFakeEmailSite,
	ChannelMohmal,
	ChannelMailgolem,
	ChannelBestTempMail,
	ChannelDisposablemailApp,
	ChannelMailtempCc,
	ChannelMinuteinbox,
	ChannelMailcatch,
	ChannelTempemailCo,
	ChannelTempemailsNet,
	ChannelAltmails,
	ChannelTempemailInfo,
	ChannelSmailpro,
	ChannelTempmailten,
	ChannelMaildropCc,
	ChannelTenminutemailNet,
	ChannelLinshiyouxiangNet,
	ChannelTempMailFyi,
	ChannelDisposablemailCom,
	ChannelTemppMails,
	ChannelEmailtempOrg,
	ChannelMytempmailCc,
	ChannelTempMailNow,
	ChannelMailTd,
	ChannelMailholeDe,
	ChannelTmailLink,
	Channel24mailChacuo,
	ChannelNimail,
	ChannelFreecustom,
	ChannelN16888888Cyou,
	ChannelN17666688Shop,
	ChannelN282mailCom,
	ChannelBlackholeDjurbySe,
	ChannelBlockBdeaCc,
	ChannelBsdu32Buzz,
	ChannelBSmellyCc,
	ChannelCarlton183ChangeipNet,
	ChannelDeaSoonIt,
	ChannelDisposableAlSudaniCom,
	ChannelDisposableNogonadNl,
	ChannelDoxu243Buzz,
	ChannelEasymePro,
	ChannelEbsComAr,
	ChannelEtgdevDe,
	ChannelEvergreencoShop,
	ChannelFwd2mEszettEs,
	ChannelJamaTrenetEu,
	ChannelJFairuseOrg,
	ChannelLayuemingPics,
	ChannelM887At,
	ChannelM8rDavidfuhrDe,
	ChannelM8rMcasalCom,
	ChannelMailBentraskCom,
	ChannelMailFsmashOrg,
	ChannelMailinatorzzmoooCom,
	ChannelMiMeonBe,
	ChannelMingyuekejiOnline,
	ChannelMingyuemingClick,
	ChannelMingyuemingShop,
	ChannelMingyukejiLol,
	ChannelMnCurppaCom,
	ChannelMNikMe,
	ChannelMtmdevCom,
	ChannelNospamThurstonsUs,
	ChannelNotfond404Mn,
	ChannelNullK3vinNet,
	ChannelNuxh62Space,
	ChannelProidCloudIpCc,
	ChannelRamjaneMoooCom,
	ChannelRauxaSenyCat,
	ChannelReallyIstrashCom,
	ChannelSbookPics,
	ChannelSpamHortukOvh,
	ChannelSpWootAt,
	ChannelTestUnergieCom,
	ChannelTorchYiOrg,
	ChannelTZibetNet,
	ChannelXue32Buzz,
	ChannelApihz,
	ChannelSogetthisCom,
	ChannelBobmailInfo,
	ChannelSuremailInfo,
	ChannelBinkmailCom,
	ChannelVeryrealemailCom,
	ChannelMailmomy,
	ChannelChammyInfo,
	ChannelThisisnotmyrealemailCom,
	ChannelNotmailinatorCom,
	ChannelSpamherepleaseCom,
	ChannelSendspamhereCom,
	ChannelSendfreeOrg,
	ChannelJunkBeatsOrg,
	ChannelJunkIhmehlCom,
	ChannelJunkNoplayOrg,
	ChannelJunkVanillasystemCom,
	ChannelSpamJasonpearceCom,
	ChannelFishSkytaleNet,
	ChannelSpamMccrewCom,
	ChannelDropmailClick,
	ChannelSpamCoroiuCom,
	ChannelSpamDeluserNet,
	ChannelSpamDhsfNet,
	ChannelSpamLucatntCom,
	ChannelSpamLyceumLifeComRu,
	ChannelSpamNetpiratesNet,
	ChannelSpamNoIpNet,
	ChannelSpamOzhOrg,
	ChannelSpamPyphusOrg,
	ChannelSpamShepPw,
	ChannelSpamWtfAt,
	ChannelSpamWulczerOrg,
	ChannelCrapKakaduaNet,
	ChannelSpamJanlugtNl,
	ChannelMinBurningfishNet,
	ChannelSinkFblayCom,
}

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

/* 渠道信息映射表 */
var channelInfoMap = map[Channel]ChannelInfo{
	ChannelTempmail:                {Channel: ChannelTempmail, Name: "TempMail", Website: "tempmail.ing"},
	ChannelTempmailCn:              {Channel: ChannelTempmailCn, Name: "TempMail CN", Website: "tempmail.cn"},
	ChannelTaEasy:                  {Channel: ChannelTaEasy, Name: "TA Easy", Website: "ta-easy.com"},
	Channel10minuteOne:             {Channel: Channel10minuteOne, Name: "10 Minute Email", Website: "10minutemail.one"},
	ChannelXghffCom:                {Channel: ChannelXghffCom, Name: "xghff.com", Website: "10minutemail.one"},
	ChannelOqqajCom:                {Channel: ChannelOqqajCom, Name: "oqqaj.com", Website: "10minutemail.one"},
	ChannelPsovvCom:                {Channel: ChannelPsovvCom, Name: "psovv.com", Website: "10minutemail.one"},
	ChannelDbwotCom:                {Channel: ChannelDbwotCom, Name: "dbwot.com", Website: "10minutemail.one"},
	ChannelYgwprCom:                {Channel: ChannelYgwprCom, Name: "ygwpr.com", Website: "10minutemail.one"},
	ChannelImxweCom:                {Channel: ChannelImxweCom, Name: "imxwe.com", Website: "10minutemail.one"},
	ChannelLinshiyou:               {Channel: ChannelLinshiyou, Name: "临时邮", Website: "linshiyou.com"},
	ChannelMffac:                   {Channel: ChannelMffac, Name: "MFFAC", Website: "mffac.com"},
	ChannelTempmailLol:             {Channel: ChannelTempmailLol, Name: "TempMail LOL", Website: "tempmail.lol"},
	ChannelChatgptOrgUk:            {Channel: ChannelChatgptOrgUk, Name: "ChatGPT Mail", Website: "mail.chatgpt.org.uk"},
	ChannelTempMailIo:              {Channel: ChannelTempMailIo, Name: "Temp-Mail.io", Website: "temp-mail.io"},
	ChannelMailCx:                  {Channel: ChannelMailCx, Name: "Mail.cx", Website: "mail.cx"},
	ChannelDdkerCom:                {Channel: ChannelDdkerCom, Name: "ddker.com", Website: "mail.cx"},
	ChannelCatchmail:               {Channel: ChannelCatchmail, Name: "Catchmail", Website: "catchmail.io"},
	ChannelCatchmailMailistry:      {Channel: ChannelCatchmailMailistry, Name: "Catchmail Mailistry", Website: "mailistry.com"},
	ChannelCatchmailZeppost:        {Channel: ChannelCatchmailZeppost, Name: "Catchmail Zeppost", Website: "zeppost.com"},
	ChannelMailforspam:             {Channel: ChannelMailforspam, Name: "MailForSpam", Website: "mailforspam.com"},
	ChannelMailforspamTempmailIo:   {Channel: ChannelMailforspamTempmailIo, Name: "MailForSpam TempMail.io", Website: "tempmail.io"},
	ChannelMailforspamDisposable:   {Channel: ChannelMailforspamDisposable, Name: "MailForSpam Disposable", Website: "disposable.email"},
	ChannelTempmailc:               {Channel: ChannelTempmailc, Name: "TempMailC", Website: "tempmailc.com"},
	ChannelMailnesia:               {Channel: ChannelMailnesia, Name: "Mailnesia", Website: "mailnesia.com"},
	ChannelThrowawaymail:           {Channel: ChannelThrowawaymail, Name: "ThrowawayMail", Website: "throwawaymail.app"},
	ChannelTempmailFish:            {Channel: ChannelTempmailFish, Name: "TempMail Fish", Website: "tempmail.fish"},
	ChannelNeighboursSh:            {Channel: ChannelNeighboursSh, Name: "Neighbours", Website: "neighbours.sh"},
	ChannelShittyEmail:             {Channel: ChannelShittyEmail, Name: "shitty.email", Website: "shitty.email"},
	ChannelTempmailpro:             {Channel: ChannelTempmailpro, Name: "TempMail Pro", Website: "tempmailpro.us"},
	ChannelDevmailUk:               {Channel: ChannelDevmailUk, Name: "DevMail UK", Website: "devmail.uk"},
	ChannelInboxkitten:             {Channel: ChannelInboxkitten, Name: "InboxKitten", Website: "inboxkitten.com"},
	ChannelCleanTempMail:           {Channel: ChannelCleanTempMail, Name: "CleanTempMail", Website: "cleantempmail.com"},
	ChannelGetnada:                 {Channel: ChannelGetnada, Name: "GetNada", Website: "getnada.net"},
	ChannelOneVpnNet:               {Channel: ChannelOneVpnNet, Name: "1vpn.net", Website: "getnada.net"},
	ChannelAbematvCom:              {Channel: ChannelAbematvCom, Name: "abematv.com", Website: "getnada.net"},
	ChannelAbematvNet:              {Channel: ChannelAbematvNet, Name: "abematv.net", Website: "getnada.net"},
	ChannelAbematvOrg:              {Channel: ChannelAbematvOrg, Name: "abematv.org", Website: "getnada.net"},
	ChannelAcehCc:                  {Channel: ChannelAcehCc, Name: "aceh.cc", Website: "getnada.net"},
	ChannelBangkabelitungNet:       {Channel: ChannelBangkabelitungNet, Name: "bangkabelitung.net", Website: "getnada.net"},
	ChannelCctruyenCom:             {Channel: ChannelCctruyenCom, Name: "cctruyen.com", Website: "getnada.net"},
	ChannelGetnadaCom:              {Channel: ChannelGetnadaCom, Name: "getnada.com", Website: "getnada.net"},
	ChannelGetnadaEmail:            {Channel: ChannelGetnadaEmail, Name: "getnada.email", Website: "getnada.net"},
	ChannelGetnadaNet:              {Channel: ChannelGetnadaNet, Name: "getnada.net", Website: "getnada.net"},
	ChannelJawatengahNet:           {Channel: ChannelJawatengahNet, Name: "jawatengah.net", Website: "getnada.net"},
	ChannelJawatimurNet:            {Channel: ChannelJawatimurNet, Name: "jawatimur.net", Website: "getnada.net"},
	ChannelKalimantanbaratNet:      {Channel: ChannelKalimantanbaratNet, Name: "kalimantanbarat.net", Website: "getnada.net"},
	ChannelKalimantanselatanNet:    {Channel: ChannelKalimantanselatanNet, Name: "kalimantanselatan.net", Website: "getnada.net"},
	ChannelKalimantantengahNet:     {Channel: ChannelKalimantantengahNet, Name: "kalimantantengah.net", Website: "getnada.net"},
	ChannelKalimantantimurNet:      {Channel: ChannelKalimantantimurNet, Name: "kalimantantimur.net", Website: "getnada.net"},
	ChannelKalimantanutaraNet:      {Channel: ChannelKalimantanutaraNet, Name: "kalimantanutara.net", Website: "getnada.net"},
	ChannelKepulauanriauNet:        {Channel: ChannelKepulauanriauNet, Name: "kepulauanriau.net", Website: "getnada.net"},
	ChannelLuxury345Com:            {Channel: ChannelLuxury345Com, Name: "luxury345.com", Website: "getnada.net"},
	ChannelMalukuutaraNet:          {Channel: ChannelMalukuutaraNet, Name: "malukuutara.net", Website: "getnada.net"},
	ChannelNusatenggarabaratNet:    {Channel: ChannelNusatenggarabaratNet, Name: "nusatenggarabarat.net", Website: "getnada.net"},
	ChannelNusatenggaratimurNet:    {Channel: ChannelNusatenggaratimurNet, Name: "nusatenggaratimur.net", Website: "getnada.net"},
	ChannelPapuabaratNet:           {Channel: ChannelPapuabaratNet, Name: "papuabarat.net", Website: "getnada.net"},
	ChannelPapuabaratdayaNet:       {Channel: ChannelPapuabaratdayaNet, Name: "papuabaratdaya.net", Website: "getnada.net"},
	ChannelPapuaselatanNet:         {Channel: ChannelPapuaselatanNet, Name: "papuaselatan.net", Website: "getnada.net"},
	ChannelPeholCom:                {Channel: ChannelPeholCom, Name: "pehol.com", Website: "getnada.net"},
	ChannelPtruyenCom:              {Channel: ChannelPtruyenCom, Name: "ptruyen.com", Website: "getnada.net"},
	ChannelPulaubaliNet:            {Channel: ChannelPulaubaliNet, Name: "pulaubali.net", Website: "getnada.net"},
	ChannelRiauNet:                 {Channel: ChannelRiauNet, Name: "riau.net", Website: "getnada.net"},
	ChannelSeokeyOrg:               {Channel: ChannelSeokeyOrg, Name: "seokey.org", Website: "getnada.net"},
	ChannelSulawesibaratNet:        {Channel: ChannelSulawesibaratNet, Name: "sulawesibarat.net", Website: "getnada.net"},
	ChannelSulawesiselatanNet:      {Channel: ChannelSulawesiselatanNet, Name: "sulawesiselatan.net", Website: "getnada.net"},
	ChannelSulawesitengahNet:       {Channel: ChannelSulawesitengahNet, Name: "sulawesitengah.net", Website: "getnada.net"},
	ChannelSulawesitenggaraNet:     {Channel: ChannelSulawesitenggaraNet, Name: "sulawesitenggara.net", Website: "getnada.net"},
	ChannelSumaterabaratNet:        {Channel: ChannelSumaterabaratNet, Name: "sumaterabarat.net", Website: "getnada.net"},
	ChannelSumateraselatanNet:      {Channel: ChannelSumateraselatanNet, Name: "sumateraselatan.net", Website: "getnada.net"},
	ChannelSumaterautaraNet:        {Channel: ChannelSumaterautaraNet, Name: "sumaterautara.net", Website: "getnada.net"},
	ChannelVillatogelCom:           {Channel: ChannelVillatogelCom, Name: "villatogel.com", Website: "getnada.net"},
	ChannelMail123:                 {Channel: ChannelMail123, Name: "Mail123", Website: "mail123.fr"},
	ChannelMail10s:                 {Channel: ChannelMail10s, Name: "Mail10s", Website: "mail10s.com"},
	ChannelWebmailtemp:             {Channel: ChannelWebmailtemp, Name: "WebMailTemp", Website: "webmailtemp.com"},
	ChannelTempfastmail:            {Channel: ChannelTempfastmail, Name: "TempFastMail", Website: "tempfastmail.com"},
	ChannelOneSecMail:              {Channel: ChannelOneSecMail, Name: "1SecMail", Website: "1sec-mail.com"},
	ChannelFakemail:                {Channel: ChannelFakemail, Name: "FakeMail", Website: "fakemail.net"},
	ChannelOpeninbox:               {Channel: ChannelOpeninbox, Name: "OpenInbox", Website: "openinbox.io"},
	ChannelInboxes:                 {Channel: ChannelInboxes, Name: "Inboxes", Website: "inboxes.com"},
	ChannelUncorreotemporal:        {Channel: ChannelUncorreotemporal, Name: "UnCorreoTemporal", Website: "uncorreotemporal.com"},
	ChannelAwamail:                 {Channel: ChannelAwamail, Name: "AwaMail", Website: "awamail.com"},
	ChannelMailTm:                  {Channel: ChannelMailTm, Name: "Mail.tm", Website: "mail.tm"},
	ChannelWebLibraryNet:           {Channel: ChannelWebLibraryNet, Name: "web-library.net", Website: "mail.tm"},
	ChannelDropmail:                {Channel: ChannelDropmail, Name: "DropMail", Website: "dropmail.me"},
	ChannelGuerrillaMail:           {Channel: ChannelGuerrillaMail, Name: "Guerrilla Mail", Website: "guerrillamail.com"},
	ChannelGuerrillamailCom:        {Channel: ChannelGuerrillamailCom, Name: "GuerrillaMail Root", Website: "guerrillamail.com"},
	ChannelMaildrop:                {Channel: ChannelMaildrop, Name: "Maildrop", Website: "maildrop.cx"},
	ChannelSmailPw:                 {Channel: ChannelSmailPw, Name: "Smail.pw", Website: "smail.pw"},
	ChannelVip215:                  {Channel: ChannelVip215, Name: "VIP 215", Website: "vip.215.im"},
	ChannelFakeLegal:               {Channel: ChannelFakeLegal, Name: "Fake Legal", Website: "fake.legal"},
	ChannelImguiDe:                 {Channel: ChannelImguiDe, Name: "imgui.de", Website: "fake.legal"},
	ChannelPulsewebmenuDe:          {Channel: ChannelPulsewebmenuDe, Name: "pulsewebmenu.de", Website: "fake.legal"},
	ChannelMoakt:                   {Channel: ChannelMoakt, Name: "Moakt", Website: "moakt.com"},
	ChannelDrmailIn:                {Channel: ChannelDrmailIn, Name: "drmail.in", Website: "moakt.com"},
	ChannelTemlNet:                 {Channel: ChannelTemlNet, Name: "teml.net", Website: "moakt.com"},
	ChannelTmpemlCom:               {Channel: ChannelTmpemlCom, Name: "tmpeml.com", Website: "moakt.com"},
	ChannelTmpboxNet:               {Channel: ChannelTmpboxNet, Name: "tmpbox.net", Website: "moakt.com"},
	ChannelMoaktCc:                 {Channel: ChannelMoaktCc, Name: "moakt.cc", Website: "moakt.com"},
	ChannelDisboxNet:               {Channel: ChannelDisboxNet, Name: "disbox.net", Website: "moakt.com"},
	ChannelTmpmailOrg:              {Channel: ChannelTmpmailOrg, Name: "tmpmail.org", Website: "moakt.com"},
	ChannelTmpmailNet:              {Channel: ChannelTmpmailNet, Name: "tmpmail.net", Website: "moakt.com"},
	ChannelTmailsNet:               {Channel: ChannelTmailsNet, Name: "tmails.net", Website: "moakt.com"},
	ChannelDisboxOrg:               {Channel: ChannelDisboxOrg, Name: "disbox.org", Website: "moakt.com"},
	ChannelMoaktCo:                 {Channel: ChannelMoaktCo, Name: "moakt.co", Website: "moakt.com"},
	ChannelMoaktWs:                 {Channel: ChannelMoaktWs, Name: "moakt.ws", Website: "moakt.com"},
	ChannelTmailWs:                 {Channel: ChannelTmailWs, Name: "tmail.ws", Website: "moakt.com"},
	ChannelBareedWs:                {Channel: ChannelBareedWs, Name: "bareed.ws", Website: "moakt.com"},
	ChannelEmail10min:              {Channel: ChannelEmail10min, Name: "Email10Min", Website: "email10min.com"},
	ChannelMjjCm:                   {Channel: ChannelMjjCm, Name: "MJJ Mail", Website: "mjj.cm"},
	ChannelLinshiCo:                {Channel: ChannelLinshiCo, Name: "临时Co", Website: "linshi.co"},
	ChannelHarakirimail:            {Channel: ChannelHarakirimail, Name: "HarakiriMail", Website: "harakirimail.com"},
	ChannelJqkjqkXyz:               {Channel: ChannelJqkjqkXyz, Name: "jqkjqk.xyz", Website: "mail.zhujump.com"},
	ChannelLyhleviCom:              {Channel: ChannelLyhleviCom, Name: "LyhLevi MoeMail", Website: "lyhlevi.com"},
	ChannelTempmailPlus:            {Channel: ChannelTempmailPlus, Name: "TempMail Plus", Website: "tempmail.plus"},
	ChannelFexpostCom:              {Channel: ChannelFexpostCom, Name: "fexpost.com", Website: "tempmail.plus"},
	ChannelFexboxOrg:               {Channel: ChannelFexboxOrg, Name: "fexbox.org", Website: "tempmail.plus"},
	ChannelMailboxInUa:             {Channel: ChannelMailboxInUa, Name: "mailbox.in.ua", Website: "tempmail.plus"},
	ChannelRoverInfo:               {Channel: ChannelRoverInfo, Name: "rover.info", Website: "tempmail.plus"},
	ChannelChitthiIn:               {Channel: ChannelChitthiIn, Name: "chitthi.in", Website: "tempmail.plus"},
	ChannelFextempCom:              {Channel: ChannelFextempCom, Name: "fextemp.com", Website: "tempmail.plus"},
	ChannelAnyPink:                 {Channel: ChannelAnyPink, Name: "any.pink", Website: "tempmail.plus"},
	ChannelMerepostCom:             {Channel: ChannelMerepostCom, Name: "merepost.com", Website: "tempmail.plus"},
	ChannelTempmailLolV2:           {Channel: ChannelTempmailLolV2, Name: "TempMail LOL V2", Website: "tempmail.lol"},
	ChannelTempgbox:                {Channel: ChannelTempgbox, Name: "TempGBox", Website: "tempgbox.net"},
	ChannelEmailnator:              {Channel: ChannelEmailnator, Name: "Emailnator", Website: "emailnator.com"},
	ChannelTemporam:                {Channel: ChannelTemporam, Name: "Temporam", Website: "temporam.com"},
	ChannelNeighbours:              {Channel: ChannelNeighbours, Name: "Neighbours", Website: "neighbours.sh"},
	ChannelSharklasers:             {Channel: ChannelSharklasers, Name: "SharkLasers", Website: "sharklasers.com"},
	ChannelSharklasersCom:          {Channel: ChannelSharklasersCom, Name: "SharkLasers Root", Website: "sharklasers.com"},
	ChannelGrrLa:                   {Channel: ChannelGrrLa, Name: "Grr.la", Website: "grr.la"},
	ChannelGrrLaCom:                {Channel: ChannelGrrLaCom, Name: "Grr.la Root", Website: "grr.la"},
	ChannelGuerrillamailInfo:       {Channel: ChannelGuerrillamailInfo, Name: "GuerrillaMail Info", Website: "guerrillamail.info"},
	ChannelSpam4me:                 {Channel: ChannelSpam4me, Name: "Spam4.me", Website: "spam4.me"},
	ChannelGuerrillamailNet:        {Channel: ChannelGuerrillamailNet, Name: "GuerrillaMail Net", Website: "guerrillamail.net"},
	ChannelGuerrillamailOrg:        {Channel: ChannelGuerrillamailOrg, Name: "GuerrillaMail Org", Website: "guerrillamail.org"},
	ChannelGuerrillamailBlock:      {Channel: ChannelGuerrillamailBlock, Name: "GuerrillaMailBlock", Website: "guerrillamailblock.com"},
	ChannelGuerrillamailComWww:     {Channel: ChannelGuerrillamailComWww, Name: "GuerrillaMail WWW", Website: "guerrillamail.com"},
	ChannelM2u:                     {Channel: ChannelM2u, Name: "MailToYou", Website: "m2u.io"},
	ChannelTempyEmail:              {Channel: ChannelTempyEmail, Name: "Tempy Email", Website: "tempy.email"},
	ChannelFmail:                   {Channel: ChannelFmail, Name: "FMail", Website: "fmail.men"},
	ChannelOckito:                  {Channel: ChannelOckito, Name: "Ockito", Website: "ockito.com"},
	ChannelAnonbox:                 {Channel: ChannelAnonbox, Name: "Anonbox", Website: "anonbox.net"},
	ChannelDuckmail:                {Channel: ChannelDuckmail, Name: "DuckMail", Website: "duckmail.sbs"},
	ChannelMailinator:              {Channel: ChannelMailinator, Name: "Mailinator", Website: "mailinator.com"},
	ChannelTempmail365:             {Channel: ChannelTempmail365, Name: "Tempmail365", Website: "https://tempmail365.cn"},
	ChannelTempinbox:               {Channel: ChannelTempinbox, Name: "TempInbox", Website: "https://www.tempinbox.xyz"},
	ChannelByom:                    {Channel: ChannelByom, Name: "Byom", Website: "byom.de"},
	ChannelAnonymmail:              {Channel: ChannelAnonymmail, Name: "AnonymMail", Website: "anonymmail.net"},
	ChannelEyepaste:                {Channel: ChannelEyepaste, Name: "EyePaste", Website: "eyepaste.com"},
	ChannelMailSunls:               {Channel: ChannelMailSunls, Name: "Mail Sunls", Website: "mail.sunls.de"},
	ChannelExpressinboxhub:         {Channel: ChannelExpressinboxhub, Name: "ExpressInboxHub", Website: "expressinboxhub.com"},
	ChannelLroid:                   {Channel: ChannelLroid, Name: "Lroid", Website: "lroid.com"},
	ChannelHaribu:                  {Channel: ChannelHaribu, Name: "Haribu", Website: "haribu.net"},
	ChannelRootsh:                  {Channel: ChannelRootsh, Name: "Rootsh(BccTo)", Website: "rootsh.com"},
	ChannelFakeEmailSite:           {Channel: ChannelFakeEmailSite, Name: "FakeEmailSite", Website: "fake-email.site"},
	ChannelMohmal:                  {Channel: ChannelMohmal, Name: "Mohmal", Website: "mohmal.com"},
	ChannelMailgolem:               {Channel: ChannelMailgolem, Name: "MailGolem", Website: "mailgolem.com"},
	ChannelBestTempMail:            {Channel: ChannelBestTempMail, Name: "BestTempMail", Website: "best-temp-mail.com"},
	ChannelDisposablemailApp:       {Channel: ChannelDisposablemailApp, Name: "DisposableMail", Website: "disposablemail.app"},
	ChannelMailtempCc:              {Channel: ChannelMailtempCc, Name: "MailTemp.cc", Website: "mailtemp.cc"},
	ChannelMinuteinbox:             {Channel: ChannelMinuteinbox, Name: "MinuteInbox", Website: "minuteinbox.com"},
	ChannelMailcatch:               {Channel: ChannelMailcatch, Name: "MailCatch", Website: "mailcatch.com"},
	ChannelTempemailCo:             {Channel: ChannelTempemailCo, Name: "TempEmail.co", Website: "tempemail.co"},
	ChannelTempemailsNet:           {Channel: ChannelTempemailsNet, Name: "TempEmails.net", Website: "tempemails.net"},
	ChannelAltmails:                {Channel: ChannelAltmails, Name: "AltMails", Website: "tempmail.altmails.com"},
	ChannelTempemailInfo:           {Channel: ChannelTempemailInfo, Name: "TempEmailInfo", Website: "tempemail.info"},
	ChannelSmailpro:                {Channel: ChannelSmailpro, Name: "SmailPro", Website: "smailpro.com"},
	ChannelTempmailten:             {Channel: ChannelTempmailten, Name: "TempMailTen", Website: "tempmailten.com"},
	ChannelMaildropCc:              {Channel: ChannelMaildropCc, Name: "MailDrop.cc", Website: "maildrop.cc"},
	ChannelTenminutemailNet:        {Channel: ChannelTenminutemailNet, Name: "10MinuteMail.net", Website: "10minutemail.net"},
	ChannelLinshiyouxiangNet:       {Channel: ChannelLinshiyouxiangNet, Name: "临时邮箱(linshiyouxiang)", Website: "linshiyouxiang.net"},
	ChannelTempMailFyi:             {Channel: ChannelTempMailFyi, Name: "Temp-Mail.fyi", Website: "temp-mail.fyi"},
	ChannelDisposablemailCom:       {Channel: ChannelDisposablemailCom, Name: "DisposableMail.com", Website: "disposablemail.com"},
	ChannelTemppMails:              {Channel: ChannelTemppMails, Name: "TemppMails", Website: "tempp-mails.com"},
	ChannelEmailtempOrg:            {Channel: ChannelEmailtempOrg, Name: "EmailTemp", Website: "emailtemp.org"},
	ChannelMytempmailCc:            {Channel: ChannelMytempmailCc, Name: "MyTempMail.cc", Website: "mytempmail.cc"},
	ChannelTempMailNow:             {Channel: ChannelTempMailNow, Name: "TempMailNow", Website: "temp-mail.now"},
	ChannelMailTd:                  {Channel: ChannelMailTd, Name: "Mail.td", Website: "mail.td"},
	ChannelMailholeDe:              {Channel: ChannelMailholeDe, Name: "Mailhole.de", Website: "mailhole.de"},
	ChannelTmailLink:               {Channel: ChannelTmailLink, Name: "TMail.link", Website: "tmail.link"},
	Channel24mailChacuo:            {Channel: Channel24mailChacuo, Name: "24Mail Chacuo", Website: "24mail.chacuo.net"},
	ChannelNimail:                  {Channel: ChannelNimail, Name: "NiMail", Website: "nimail.cn"},
	ChannelFreecustom:              {Channel: ChannelFreecustom, Name: "FreeCustom.Email", Website: "freecustom.email"},
	ChannelN16888888Cyou:           {Channel: ChannelN16888888Cyou, Name: "Mailmomy (16888888.cyou)", Website: "mailmomy.com"},
	ChannelN17666688Shop:           {Channel: ChannelN17666688Shop, Name: "Mailmomy (17666688.shop)", Website: "mailmomy.com"},
	ChannelN282mailCom:             {Channel: ChannelN282mailCom, Name: "Mailmomy (282mail.com)", Website: "mailmomy.com"},
	ChannelBlackholeDjurbySe:       {Channel: ChannelBlackholeDjurbySe, Name: "Mailinator (blackhole.djurby.se)", Website: "mailinator.com"},
	ChannelBlockBdeaCc:             {Channel: ChannelBlockBdeaCc, Name: "Mailinator (block.bdea.cc)", Website: "mailinator.com"},
	ChannelBsdu32Buzz:              {Channel: ChannelBsdu32Buzz, Name: "Mailmomy (bsdu32.buzz)", Website: "mailmomy.com"},
	ChannelBSmellyCc:               {Channel: ChannelBSmellyCc, Name: "Mailinator (b.smelly.cc)", Website: "mailinator.com"},
	ChannelCarlton183ChangeipNet:   {Channel: ChannelCarlton183ChangeipNet, Name: "Mailinator (183carlton.changeip.net)", Website: "mailinator.com"},
	ChannelDeaSoonIt:               {Channel: ChannelDeaSoonIt, Name: "Mailinator (dea.soon.it)", Website: "mailinator.com"},
	ChannelDisposableAlSudaniCom:   {Channel: ChannelDisposableAlSudaniCom, Name: "Mailinator (disposable.al-sudani.com)", Website: "mailinator.com"},
	ChannelDisposableNogonadNl:     {Channel: ChannelDisposableNogonadNl, Name: "Mailinator (disposable.nogonad.nl)", Website: "mailinator.com"},
	ChannelDoxu243Buzz:             {Channel: ChannelDoxu243Buzz, Name: "Mailmomy (doxu243.buzz)", Website: "mailmomy.com"},
	ChannelEasymePro:               {Channel: ChannelEasymePro, Name: "Mailmomy (easyme.pro)", Website: "mailmomy.com"},
	ChannelEbsComAr:                {Channel: ChannelEbsComAr, Name: "Mailinator (ebs.com.ar)", Website: "mailinator.com"},
	ChannelEtgdevDe:                {Channel: ChannelEtgdevDe, Name: "Mailinator (etgdev.de)", Website: "mailinator.com"},
	ChannelEvergreencoShop:         {Channel: ChannelEvergreencoShop, Name: "Mailmomy (evergreenco.shop)", Website: "mailmomy.com"},
	ChannelFwd2mEszettEs:           {Channel: ChannelFwd2mEszettEs, Name: "Mailinator (fwd2m.eszett.es)", Website: "mailinator.com"},
	ChannelJamaTrenetEu:            {Channel: ChannelJamaTrenetEu, Name: "Mailinator (jama.trenet.eu)", Website: "mailinator.com"},
	ChannelJFairuseOrg:             {Channel: ChannelJFairuseOrg, Name: "Mailinator (j.fairuse.org)", Website: "mailinator.com"},
	ChannelLayuemingPics:           {Channel: ChannelLayuemingPics, Name: "Mailmomy (layueming.pics)", Website: "mailmomy.com"},
	ChannelM887At:                  {Channel: ChannelM887At, Name: "Mailinator (m.887.at)", Website: "mailinator.com"},
	ChannelM8rDavidfuhrDe:          {Channel: ChannelM8rDavidfuhrDe, Name: "Mailinator (m8r.davidfuhr.de)", Website: "mailinator.com"},
	ChannelM8rMcasalCom:            {Channel: ChannelM8rMcasalCom, Name: "Mailinator (m8r.mcasal.com)", Website: "mailinator.com"},
	ChannelMailBentraskCom:         {Channel: ChannelMailBentraskCom, Name: "Mailinator (mail.bentrask.com)", Website: "mailinator.com"},
	ChannelMailFsmashOrg:           {Channel: ChannelMailFsmashOrg, Name: "Mailinator (mail.fsmash.org)", Website: "mailinator.com"},
	ChannelMailinatorzzmoooCom:     {Channel: ChannelMailinatorzzmoooCom, Name: "Mailinator (mailinatorzz.mooo.com)", Website: "mailinator.com"},
	ChannelMiMeonBe:                {Channel: ChannelMiMeonBe, Name: "Mailinator (mi.meon.be)", Website: "mailinator.com"},
	ChannelMingyuekejiOnline:       {Channel: ChannelMingyuekejiOnline, Name: "Mailmomy (mingyuekeji.online)", Website: "mailmomy.com"},
	ChannelMingyuemingClick:        {Channel: ChannelMingyuemingClick, Name: "Mailmomy (mingyueming.click)", Website: "mailmomy.com"},
	ChannelMingyuemingShop:         {Channel: ChannelMingyuemingShop, Name: "Mailmomy (mingyueming.shop)", Website: "mailmomy.com"},
	ChannelMingyukejiLol:           {Channel: ChannelMingyukejiLol, Name: "Mailmomy (mingyukeji.lol)", Website: "mailmomy.com"},
	ChannelMnCurppaCom:             {Channel: ChannelMnCurppaCom, Name: "Mailinator (mn.curppa.com)", Website: "mailinator.com"},
	ChannelMNikMe:                  {Channel: ChannelMNikMe, Name: "Mailinator (m.nik.me)", Website: "mailinator.com"},
	ChannelMtmdevCom:               {Channel: ChannelMtmdevCom, Name: "Mailinator (mtmdev.com)", Website: "mailinator.com"},
	ChannelNospamThurstonsUs:       {Channel: ChannelNospamThurstonsUs, Name: "Mailinator (nospam.thurstons.us)", Website: "mailinator.com"},
	ChannelNotfond404Mn:            {Channel: ChannelNotfond404Mn, Name: "Mailinator (notfond.404.mn)", Website: "mailinator.com"},
	ChannelNullK3vinNet:            {Channel: ChannelNullK3vinNet, Name: "Mailinator (null.k3vin.net)", Website: "mailinator.com"},
	ChannelNuxh62Space:             {Channel: ChannelNuxh62Space, Name: "Mailmomy (nuxh62.space)", Website: "mailmomy.com"},
	ChannelProidCloudIpCc:          {Channel: ChannelProidCloudIpCc, Name: "Mailmomy (proid.cloud-ip.cc)", Website: "mailmomy.com"},
	ChannelRamjaneMoooCom:          {Channel: ChannelRamjaneMoooCom, Name: "Mailinator (ramjane.mooo.com)", Website: "mailinator.com"},
	ChannelRauxaSenyCat:            {Channel: ChannelRauxaSenyCat, Name: "Mailinator (rauxa.seny.cat)", Website: "mailinator.com"},
	ChannelReallyIstrashCom:        {Channel: ChannelReallyIstrashCom, Name: "Mailinator (really.istrash.com)", Website: "mailinator.com"},
	ChannelSbookPics:               {Channel: ChannelSbookPics, Name: "Mailmomy (sbook.pics)", Website: "mailmomy.com"},
	ChannelSpamHortukOvh:           {Channel: ChannelSpamHortukOvh, Name: "Mailinator (spam.hortuk.ovh)", Website: "mailinator.com"},
	ChannelSpWootAt:                {Channel: ChannelSpWootAt, Name: "Mailinator (sp.woot.at)", Website: "mailinator.com"},
	ChannelTestUnergieCom:          {Channel: ChannelTestUnergieCom, Name: "Mailinator (test.unergie.com)", Website: "mailinator.com"},
	ChannelTorchYiOrg:              {Channel: ChannelTorchYiOrg, Name: "Mailinator (torch.yi.org)", Website: "mailinator.com"},
	ChannelTZibetNet:               {Channel: ChannelTZibetNet, Name: "Mailinator (t.zibet.net)", Website: "mailinator.com"},
	ChannelXue32Buzz:               {Channel: ChannelXue32Buzz, Name: "Mailmomy (xue32.buzz)", Website: "mailmomy.com"},
	ChannelApihz:                   {Channel: ChannelApihz, Name: "ApiHz TempMail", Website: "apihz.cn"},
	ChannelSogetthisCom:            {Channel: ChannelSogetthisCom, Name: "Mailinator (sogetthis.com)", Website: "mailinator.com"},
	ChannelBobmailInfo:             {Channel: ChannelBobmailInfo, Name: "Mailinator (bobmail.info)", Website: "mailinator.com"},
	ChannelSuremailInfo:            {Channel: ChannelSuremailInfo, Name: "Mailinator (suremail.info)", Website: "mailinator.com"},
	ChannelBinkmailCom:             {Channel: ChannelBinkmailCom, Name: "Mailinator (binkmail.com)", Website: "mailinator.com"},
	ChannelVeryrealemailCom:        {Channel: ChannelVeryrealemailCom, Name: "Mailinator (veryrealemail.com)", Website: "mailinator.com"},
	ChannelMailmomy:                {Channel: ChannelMailmomy, Name: "Mailmomy", Website: "mailmomy.com"},
	ChannelChammyInfo:              {Channel: ChannelChammyInfo, Name: "Mailinator (chammy.info)", Website: "mailinator.com"},
	ChannelThisisnotmyrealemailCom: {Channel: ChannelThisisnotmyrealemailCom, Name: "Mailinator (thisisnotmyrealemail.com)", Website: "mailinator.com"},
	ChannelNotmailinatorCom:        {Channel: ChannelNotmailinatorCom, Name: "Mailinator (notmailinator.com)", Website: "mailinator.com"},
	ChannelSpamherepleaseCom:       {Channel: ChannelSpamherepleaseCom, Name: "Mailinator (spamhereplease.com)", Website: "mailinator.com"},
	ChannelSendspamhereCom:         {Channel: ChannelSendspamhereCom, Name: "Mailinator (sendspamhere.com)", Website: "mailinator.com"},
	ChannelSendfreeOrg:             {Channel: ChannelSendfreeOrg, Name: "Mailinator (sendfree.org)", Website: "mailinator.com"},
	ChannelJunkBeatsOrg:            {Channel: ChannelJunkBeatsOrg, Name: "Mailinator (junk.beats.org)", Website: "mailinator.com"},
	ChannelJunkIhmehlCom:           {Channel: ChannelJunkIhmehlCom, Name: "Mailinator (junk.ihmehl.com)", Website: "mailinator.com"},
	ChannelJunkNoplayOrg:           {Channel: ChannelJunkNoplayOrg, Name: "Mailinator (junk.noplay.org)", Website: "mailinator.com"},
	ChannelJunkVanillasystemCom:    {Channel: ChannelJunkVanillasystemCom, Name: "Mailinator (junk.vanillasystem.com)", Website: "mailinator.com"},
	ChannelSpamJasonpearceCom:      {Channel: ChannelSpamJasonpearceCom, Name: "Mailinator (spam.jasonpearce.com)", Website: "mailinator.com"},
	ChannelFishSkytaleNet:          {Channel: ChannelFishSkytaleNet, Name: "Mailinator (fish.skytale.net)", Website: "mailinator.com"},
	ChannelSpamMccrewCom:           {Channel: ChannelSpamMccrewCom, Name: "Mailinator (spam.mccrew.com)", Website: "mailinator.com"},
	ChannelDropmailClick:           {Channel: ChannelDropmailClick, Name: "DropMail.click", Website: "dropmail.click"},
	ChannelSpamCoroiuCom:           {Channel: ChannelSpamCoroiuCom, Name: "Mailinator (spam.coroiu.com)", Website: "mailinator.com"},
	ChannelSpamDeluserNet:          {Channel: ChannelSpamDeluserNet, Name: "Mailinator (spam.deluser.net)", Website: "mailinator.com"},
	ChannelSpamDhsfNet:             {Channel: ChannelSpamDhsfNet, Name: "Mailinator (spam.dhsf.net)", Website: "mailinator.com"},
	ChannelSpamLucatntCom:          {Channel: ChannelSpamLucatntCom, Name: "Mailinator (spam.lucatnt.com)", Website: "mailinator.com"},
	ChannelSpamLyceumLifeComRu:     {Channel: ChannelSpamLyceumLifeComRu, Name: "Mailinator (spam.lyceum-life.com.ru)", Website: "mailinator.com"},
	ChannelSpamNetpiratesNet:       {Channel: ChannelSpamNetpiratesNet, Name: "Mailinator (spam.netpirates.net)", Website: "mailinator.com"},
	ChannelSpamNoIpNet:             {Channel: ChannelSpamNoIpNet, Name: "Mailinator (spam.no-ip.net)", Website: "mailinator.com"},
	ChannelSpamOzhOrg:              {Channel: ChannelSpamOzhOrg, Name: "Mailinator (spam.ozh.org)", Website: "mailinator.com"},
	ChannelSpamPyphusOrg:           {Channel: ChannelSpamPyphusOrg, Name: "Mailinator (spam.pyphus.org)", Website: "mailinator.com"},
	ChannelSpamShepPw:              {Channel: ChannelSpamShepPw, Name: "Mailinator (spam.shep.pw)", Website: "mailinator.com"},
	ChannelSpamWtfAt:               {Channel: ChannelSpamWtfAt, Name: "Mailinator (spam.wtf.at)", Website: "mailinator.com"},
	ChannelSpamWulczerOrg:          {Channel: ChannelSpamWulczerOrg, Name: "Mailinator (spam.wulczer.org)", Website: "mailinator.com"},
	ChannelCrapKakaduaNet:          {Channel: ChannelCrapKakaduaNet, Name: "Mailinator (crap.kakadua.net)", Website: "mailinator.com"},
	ChannelSpamJanlugtNl:           {Channel: ChannelSpamJanlugtNl, Name: "Mailinator (spam.janlugt.nl)", Website: "mailinator.com"},
	ChannelMinBurningfishNet:       {Channel: ChannelMinBurningfishNet, Name: "Mailinator (min.burningfish.net)", Website: "mailinator.com"},
	ChannelSinkFblayCom:            {Channel: ChannelSinkFblayCom, Name: "Mailinator (sink.fblay.com)", Website: "mailinator.com"},
}

/*
 * ListChannels 获取所有支持的渠道列表
 * 返回所有渠道的信息数组，包含渠道名称和对应网站
 */
func ListChannels() []ChannelInfo {
	result := make([]ChannelInfo, len(allChannels))
	for i, ch := range allChannels {
		result[i] = channelInfoMap[ch]
	}
	return result
}

/*
 * GetChannelInfo 获取指定渠道的详细信息
 * 返回渠道信息和是否存在的标记
 */
func GetChannelInfo(channel Channel) (ChannelInfo, bool) {
	info, ok := channelInfoMap[channel]
	return info, ok
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

	/*
	 * 构建尝试顺序：
	 * - 指定渠道 → 优先尝试该渠道，失败后随机尝试其他渠道
	 * - 未指定 → 打乱全部渠道逐个尝试
	 */
	tryOrder := buildChannelOrder(opts.Channel)

	channelsTried := 0
	var lastErrMsg string
	for _, ch := range tryOrder {
		channelsTried++
		sdkLogger.Info("创建临时邮箱", "channel", string(ch))
		result, attempts, err := withRetryAndAttempts(func() (*EmailInfo, error) {
			return generateEmailOnce(ch, opts)
		}, opts.Retry)
		if err == nil && result != nil {
			sdkLogger.Info("邮箱创建成功", "channel", string(ch), "email", result.Email)
			reportTelemetry("generate_email", string(ch), true, attempts, channelsTried, "")
			return result, nil
		}
		errMsg := "unknown error"
		if err != nil {
			errMsg = err.Error()
			lastErrMsg = errMsg
		}
		sdkLogger.Warn("渠道不可用，尝试下一个", "channel", string(ch), "error", errMsg)
	}

	sdkLogger.Error("所有渠道均不可用，创建邮箱失败")
	reportTelemetry("generate_email", "", false, 0, channelsTried, lastErrMsg)
	return nil, nil
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
	result := []Channel{preferred}
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
	switch channel {
	case ChannelTempmail:
		duration := opts.Duration
		if duration <= 0 {
			duration = 30
		}
		return fromMailbox(prov.TempmailGenerate(duration))

	case ChannelTempmailCn:
		return fromMailbox(prov.TempmailCNGenerate(opts.Domain))
	case ChannelTaEasy:
		return fromMailbox(prov.TaEasyGenerate())

	case Channel10minuteOne:
		return fromMailbox(prov.TenminuteOneGenerate(opts.Domain))

	case ChannelLinshiyou:
		return fromMailbox(prov.LinshiyouGenerate())

	case ChannelMffac:
		return fromMailbox(prov.MffacGenerate())

	case ChannelTempmailLol:
		return fromMailbox(prov.TempmailLolGenerate(opts.Domain))

	case ChannelChatgptOrgUk:
		return fromMailbox(prov.ChatgptOrgUkGenerate())

	case ChannelTempMailIo:
		return fromMailbox(prov.TempMailIoGenerate())

	case ChannelMailCx:
		return fromMailbox(prov.MailCxGenerate(opts.Domain))

	case ChannelCatchmail:
		return fromMailbox(prov.CatchmailGenerate(opts.Domain))

	case ChannelCatchmailMailistry:
		return fromMailbox(prov.CatchmailGenerate(fixedDomain("mailistry.com"), string(ChannelCatchmailMailistry)))

	case ChannelCatchmailZeppost:
		return fromMailbox(prov.CatchmailGenerate(fixedDomain("zeppost.com"), string(ChannelCatchmailZeppost)))

	case ChannelMailforspam:
		return fromMailbox(prov.MailforspamGenerate(opts.Domain))

	case ChannelMailforspamTempmailIo:
		return fromMailbox(prov.MailforspamGenerate(fixedDomain("tempmail.io"), string(ChannelMailforspamTempmailIo)))

	case ChannelMailforspamDisposable:
		return fromMailbox(prov.MailforspamGenerate(fixedDomain("disposable.email"), string(ChannelMailforspamDisposable)))

	case ChannelTempmailc:
		return fromMailbox(prov.TempmailcGenerate())

	case ChannelMailnesia:
		return fromMailbox(prov.MailnesiaGenerate())

	case ChannelThrowawaymail:
		return fromMailbox(prov.ThrowawaymailGenerate())

	case ChannelTempmailFish:
		return fromMailbox(prov.TempmailFishGenerate())

	case ChannelNeighboursSh:
		return fromMailbox(prov.NeighboursShGenerate())

	case ChannelInboxkitten:
		return fromMailbox(prov.InboxkittenGenerate())

	case ChannelGetnada:
		return fromMailbox(prov.GetnadaGenerate(opts.Domain))

	case ChannelMail123:
		return fromMailbox(prov.Mail123Generate())

	case ChannelOneSecMail:
		return fromMailbox(prov.OneSecMailGenerate())

	case ChannelFakemail:
		return fromMailbox(prov.FakemailGenerate())

	case ChannelOpeninbox:
		return fromMailbox(prov.OpeninboxGenerate())

	case ChannelInboxes:
		return fromMailbox(prov.InboxesGenerate(opts.Domain))

	case ChannelUncorreotemporal:
		return fromMailbox(prov.UncorreotemporalGenerate())

	case ChannelAwamail:
		return fromMailbox(prov.AwamailGenerate())

	case ChannelMailTm:
		return fromMailbox(prov.MailTmGenerate())

	case ChannelDropmail:
		return fromMailbox(prov.DropmailGenerate())

	case ChannelGuerrillaMail:
		return fromMailbox(prov.GuerrillaMailGenerate())

	case ChannelGuerrillamailCom:
		return fromMailbox(prov.GuerrillamailMirrorGenerate("guerrillamail-com", "https://guerrillamail.com/ajax.php"))

	case ChannelMaildrop:
		return fromMailbox(prov.MaildropGenerate(opts.Domain))

	case ChannelSmailPw:
		return fromMailbox(prov.SmailPwGenerate())
	case ChannelVip215:
		return fromMailbox(prov.MailVip215Generate())
	case ChannelFakeLegal:
		return fromMailbox(prov.FakeLegalGenerate(opts.Domain))

	case ChannelMoakt:
		return fromMailbox(prov.MoaktGenerate(opts.Domain))
	case ChannelDrmailIn:
		return genMoaktVariant("drmail.in", ChannelDrmailIn)
	case ChannelTemlNet:
		return genMoaktVariant("teml.net", ChannelTemlNet)
	case ChannelTmpemlCom:
		return genMoaktVariant("tmpeml.com", ChannelTmpemlCom)
	case ChannelTmpboxNet:
		return genMoaktVariant("tmpbox.net", ChannelTmpboxNet)
	case ChannelMoaktCc:
		return genMoaktVariant("moakt.cc", ChannelMoaktCc)
	case ChannelDisboxNet:
		return genMoaktVariant("disbox.net", ChannelDisboxNet)
	case ChannelTmpmailOrg:
		return genMoaktVariant("tmpmail.org", ChannelTmpmailOrg)
	case ChannelTmpmailNet:
		return genMoaktVariant("tmpmail.net", ChannelTmpmailNet)
	case ChannelTmailsNet:
		return genMoaktVariant("tmails.net", ChannelTmailsNet)
	case ChannelDisboxOrg:
		return genMoaktVariant("disbox.org", ChannelDisboxOrg)
	case ChannelMoaktCo:
		return genMoaktVariant("moakt.co", ChannelMoaktCo)
	case ChannelMoaktWs:
		return genMoaktVariant("moakt.ws", ChannelMoaktWs)
	case ChannelTmailWs:
		return genMoaktVariant("tmail.ws", ChannelTmailWs)
	case ChannelBareedWs:
		return genMoaktVariant("bareed.ws", ChannelBareedWs)
	case ChannelEmail10min:
		return fromMailbox(prov.Email10minGenerate())

	case ChannelMjjCm:
		return fromMailbox(prov.MjjCmGenerate())
	case ChannelLinshiCo:
		return fromMailbox(prov.LinshiCoGenerate())

	case ChannelHarakirimail:
		return fromMailbox(prov.HarakirimailGenerate())

	case ChannelTempmailPlus:
		return fromMailbox(prov.TempmailPlusGenerate(opts.Domain))

	case ChannelTempmailLolV2:
		return fromMailbox(prov.TempmailLolV2Generate())

	case ChannelSharklasers:
		return fromMailbox(prov.GuerrillamailMirrorGenerate("sharklasers", "https://www.sharklasers.com/ajax.php"))

	case ChannelSharklasersCom:
		return fromMailbox(prov.GuerrillamailMirrorGenerate("sharklasers-com", "https://sharklasers.com/ajax.php"))

	case ChannelGrrLa:
		return fromMailbox(prov.GuerrillamailMirrorGenerate("grr-la", "https://www.grr.la/ajax.php"))

	case ChannelGrrLaCom:
		return fromMailbox(prov.GuerrillamailMirrorGenerate("grr-la-com", "https://grr.la/ajax.php"))

	case ChannelGuerrillamailInfo:
		return fromMailbox(prov.GuerrillamailMirrorGenerate("guerrillamail-info", "https://www.guerrillamail.info/ajax.php"))

	case ChannelSpam4me:
		return fromMailbox(prov.GuerrillamailMirrorGenerate("spam4me", "https://www.spam4.me/ajax.php"))

	case ChannelGuerrillamailNet:
		return fromMailbox(prov.GuerrillamailMirrorGenerate("guerrillamail-net", "https://www.guerrillamail.net/ajax.php"))

	case ChannelGuerrillamailOrg:
		return fromMailbox(prov.GuerrillamailMirrorGenerate("guerrillamail-org", "https://www.guerrillamail.org/ajax.php"))

	case ChannelGuerrillamailBlock:
		return fromMailbox(prov.GuerrillamailMirrorGenerate("guerrillamailblock", "https://www.guerrillamailblock.com/ajax.php"))

	case ChannelGuerrillamailComWww:
		return fromMailbox(prov.GuerrillamailMirrorGenerate("guerrillamail-com-www", "https://www.guerrillamail.com/ajax.php"))

	case ChannelMytempmailCc:
		return fromMailbox(prov.MytempmailCcGenerate())

	case ChannelTempMailNow:
		return fromMailbox(prov.TempMailNowGenerate())

	case ChannelMailTd:
		return fromMailbox(prov.MailTdGenerate())

	case ChannelMailholeDe:
		return fromMailbox(prov.MailholeDeGenerate())

	case ChannelTmailLink:
		return fromMailbox(prov.TmailLinkGenerate())

	case Channel24mailChacuo:
		return fromMailbox(prov.TwentyfourmailChacuoGenerate())

	case ChannelNimail:
		return fromMailbox(prov.NimailGenerate())

	case ChannelFreecustom:
		return fromMailbox(prov.FreecustomGenerate())

	case ChannelApihz:
		return fromMailbox(prov.ApihzGenerate())

	case ChannelMailmomy:
		return fromMailbox(prov.MailmomyGenerate())
	case ChannelSogetthisCom:
		return fromMailbox(prov.SogetthisComGenerate())
	case ChannelBobmailInfo:
		return fromMailbox(prov.BobmailInfoGenerate())
	case ChannelSuremailInfo:
		return fromMailbox(prov.SuremailInfoGenerate())
	case ChannelBinkmailCom:
		return fromMailbox(prov.BinkmailComGenerate())
	case ChannelVeryrealemailCom:
		return fromMailbox(prov.VeryrealemailComGenerate())
	case ChannelChammyInfo:
		return fromMailbox(prov.ChammyInfoGenerate())
	case ChannelThisisnotmyrealemailCom:
		return fromMailbox(prov.ThisisnotmyrealemailComGenerate())
	case ChannelNotmailinatorCom:
		return fromMailbox(prov.NotmailinatorComGenerate())
	case ChannelSpamherepleaseCom:
		return fromMailbox(prov.SpamherepleaseComGenerate())
	case ChannelSendspamhereCom:
		return fromMailbox(prov.SendspamhereComGenerate())
	case ChannelSendfreeOrg:
		return fromMailbox(prov.SendfreeOrgGenerate())
	case ChannelJunkBeatsOrg:
		return fromMailbox(prov.JunkBeatsOrgGenerate())
	case ChannelJunkIhmehlCom:
		return fromMailbox(prov.JunkIhmehlComGenerate())
	case ChannelJunkNoplayOrg:
		return fromMailbox(prov.JunkNoplayOrgGenerate())
	case ChannelJunkVanillasystemCom:
		return fromMailbox(prov.JunkVanillasystemComGenerate())
	case ChannelSpamJasonpearceCom:
		return fromMailbox(prov.SpamJasonpearceComGenerate())
	case ChannelFishSkytaleNet:
		return fromMailbox(prov.FishSkytaleNetGenerate())
	case ChannelSpamMccrewCom:
		return fromMailbox(prov.SpamMccrewComGenerate())
	case ChannelDropmailClick:
		return fromMailbox(prov.DropmailClickGenerate())
	case ChannelSpamCoroiuCom:
		return fromMailbox(prov.SpamCoroiuComGenerate())
	case ChannelSpamDeluserNet:
		return fromMailbox(prov.SpamDeluserNetGenerate())
	case ChannelSpamDhsfNet:
		return fromMailbox(prov.SpamDhsfNetGenerate())
	case ChannelSpamLucatntCom:
		return fromMailbox(prov.SpamLucatntComGenerate())
	case ChannelSpamLyceumLifeComRu:
		return fromMailbox(prov.SpamLyceumLifeComRuGenerate())
	case ChannelSpamNetpiratesNet:
		return fromMailbox(prov.SpamNetpiratesNetGenerate())
	case ChannelSpamNoIpNet:
		return fromMailbox(prov.SpamNoIpNetGenerate())
	case ChannelSpamOzhOrg:
		return fromMailbox(prov.SpamOzhOrgGenerate())
	case ChannelSpamPyphusOrg:
		return fromMailbox(prov.SpamPyphusOrgGenerate())
	case ChannelSpamShepPw:
		return fromMailbox(prov.SpamShepPwGenerate())
	case ChannelSpamWtfAt:
		return fromMailbox(prov.SpamWtfAtGenerate())
	case ChannelSpamWulczerOrg:
		return fromMailbox(prov.SpamWulczerOrgGenerate())
	case ChannelCrapKakaduaNet:
		return fromMailbox(prov.CrapKakaduaNetGenerate())
	case ChannelSpamJanlugtNl:
		return fromMailbox(prov.SpamJanlugtNlGenerate())
	case ChannelMinBurningfishNet:
		return fromMailbox(prov.MinBurningfishNetGenerate())
	case ChannelSinkFblayCom:
		return fromMailbox(prov.SinkFblayComGenerate())
	case ChannelEtgdevDe:
		return fromMailbox(prov.EtgdevDeGenerate())
	case ChannelMtmdevCom:
		return fromMailbox(prov.MtmdevComGenerate())
	case ChannelTestUnergieCom:
		return fromMailbox(prov.TestUnergieComGenerate())
	case ChannelBlockBdeaCc:
		return fromMailbox(prov.BlockBdeaCcGenerate())
	case ChannelTorchYiOrg:
		return fromMailbox(prov.TorchYiOrgGenerate())
	case ChannelCarlton183ChangeipNet:
		return fromMailbox(prov.Carlton183ChangeipNetGenerate())
	case ChannelMailFsmashOrg:
		return fromMailbox(prov.MailFsmashOrgGenerate())
	case ChannelEbsComAr:
		return fromMailbox(prov.EbsComArGenerate())
	case ChannelJamaTrenetEu:
		return fromMailbox(prov.JamaTrenetEuGenerate())
	case ChannelBlackholeDjurbySe:
		return fromMailbox(prov.BlackholeDjurbySeGenerate())
	case ChannelM8rDavidfuhrDe:
		return fromMailbox(prov.M8rDavidfuhrDeGenerate())
	case ChannelMiMeonBe:
		return fromMailbox(prov.MiMeonBeGenerate())
	case ChannelMNikMe:
		return fromMailbox(prov.MNikMeGenerate())
	case ChannelMailBentraskCom:
		return fromMailbox(prov.MailBentraskComGenerate())
	case ChannelTZibetNet:
		return fromMailbox(prov.TZibetNetGenerate())
	case ChannelM8rMcasalCom:
		return fromMailbox(prov.M8rMcasalComGenerate())
	case ChannelRamjaneMoooCom:
		return fromMailbox(prov.RamjaneMoooComGenerate())
	case ChannelRauxaSenyCat:
		return fromMailbox(prov.RauxaSenyCatGenerate())
	case ChannelSpWootAt:
		return fromMailbox(prov.SpWootAtGenerate())
	case ChannelFwd2mEszettEs:
		return fromMailbox(prov.Fwd2mEszettEsGenerate())
	case ChannelM887At:
		return fromMailbox(prov.M887AtGenerate())
	case ChannelN16888888Cyou:
		return fromMailbox(prov.N16888888CyouGenerate())
	case ChannelN17666688Shop:
		return fromMailbox(prov.N17666688ShopGenerate())
	case ChannelN282mailCom:
		return fromMailbox(prov.N282mailComGenerate())
	case ChannelBsdu32Buzz:
		return fromMailbox(prov.Bsdu32BuzzGenerate())
	case ChannelDoxu243Buzz:
		return fromMailbox(prov.Doxu243BuzzGenerate())
	case ChannelEasymePro:
		return fromMailbox(prov.EasymeProGenerate())
	case ChannelEvergreencoShop:
		return fromMailbox(prov.EvergreencoShopGenerate())
	case ChannelLayuemingPics:
		return fromMailbox(prov.LayuemingPicsGenerate())
	case ChannelMingyuekejiOnline:
		return fromMailbox(prov.MingyuekejiOnlineGenerate())
	case ChannelMingyuemingClick:
		return fromMailbox(prov.MingyuemingClickGenerate())
	case ChannelMingyuemingShop:
		return fromMailbox(prov.MingyuemingShopGenerate())
	case ChannelMingyukejiLol:
		return fromMailbox(prov.MingyukejiLolGenerate())
	case ChannelNuxh62Space:
		return fromMailbox(prov.Nuxh62SpaceGenerate())
	case ChannelProidCloudIpCc:
		return fromMailbox(prov.ProidCloudIpCcGenerate())
	case ChannelSbookPics:
		return fromMailbox(prov.SbookPicsGenerate())
	case ChannelXue32Buzz:
		return fromMailbox(prov.Xue32BuzzGenerate())
	case ChannelBSmellyCc:
		return fromMailbox(prov.BSmellyCcGenerate())
	case ChannelDeaSoonIt:
		return fromMailbox(prov.DeaSoonItGenerate())
	case ChannelDisposableAlSudaniCom:
		return fromMailbox(prov.DisposableAlSudaniComGenerate())
	case ChannelDisposableNogonadNl:
		return fromMailbox(prov.DisposableNogonadNlGenerate())
	case ChannelJFairuseOrg:
		return fromMailbox(prov.JFairuseOrgGenerate())
	case ChannelMnCurppaCom:
		return fromMailbox(prov.MnCurppaComGenerate())
	case ChannelMailinatorzzmoooCom:
		return fromMailbox(prov.MailinatorzzmoooComGenerate())
	case ChannelNotfond404Mn:
		return fromMailbox(prov.Notfond404MnGenerate())
	case ChannelNospamThurstonsUs:
		return fromMailbox(prov.NospamThurstonsUsGenerate())
	case ChannelNullK3vinNet:
		return fromMailbox(prov.NullK3vinNetGenerate())
	case ChannelReallyIstrashCom:
		return fromMailbox(prov.ReallyIstrashComGenerate())
	case ChannelSpamHortukOvh:
		return fromMailbox(prov.SpamHortukOvhGenerate())

	// ===== 以下为补齐的 107 个渠道 dispatch 接线（generate 侧） =====

	// getnada 家族：同后端多固定域名，GetnadaGenerate 变参回填 channel
	case ChannelOneVpnNet:
		return fromMailbox(prov.GetnadaGenerate(fixedDomain("1vpn.net"), string(ChannelOneVpnNet)))
	case ChannelAbematvCom:
		return fromMailbox(prov.GetnadaGenerate(fixedDomain("abematv.com"), string(ChannelAbematvCom)))
	case ChannelAbematvNet:
		return fromMailbox(prov.GetnadaGenerate(fixedDomain("abematv.net"), string(ChannelAbematvNet)))
	case ChannelAbematvOrg:
		return fromMailbox(prov.GetnadaGenerate(fixedDomain("abematv.org"), string(ChannelAbematvOrg)))
	case ChannelAcehCc:
		return fromMailbox(prov.GetnadaGenerate(fixedDomain("aceh.cc"), string(ChannelAcehCc)))
	case ChannelBangkabelitungNet:
		return fromMailbox(prov.GetnadaGenerate(fixedDomain("bangkabelitung.net"), string(ChannelBangkabelitungNet)))
	case ChannelCctruyenCom:
		return fromMailbox(prov.GetnadaGenerate(fixedDomain("cctruyen.com"), string(ChannelCctruyenCom)))
	case ChannelGetnadaCom:
		return fromMailbox(prov.GetnadaGenerate(fixedDomain("getnada.com"), string(ChannelGetnadaCom)))
	case ChannelGetnadaEmail:
		return fromMailbox(prov.GetnadaGenerate(fixedDomain("getnada.email"), string(ChannelGetnadaEmail)))
	case ChannelGetnadaNet:
		return fromMailbox(prov.GetnadaGenerate(fixedDomain("getnada.net"), string(ChannelGetnadaNet)))
	case ChannelJawatengahNet:
		return fromMailbox(prov.GetnadaGenerate(fixedDomain("jawatengah.net"), string(ChannelJawatengahNet)))
	case ChannelJawatimurNet:
		return fromMailbox(prov.GetnadaGenerate(fixedDomain("jawatimur.net"), string(ChannelJawatimurNet)))
	case ChannelKalimantanbaratNet:
		return fromMailbox(prov.GetnadaGenerate(fixedDomain("kalimantanbarat.net"), string(ChannelKalimantanbaratNet)))
	case ChannelKalimantanselatanNet:
		return fromMailbox(prov.GetnadaGenerate(fixedDomain("kalimantanselatan.net"), string(ChannelKalimantanselatanNet)))
	case ChannelKalimantantengahNet:
		return fromMailbox(prov.GetnadaGenerate(fixedDomain("kalimantantengah.net"), string(ChannelKalimantantengahNet)))
	case ChannelKalimantantimurNet:
		return fromMailbox(prov.GetnadaGenerate(fixedDomain("kalimantantimur.net"), string(ChannelKalimantantimurNet)))
	case ChannelKalimantanutaraNet:
		return fromMailbox(prov.GetnadaGenerate(fixedDomain("kalimantanutara.net"), string(ChannelKalimantanutaraNet)))
	case ChannelKepulauanriauNet:
		return fromMailbox(prov.GetnadaGenerate(fixedDomain("kepulauanriau.net"), string(ChannelKepulauanriauNet)))
	case ChannelLuxury345Com:
		return fromMailbox(prov.GetnadaGenerate(fixedDomain("luxury345.com"), string(ChannelLuxury345Com)))
	case ChannelMalukuutaraNet:
		return fromMailbox(prov.GetnadaGenerate(fixedDomain("malukuutara.net"), string(ChannelMalukuutaraNet)))
	case ChannelNusatenggarabaratNet:
		return fromMailbox(prov.GetnadaGenerate(fixedDomain("nusatenggarabarat.net"), string(ChannelNusatenggarabaratNet)))
	case ChannelNusatenggaratimurNet:
		return fromMailbox(prov.GetnadaGenerate(fixedDomain("nusatenggaratimur.net"), string(ChannelNusatenggaratimurNet)))
	case ChannelPapuabaratNet:
		return fromMailbox(prov.GetnadaGenerate(fixedDomain("papuabarat.net"), string(ChannelPapuabaratNet)))
	case ChannelPapuabaratdayaNet:
		return fromMailbox(prov.GetnadaGenerate(fixedDomain("papuabaratdaya.net"), string(ChannelPapuabaratdayaNet)))
	case ChannelPapuaselatanNet:
		return fromMailbox(prov.GetnadaGenerate(fixedDomain("papuaselatan.net"), string(ChannelPapuaselatanNet)))
	case ChannelPeholCom:
		return fromMailbox(prov.GetnadaGenerate(fixedDomain("pehol.com"), string(ChannelPeholCom)))
	case ChannelPtruyenCom:
		return fromMailbox(prov.GetnadaGenerate(fixedDomain("ptruyen.com"), string(ChannelPtruyenCom)))
	case ChannelPulaubaliNet:
		return fromMailbox(prov.GetnadaGenerate(fixedDomain("pulaubali.net"), string(ChannelPulaubaliNet)))
	case ChannelRiauNet:
		return fromMailbox(prov.GetnadaGenerate(fixedDomain("riau.net"), string(ChannelRiauNet)))
	case ChannelSeokeyOrg:
		return fromMailbox(prov.GetnadaGenerate(fixedDomain("seokey.org"), string(ChannelSeokeyOrg)))
	case ChannelSulawesibaratNet:
		return fromMailbox(prov.GetnadaGenerate(fixedDomain("sulawesibarat.net"), string(ChannelSulawesibaratNet)))
	case ChannelSulawesiselatanNet:
		return fromMailbox(prov.GetnadaGenerate(fixedDomain("sulawesiselatan.net"), string(ChannelSulawesiselatanNet)))
	case ChannelSulawesitengahNet:
		return fromMailbox(prov.GetnadaGenerate(fixedDomain("sulawesitengah.net"), string(ChannelSulawesitengahNet)))
	case ChannelSulawesitenggaraNet:
		return fromMailbox(prov.GetnadaGenerate(fixedDomain("sulawesitenggara.net"), string(ChannelSulawesitenggaraNet)))
	case ChannelSumaterabaratNet:
		return fromMailbox(prov.GetnadaGenerate(fixedDomain("sumaterabarat.net"), string(ChannelSumaterabaratNet)))
	case ChannelSumateraselatanNet:
		return fromMailbox(prov.GetnadaGenerate(fixedDomain("sumateraselatan.net"), string(ChannelSumateraselatanNet)))
	case ChannelSumaterautaraNet:
		return fromMailbox(prov.GetnadaGenerate(fixedDomain("sumaterautara.net"), string(ChannelSumaterautaraNet)))
	case ChannelVillatogelCom:
		return fromMailbox(prov.GetnadaGenerate(fixedDomain("villatogel.com"), string(ChannelVillatogelCom)))

	// 10minute-one 家族：TenminuteOneGenerate 无 channel 参数，需回填
	case ChannelXghffCom:
		return genTenminuteVariant("xghff.com", ChannelXghffCom)
	case ChannelOqqajCom:
		return genTenminuteVariant("oqqaj.com", ChannelOqqajCom)
	case ChannelPsovvCom:
		return genTenminuteVariant("psovv.com", ChannelPsovvCom)
	case ChannelDbwotCom:
		return genTenminuteVariant("dbwot.com", ChannelDbwotCom)
	case ChannelYgwprCom:
		return genTenminuteVariant("ygwpr.com", ChannelYgwprCom)
	case ChannelImxweCom:
		return genTenminuteVariant("imxwe.com", ChannelImxweCom)

	// mail.cx 变体（ddker.com）：MailCxGenerate 无 channel 参数，需回填
	case ChannelDdkerCom:
		info, err := fromMailbox(prov.MailCxGenerate(fixedDomain("ddker.com")))
		if err != nil {
			return nil, err
		}
		info.Channel = ChannelDdkerCom
		return info, nil

	// fake.legal 变体：FakeLegalGenerate 变参回填 channel
	case ChannelImguiDe:
		return fromMailbox(prov.FakeLegalGenerate(fixedDomain("imgui.de"), string(ChannelImguiDe)))
	case ChannelPulsewebmenuDe:
		return fromMailbox(prov.FakeLegalGenerate(fixedDomain("pulsewebmenu.de"), string(ChannelPulsewebmenuDe)))

	// tempmail.plus 变体：TempmailPlusGenerate 变参回填 channel
	case ChannelFexpostCom:
		return fromMailbox(prov.TempmailPlusGenerate(fixedDomain("fexpost.com"), string(ChannelFexpostCom)))
	case ChannelFexboxOrg:
		return fromMailbox(prov.TempmailPlusGenerate(fixedDomain("fexbox.org"), string(ChannelFexboxOrg)))
	case ChannelMailboxInUa:
		return fromMailbox(prov.TempmailPlusGenerate(fixedDomain("mailbox.in.ua"), string(ChannelMailboxInUa)))
	case ChannelRoverInfo:
		return fromMailbox(prov.TempmailPlusGenerate(fixedDomain("rover.info"), string(ChannelRoverInfo)))
	case ChannelChitthiIn:
		return fromMailbox(prov.TempmailPlusGenerate(fixedDomain("chitthi.in"), string(ChannelChitthiIn)))
	case ChannelFextempCom:
		return fromMailbox(prov.TempmailPlusGenerate(fixedDomain("fextemp.com"), string(ChannelFextempCom)))
	case ChannelAnyPink:
		return fromMailbox(prov.TempmailPlusGenerate(fixedDomain("any.pink"), string(ChannelAnyPink)))
	case ChannelMerepostCom:
		return fromMailbox(prov.TempmailPlusGenerate(fixedDomain("merepost.com"), string(ChannelMerepostCom)))

	// zhujump / moemail
	case ChannelJqkjqkXyz:
		return fromMailbox(prov.ZhujumpGenerate("jqkjqk.xyz", string(ChannelJqkjqkXyz)))
	case ChannelLyhleviCom:
		return fromMailbox(prov.MoemailGenerate("https://lyhlevi.com", "lyhlevi.com", string(ChannelLyhleviCom), 24*60*60*1000))

	// mail.tm 变体（web-library.net）：MailTmGenerate 无 channel 参数，需回填
	case ChannelWebLibraryNet:
		info, err := fromMailbox(prov.MailTmGenerate())
		if err != nil {
			return nil, err
		}
		info.Channel = ChannelWebLibraryNet
		return info, nil

	// 带用户可选域名的独立渠道
	case ChannelFmail:
		return fromMailbox(prov.FmailGenerate(opts.Domain))
	case ChannelNeighbours:
		return fromMailbox(prov.NeighboursGenerate(opts.Domain))
	case ChannelTempinbox:
		return fromMailbox(prov.TempinboxGenerate(opts.Domain))
	case ChannelTempyEmail:
		return fromMailbox(prov.TempyEmailGenerate(opts.Domain))
	case ChannelTemporam:
		return fromMailbox(prov.TemporamGenerate(opts.Domain))

	// 无参独立渠道
	case ChannelAltmails:
		return fromMailbox(prov.AltmailsGenerate())
	case ChannelAnonbox:
		return fromMailbox(prov.AnonboxGenerate())
	case ChannelAnonymmail:
		return fromMailbox(prov.AnonymmailGenerate())
	case ChannelBestTempMail:
		return fromMailbox(prov.BestTempMailGenerate())
	case ChannelByom:
		return fromMailbox(prov.ByomGenerate())
	case ChannelCleanTempMail:
		return fromMailbox(prov.CleanTempMailGenerate())
	case ChannelDevmailUk:
		return fromMailbox(prov.DevmailUkGenerate())
	case ChannelDisposablemailApp:
		return fromMailbox(prov.DisposablemailAppGenerate())
	case ChannelDisposablemailCom:
		return fromMailbox(prov.DisposablemailGenerate())
	case ChannelDuckmail:
		return fromMailbox(prov.DuckmailGenerate())
	case ChannelEmailnator:
		return fromMailbox(prov.EmailnatorGenerate())
	case ChannelEmailtempOrg:
		return fromMailbox(prov.EmailtempOrgGenerate())
	case ChannelExpressinboxhub:
		return fromMailbox(prov.ExpressinboxhubGenerate())
	case ChannelEyepaste:
		return fromMailbox(prov.EyepasteGenerate())
	case ChannelFakeEmailSite:
		return fromMailbox(prov.FakeEmailSiteGenerate())
	case ChannelHaribu:
		return fromMailbox(prov.HaribuGenerate())
	case ChannelLinshiyouxiangNet:
		return fromMailbox(prov.LinshiyouxiangNetGenerate())
	case ChannelLroid:
		return fromMailbox(prov.LroidGenerate())
	case ChannelM2u:
		return fromMailbox(prov.M2uGenerate())
	case ChannelMail10s:
		return fromMailbox(prov.Mail10sGenerate())
	case ChannelMailSunls:
		return fromMailbox(prov.MailSunlsGenerate())
	case ChannelMailcatch:
		return fromMailbox(prov.MailcatchGenerate())
	case ChannelMaildropCc:
		return fromMailbox(prov.MaildropCcGenerate())
	case ChannelMailgolem:
		return fromMailbox(prov.MailgolemGenerate())
	case ChannelMailinator:
		return fromMailbox(prov.MailinatorGenerate())
	case ChannelMailtempCc:
		return fromMailbox(prov.MailtempCcGenerate())
	case ChannelMinuteinbox:
		return fromMailbox(prov.MinuteinboxGenerate())
	case ChannelMohmal:
		return fromMailbox(prov.MohmalGenerate())
	case ChannelOckito:
		return fromMailbox(prov.OckitoGenerate())
	case ChannelRootsh:
		return fromMailbox(prov.RootshGenerate())
	case ChannelShittyEmail:
		return fromMailbox(prov.ShittyEmailGenerate())
	case ChannelSmailpro:
		return fromMailbox(prov.SmailproGenerate())
	case ChannelTempMailFyi:
		return fromMailbox(prov.TempMailFyiGenerate())
	case ChannelTempemailCo:
		return fromMailbox(prov.TempemailCoGenerate())
	case ChannelTempemailInfo:
		return fromMailbox(prov.TempemailInfoGenerate())
	case ChannelTempemailsNet:
		return fromMailbox(prov.TempemailsNetGenerate())
	case ChannelTempfastmail:
		return fromMailbox(prov.TempfastmailGenerate())
	case ChannelTempgbox:
		return fromMailbox(prov.TempgboxGenerate())
	case ChannelTempmail365:
		return fromMailbox(prov.Tempmail365Generate())
	case ChannelTempmailpro:
		return fromMailbox(prov.TempmailproGenerate())
	case ChannelTempmailten:
		return fromMailbox(prov.TempmailtenGenerate())
	case ChannelTemppMails:
		return fromMailbox(prov.TemppMailsGenerate())
	case ChannelTenminutemailNet:
		return fromMailbox(prov.TenminutemailNetGenerate())
	case ChannelWebmailtemp:
		return fromMailbox(prov.WebmailtempGenerate())

	default:
		return nil, fmt.Errorf("unknown channel: %s", channel)
	}
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
	switch channel {
	case ChannelTempmail:
		return normEmailsResult(prov.TempmailGetEmails(email))

	case ChannelTempmailCn:
		return normEmailsResult(prov.TempmailCNGetEmails(email))
	case ChannelTaEasy:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for ta-easy channel")
		}
		return normEmailsResult(prov.TaEasyGetEmails(email, token))

	case Channel10minuteOne:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for 10minute-one channel")
		}
		return normEmailsResult(prov.TenminuteOneGetEmails(email, token))

	case ChannelLinshiyou:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for linshiyou channel")
		}
		return normEmailsResult(prov.LinshiyouGetEmails(token, email))

	case ChannelMffac:
		return normEmailsResult(prov.MffacGetEmails(email, token))

	case ChannelTempmailLol:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for tempmail-lol channel")
		}
		return normEmailsResult(prov.TempmailLolGetEmails(token, email))

	case ChannelChatgptOrgUk:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for chatgpt-org-uk channel")
		}
		return normEmailsResult(prov.ChatgptOrgUkGetEmails(email, token))

	case ChannelTempMailIo:
		return normEmailsResult(prov.TempMailIoGetEmails(email))

	case ChannelMailCx:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for mail-cx channel")
		}
		return normEmailsResult(prov.MailCxGetEmails(token, email))

	case ChannelCatchmail, ChannelCatchmailMailistry, ChannelCatchmailZeppost:
		return normEmailsResult(prov.CatchmailGetEmails(email))

	case ChannelMailforspam, ChannelMailforspamTempmailIo, ChannelMailforspamDisposable:
		return normEmailsResult(prov.MailforspamGetEmails(email))

	case ChannelTempmailc:
		return normEmailsResult(prov.TempmailcGetEmails(email))

	case ChannelMailnesia:
		return normEmailsResult(prov.MailnesiaGetEmails(email))

	case ChannelThrowawaymail:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for throwawaymail channel")
		}
		return normEmailsResult(prov.ThrowawaymailGetEmails(token, email))

	case ChannelTempmailFish:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for tempmail-fish channel")
		}
		return normEmailsResult(prov.TempmailFishGetEmails(token, email))

	case ChannelNeighboursSh:
		return normEmailsResult(prov.NeighboursShGetEmails(token, email))

	case ChannelInboxkitten:
		return normEmailsResult(prov.InboxkittenGetEmails(email))

	case ChannelGetnada:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for getnada channel")
		}
		return normEmailsResult(prov.GetnadaGetEmails(token, email))

	case ChannelMail123:
		return normEmailsResult(prov.Mail123GetEmails(email))

	case ChannelOneSecMail:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for 1sec-mail channel")
		}
		return normEmailsResult(prov.OneSecMailGetEmails(token, email))

	case ChannelFakemail:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for fakemail channel")
		}
		return normEmailsResult(prov.FakemailGetEmails(token, email))

	case ChannelOpeninbox:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for openinbox channel")
		}
		return normEmailsResult(prov.OpeninboxGetEmails(token, email))

	case ChannelInboxes:
		return normEmailsResult(prov.InboxesGetEmails(email))

	case ChannelUncorreotemporal:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for uncorreotemporal channel")
		}
		return normEmailsResult(prov.UncorreotemporalGetEmails(token, email))

	case ChannelAwamail:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for awamail channel")
		}
		return normEmailsResult(prov.AwamailGetEmails(token, email))

	case ChannelMailTm:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for mail-tm channel")
		}
		return normEmailsResult(prov.MailTmGetEmails(token, email))

	case ChannelDropmail:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for dropmail channel")
		}
		return normEmailsResult(prov.DropmailGetEmails(token, email))

	case ChannelGuerrillaMail:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for guerrillamail channel")
		}
		return normEmailsResult(prov.GuerrillaMailGetEmails(token, email))

	case ChannelGuerrillamailCom:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for guerrillamail-com channel")
		}
		return normEmailsResult(prov.GuerrillamailMirrorGetEmails("https://guerrillamail.com/ajax.php", token, email))

	case ChannelMaildrop:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for maildrop channel")
		}
		return normEmailsResult(prov.MaildropGetEmails(token, email))

	case ChannelSmailPw:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for smail-pw channel")
		}
		return normEmailsResult(prov.SmailPwGetEmails(token, email))
	case ChannelVip215:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for vip-215 channel")
		}
		return normEmailsResult(prov.MailVip215GetEmails(token, email))
	case ChannelFakeLegal:
		return normEmailsResult(prov.FakeLegalGetEmails(email))

	case ChannelMoakt:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for moakt channel")
		}
		return normEmailsResult(prov.MoaktGetEmails(email, token))
	case ChannelDrmailIn, ChannelTemlNet, ChannelTmpemlCom, ChannelTmpboxNet,
		ChannelMoaktCc, ChannelDisboxNet, ChannelTmpmailOrg, ChannelTmpmailNet,
		ChannelTmailsNet, ChannelDisboxOrg, ChannelMoaktCo, ChannelMoaktWs,
		ChannelTmailWs, ChannelBareedWs:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for %s channel", channel)
		}
		return normEmailsResult(prov.MoaktGetEmails(email, token))
	case ChannelEmail10min:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for email10min channel")
		}
		return normEmailsResult(prov.Email10minGetEmails(email, token))

	case ChannelMjjCm:
		return normEmailsResult(prov.MjjCmGetEmails(email))
	case ChannelLinshiCo:
		return normEmailsResult(prov.LinshiCoGetEmails(email))

	case ChannelHarakirimail:
		return normEmailsResult(prov.HarakirimailGetEmails(email))

	case ChannelTempmailPlus:
		return normEmailsResult(prov.TempmailPlusGetEmails(email))

	case ChannelTempmailLolV2:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for tempmail-lol-v2 channel")
		}
		return normEmailsResult(prov.TempmailLolV2GetEmails(token, email))

	case ChannelSharklasers:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for sharklasers channel")
		}
		return normEmailsResult(prov.GuerrillamailMirrorGetEmails("https://www.sharklasers.com/ajax.php", token, email))

	case ChannelSharklasersCom:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for sharklasers-com channel")
		}
		return normEmailsResult(prov.GuerrillamailMirrorGetEmails("https://sharklasers.com/ajax.php", token, email))

	case ChannelGrrLa:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for grr-la channel")
		}
		return normEmailsResult(prov.GuerrillamailMirrorGetEmails("https://www.grr.la/ajax.php", token, email))

	case ChannelGrrLaCom:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for grr-la-com channel")
		}
		return normEmailsResult(prov.GuerrillamailMirrorGetEmails("https://grr.la/ajax.php", token, email))

	case ChannelGuerrillamailInfo:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for guerrillamail-info channel")
		}
		return normEmailsResult(prov.GuerrillamailMirrorGetEmails("https://www.guerrillamail.info/ajax.php", token, email))

	case ChannelSpam4me:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for spam4me channel")
		}
		return normEmailsResult(prov.GuerrillamailMirrorGetEmails("https://www.spam4.me/ajax.php", token, email))

	case ChannelGuerrillamailNet:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for guerrillamail-net channel")
		}
		return normEmailsResult(prov.GuerrillamailMirrorGetEmails("https://www.guerrillamail.net/ajax.php", token, email))

	case ChannelGuerrillamailOrg:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for guerrillamail-org channel")
		}
		return normEmailsResult(prov.GuerrillamailMirrorGetEmails("https://www.guerrillamail.org/ajax.php", token, email))

	case ChannelGuerrillamailBlock:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for guerrillamailblock channel")
		}
		return normEmailsResult(prov.GuerrillamailMirrorGetEmails("https://www.guerrillamailblock.com/ajax.php", token, email))

	case ChannelGuerrillamailComWww:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for guerrillamail-com-www channel")
		}
		return normEmailsResult(prov.GuerrillamailMirrorGetEmails("https://www.guerrillamail.com/ajax.php", token, email))

	case ChannelMytempmailCc:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for mytempmail-cc channel")
		}
		return normEmailsResult(prov.MytempmailCcGetEmails(token, email))

	case ChannelTempMailNow:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for temp-mail-now channel")
		}
		return normEmailsResult(prov.TempMailNowGetEmails(token, email))

	case ChannelMailTd:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for mail-td channel")
		}
		return normEmailsResult(prov.MailTdGetEmails(token, email))

	case ChannelMailholeDe:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for mailhole-de channel")
		}
		return normEmailsResult(prov.MailholeDeGetEmails(token, email))

	case ChannelTmailLink:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for tmail-link channel")
		}
		return normEmailsResult(prov.TmailLinkGetEmails(token, email))

	case Channel24mailChacuo:
		return normEmailsResult(prov.TwentyfourmailChacuoGetEmails(email))

	case ChannelNimail:
		return normEmailsResult(prov.NimailGetEmails(email))

	case ChannelFreecustom:
		return normEmailsResult(prov.FreecustomGetEmails(email))

	case ChannelApihz:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for apihz channel")
		}
		return normEmailsResult(prov.ApihzGetEmails(token))

	case ChannelMailmomy:
		return normEmailsResult(prov.MailmomyGetEmails(email))
	case ChannelSogetthisCom:
		return normEmailsResult(prov.SogetthisComGetEmails(email))
	case ChannelBobmailInfo:
		return normEmailsResult(prov.BobmailInfoGetEmails(email))
	case ChannelSuremailInfo:
		return normEmailsResult(prov.SuremailInfoGetEmails(email))
	case ChannelBinkmailCom:
		return normEmailsResult(prov.BinkmailComGetEmails(email))
	case ChannelVeryrealemailCom:
		return normEmailsResult(prov.VeryrealemailComGetEmails(email))
	case ChannelChammyInfo:
		return normEmailsResult(prov.ChammyInfoGetEmails(email))
	case ChannelThisisnotmyrealemailCom:
		return normEmailsResult(prov.ThisisnotmyrealemailComGetEmails(email))
	case ChannelNotmailinatorCom:
		return normEmailsResult(prov.NotmailinatorComGetEmails(email))
	case ChannelSpamherepleaseCom:
		return normEmailsResult(prov.SpamherepleaseComGetEmails(email))
	case ChannelSendspamhereCom:
		return normEmailsResult(prov.SendspamhereComGetEmails(email))
	case ChannelSendfreeOrg:
		return normEmailsResult(prov.SendfreeOrgGetEmails(email))
	case ChannelJunkBeatsOrg:
		return normEmailsResult(prov.JunkBeatsOrgGetEmails(email))
	case ChannelJunkIhmehlCom:
		return normEmailsResult(prov.JunkIhmehlComGetEmails(email))
	case ChannelJunkNoplayOrg:
		return normEmailsResult(prov.JunkNoplayOrgGetEmails(email))
	case ChannelJunkVanillasystemCom:
		return normEmailsResult(prov.JunkVanillasystemComGetEmails(email))
	case ChannelSpamJasonpearceCom:
		return normEmailsResult(prov.SpamJasonpearceComGetEmails(email))
	case ChannelFishSkytaleNet:
		return normEmailsResult(prov.FishSkytaleNetGetEmails(email))
	case ChannelSpamMccrewCom:
		return normEmailsResult(prov.SpamMccrewComGetEmails(email))
	case ChannelDropmailClick:
		return normEmailsResult(prov.DropmailClickGetEmails(email))
	case ChannelSpamCoroiuCom:
		return normEmailsResult(prov.SpamCoroiuComGetEmails(email))
	case ChannelSpamDeluserNet:
		return normEmailsResult(prov.SpamDeluserNetGetEmails(email))
	case ChannelSpamDhsfNet:
		return normEmailsResult(prov.SpamDhsfNetGetEmails(email))
	case ChannelSpamLucatntCom:
		return normEmailsResult(prov.SpamLucatntComGetEmails(email))
	case ChannelSpamLyceumLifeComRu:
		return normEmailsResult(prov.SpamLyceumLifeComRuGetEmails(email))
	case ChannelSpamNetpiratesNet:
		return normEmailsResult(prov.SpamNetpiratesNetGetEmails(email))
	case ChannelSpamNoIpNet:
		return normEmailsResult(prov.SpamNoIpNetGetEmails(email))
	case ChannelSpamOzhOrg:
		return normEmailsResult(prov.SpamOzhOrgGetEmails(email))
	case ChannelSpamPyphusOrg:
		return normEmailsResult(prov.SpamPyphusOrgGetEmails(email))
	case ChannelSpamShepPw:
		return normEmailsResult(prov.SpamShepPwGetEmails(email))
	case ChannelSpamWtfAt:
		return normEmailsResult(prov.SpamWtfAtGetEmails(email))
	case ChannelSpamWulczerOrg:
		return normEmailsResult(prov.SpamWulczerOrgGetEmails(email))
	case ChannelCrapKakaduaNet:
		return normEmailsResult(prov.CrapKakaduaNetGetEmails(email))
	case ChannelSpamJanlugtNl:
		return normEmailsResult(prov.SpamJanlugtNlGetEmails(email))
	case ChannelMinBurningfishNet:
		return normEmailsResult(prov.MinBurningfishNetGetEmails(email))
	case ChannelSinkFblayCom:
		return normEmailsResult(prov.SinkFblayComGetEmails(email))
	case ChannelEtgdevDe:
		return normEmailsResult(prov.EtgdevDeGetEmails(email))
	case ChannelMtmdevCom:
		return normEmailsResult(prov.MtmdevComGetEmails(email))
	case ChannelTestUnergieCom:
		return normEmailsResult(prov.TestUnergieComGetEmails(email))
	case ChannelBlockBdeaCc:
		return normEmailsResult(prov.BlockBdeaCcGetEmails(email))
	case ChannelTorchYiOrg:
		return normEmailsResult(prov.TorchYiOrgGetEmails(email))
	case ChannelCarlton183ChangeipNet:
		return normEmailsResult(prov.Carlton183ChangeipNetGetEmails(email))
	case ChannelMailFsmashOrg:
		return normEmailsResult(prov.MailFsmashOrgGetEmails(email))
	case ChannelEbsComAr:
		return normEmailsResult(prov.EbsComArGetEmails(email))
	case ChannelJamaTrenetEu:
		return normEmailsResult(prov.JamaTrenetEuGetEmails(email))
	case ChannelBlackholeDjurbySe:
		return normEmailsResult(prov.BlackholeDjurbySeGetEmails(email))
	case ChannelM8rDavidfuhrDe:
		return normEmailsResult(prov.M8rDavidfuhrDeGetEmails(email))
	case ChannelMiMeonBe:
		return normEmailsResult(prov.MiMeonBeGetEmails(email))
	case ChannelMNikMe:
		return normEmailsResult(prov.MNikMeGetEmails(email))
	case ChannelMailBentraskCom:
		return normEmailsResult(prov.MailBentraskComGetEmails(email))
	case ChannelTZibetNet:
		return normEmailsResult(prov.TZibetNetGetEmails(email))
	case ChannelM8rMcasalCom:
		return normEmailsResult(prov.M8rMcasalComGetEmails(email))
	case ChannelRamjaneMoooCom:
		return normEmailsResult(prov.RamjaneMoooComGetEmails(email))
	case ChannelRauxaSenyCat:
		return normEmailsResult(prov.RauxaSenyCatGetEmails(email))
	case ChannelSpWootAt:
		return normEmailsResult(prov.SpWootAtGetEmails(email))
	case ChannelFwd2mEszettEs:
		return normEmailsResult(prov.Fwd2mEszettEsGetEmails(email))
	case ChannelM887At:
		return normEmailsResult(prov.M887AtGetEmails(email))
	case ChannelN16888888Cyou:
		return normEmailsResult(prov.N16888888CyouGetEmails(email))
	case ChannelN17666688Shop:
		return normEmailsResult(prov.N17666688ShopGetEmails(email))
	case ChannelN282mailCom:
		return normEmailsResult(prov.N282mailComGetEmails(email))
	case ChannelBsdu32Buzz:
		return normEmailsResult(prov.Bsdu32BuzzGetEmails(email))
	case ChannelDoxu243Buzz:
		return normEmailsResult(prov.Doxu243BuzzGetEmails(email))
	case ChannelEasymePro:
		return normEmailsResult(prov.EasymeProGetEmails(email))
	case ChannelEvergreencoShop:
		return normEmailsResult(prov.EvergreencoShopGetEmails(email))
	case ChannelLayuemingPics:
		return normEmailsResult(prov.LayuemingPicsGetEmails(email))
	case ChannelMingyuekejiOnline:
		return normEmailsResult(prov.MingyuekejiOnlineGetEmails(email))
	case ChannelMingyuemingClick:
		return normEmailsResult(prov.MingyuemingClickGetEmails(email))
	case ChannelMingyuemingShop:
		return normEmailsResult(prov.MingyuemingShopGetEmails(email))
	case ChannelMingyukejiLol:
		return normEmailsResult(prov.MingyukejiLolGetEmails(email))
	case ChannelNuxh62Space:
		return normEmailsResult(prov.Nuxh62SpaceGetEmails(email))
	case ChannelProidCloudIpCc:
		return normEmailsResult(prov.ProidCloudIpCcGetEmails(email))
	case ChannelSbookPics:
		return normEmailsResult(prov.SbookPicsGetEmails(email))
	case ChannelXue32Buzz:
		return normEmailsResult(prov.Xue32BuzzGetEmails(email))
	case ChannelBSmellyCc:
		return normEmailsResult(prov.BSmellyCcGetEmails(email))
	case ChannelDeaSoonIt:
		return normEmailsResult(prov.DeaSoonItGetEmails(email))
	case ChannelDisposableAlSudaniCom:
		return normEmailsResult(prov.DisposableAlSudaniComGetEmails(email))
	case ChannelDisposableNogonadNl:
		return normEmailsResult(prov.DisposableNogonadNlGetEmails(email))
	case ChannelJFairuseOrg:
		return normEmailsResult(prov.JFairuseOrgGetEmails(email))
	case ChannelMnCurppaCom:
		return normEmailsResult(prov.MnCurppaComGetEmails(email))
	case ChannelMailinatorzzmoooCom:
		return normEmailsResult(prov.MailinatorzzmoooComGetEmails(email))
	case ChannelNotfond404Mn:
		return normEmailsResult(prov.Notfond404MnGetEmails(email))
	case ChannelNospamThurstonsUs:
		return normEmailsResult(prov.NospamThurstonsUsGetEmails(email))
	case ChannelNullK3vinNet:
		return normEmailsResult(prov.NullK3vinNetGetEmails(email))
	case ChannelReallyIstrashCom:
		return normEmailsResult(prov.ReallyIstrashComGetEmails(email))
	case ChannelSpamHortukOvh:
		return normEmailsResult(prov.SpamHortukOvhGetEmails(email))

	// ===== 以下为补齐的 107 个渠道 dispatch 接线（getEmails 侧） =====

	// getnada 家族：GetnadaGetEmails(token, email) 需 token
	case ChannelOneVpnNet, ChannelAbematvCom, ChannelAbematvNet, ChannelAbematvOrg,
		ChannelAcehCc, ChannelBangkabelitungNet, ChannelCctruyenCom, ChannelGetnadaCom,
		ChannelGetnadaEmail, ChannelGetnadaNet, ChannelJawatengahNet, ChannelJawatimurNet,
		ChannelKalimantanbaratNet, ChannelKalimantanselatanNet, ChannelKalimantantengahNet,
		ChannelKalimantantimurNet, ChannelKalimantanutaraNet, ChannelKepulauanriauNet,
		ChannelLuxury345Com, ChannelMalukuutaraNet, ChannelNusatenggarabaratNet,
		ChannelNusatenggaratimurNet, ChannelPapuabaratNet, ChannelPapuabaratdayaNet,
		ChannelPapuaselatanNet, ChannelPeholCom, ChannelPtruyenCom, ChannelPulaubaliNet,
		ChannelRiauNet, ChannelSeokeyOrg, ChannelSulawesibaratNet, ChannelSulawesiselatanNet,
		ChannelSulawesitengahNet, ChannelSulawesitenggaraNet, ChannelSumaterabaratNet,
		ChannelSumateraselatanNet, ChannelSumaterautaraNet, ChannelVillatogelCom:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for %s channel", channel)
		}
		return normEmailsResult(prov.GetnadaGetEmails(token, email))

	// 10minute-one 家族：TenminuteOneGetEmails(email, token) 需 token
	case ChannelXghffCom, ChannelOqqajCom, ChannelPsovvCom, ChannelDbwotCom,
		ChannelYgwprCom, ChannelImxweCom:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for %s channel", channel)
		}
		return normEmailsResult(prov.TenminuteOneGetEmails(email, token))

	// mail.cx 变体（ddker.com）：MailCxGetEmails(token, email) 需 token
	case ChannelDdkerCom:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for ddker-com channel")
		}
		return normEmailsResult(prov.MailCxGetEmails(token, email))

	// fake.legal 变体：FakeLegalGetEmails(email) 无 token
	case ChannelImguiDe, ChannelPulsewebmenuDe:
		return normEmailsResult(prov.FakeLegalGetEmails(email))

	// tempmail.plus 变体：TempmailPlusGetEmails(email) 无 token
	case ChannelFexpostCom, ChannelFexboxOrg, ChannelMailboxInUa, ChannelRoverInfo,
		ChannelChitthiIn, ChannelFextempCom, ChannelAnyPink, ChannelMerepostCom:
		return normEmailsResult(prov.TempmailPlusGetEmails(email))

	// zhujump / moemail：ZhujumpGetEmails(email, token) 需 token
	case ChannelJqkjqkXyz, ChannelLyhleviCom:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for %s channel", channel)
		}
		return normEmailsResult(prov.ZhujumpGetEmails(email, token))

	// mail.tm 变体（web-library.net）：MailTmGetEmails(token, email) 需 token
	case ChannelWebLibraryNet:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for web-library-net channel")
		}
		return normEmailsResult(prov.MailTmGetEmails(token, email))

	// 需要 token 的独立渠道
	case ChannelAltmails:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for altmails channel")
		}
		return normEmailsResult(prov.AltmailsGetEmails(email, token))
	case ChannelAnonbox:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for anonbox channel")
		}
		return normEmailsResult(prov.AnonboxGetEmails(token, email))
	case ChannelBestTempMail:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for best-temp-mail channel")
		}
		return normEmailsResult(prov.BestTempMailGetEmails(email, token))
	case ChannelDisposablemailApp:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for disposablemail-app channel")
		}
		return normEmailsResult(prov.DisposablemailAppGetEmails(email, token))
	case ChannelDisposablemailCom:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for disposablemail-com channel")
		}
		return normEmailsResult(prov.DisposablemailGetEmails(email, token))
	case ChannelDuckmail:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for duckmail channel")
		}
		return normEmailsResult(prov.DuckmailGetEmails(token, email))
	case ChannelEmailnator:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for emailnator channel")
		}
		return normEmailsResult(prov.EmailnatorGetEmails(token, email))
	case ChannelEmailtempOrg:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for emailtemp-org channel")
		}
		return normEmailsResult(prov.EmailtempOrgGetEmails(email, token))
	case ChannelExpressinboxhub:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for expressinboxhub channel")
		}
		return normEmailsResult(prov.ExpressinboxhubGetEmails(email, token))
	case ChannelHaribu:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for haribu channel")
		}
		return normEmailsResult(prov.HaribuGetEmails(email, token))
	case ChannelLinshiyouxiangNet:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for linshiyouxiang-net channel")
		}
		return normEmailsResult(prov.LinshiyouxiangNetGetEmails(email, token))
	case ChannelLroid:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for lroid channel")
		}
		return normEmailsResult(prov.LroidGetEmails(token, email))
	case ChannelM2u:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for m2u channel")
		}
		return normEmailsResult(prov.M2uGetEmails(token, email))
	case ChannelMailgolem:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for mailgolem channel")
		}
		return normEmailsResult(prov.MailgolemGetEmails(email, token))
	case ChannelMailtempCc:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for mailtemp-cc channel")
		}
		return normEmailsResult(prov.MailtempCcGetEmails(email, token))
	case ChannelMinuteinbox:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for minuteinbox channel")
		}
		return normEmailsResult(prov.MinuteinboxGetEmails(email, token))
	case ChannelMohmal:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for mohmal channel")
		}
		return normEmailsResult(prov.MohmalGetEmails(email, token))
	case ChannelOckito:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for ockito channel")
		}
		return normEmailsResult(prov.OckitoGetEmails(token, email))
	case ChannelRootsh:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for rootsh channel")
		}
		return normEmailsResult(prov.RootshGetEmails(email, token))
	case ChannelShittyEmail:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for shitty-email channel")
		}
		return normEmailsResult(prov.ShittyEmailGetEmails(token, email))
	case ChannelTempMailFyi:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for tempmail-fyi channel")
		}
		return normEmailsResult(prov.TempMailFyiGetEmails(email, token))
	case ChannelTempemailCo:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for tempemail-co channel")
		}
		return normEmailsResult(prov.TempemailCoGetEmails(email, token))
	case ChannelTempemailInfo:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for tempemail-info channel")
		}
		return normEmailsResult(prov.TempemailInfoGetEmails(email, token))
	case ChannelTempemailsNet:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for tempemails-net channel")
		}
		return normEmailsResult(prov.TempemailsNetGetEmails(email, token))
	case ChannelTempfastmail:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for tempfastmail channel")
		}
		return normEmailsResult(prov.TempfastmailGetEmails(token, email))
	case ChannelTempgbox:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for tempgbox channel")
		}
		return normEmailsResult(prov.TempgboxGetEmails(token, email))
	case ChannelTempmailpro:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for tempmailpro channel")
		}
		return normEmailsResult(prov.TempmailproGetEmails(token, email))
	case ChannelTempmailten:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for tempmailten channel")
		}
		return normEmailsResult(prov.TempmailtenGetEmails(email, token))
	case ChannelTemppMails:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for tempp-mails channel")
		}
		return normEmailsResult(prov.TemppMailsGetEmails(email, token))
	case ChannelTenminutemailNet:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for 10minutemail-net channel")
		}
		return normEmailsResult(prov.TenminutemailNetGetEmails(email, token))
	case ChannelWebmailtemp:
		if token == "" {
			return nil, fmt.Errorf("internal error: token missing for webmailtemp channel")
		}
		return normEmailsResult(prov.WebmailtempGetEmails(token, email))

	// 无需 token 的独立渠道
	case ChannelAnonymmail:
		return normEmailsResult(prov.AnonymmailGetEmails(email))
	case ChannelByom:
		return normEmailsResult(prov.ByomGetEmails(email))
	case ChannelCleanTempMail:
		return normEmailsResult(prov.CleanTempMailGetEmails(email))
	case ChannelDevmailUk:
		return normEmailsResult(prov.DevmailUkGetEmails(email))
	case ChannelEyepaste:
		return normEmailsResult(prov.EyepasteGetEmails(email))
	case ChannelFakeEmailSite:
		return normEmailsResult(prov.FakeEmailSiteGetEmails(email))
	case ChannelFmail:
		return normEmailsResult(prov.FmailGetEmails(email))
	case ChannelMail10s:
		return normEmailsResult(prov.Mail10sGetEmails(email))
	case ChannelMailSunls:
		return normEmailsResult(prov.MailSunlsGetEmails(email))
	case ChannelMailcatch:
		return normEmailsResult(prov.MailcatchGetEmails(email))
	case ChannelMaildropCc:
		return normEmailsResult(prov.MaildropCcGetEmails(email, token))
	case ChannelMailinator:
		return normEmailsResult(prov.MailinatorGetEmails(email))
	case ChannelNeighbours:
		return normEmailsResult(prov.NeighboursGetEmails(email))
	case ChannelSmailpro:
		return normEmailsResult(prov.SmailproGetEmails(email, token))
	case ChannelTempinbox:
		return normEmailsResult(prov.TempinboxGetEmails(email))
	case ChannelTempmail365:
		return normEmailsResult(prov.Tempmail365GetEmails(email))
	case ChannelTempyEmail:
		return normEmailsResult(prov.TempyEmailGetEmails(email))
	case ChannelTemporam:
		return normEmailsResult(prov.TemporamGetEmails(token, email))

	default:
		return nil, fmt.Errorf("unsupported channel: %s", channel)
	}
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
