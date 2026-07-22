/*
 * 渠道注册文件（自动组织）：每个渠道一处 registerChannel 调用，
 * 注册顺序即为 listChannels() 的枚举顺序（硬约束，五端一致，与 .baseline_channels.txt 对齐）。
 * 闭包内 generate / getEmails 的确切调用与原 dispatch switch 逐一对应，语义保持不变。
 */

import * as tempmail from "./providers/tempmail";
import * as tempmailCn from "./providers/tempmail-cn";
import * as taEasy from "./providers/ta-easy";
import * as linshiyou from "./providers/linshiyou";
import * as mffac from "./providers/mffac";
import * as tempmailLol from "./providers/tempmail-lol";
import * as chatgptOrgUk from "./providers/chatgpt-org-uk";
import * as tempMailIo from "./providers/temp-mail-io";
import * as mailCx from "./providers/mail-cx";
import * as catchmail from "./providers/catchmail";
import * as mailforspam from "./providers/mailforspam";
import * as tempmailc from "./providers/tempmailc";
import * as mailnesia from "./providers/mailnesia";
import * as throwawaymail from "./providers/throwawaymail";
import * as tempmailFish from "./providers/tempmail-fish";
import * as neighboursSh from "./providers/neighbours-sh";
import * as shittyEmail from "./providers/shitty-email";
import * as tempmailpro from "./providers/tempmailpro";
import * as m2u from "./providers/m2u";
import * as tempyEmail from "./providers/tempy-email";
import * as devmailUk from "./providers/devmail-uk";
import * as cleantempmail from "./providers/cleantempmail";
import * as inboxkitten from "./providers/inboxkitten";
import * as getnada from "./providers/getnada";
import * as mail123 from "./providers/mail123";
import * as mail10s from "./providers/mail10s";
import * as webmailtemp from "./providers/webmailtemp";
import * as tempfastmail from "./providers/tempfastmail";
import * as oneSecMail from "./providers/one-sec-mail";
import * as fakemail from "./providers/fakemail";
import * as openinbox from "./providers/openinbox";
import * as inboxes from "./providers/inboxes";
import * as uncorreotemporal from "./providers/uncorreotemporal";
import * as awamail from "./providers/awamail";
import * as mailTm from "./providers/mail-tm";
import * as dropmail from "./providers/dropmail";
import * as guerrillamail from "./providers/guerrillamail";
import * as maildropProvider from "./providers/maildrop";
import * as smailPw from "./providers/smail-pw";
import * as vip215 from "./providers/vip-215";
import * as fakeLegal from "./providers/fake-legal";
import * as moakt from "./providers/moakt";
import * as tenminuteOne from "./providers/10minute-one";
import * as email10min from "./providers/email10min";
import * as mjjCm from "./providers/mjj-cm";
import * as linshiCo from "./providers/linshi-co";
import * as harakirimail from "./providers/harakirimail";
import * as zhujump from "./providers/zhujump";
import * as tempmailPlus from "./providers/tempmail-plus";
import * as tempmailLolV2 from "./providers/tempmail-lol-v2";
import * as tempgbox from "./providers/tempgbox";
import * as emailnator from "./providers/emailnator";
import * as tempgmailer from "./providers/tempgmailer";
import * as temporam from "./providers/temporam";
import * as neighbours from "./providers/neighbours";
import * as fmail from "./providers/fmail";
import * as ockito from "./providers/ockito";
import * as anonbox from "./providers/anonbox";
import * as duckmail from "./providers/duckmail";
import * as mailinator from "./providers/mailinator";
import * as sogetthisCom from "./providers/sogetthis-com";
import * as bobmailInfo from "./providers/bobmail-info";
import * as suremailInfo from "./providers/suremail-info";
import * as binkmailCom from "./providers/binkmail-com";
import * as veryrealemailCom from "./providers/veryrealemail-com";
import * as mailmomy from "./providers/mailmomy";
import * as chammyInfo from "./providers/chammy-info";
import * as thisisnotmyrealemailCom from "./providers/thisisnotmyrealemail-com";
import * as notmailinatorCom from "./providers/notmailinator-com";
import * as spamherepleaseCom from "./providers/spamhereplease-com";
import * as sendspamhereCom from "./providers/sendspamhere-com";
import * as sendfreeOrg from "./providers/sendfree-org";
import * as junkBeatsOrg from "./providers/junk-beats-org";
import * as junkIhmehlCom from "./providers/junk-ihmehl-com";
import * as junkNoplayOrg from "./providers/junk-noplay-org";
import * as junkVanillasystemCom from "./providers/junk-vanillasystem-com";
import * as spamJasonpearceCom from "./providers/spam-jasonpearce-com";
import * as fishSkytaleNet from "./providers/fish-skytale-net";
import * as spamMccrewCom from "./providers/spam-mccrew-com";
import * as dropmailClick from "./providers/dropmail-click";
import * as spamCoroiuCom from "./providers/spam-coroiu-com";
import * as spamDeluserNet from "./providers/spam-deluser-net";
import * as spamDhsfNet from "./providers/spam-dhsf-net";
import * as spamLucatntCom from "./providers/spam-lucatnt-com";
import * as spamLyceumLifeComRu from "./providers/spam-lyceum-life-com-ru";
import * as spamNetpiratesNet from "./providers/spam-netpirates-net";
import * as spamNoIpNet from "./providers/spam-no-ip-net";
import * as spamOzhOrg from "./providers/spam-ozh-org";
import * as spamPyphusOrg from "./providers/spam-pyphus-org";
import * as spamShepPw from "./providers/spam-shep-pw";
import * as spamWtfAt from "./providers/spam-wtf-at";
import * as spamWulczerOrg from "./providers/spam-wulczer-org";
import * as crapKakaduaNet from "./providers/crap-kakadua-net";
import * as spamJanlugtNl from "./providers/spam-janlugt-nl";
import * as spamHortukOvh from "./providers/spam-hortuk-ovh";
import * as reallyIstrashCom from "./providers/really-istrash-com";
import * as nullK3vinNet from "./providers/null-k3vin-net";
import * as nospamThurstonsUs from "./providers/nospam-thurstons-us";
import * as minBurningfishNet from "./providers/min-burningfish-net";
import * as testUnergieCom from "./providers/test-unergie-com";
import * as blockBdeaCc from "./providers/block-bdea-cc";
import * as torchYiOrg from "./providers/torch-yi-org";
import * as carlton183ChangeipNet from "./providers/carlton183-changeip-net";
import * as mailFsmashOrg from "./providers/mail-fsmash-org";
import * as ebsComAr from "./providers/ebs-com-ar";
import * as jamaTrenetEu from "./providers/jama-trenet-eu";
import * as blackholeDjurbySe from "./providers/blackhole-djurby-se";
import * as m8rDavidfuhrDe from "./providers/m8r-davidfuhr-de";
import * as miMeonBe from "./providers/mi-meon-be";
import * as mNikMe from "./providers/m-nik-me";
import * as mailBentraskCom from "./providers/mail-bentrask-com";
import * as tZibetNet from "./providers/t-zibet-net";
import * as m8rMcasalCom from "./providers/m8r-mcasal-com";
import * as ramjaneMoooCom from "./providers/ramjane-mooo-com";
import * as rauxaSenyCat from "./providers/rauxa-seny-cat";
import * as spWootAt from "./providers/sp-woot-at";
import * as fwd2mEszettEs from "./providers/fwd2m-eszett-es";
import * as m887At from "./providers/m-887-at";
import * as n16888888Cyou from "./providers/16888888-cyou";
import * as n17666688Shop from "./providers/17666688-shop";
import * as n282mailCom from "./providers/282mail-com";
import * as bsdu32Buzz from "./providers/bsdu32-buzz";
import * as doxu243Buzz from "./providers/doxu243-buzz";
import * as easymePro from "./providers/easyme-pro";
import * as evergreencoShop from "./providers/evergreenco-shop";
import * as layuemingPics from "./providers/layueming-pics";
import * as mingyuekejiOnline from "./providers/mingyuekeji-online";
import * as mingyuemingClick from "./providers/mingyueming-click";
import * as mingyuemingShop from "./providers/mingyueming-shop";
import * as mingyukejiLol from "./providers/mingyukeji-lol";
import * as nuxh62Space from "./providers/nuxh62-space";
import * as proidCloudIpCc from "./providers/proid-cloud-ip-cc";
import * as sbookPics from "./providers/sbook-pics";
import * as xue32Buzz from "./providers/xue32-buzz";
import * as bSmellyCc from "./providers/b-smelly-cc";
import * as deaSoonIt from "./providers/dea-soon-it";
import * as disposableAlSudaniCom from "./providers/disposable-al-sudani-com";
import * as disposableNogonadNl from "./providers/disposable-nogonad-nl";
import * as jFairuseOrg from "./providers/j-fairuse-org";
import * as mnCurppaCom from "./providers/mn-curppa-com";
import * as mailinatorzzMoooCom from "./providers/mailinatorzz-mooo-com";
import * as notfond404Mn from "./providers/notfond-404-mn";
import * as mtmdevCom from "./providers/mtmdev-com";
import * as etgdevDe from "./providers/etgdev-de";
import * as sinkFblayCom from "./providers/sink-fblay-com";
import * as tempMailOrg from "./providers/temp-mail-org";

