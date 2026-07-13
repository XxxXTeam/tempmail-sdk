/**
 * 类型相关的工具函数
 */

#include "tempmail_internal.h"

/* 渠道名称字符串映射 */
static const char *channel_names[] = {
    "tempmail",
    "linshiyou",
    "tempmail-lol",
    "chatgpt-org-uk",
    "awamail",
    "mail-tm",
    "dropmail",
    "guerrillamail",
    "maildrop",
    "smail-pw",
    "catchmail-mailistry",
    "catchmail-zeppost",
    "vip-215",
    "mailforspam-tempmail-io",
    "fake-legal",
    "mffac",
    "tempmail-cn",
    "ta-easy",
    "mailforspam-disposable",
    "moakt",
    "10minute-one",
    "guerrillamail-com",
    "sharklasers-com",
    "email10min",
    "mjj-cm",
    "grr-la-com",
    "linshi-co",
    "harakirimail",
    "tempmail-plus",
    "tempmail-lol-v2",
    "sharklasers",
    "grr-la",
    "guerrillamail-info",
    "temp-mail-io",
    "mail-cx",
    "catchmail",
    "mailforspam",
    "tempmailc",
    "mailnesia",
    "throwawaymail",
    "inboxkitten",
    "getnada",
    "mail123",
    "spam4me",
    "guerrillamail-net",
    "guerrillamail-org",
    "guerrillamailblock",
    "guerrillamail-com-www",
    "1sec-mail",
    "fakemail",
    "openinbox",
    "inboxes",
    "uncorreotemporal",
    "jqkjqk-xyz",
    "imgui-de",
    "pulsewebmenu-de",
    "xghff-com",
    "oqqaj-com",
    "psovv-com",
    "dbwot-com",
    "ygwpr-com",
    "imxwe-com",
    "ddker-com",
    "web-library-net",
    "1vpn-net",
    "abematv-com",
    "abematv-net",
    "abematv-org",
    "aceh-cc",
    "bangkabelitung-net",
    "cctruyen-com",
    "getnada-com",
    "getnada-email",
    "getnada-net",
    "jawatengah-net",
    "jawatimur-net",
    "kalimantanbarat-net",
    "kalimantanselatan-net",
    "kalimantantengah-net",
    "kalimantantimur-net",
    "kalimantanutara-net",
    "kepulauanriau-net",
    "luxury345-com",
    "malukuutara-net",
    "nusatenggarabarat-net",
    "nusatenggaratimur-net",
    "papuabarat-net",
    "papuabaratdaya-net",
    "papuaselatan-net",
    "pehol-com",
    "ptruyen-com",
    "pulaubali-net",
    "riau-net",
    "seokey-org",
    "sulawesibarat-net",
    "sulawesiselatan-net",
    "sulawesitengah-net",
    "sulawesitenggara-net",
    "sumaterabarat-net",
    "sumateraselatan-net",
    "sumaterautara-net",
    "villatogel-com",
    "drmail-in",
    "teml-net",
    "tmpeml-com",
    "tmpbox-net",
    "moakt-cc",
    "disbox-net",
    "tmpmail-org",
    "tmpmail-net",
    "tmails-net",
    "disbox-org",
    "moakt-co",
    "moakt-ws",
    "tmail-ws",
    "bareed-ws",
    "fexpost-com",
    "fexbox-org",
    "mailbox-in-ua",
    "rover-info",
    "chitthi-in",
    "fextemp-com",
    "any-pink",
    "merepost-com",
    "tempgbox",
    "emailnator",
    "temporam",
    "lyhlevi-com",
    "mail10s",
    "webmailtemp",
    "tempfastmail",
    "shitty-email",
    "tempmailpro",
    "neighbours",
    "cleantempmail",
    "m2u",
    "tempy-email",
    "fmail",
    "ockito",
    "anonbox",
    "duckmail",
    "mailinator",
    "devmail-uk",
    "tempmail365",
    "tempinbox",
    "byom",
    "anonymmail",
    "eyepaste",
    "mail-sunls",
    "expressinboxhub",
    "lroid",
    "haribu",
    "rootsh",
    "fake-email-site",
    "mohmal",
    "mailgolem",
    "best-temp-mail",
    "disposablemail-app",
    "mailtemp-cc",
    "minuteinbox",
    "mailcatch",
    "tempemail-co",
    "tempemails-net",
    "altmails",
    "tempemail-info",
    "smailpro",
    "tempmailten",
    "maildrop-cc",
    "10minutemail-net",
    "linshiyouxiang-net",
    "tempmail-fyi",
    "disposablemail-com",
    "tempp-mails",
    "emailtemp-org",
    "mytempmail-cc",
    "temp-mail-now",
    "mail-td",
    "mailhole-de",
    "tmail-link",
    "24mail-chacuo",
    "nimail",
    "freecustom",
    "apihz",
    "sogetthis-com",
    "bobmail-info",
    "suremail-info",
    "binkmail-com",
    "veryrealemail-com",
    "mailmomy",
    "chammy-info",
    "thisisnotmyrealemail-com",
    "notmailinator-com",
    "spamhereplease-com",
    "sendspamhere-com",
    "sendfree-org",
    "junk-beats-org",
    "junk-ihmehl-com",
    "junk-noplay-org",
    "junk-vanillasystem-com",
    "spam-jasonpearce-com",
    "fish-skytale-net",
    "spam-mccrew-com",
    "dropmail-click",
    "spam-coroiu-com",
    "spam-deluser-net",
    "spam-dhsf-net",
    "spam-lucatnt-com",
    "spam-lyceum-life-com-ru",
    "spam-netpirates-net",
    "spam-no-ip-net",
    "spam-ozh-org",
    "spam-pyphus-org",
    "spam-shep-pw",
    "spam-wtf-at",
    "spam-wulczer-org",
    "crap-kakadua-net",
    "spam-janlugt-nl",
    "min-burningfish-net",
    "sink-fblay-com",
    "etgdev-de",
    "mtmdev-com",
    "test-unergie-com",
    "block-bdea-cc",
    "torch-yi-org",
    "carlton183-changeip-net",
    "mail-fsmash-org",
    "ebs-com-ar",
    "jama-trenet-eu",
    "blackhole-djurby-se",
    "m8r-davidfuhr-de",
    "mi-meon-be",
    "m-nik-me",
    "mail-bentrask-com",
    "t-zibet-net",
    "m8r-mcasal-com",
    "ramjane-mooo-com",
    "rauxa-seny-cat",
    "sp-woot-at",
    "fwd2m-eszett-es",
    "m-887-at",
    "nospam-thurstons-us",
    "null-k3vin-net",
    "really-istrash-com",
    "spam-hortuk-ovh",
    "16888888-cyou",
    "17666688-shop",
    "282mail-com",
    "bsdu32-buzz",
    "doxu243-buzz",
    "easyme-pro",
    "evergreenco-shop",
    "layueming-pics",
    "mingyuekeji-online",
    "mingyueming-click",
    "mingyueming-shop",
    "mingyukeji-lol",
    "nuxh62-space",
    "proid-cloud-ip-cc",
    "sbook-pics",
    "xue32-buzz",
    "b-smelly-cc",
    "dea-soon-it",
    "disposable-al-sudani-com",
    "disposable-nogonad-nl",
    "j-fairuse-org",
    "mn-curppa-com",
    "mailinatorzz-mooo-com",
    "notfond-404-mn",
};

const char *tm_channel_name(tm_channel_t channel) {
  if (channel >= 0 && channel < CHANNEL_COUNT) {
    return channel_names[channel];
  }
  return "unknown";
}

char *tm_strdup(const char *s) {
  if (!s) {
    char *empty = (char *)malloc(1);
    if (empty)
      empty[0] = '\0';
    return empty;
  }
  size_t len = strlen(s);
  char *dup = (char *)malloc(len + 1);
  if (dup) {
    memcpy(dup, s, len + 1);
  }
  return dup;
}

char *tm_json_get_str(const cJSON *obj, const char **keys, int key_count) {
  for (int i = 0; i < key_count; i++) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, keys[i]);
    if (cJSON_IsString(item) && item->valuestring) {
      return tm_strdup(item->valuestring);
    }
    if (cJSON_IsNumber(item)) {
      char buf[64];
      snprintf(buf, sizeof(buf), "%g", item->valuedouble);
      return tm_strdup(buf);
    }
  }
  return tm_strdup("");
}

tm_email_info_t *tm_email_info_new(void) {
  tm_email_info_t *info = (tm_email_info_t *)calloc(1, sizeof(tm_email_info_t));
  return info;
}

tm_email_t *tm_emails_new(int count) {
  if (count <= 0)
    return NULL;
  tm_email_t *emails = (tm_email_t *)calloc(count, sizeof(tm_email_t));
  return emails;
}

void tm_free_email_info(tm_email_info_t *info) {
  if (!info)
    return;
  free(info->email);
  free(info->token);
  free(info->created_at);
  free(info);
}

void tm_free_email(tm_email_t *email) {
  if (!email)
    return;
  free(email->id);
  free(email->from_addr);
  free(email->to);
  free(email->subject);
  free(email->text);
  free(email->html);
  free(email->date);
  if (email->attachments) {
    for (int i = 0; i < email->attachment_count; i++) {
      free(email->attachments[i].filename);
      free(email->attachments[i].content_type);
      free(email->attachments[i].url);
    }
    free(email->attachments);
  }
}

void tm_free_get_emails_result(tm_get_emails_result_t *result) {
  if (!result)
    return;
  free(result->email);
  free(result->error);
  if (result->emails) {
    for (int i = 0; i < result->email_count; i++) {
      tm_free_email(&result->emails[i]);
    }
    free(result->emails);
  }
  free(result);
}

tm_retry_config_t tm_default_retry_config(void) {
  tm_retry_config_t cfg;
  cfg.max_retries = 2;
  cfg.initial_delay_ms = 1000;
  cfg.max_delay_ms = 5000;
  cfg.timeout_secs = 15;
  return cfg;
}