import * as tempmail365 from "./providers/tempmail365";
import * as tempinbox from "./providers/tempinbox";
import * as byom from "./providers/byom";
import * as anonymmail from "./providers/anonymmail";
import * as eyepaste from "./providers/eyepaste";
import * as mailSunls from "./providers/mail-sunls";
import * as expressinboxhub from "./providers/expressinboxhub";
import * as lroid from "./providers/lroid";
import * as haribu from "./providers/haribu";
import * as rootsh from "./providers/rootsh";
import * as fakeEmailSite from "./providers/fake-email-site";
import * as mohmal from "./providers/mohmal";
import * as mailgolem from "./providers/mailgolem";
import * as bestTempMail from "./providers/best-temp-mail";
import * as disposablemailApp from "./providers/disposablemail-app";
import * as mailtempCc from "./providers/mailtemp-cc";
import * as minuteinbox from "./providers/minuteinbox";
import * as mailcatch from "./providers/mailcatch";
import * as tempemailCo from "./providers/tempemail-co";
import * as tempemailsNet from "./providers/tempemails-net";
import * as altmails from "./providers/altmails";
import * as tempemailInfo from "./providers/tempemail-info";
import * as smailpro from "./providers/smailpro";
import * as tempmailten from "./providers/tempmailten";
import * as maildropCc from "./providers/maildrop-cc";
import * as tenminutemailNet from "./providers/10minutemail-net";
import * as linshiyouxiangNet from "./providers/linshiyouxiang-net";
import * as tempmailFyi from "./providers/tempmail-fyi";
import * as disposablemailCom from "./providers/disposablemail-com";
import * as temppMails from "./providers/tempp-mails";
import * as emailtempOrg from "./providers/emailtemp-org";
import * as mytempmailCc from "./providers/mytempmail-cc";
import * as tempMailNow from "./providers/temp-mail-now";
import * as mailTd from "./providers/mail-td";
import * as mailholeDe from "./providers/mailhole-de";
import * as tmailLink from "./providers/tmail-link";
import * as twentyfourmailChacuo from "./providers/24mail-chacuo";
import * as nimail from "./providers/nimail";
import * as freecustom from "./providers/freecustom";
import * as apihz from "./providers/apihz";
import * as xkxMe from "./providers/xkx-me";
import * as goneboxEmail from "./providers/gonebox-email";
import * as mailcatAi from "./providers/mailcat-ai";
import * as tempgoEmail from "./providers/tempgo-email";
import * as restmailNet from "./providers/restmail-net";
import * as dropmailMe from "./providers/dropmail-me";
import * as tenMinuteMailNet from "./providers/ten-minute-mail-net";
import {
  sharklasers,
  sharklasersCom,
  grrla,
  grrlaCom,
  guerrillainfoMirror,
  spam4meMirror,
  guerrillamailNetMirror,
  guerrillamailOrgMirror,
  guerrillamailBlockMirror,
  guerrillamailComMirror,
  guerrillamailComWwwMirror,
} from "./providers/guerrillamail-mirrors";
import {
  Channel,
  Email,
  InternalEmailInfo,
  GenerateEmailOptions,
} from "./types";
import { registerChannel } from "./registry";

  registerChannel({
    channel: "tempmail",
    name: "TempMail",
    website: "tempmail.ing",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return tempmail.generateEmail(options.duration || 30);
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return tempmail.getEmails(email);
    },
  });

  registerChannel({
    channel: "tempmail-cn",
    name: "TempMail CN",
    website: "tempmail.cn",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return tempmailCn.generateEmail(options.domain ?? null);
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return tempmailCn.getEmails(email);
    },
  });

  registerChannel({
    channel: "ta-easy",
    name: "TA Easy",
    website: "ta-easy.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return taEasy.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for ta-easy");
        return taEasy.getEmails(email, token);
    },
  });

  registerChannel({
    channel: "10minute-one",
    name: "10 Minute Email",
    website: "10minutemail.one",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return tenminuteOne.generateEmail(options.domain ?? null);
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token)
          throw new Error("internal error: token missing for 10minute-one");
        return tenminuteOne.getEmails(email, token);
    },
  });

  registerChannel({
    channel: "xghff-com",
    name: "xghff.com",
    website: "10minutemail.one",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return tenminuteOne
          .generateEmail("xghff.com")
          .then((info) => ({ ...info, channel: "xghff-com" }));
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token)
          throw new Error("internal error: token missing for 10minute-one");
        return tenminuteOne.getEmails(email, token);
    },
  });

  registerChannel({
    channel: "oqqaj-com",
    name: "oqqaj.com",
    website: "10minutemail.one",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return tenminuteOne
          .generateEmail("oqqaj.com")
          .then((info) => ({ ...info, channel: "oqqaj-com" }));
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token)
          throw new Error("internal error: token missing for 10minute-one");
        return tenminuteOne.getEmails(email, token);
    },
  });

  registerChannel({
    channel: "psovv-com",
    name: "psovv.com",
    website: "10minutemail.one",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return tenminuteOne
          .generateEmail("psovv.com")
          .then((info) => ({ ...info, channel: "psovv-com" }));
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token)
          throw new Error("internal error: token missing for 10minute-one");
        return tenminuteOne.getEmails(email, token);
    },
  });

  registerChannel({
    channel: "dbwot-com",
    name: "dbwot.com",
    website: "10minutemail.one",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return tenminuteOne
          .generateEmail("dbwot.com")
          .then((info) => ({ ...info, channel: "dbwot-com" }));
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token)
          throw new Error("internal error: token missing for 10minute-one");
        return tenminuteOne.getEmails(email, token);
    },
  });

  registerChannel({
    channel: "ygwpr-com",
    name: "ygwpr.com",
    website: "10minutemail.one",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return tenminuteOne
          .generateEmail("ygwpr.com")
          .then((info) => ({ ...info, channel: "ygwpr-com" }));
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token)
          throw new Error("internal error: token missing for 10minute-one");
        return tenminuteOne.getEmails(email, token);
    },
  });

  registerChannel({
    channel: "imxwe-com",
    name: "imxwe.com",
    website: "10minutemail.one",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return tenminuteOne
          .generateEmail("imxwe.com")
          .then((info) => ({ ...info, channel: "imxwe-com" }));
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token)
          throw new Error("internal error: token missing for 10minute-one");
        return tenminuteOne.getEmails(email, token);
    },
  });

  registerChannel({
    channel: "linshiyou",
    name: "临时邮",
    website: "linshiyou.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return linshiyou.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token)
          throw new Error("internal error: token missing for linshiyou");
        return linshiyou.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "mffac",
    name: "MFFAC",
    website: "mffac.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return mffac.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return mffac.getEmails(email, token);
    },
  });

  registerChannel({
    channel: "tempmail-lol",
    name: "TempMail LOL",
    website: "tempmail.lol",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return tempmailLol.generateEmail(options.domain || null);
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token)
          throw new Error("internal error: token missing for tempmail-lol");
        return tempmailLol.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "chatgpt-org-uk",
    name: "ChatGPT Mail",
    website: "mail.chatgpt.org.uk",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return chatgptOrgUk.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token)
          throw new Error("internal error: token missing for chatgpt-org-uk");
        return chatgptOrgUk.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "temp-mail-io",
    name: "Temp-Mail.io",
    website: "temp-mail.io",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return tempMailIo.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return tempMailIo.getEmails(email);
    },
  });

  registerChannel({
    channel: "mail-cx",
    name: "Mail.cx",
    website: "mail.cx",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return mailCx.generateEmail(options.domain ?? null);
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for mail-cx");
        return mailCx.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "ddker-com",
    name: "ddker.com",
    website: "mail.cx",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return mailCx
          .generateEmail("ddker.com")
          .then((info) => ({ ...info, channel: "ddker-com" }));
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for mail-cx");
        return mailCx.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "catchmail",
    name: "Catchmail",
    website: "catchmail.io",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return catchmail.generateEmail(options.domain ?? null);
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return catchmail.getEmails(email);
    },
  });

  registerChannel({
    channel: "catchmail-mailistry",
    name: "Catchmail Mailistry",
    website: "mailistry.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return catchmail.generateEmail("mailistry.com", "catchmail-mailistry");
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return catchmail.getEmails(email);
    },
  });

  registerChannel({
    channel: "catchmail-zeppost",
    name: "Catchmail Zeppost",
    website: "zeppost.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return catchmail.generateEmail("zeppost.com", "catchmail-zeppost");
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return catchmail.getEmails(email);
    },
  });

  registerChannel({
    channel: "mailforspam",
    name: "MailForSpam",
    website: "mailforspam.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return mailforspam.generateEmail(options.domain ?? null);
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return mailforspam.getEmails(email);
    },
  });

  registerChannel({
    channel: "mailforspam-tempmail-io",
    name: "MailForSpam TempMail.io",
    website: "tempmail.io",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return mailforspam.generateEmail(
          "tempmail.io",
          "mailforspam-tempmail-io",
        );
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return mailforspam.getEmails(email);
    },
  });

  registerChannel({
    channel: "mailforspam-disposable",
    name: "MailForSpam Disposable",
    website: "disposable.email",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return mailforspam.generateEmail(
          "disposable.email",
          "mailforspam-disposable",
        );
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return mailforspam.getEmails(email);
    },
  });

  registerChannel({
    channel: "tempmailc",
    name: "TempMailC",
    website: "tempmailc.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return tempmailc.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return tempmailc.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "mailnesia",
    name: "Mailnesia",
    website: "mailnesia.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return mailnesia.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return mailnesia.getEmails(email);
    },
  });

  registerChannel({
    channel: "throwawaymail",
    name: "ThrowawayMail",
    website: "throwawaymail.app",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return throwawaymail.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token)
          throw new Error("internal error: token missing for throwawaymail");
        return throwawaymail.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "tempmail-fish",
    name: "TempMail Fish",
    website: "tempmail.fish",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return tempmailFish.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token)
          throw new Error("internal error: token missing for tempmail-fish");
        return tempmailFish.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "neighbours-sh",
    name: "Neighbours",
    website: "neighbours.sh",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return neighboursSh.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return neighboursSh.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "shitty-email",
    name: "shitty.email",
    website: "shitty.email",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return shittyEmail.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token)
          throw new Error("internal error: token missing for shitty-email");
        return shittyEmail.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "tempmailpro",
    name: "TempMail Pro",
    website: "tempmailpro.us",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return tempmailpro.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token)
          throw new Error("internal error: token missing for tempmailpro");
        return tempmailpro.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "devmail-uk",
    name: "DevMail UK",
    website: "devmail.uk",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return devmailUk.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return devmailUk.getEmails(email);
    },
  });

  registerChannel({
    channel: "inboxkitten",
    name: "InboxKitten",
    website: "inboxkitten.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return inboxkitten.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return inboxkitten.getEmails(email);
    },
  });

  registerChannel({
    channel: "cleantempmail",
    name: "CleanTempMail",
    website: "cleantempmail.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return cleantempmail.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return cleantempmail.getEmails(email);
    },
  });

  registerChannel({
    channel: "getnada",
    name: "GetNada",
    website: "getnada.net",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return getnada.generateEmail(options.domain ?? null);
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for getnada");
        return getnada.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "1vpn-net",
    name: "1vpn.net",
    website: "getnada.net",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return getnada.generateEmail("1vpn.net", "1vpn-net");
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for getnada");
        return getnada.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "abematv-com",
    name: "abematv.com",
    website: "getnada.net",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return getnada.generateEmail("abematv.com", "abematv-com");
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for getnada");
        return getnada.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "abematv-net",
    name: "abematv.net",
    website: "getnada.net",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return getnada.generateEmail("abematv.net", "abematv-net");
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for getnada");
        return getnada.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "abematv-org",
    name: "abematv.org",
    website: "getnada.net",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return getnada.generateEmail("abematv.org", "abematv-org");
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for getnada");
        return getnada.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "aceh-cc",
    name: "aceh.cc",
    website: "getnada.net",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return getnada.generateEmail("aceh.cc", "aceh-cc");
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for getnada");
        return getnada.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "bangkabelitung-net",
    name: "bangkabelitung.net",
    website: "getnada.net",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return getnada.generateEmail("bangkabelitung.net", "bangkabelitung-net");
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for getnada");
        return getnada.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "cctruyen-com",
    name: "cctruyen.com",
    website: "getnada.net",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return getnada.generateEmail("cctruyen.com", "cctruyen-com");
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for getnada");
        return getnada.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "getnada-com",
    name: "getnada.com",
    website: "getnada.net",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return getnada.generateEmail("getnada.com", "getnada-com");
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for getnada");
        return getnada.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "getnada-email",
    name: "getnada.email",
    website: "getnada.net",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return getnada.generateEmail("getnada.email", "getnada-email");
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for getnada");
        return getnada.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "getnada-net",
    name: "getnada.net",
    website: "getnada.net",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return getnada.generateEmail("getnada.net", "getnada-net");
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for getnada");
        return getnada.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "jawatengah-net",
    name: "jawatengah.net",
    website: "getnada.net",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return getnada.generateEmail("jawatengah.net", "jawatengah-net");
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for getnada");
        return getnada.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "jawatimur-net",
    name: "jawatimur.net",
    website: "getnada.net",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return getnada.generateEmail("jawatimur.net", "jawatimur-net");
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for getnada");
        return getnada.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "kalimantanbarat-net",
    name: "kalimantanbarat.net",
    website: "getnada.net",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return getnada.generateEmail(
          "kalimantanbarat.net",
          "kalimantanbarat-net",
        );
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for getnada");
        return getnada.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "kalimantanselatan-net",
    name: "kalimantanselatan.net",
    website: "getnada.net",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return getnada.generateEmail(
          "kalimantanselatan.net",
          "kalimantanselatan-net",
        );
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for getnada");
        return getnada.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "kalimantantengah-net",
    name: "kalimantantengah.net",
    website: "getnada.net",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return getnada.generateEmail(
          "kalimantantengah.net",
          "kalimantantengah-net",
        );
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for getnada");
        return getnada.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "kalimantantimur-net",
    name: "kalimantantimur.net",
    website: "getnada.net",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return getnada.generateEmail(
          "kalimantantimur.net",
          "kalimantantimur-net",
        );
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for getnada");
        return getnada.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "kalimantanutara-net",
    name: "kalimantanutara.net",
    website: "getnada.net",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return getnada.generateEmail(
          "kalimantanutara.net",
          "kalimantanutara-net",
        );
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for getnada");
        return getnada.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "kepulauanriau-net",
    name: "kepulauanriau.net",
    website: "getnada.net",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return getnada.generateEmail("kepulauanriau.net", "kepulauanriau-net");
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for getnada");
        return getnada.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "luxury345-com",
    name: "luxury345.com",
    website: "getnada.net",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return getnada.generateEmail("luxury345.com", "luxury345-com");
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for getnada");
        return getnada.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "malukuutara-net",
    name: "malukuutara.net",
    website: "getnada.net",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return getnada.generateEmail("malukuutara.net", "malukuutara-net");
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for getnada");
        return getnada.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "nusatenggarabarat-net",
    name: "nusatenggarabarat.net",
    website: "getnada.net",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return getnada.generateEmail(
          "nusatenggarabarat.net",
          "nusatenggarabarat-net",
        );
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for getnada");
        return getnada.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "nusatenggaratimur-net",
    name: "nusatenggaratimur.net",
    website: "getnada.net",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return getnada.generateEmail(
          "nusatenggaratimur.net",
          "nusatenggaratimur-net",
        );
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for getnada");
        return getnada.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "papuabarat-net",
    name: "papuabarat.net",
    website: "getnada.net",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return getnada.generateEmail("papuabarat.net", "papuabarat-net");
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for getnada");
        return getnada.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "papuabaratdaya-net",
    name: "papuabaratdaya.net",
    website: "getnada.net",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return getnada.generateEmail("papuabaratdaya.net", "papuabaratdaya-net");
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for getnada");
        return getnada.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "papuaselatan-net",
    name: "papuaselatan.net",
    website: "getnada.net",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return getnada.generateEmail("papuaselatan.net", "papuaselatan-net");
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for getnada");
        return getnada.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "pehol-com",
    name: "pehol.com",
    website: "getnada.net",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return getnada.generateEmail("pehol.com", "pehol-com");
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for getnada");
        return getnada.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "ptruyen-com",
    name: "ptruyen.com",
    website: "getnada.net",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return getnada.generateEmail("ptruyen.com", "ptruyen-com");
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for getnada");
        return getnada.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "pulaubali-net",
    name: "pulaubali.net",
    website: "getnada.net",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return getnada.generateEmail("pulaubali.net", "pulaubali-net");
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for getnada");
        return getnada.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "riau-net",
    name: "riau.net",
    website: "getnada.net",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return getnada.generateEmail("riau.net", "riau-net");
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for getnada");
        return getnada.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "seokey-org",
    name: "seokey.org",
    website: "getnada.net",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return getnada.generateEmail("seokey.org", "seokey-org");
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for getnada");
        return getnada.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "sulawesibarat-net",
    name: "sulawesibarat.net",
    website: "getnada.net",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return getnada.generateEmail("sulawesibarat.net", "sulawesibarat-net");
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for getnada");
        return getnada.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "sulawesiselatan-net",
    name: "sulawesiselatan.net",
    website: "getnada.net",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return getnada.generateEmail(
          "sulawesiselatan.net",
          "sulawesiselatan-net",
        );
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for getnada");
        return getnada.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "sulawesitengah-net",
    name: "sulawesitengah.net",
    website: "getnada.net",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return getnada.generateEmail("sulawesitengah.net", "sulawesitengah-net");
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for getnada");
        return getnada.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "sulawesitenggara-net",
    name: "sulawesitenggara.net",
    website: "getnada.net",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return getnada.generateEmail(
          "sulawesitenggara.net",
          "sulawesitenggara-net",
        );
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for getnada");
        return getnada.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "sumaterabarat-net",
    name: "sumaterabarat.net",
    website: "getnada.net",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return getnada.generateEmail("sumaterabarat.net", "sumaterabarat-net");
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for getnada");
        return getnada.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "sumateraselatan-net",
    name: "sumateraselatan.net",
    website: "getnada.net",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return getnada.generateEmail(
          "sumateraselatan.net",
          "sumateraselatan-net",
        );
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for getnada");
        return getnada.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "sumaterautara-net",
    name: "sumaterautara.net",
    website: "getnada.net",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return getnada.generateEmail("sumaterautara.net", "sumaterautara-net");
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for getnada");
        return getnada.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "villatogel-com",
    name: "villatogel.com",
    website: "getnada.net",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return getnada.generateEmail("villatogel.com", "villatogel-com");
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for getnada");
        return getnada.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "mail123",
    name: "Mail123",
    website: "mail123.fr",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return mail123.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return mail123.getEmails(email);
    },
  });

  registerChannel({
    channel: "mail10s",
    name: "Mail10s",
    website: "mail10s.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return mail10s.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return mail10s.getEmails(email);
    },
  });

  registerChannel({
    channel: "webmailtemp",
    name: "WebMailTemp",
    website: "webmailtemp.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return webmailtemp.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token)
          throw new Error("internal error: token missing for webmailtemp");
        return webmailtemp.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "tempfastmail",
    name: "TempFastMail",
    website: "tempfastmail.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return tempfastmail.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token)
          throw new Error("internal error: token missing for tempfastmail");
        return tempfastmail.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "1sec-mail",
    name: "1SecMail",
    website: "1sec-mail.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return oneSecMail.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token)
          throw new Error("internal error: token missing for 1sec-mail");
        return oneSecMail.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "fakemail",
    name: "FakeMail",
    website: "fakemail.net",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return fakemail.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for fakemail");
        return fakemail.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "openinbox",
    name: "OpenInbox",
    website: "openinbox.io",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return openinbox.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token)
          throw new Error("internal error: token missing for openinbox");
        return openinbox.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "inboxes",
    name: "Inboxes",
    website: "inboxes.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return inboxes.generateEmail(options.domain ?? null);
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return inboxes.getEmails(email);
    },
  });

  registerChannel({
    channel: "uncorreotemporal",
    name: "UnCorreoTemporal",
    website: "uncorreotemporal.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return uncorreotemporal.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token)
          throw new Error("internal error: token missing for uncorreotemporal");
        return uncorreotemporal.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "awamail",
    name: "AwaMail",
    website: "awamail.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return awamail.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for awamail");
        return awamail.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "mail-tm",
    name: "Mail.tm",
    website: "mail.tm",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return mailTm.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for mail-tm");
        return mailTm.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "web-library-net",
    name: "web-library.net",
    website: "mail.tm",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return mailTm
          .generateEmail()
          .then((info) => ({ ...info, channel: "web-library-net" }));
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for mail-tm");
        return mailTm.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "dropmail",
    name: "DropMail",
    website: "dropmail.me",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return dropmail.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for dropmail");
        return dropmail.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "guerrillamail",
    name: "Guerrilla Mail",
    website: "guerrillamail.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return guerrillamail.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token)
          throw new Error("internal error: token missing for guerrillamail");
        return guerrillamail.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "guerrillamail-com",
    name: "GuerrillaMail Root",
    website: "guerrillamail.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return guerrillamailComMirror.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token)
          throw new Error("internal error: token missing for guerrillamail-com");
        return guerrillamailComMirror.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "maildrop",
    name: "Maildrop",
    website: "maildrop.cx",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return maildropProvider.generateEmail(options.domain ?? null);
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for maildrop");
        return maildropProvider.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "smail-pw",
    name: "Smail.pw",
    website: "smail.pw",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return smailPw.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for smail-pw");
        return smailPw.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "vip-215",
    name: "VIP 215",
    website: "vip.215.im",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return vip215.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for vip-215");
        return vip215.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "fake-legal",
    name: "Fake Legal",
    website: "fake.legal",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return fakeLegal.generateEmail(options.domain ?? null);
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return fakeLegal.getEmails(email);
    },
  });

  registerChannel({
    channel: "imgui-de",
    name: "imgui.de",
    website: "fake.legal",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return fakeLegal.generateEmail("imgui.de", "imgui-de");
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return fakeLegal.getEmails(email);
    },
  });

  registerChannel({
    channel: "pulsewebmenu-de",
    name: "pulsewebmenu.de",
    website: "fake.legal",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return fakeLegal.generateEmail("pulsewebmenu.de", "pulsewebmenu-de");
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return fakeLegal.getEmails(email);
    },
  });

  registerChannel({
    channel: "moakt",
    name: "Moakt",
    website: "moakt.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return moakt.generateEmail(options.domain ?? null);
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for moakt");
        return moakt.getEmails(email, token);
    },
  });

  registerChannel({
    channel: "drmail-in",
    name: "drmail.in",
    website: "moakt.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return moakt
          .generateEmail("drmail.in")
          .then((info) => ({ ...info, channel: "drmail-in" }));
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for moakt");
        return moakt.getEmails(email, token);
    },
  });

  registerChannel({
    channel: "teml-net",
    name: "teml.net",
    website: "moakt.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return moakt
          .generateEmail("teml.net")
          .then((info) => ({ ...info, channel: "teml-net" }));
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for moakt");
        return moakt.getEmails(email, token);
    },
  });

  registerChannel({
    channel: "tmpeml-com",
    name: "tmpeml.com",
    website: "moakt.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return moakt
          .generateEmail("tmpeml.com")
          .then((info) => ({ ...info, channel: "tmpeml-com" }));
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for moakt");
        return moakt.getEmails(email, token);
    },
  });

  registerChannel({
    channel: "tmpbox-net",
    name: "tmpbox.net",
    website: "moakt.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return moakt
          .generateEmail("tmpbox.net")
          .then((info) => ({ ...info, channel: "tmpbox-net" }));
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for moakt");
        return moakt.getEmails(email, token);
    },
  });

  registerChannel({
    channel: "moakt-cc",
    name: "moakt.cc",
    website: "moakt.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return moakt
          .generateEmail("moakt.cc")
          .then((info) => ({ ...info, channel: "moakt-cc" }));
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for moakt");
        return moakt.getEmails(email, token);
    },
  });

  registerChannel({
    channel: "disbox-net",
    name: "disbox.net",
    website: "moakt.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return moakt
          .generateEmail("disbox.net")
          .then((info) => ({ ...info, channel: "disbox-net" }));
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for moakt");
        return moakt.getEmails(email, token);
    },
  });

  registerChannel({
    channel: "tmpmail-org",
    name: "tmpmail.org",
    website: "moakt.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return moakt
          .generateEmail("tmpmail.org")
          .then((info) => ({ ...info, channel: "tmpmail-org" }));
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for moakt");
        return moakt.getEmails(email, token);
    },
  });

  registerChannel({
    channel: "tmpmail-net",
    name: "tmpmail.net",
    website: "moakt.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return moakt
          .generateEmail("tmpmail.net")
          .then((info) => ({ ...info, channel: "tmpmail-net" }));
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for moakt");
        return moakt.getEmails(email, token);
    },
  });

  registerChannel({
    channel: "tmails-net",
    name: "tmails.net",
    website: "moakt.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return moakt
          .generateEmail("tmails.net")
          .then((info) => ({ ...info, channel: "tmails-net" }));
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for moakt");
        return moakt.getEmails(email, token);
    },
  });

  registerChannel({
    channel: "disbox-org",
    name: "disbox.org",
    website: "moakt.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return moakt
          .generateEmail("disbox.org")
          .then((info) => ({ ...info, channel: "disbox-org" }));
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for moakt");
        return moakt.getEmails(email, token);
    },
  });

  registerChannel({
    channel: "moakt-co",
    name: "moakt.co",
    website: "moakt.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return moakt
          .generateEmail("moakt.co")
          .then((info) => ({ ...info, channel: "moakt-co" }));
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for moakt");
        return moakt.getEmails(email, token);
    },
  });

  registerChannel({
    channel: "moakt-ws",
    name: "moakt.ws",
    website: "moakt.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return moakt
          .generateEmail("moakt.ws")
          .then((info) => ({ ...info, channel: "moakt-ws" }));
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for moakt");
        return moakt.getEmails(email, token);
    },
  });

  registerChannel({
    channel: "tmail-ws",
    name: "tmail.ws",
    website: "moakt.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return moakt
          .generateEmail("tmail.ws")
          .then((info) => ({ ...info, channel: "tmail-ws" }));
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for moakt");
        return moakt.getEmails(email, token);
    },
  });

  registerChannel({
    channel: "bareed-ws",
    name: "bareed.ws",
    website: "moakt.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return moakt
          .generateEmail("bareed.ws")
          .then((info) => ({ ...info, channel: "bareed-ws" }));
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for moakt");
        return moakt.getEmails(email, token);
    },
  });

  registerChannel({
    channel: "email10min",
    name: "Email10Min",
    website: "email10min.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return email10min.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token)
          throw new Error("internal error: token missing for email10min");
        return email10min.getEmails(email, token);
    },
  });

  registerChannel({
    channel: "mjj-cm",
    name: "MJJ Mail",
    website: "mjj.cm",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return mjjCm.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return mjjCm.getEmails(email);
    },
  });

  registerChannel({
    channel: "linshi-co",
    name: "临时Co",
    website: "linshi.co",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return linshiCo.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return linshiCo.getEmails(email);
    },
  });

  registerChannel({
    channel: "harakirimail",
    name: "HarakiriMail",
    website: "harakirimail.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return harakirimail.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return harakirimail.getEmails(email);
    },
  });

  registerChannel({
    channel: "jqkjqk-xyz",
    name: "jqkjqk.xyz",
    website: "mail.zhujump.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return zhujump.generateEmail("jqkjqk.xyz", "jqkjqk-xyz");
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        const channel: Channel = "jqkjqk-xyz";
        if (!token)
          throw new Error(`internal error: token missing for ${channel}`);
        return zhujump.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "lyhlevi-com",
    name: "LyhLevi MoeMail",
    website: "lyhlevi.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return zhujump.generateEmailForInstance({
          baseUrl: "https://lyhlevi.com",
          domain: "lyhlevi.com",
          channel: "lyhlevi-com",
          expiryTime: 24 * 60 * 60 * 1000,
        });
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        const channel: Channel = "lyhlevi-com";
        if (!token)
          throw new Error(`internal error: token missing for ${channel}`);
        return zhujump.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "tempmail-plus",
    name: "TempMail Plus",
    website: "tempmail.plus",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return tempmailPlus.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return tempmailPlus.getEmails(email);
    },
  });

  registerChannel({
    channel: "fexpost-com",
    name: "fexpost.com",
    website: "tempmail.plus",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return tempmailPlus.generateEmail("fexpost.com", "fexpost-com");
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return tempmailPlus.getEmails(email);
    },
  });

  registerChannel({
    channel: "fexbox-org",
    name: "fexbox.org",
    website: "tempmail.plus",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return tempmailPlus.generateEmail("fexbox.org", "fexbox-org");
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return tempmailPlus.getEmails(email);
    },
  });

  registerChannel({
    channel: "mailbox-in-ua",
    name: "mailbox.in.ua",
    website: "tempmail.plus",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return tempmailPlus.generateEmail("mailbox.in.ua", "mailbox-in-ua");
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return tempmailPlus.getEmails(email);
    },
  });

  registerChannel({
    channel: "rover-info",
    name: "rover.info",
    website: "tempmail.plus",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return tempmailPlus.generateEmail("rover.info", "rover-info");
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return tempmailPlus.getEmails(email);
    },
  });

  registerChannel({
    channel: "chitthi-in",
    name: "chitthi.in",
    website: "tempmail.plus",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return tempmailPlus.generateEmail("chitthi.in", "chitthi-in");
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return tempmailPlus.getEmails(email);
    },
  });

  registerChannel({
    channel: "fextemp-com",
    name: "fextemp.com",
    website: "tempmail.plus",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return tempmailPlus.generateEmail("fextemp.com", "fextemp-com");
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return tempmailPlus.getEmails(email);
    },
  });

  registerChannel({
    channel: "any-pink",
    name: "any.pink",
    website: "tempmail.plus",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return tempmailPlus.generateEmail("any.pink", "any-pink");
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return tempmailPlus.getEmails(email);
    },
  });

  registerChannel({
    channel: "merepost-com",
    name: "merepost.com",
    website: "tempmail.plus",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return tempmailPlus.generateEmail("merepost.com", "merepost-com");
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return tempmailPlus.getEmails(email);
    },
  });

  registerChannel({
    channel: "tempmail-lol-v2",
    name: "TempMail LOL V2",
    website: "tempmail.lol",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return tempmailLolV2.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token)
          throw new Error("internal error: token missing for tempmail-lol-v2");
        return tempmailLolV2.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "tempgbox",
    name: "TempGBox",
    website: "tempgbox.net",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return tempgbox.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for tempgbox");
        return tempgbox.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "emailnator",
    name: "Emailnator",
    website: "emailnator.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return emailnator.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token)
          throw new Error("internal error: token missing for emailnator");
        return emailnator.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "temporam",
    name: "Temporam",
    website: "temporam.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return temporam.generateEmail(options.domain ?? null);
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return temporam.getEmails(email);
    },
  });

  registerChannel({
    channel: "neighbours",
    name: "Neighbours",
    website: "neighbours.sh",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return neighbours.generateEmail(options.domain ?? null);
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return neighbours.getEmails(email);
    },
  });

  registerChannel({
    channel: "sharklasers",
    name: "SharkLasers",
    website: "sharklasers.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return sharklasers.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token)
          throw new Error("internal error: token missing for sharklasers");
        return sharklasers.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "sharklasers-com",
    name: "SharkLasers Root",
    website: "sharklasers.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return sharklasersCom.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token)
          throw new Error("internal error: token missing for sharklasers-com");
        return sharklasersCom.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "grr-la",
    name: "Grr.la",
    website: "grr.la",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return grrla.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for grr-la");
        return grrla.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "grr-la-com",
    name: "Grr.la Root",
    website: "grr.la",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return grrlaCom.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token)
          throw new Error("internal error: token missing for grr-la-com");
        return grrlaCom.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "guerrillamail-info",
    name: "GuerrillaMail Info",
    website: "guerrillamail.info",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return guerrillainfoMirror.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token)
          throw new Error("internal error: token missing for guerrillamail-info");
        return guerrillainfoMirror.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "spam4me",
    name: "Spam4.me",
    website: "spam4.me",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return spam4meMirror.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for spam4me");
        return spam4meMirror.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "guerrillamail-net",
    name: "GuerrillaMail Net",
    website: "guerrillamail.net",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return guerrillamailNetMirror.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token)
          throw new Error("internal error: token missing for guerrillamail-net");
        return guerrillamailNetMirror.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "guerrillamail-org",
    name: "GuerrillaMail Org",
    website: "guerrillamail.org",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return guerrillamailOrgMirror.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token)
          throw new Error("internal error: token missing for guerrillamail-org");
        return guerrillamailOrgMirror.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "guerrillamailblock",
    name: "GuerrillaMailBlock",
    website: "guerrillamailblock.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return guerrillamailBlockMirror.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token)
          throw new Error("internal error: token missing for guerrillamailblock");
        return guerrillamailBlockMirror.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "guerrillamail-com-www",
    name: "GuerrillaMail WWW",
    website: "guerrillamail.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return guerrillamailComWwwMirror.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token)
          throw new Error(
            "internal error: token missing for guerrillamail-com-www",
          );
        return guerrillamailComWwwMirror.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "m2u",
    name: "MailToYou",
    website: "m2u.io",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return m2u.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for m2u");
        return m2u.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "tempy-email",
    name: "Tempy Email",
    website: "tempy.email",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return tempyEmail.generateEmail(options.domain ?? null);
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return tempyEmail.getEmails(email);
    },
  });

  registerChannel({
    channel: "fmail",
    name: "FMail",
    website: "fmail.men",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return fmail.generateEmail(options.domain ?? null);
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return fmail.getEmails(email);
    },
  });

  registerChannel({
    channel: "ockito",
    name: "Ockito",
    website: "ockito.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return ockito.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for ockito");
        return ockito.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "anonbox",
    name: "Anonbox",
    website: "anonbox.net",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return anonbox.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for anonbox");
        return anonbox.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "duckmail",
    name: "DuckMail",
    website: "duckmail.sbs",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return duckmail.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for duckmail");
        return duckmail.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "mailinator",
    name: "Mailinator",
    website: "mailinator.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return mailinator.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return mailinator.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "tempmail365",
    name: "Tempmail365",
    website: "https://tempmail365.cn",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return tempmail365.generateEmail(options.domain ?? null);
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return tempmail365.getEmails(email);
    },
  });

  registerChannel({
    channel: "tempinbox",
    name: "TempInbox",
    website: "https://www.tempinbox.xyz",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return tempinbox.generateEmail(options.domain ?? null);
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return tempinbox.getEmails(email);
    },
  });

  registerChannel({
    channel: "byom",
    name: "Byom",
    website: "byom.de",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return byom.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return byom.getEmails(email);
    },
  });

  registerChannel({
    channel: "anonymmail",
    name: "AnonymMail",
    website: "anonymmail.net",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return anonymmail.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return anonymmail.getEmails(email);
    },
  });

  registerChannel({
    channel: "eyepaste",
    name: "EyePaste",
    website: "eyepaste.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return eyepaste.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return eyepaste.getEmails(email);
    },
  });

  registerChannel({
    channel: "mail-sunls",
    name: "Mail Sunls",
    website: "mail.sunls.de",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return mailSunls.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return mailSunls.getEmails(email);
    },
  });

  registerChannel({
    channel: "expressinboxhub",
    name: "ExpressInboxHub",
    website: "expressinboxhub.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return expressinboxhub.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token)
          throw new Error("internal error: token missing for expressinboxhub");
        return expressinboxhub.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "lroid",
    name: "Lroid",
    website: "lroid.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return lroid.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for lroid");
        return lroid.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "haribu",
    name: "Haribu",
    website: "haribu.net",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return haribu.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for haribu");
        return haribu.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "rootsh",
    name: "Rootsh(BccTo)",
    website: "rootsh.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return rootsh.generateEmail(options.domain ?? undefined);
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for rootsh");
        return rootsh.getEmails(email, token);
    },
  });

  registerChannel({
    channel: "fake-email-site",
    name: "FakeEmailSite",
    website: "fake-email.site",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return fakeEmailSite.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return fakeEmailSite.getEmails(email);
    },
  });

  registerChannel({
    channel: "mohmal",
    name: "Mohmal",
    website: "mohmal.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return mohmal.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for mohmal");
        return mohmal.getEmails(email, token);
    },
  });

  registerChannel({
    channel: "mailgolem",
    name: "MailGolem",
    website: "mailgolem.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return mailgolem.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token)
          throw new Error("internal error: token missing for mailgolem");
        return mailgolem.getEmails(email, token);
    },
  });

  registerChannel({
    channel: "best-temp-mail",
    name: "BestTempMail",
    website: "best-temp-mail.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return bestTempMail.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token)
          throw new Error("internal error: token missing for best-temp-mail");
        return bestTempMail.getEmails(email, token);
    },
  });

  registerChannel({
    channel: "disposablemail-app",
    name: "DisposableMail",
    website: "disposablemail.app",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return disposablemailApp.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        const channel: Channel = "disposablemail-app";
        if (!token)
          throw new Error(`internal error: token missing for ${channel}`);
        return disposablemailApp.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "mailtemp-cc",
    name: "MailTemp.cc",
    website: "mailtemp.cc",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return mailtempCc.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token)
          throw new Error("internal error: token missing for mailtemp-cc");
        return mailtempCc.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "minuteinbox",
    name: "MinuteInbox",
    website: "minuteinbox.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return minuteinbox.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        const channel: Channel = "minuteinbox";
        if (!token)
          throw new Error(`internal error: token missing for ${channel}`);
        return minuteinbox.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "mailcatch",
    name: "MailCatch",
    website: "mailcatch.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return mailcatch.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token)
          throw new Error("internal error: token missing for mailcatch");
        return mailcatch.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "tempemail-co",
    name: "TempEmail.co",
    website: "tempemail.co",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return tempemailCo.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token)
          throw new Error("internal error: token missing for tempemail-co");
        return tempemailCo.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "tempemails-net",
    name: "TempEmails.net",
    website: "tempemails.net",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        const channel: Channel = "tempemails-net";
        return tempemailsNet
          .generateEmail()
          .then((info) => ({ ...info, channel }));
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        const channel: Channel = "tempemails-net";
        if (!token)
          throw new Error(`internal error: token missing for ${channel}`);
        return tempemailsNet.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "altmails",
    name: "AltMails",
    website: "tempmail.altmails.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return altmails.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for altmails");
        return altmails.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "tempemail-info",
    name: "TempEmailInfo",
    website: "tempemail.info",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return tempemailInfo.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token)
          throw new Error("internal error: token missing for tempemail-info");
        return tempemailInfo.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "smailpro",
    name: "SmailPro",
    website: "smailpro.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return smailpro.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        /* smailpro 无状态：token 由 provider 自行处理，dispatch 不强制要求 */
        return smailpro.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "tempmailten",
    name: "TempMailTen",
    website: "tempmailten.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return tempmailten.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token)
          throw new Error("internal error: token missing for tempmailten");
        return tempmailten.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "maildrop-cc",
    name: "MailDrop.cc",
    website: "maildrop.cc",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return maildropCc.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        /* maildrop-cc 无状态：token 由 provider 自行处理，dispatch 不强制要求 */
        return maildropCc.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "10minutemail-net",
    name: "10MinuteMail.net",
    website: "10minutemail.net",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return tenminutemailNet.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token)
          throw new Error("internal error: token missing for 10minutemail-net");
        return tenminutemailNet.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "linshiyouxiang-net",
    name: "临时邮箱(linshiyouxiang)",
    website: "linshiyouxiang.net",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return linshiyouxiangNet.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token)
          throw new Error("internal error: token missing for linshiyouxiang-net");
        return linshiyouxiangNet.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "tempmail-fyi",
    name: "Temp-Mail.fyi",
    website: "temp-mail.fyi",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return tempmailFyi.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token)
          throw new Error("internal error: token missing for tempmail-fyi");
        return tempmailFyi.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "disposablemail-com",
    name: "DisposableMail.com",
    website: "disposablemail.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return disposablemailCom.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token)
          throw new Error("internal error: token missing for disposablemail-com");
        return disposablemailCom.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "tempp-mails",
    name: "TemppMails",
    website: "tempp-mails.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return temppMails.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token)
          throw new Error("internal error: token missing for tempp-mails");
        return temppMails.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "emailtemp-org",
    name: "EmailTemp",
    website: "emailtemp.org",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return emailtempOrg.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token)
          throw new Error("internal error: token missing for emailtemp-org");
        return emailtempOrg.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "mytempmail-cc",
    name: "MyTempMail.cc",
    website: "mytempmail.cc",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return mytempmailCc.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token)
          throw new Error("internal error: token missing for mytempmail-cc");
        return mytempmailCc.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "temp-mail-now",
    name: "TempMailNow",
    website: "temp-mail.now",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return tempMailNow.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token)
          throw new Error("internal error: token missing for temp-mail-now");
        return tempMailNow.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "mail-td",
    name: "Mail.td",
    website: "mail.td",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return mailTd.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for mail-td");
        return mailTd.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "mailhole-de",
    name: "Mailhole.de",
    website: "mailhole.de",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return mailholeDe.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token)
          throw new Error("internal error: token missing for mailhole-de");
        return mailholeDe.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "tmail-link",
    name: "TMail.link",
    website: "tmail.link",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return tmailLink.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token)
          throw new Error("internal error: token missing for tmail-link");
        return tmailLink.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "24mail-chacuo",
    name: "24Mail Chacuo",
    website: "24mail.chacuo.net",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return twentyfourmailChacuo.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token)
          throw new Error("internal error: token missing for 24mail-chacuo");
        return twentyfourmailChacuo.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "nimail",
    name: "NiMail",
    website: "nimail.cn",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return nimail.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return nimail.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "freecustom",
    name: "FreeCustom.Email",
    website: "freecustom.email",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return freecustom.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return freecustom.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "16888888-cyou",
    name: "Mailmomy (16888888.cyou)",
    website: "mailmomy.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return n16888888Cyou.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return n16888888Cyou.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "17666688-shop",
    name: "Mailmomy (17666688.shop)",
    website: "mailmomy.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return n17666688Shop.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return n17666688Shop.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "282mail-com",
    name: "Mailmomy (282mail.com)",
    website: "mailmomy.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return n282mailCom.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return n282mailCom.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "blackhole-djurby-se",
    name: "Mailinator (blackhole.djurby.se)",
    website: "mailinator.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return blackholeDjurbySe.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return blackholeDjurbySe.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "block-bdea-cc",
    name: "Mailinator (block.bdea.cc)",
    website: "mailinator.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return blockBdeaCc.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return blockBdeaCc.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "bsdu32-buzz",
    name: "Mailmomy (bsdu32.buzz)",
    website: "mailmomy.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return bsdu32Buzz.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return bsdu32Buzz.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "b-smelly-cc",
    name: "Mailinator (b.smelly.cc)",
    website: "mailinator.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return bSmellyCc.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return bSmellyCc.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "carlton183-changeip-net",
    name: "Mailinator (183carlton.changeip.net)",
    website: "mailinator.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return carlton183ChangeipNet.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return carlton183ChangeipNet.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "dea-soon-it",
    name: "Mailinator (dea.soon.it)",
    website: "mailinator.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return deaSoonIt.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return deaSoonIt.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "disposable-al-sudani-com",
    name: "Mailinator (disposable.al-sudani.com)",
    website: "mailinator.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return disposableAlSudaniCom.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return disposableAlSudaniCom.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "disposable-nogonad-nl",
    name: "Mailinator (disposable.nogonad.nl)",
    website: "mailinator.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return disposableNogonadNl.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return disposableNogonadNl.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "doxu243-buzz",
    name: "Mailmomy (doxu243.buzz)",
    website: "mailmomy.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return doxu243Buzz.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return doxu243Buzz.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "easyme-pro",
    name: "Mailmomy (easyme.pro)",
    website: "mailmomy.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return easymePro.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return easymePro.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "ebs-com-ar",
    name: "Mailinator (ebs.com.ar)",
    website: "mailinator.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return ebsComAr.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return ebsComAr.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "etgdev-de",
    name: "Mailinator (etgdev.de)",
    website: "mailinator.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return etgdevDe.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return etgdevDe.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "evergreenco-shop",
    name: "Mailmomy (evergreenco.shop)",
    website: "mailmomy.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return evergreencoShop.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return evergreencoShop.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "fwd2m-eszett-es",
    name: "Mailinator (fwd2m.eszett.es)",
    website: "mailinator.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return fwd2mEszettEs.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return fwd2mEszettEs.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "jama-trenet-eu",
    name: "Mailinator (jama.trenet.eu)",
    website: "mailinator.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return jamaTrenetEu.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return jamaTrenetEu.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "j-fairuse-org",
    name: "Mailinator (j.fairuse.org)",
    website: "mailinator.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return jFairuseOrg.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return jFairuseOrg.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "layueming-pics",
    name: "Mailmomy (layueming.pics)",
    website: "mailmomy.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return layuemingPics.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return layuemingPics.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "m-887-at",
    name: "Mailinator (m.887.at)",
    website: "mailinator.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return m887At.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return m887At.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "m8r-davidfuhr-de",
    name: "Mailinator (m8r.davidfuhr.de)",
    website: "mailinator.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return m8rDavidfuhrDe.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return m8rDavidfuhrDe.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "m8r-mcasal-com",
    name: "Mailinator (m8r.mcasal.com)",
    website: "mailinator.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return m8rMcasalCom.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return m8rMcasalCom.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "mail-bentrask-com",
    name: "Mailinator (mail.bentrask.com)",
    website: "mailinator.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return mailBentraskCom.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return mailBentraskCom.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "mail-fsmash-org",
    name: "Mailinator (mail.fsmash.org)",
    website: "mailinator.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return mailFsmashOrg.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return mailFsmashOrg.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "mailinatorzz-mooo-com",
    name: "Mailinator (mailinatorzz.mooo.com)",
    website: "mailinator.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return mailinatorzzMoooCom.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return mailinatorzzMoooCom.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "mi-meon-be",
    name: "Mailinator (mi.meon.be)",
    website: "mailinator.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return miMeonBe.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return miMeonBe.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "mingyuekeji-online",
    name: "Mailmomy (mingyuekeji.online)",
    website: "mailmomy.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return mingyuekejiOnline.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return mingyuekejiOnline.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "mingyueming-click",
    name: "Mailmomy (mingyueming.click)",
    website: "mailmomy.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return mingyuemingClick.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return mingyuemingClick.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "mingyueming-shop",
    name: "Mailmomy (mingyueming.shop)",
    website: "mailmomy.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return mingyuemingShop.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return mingyuemingShop.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "mingyukeji-lol",
    name: "Mailmomy (mingyukeji.lol)",
    website: "mailmomy.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return mingyukejiLol.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return mingyukejiLol.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "mn-curppa-com",
    name: "Mailinator (mn.curppa.com)",
    website: "mailinator.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return mnCurppaCom.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return mnCurppaCom.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "m-nik-me",
    name: "Mailinator (m.nik.me)",
    website: "mailinator.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return mNikMe.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return mNikMe.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "mtmdev-com",
    name: "Mailinator (mtmdev.com)",
    website: "mailinator.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return mtmdevCom.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return mtmdevCom.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "nospam-thurstons-us",
    name: "Mailinator (nospam.thurstons.us)",
    website: "mailinator.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return nospamThurstonsUs.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return nospamThurstonsUs.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "notfond-404-mn",
    name: "Mailinator (notfond.404.mn)",
    website: "mailinator.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return notfond404Mn.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return notfond404Mn.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "null-k3vin-net",
    name: "Mailinator (null.k3vin.net)",
    website: "mailinator.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return nullK3vinNet.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return nullK3vinNet.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "nuxh62-space",
    name: "Mailmomy (nuxh62.space)",
    website: "mailmomy.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return nuxh62Space.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return nuxh62Space.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "proid-cloud-ip-cc",
    name: "Mailmomy (proid.cloud-ip.cc)",
    website: "mailmomy.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return proidCloudIpCc.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return proidCloudIpCc.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "ramjane-mooo-com",
    name: "Mailinator (ramjane.mooo.com)",
    website: "mailinator.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return ramjaneMoooCom.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return ramjaneMoooCom.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "rauxa-seny-cat",
    name: "Mailinator (rauxa.seny.cat)",
    website: "mailinator.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return rauxaSenyCat.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return rauxaSenyCat.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "really-istrash-com",
    name: "Mailinator (really.istrash.com)",
    website: "mailinator.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return reallyIstrashCom.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return reallyIstrashCom.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "sbook-pics",
    name: "Mailmomy (sbook.pics)",
    website: "mailmomy.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return sbookPics.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return sbookPics.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "spam-hortuk-ovh",
    name: "Mailinator (spam.hortuk.ovh)",
    website: "mailinator.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return spamHortukOvh.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return spamHortukOvh.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "sp-woot-at",
    name: "Mailinator (sp.woot.at)",
    website: "mailinator.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return spWootAt.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return spWootAt.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "test-unergie-com",
    name: "Mailinator (test.unergie.com)",
    website: "mailinator.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return testUnergieCom.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return testUnergieCom.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "torch-yi-org",
    name: "Mailinator (torch.yi.org)",
    website: "mailinator.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return torchYiOrg.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return torchYiOrg.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "t-zibet-net",
    name: "Mailinator (t.zibet.net)",
    website: "mailinator.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return tZibetNet.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return tZibetNet.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "xue32-buzz",
    name: "Mailmomy (xue32.buzz)",
    website: "mailmomy.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return xue32Buzz.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return xue32Buzz.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "apihz",
    name: "ApiHz TempMail",
    website: "apihz.cn",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return apihz.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token) throw new Error("internal error: token missing for apihz");
        return apihz.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "sogetthis-com",
    name: "Mailinator (sogetthis.com)",
    website: "mailinator.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return sogetthisCom.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return sogetthisCom.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "bobmail-info",
    name: "Mailinator (bobmail.info)",
    website: "mailinator.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return bobmailInfo.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return bobmailInfo.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "suremail-info",
    name: "Mailinator (suremail.info)",
    website: "mailinator.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return suremailInfo.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return suremailInfo.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "binkmail-com",
    name: "Mailinator (binkmail.com)",
    website: "mailinator.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return binkmailCom.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return binkmailCom.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "veryrealemail-com",
    name: "Mailinator (veryrealemail.com)",
    website: "mailinator.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return veryrealemailCom.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return veryrealemailCom.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "mailmomy",
    name: "Mailmomy",
    website: "mailmomy.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return mailmomy.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return mailmomy.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "chammy-info",
    name: "Mailinator (chammy.info)",
    website: "mailinator.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return chammyInfo.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return chammyInfo.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "thisisnotmyrealemail-com",
    name: "Mailinator (thisisnotmyrealemail.com)",
    website: "mailinator.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return thisisnotmyrealemailCom.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return thisisnotmyrealemailCom.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "notmailinator-com",
    name: "Mailinator (notmailinator.com)",
    website: "mailinator.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return notmailinatorCom.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return notmailinatorCom.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "spamhereplease-com",
    name: "Mailinator (spamhereplease.com)",
    website: "mailinator.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return spamherepleaseCom.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return spamherepleaseCom.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "sendspamhere-com",
    name: "Mailinator (sendspamhere.com)",
    website: "mailinator.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return sendspamhereCom.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return sendspamhereCom.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "sendfree-org",
    name: "Mailinator (sendfree.org)",
    website: "mailinator.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return sendfreeOrg.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return sendfreeOrg.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "junk-beats-org",
    name: "Mailinator (junk.beats.org)",
    website: "mailinator.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return junkBeatsOrg.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return junkBeatsOrg.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "junk-ihmehl-com",
    name: "Mailinator (junk.ihmehl.com)",
    website: "mailinator.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return junkIhmehlCom.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return junkIhmehlCom.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "junk-noplay-org",
    name: "Mailinator (junk.noplay.org)",
    website: "mailinator.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return junkNoplayOrg.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return junkNoplayOrg.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "junk-vanillasystem-com",
    name: "Mailinator (junk.vanillasystem.com)",
    website: "mailinator.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return junkVanillasystemCom.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return junkVanillasystemCom.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "spam-jasonpearce-com",
    name: "Mailinator (spam.jasonpearce.com)",
    website: "mailinator.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return spamJasonpearceCom.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return spamJasonpearceCom.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "fish-skytale-net",
    name: "Mailinator (fish.skytale.net)",
    website: "mailinator.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return fishSkytaleNet.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return fishSkytaleNet.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "spam-mccrew-com",
    name: "Mailinator (spam.mccrew.com)",
    website: "mailinator.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return spamMccrewCom.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return spamMccrewCom.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "dropmail-click",
    name: "DropMail.click",
    website: "dropmail.click",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return dropmailClick.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return dropmailClick.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "spam-coroiu-com",
    name: "Mailinator (spam.coroiu.com)",
    website: "mailinator.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return spamCoroiuCom.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return spamCoroiuCom.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "spam-deluser-net",
    name: "Mailinator (spam.deluser.net)",
    website: "mailinator.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return spamDeluserNet.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return spamDeluserNet.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "spam-dhsf-net",
    name: "Mailinator (spam.dhsf.net)",
    website: "mailinator.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return spamDhsfNet.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return spamDhsfNet.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "spam-lucatnt-com",
    name: "Mailinator (spam.lucatnt.com)",
    website: "mailinator.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return spamLucatntCom.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return spamLucatntCom.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "spam-lyceum-life-com-ru",
    name: "Mailinator (spam.lyceum-life.com.ru)",
    website: "mailinator.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return spamLyceumLifeComRu.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return spamLyceumLifeComRu.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "spam-netpirates-net",
    name: "Mailinator (spam.netpirates.net)",
    website: "mailinator.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return spamNetpiratesNet.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return spamNetpiratesNet.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "spam-no-ip-net",
    name: "Mailinator (spam.no-ip.net)",
    website: "mailinator.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return spamNoIpNet.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return spamNoIpNet.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "spam-ozh-org",
    name: "Mailinator (spam.ozh.org)",
    website: "mailinator.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return spamOzhOrg.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return spamOzhOrg.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "spam-pyphus-org",
    name: "Mailinator (spam.pyphus.org)",
    website: "mailinator.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return spamPyphusOrg.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return spamPyphusOrg.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "spam-shep-pw",
    name: "Mailinator (spam.shep.pw)",
    website: "mailinator.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return spamShepPw.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return spamShepPw.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "spam-wtf-at",
    name: "Mailinator (spam.wtf.at)",
    website: "mailinator.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return spamWtfAt.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return spamWtfAt.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "spam-wulczer-org",
    name: "Mailinator (spam.wulczer.org)",
    website: "mailinator.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return spamWulczerOrg.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return spamWulczerOrg.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "crap-kakadua-net",
    name: "Mailinator (crap.kakadua.net)",
    website: "mailinator.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return crapKakaduaNet.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return crapKakaduaNet.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "spam-janlugt-nl",
    name: "Mailinator (spam.janlugt.nl)",
    website: "mailinator.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return spamJanlugtNl.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return spamJanlugtNl.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "min-burningfish-net",
    name: "Mailinator (min.burningfish.net)",
    website: "mailinator.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return minBurningfishNet.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return minBurningfishNet.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "sink-fblay-com",
    name: "Mailinator (sink.fblay.com)",
    website: "mailinator.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return sinkFblayCom.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return sinkFblayCom.getEmails(token || "", email);
    },
  });

  registerChannel({
    channel: "tempgmailer",
    name: "TempGmailer",
    website: "tempgmailer.com",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return tempgmailer.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token)
          throw new Error("internal error: token missing for tempgmailer");
        return tempgmailer.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "temp-mail-org",
    name: "Temp-Mail.org",
    website: "temp-mail.org",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return tempMailOrg.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token)
          throw new Error("internal error: token missing for temp-mail-org");
        return tempMailOrg.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "xkx-me",
    name: "XKX.me",
    website: "xkx.me",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return xkxMe.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token)
          throw new Error("internal error: token missing for xkx-me");
        return xkxMe.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "gonebox-email",
    name: "GoneBox Email",
    website: "gonebox.email",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return goneboxEmail.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return goneboxEmail.getEmails("", email);
    },
  });

  registerChannel({
    channel: "mailcat-ai",
    name: "MailCat AI",
    website: "mailcat.ai",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return mailcatAi.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token)
          throw new Error("internal error: token missing for mailcat-ai");
        return mailcatAi.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "tempgo-email",
    name: "TempGo Email",
    website: "tempgo.email",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return tempgoEmail.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token)
          throw new Error("internal error: token missing for tempgo-email");
        return tempgoEmail.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "restmail-net",
    name: "Restmail.net",
    website: "restmail.net",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return restmailNet.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        return restmailNet.getEmails("", email);
    },
  });

  registerChannel({
    channel: "dropmail-me",
    name: "DropMail.me",
    website: "dropmail.me",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return dropmailMe.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token)
          throw new Error("internal error: token missing for dropmail-me");
        return dropmailMe.getEmails(token, email);
    },
  });

  registerChannel({
    channel: "ten-minute-mail-net",
    name: "10 Minute Mail.net",
    website: "10minutemail.net",
    generate: (options: GenerateEmailOptions): Promise<InternalEmailInfo> => {
        return tenMinuteMailNet.generateEmail();
    },
    getEmails: (email: string, token?: string): Promise<Email[]> => {
        if (!token)
          throw new Error("internal error: token missing for ten-minute-mail-net");
        return tenMinuteMailNet.getEmails(token, email);
    },
  });
