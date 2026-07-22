/**
 * 渠道注册表（有序）
 *
 * 设计目的：把"新增渠道需手动同步多处平行结构"收敛为"每渠道一处注册"。
 * 本文件用函数指针结构体数组复刻 Go 端 registry.go 的 ChannelSpec 精神
 * （C 无闭包，故每渠道用一个 static 包裹函数 thunk 把因渠道而异的 provider
 * 调用统一成固定签名）。
 *
 * 派生关系：
 *   - g_channel_registry[] 有序数组，顺序即 Go allChannels / listChannels 顺序
 *     （与 .baseline_channels.txt 逐行一致），也决定 tm_list_channels 的输出顺序。
 *   - tm_registry_find() 按 tm_channel_t 线性查找规格，供 client.c 分发调用。
 *   - name / website 直接内联到注册项，tm_list_channels 从本数组派生。
 *
 * 硬约束：注释与 slug 顺序须与 include/tempmail_sdk.h 的枚举、src/types.c 的
 * channel_names[] 对齐。thunk 内的 provider 调用（含固定域名、token 校验、
 * 参数顺序）逐字复刻自原 client.c 的两个 switch，行为零改动。
 */

#include "tempmail_internal.h"
#include "tempmail_registry.h"

/* ========== 创建邮箱 thunk（统一签名，逐字复刻原 switch 分支） ========== */

static tm_email_info_t *tm_reg_gen_0(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_TEMPMAIL */
  return tm_provider_tempmail_generate(ctx->duration);
}

static tm_email_info_t *tm_reg_gen_1(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_TEMPMAIL_CN */
  return tm_provider_tempmail_cn_generate(ctx->domain);
}

static tm_email_info_t *tm_reg_gen_2(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_LINSHIYOU */
  (void)ctx;
  return tm_provider_linshiyou_generate();
}

static tm_email_info_t *tm_reg_gen_3(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_TEMPMAIL_LOL */
  return tm_provider_tempmail_lol_generate(ctx->domain);
}

static tm_email_info_t *tm_reg_gen_4(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_CHATGPT_ORG_UK */
  (void)ctx;
  return tm_provider_chatgpt_org_uk_generate();
}

static tm_email_info_t *tm_reg_gen_5(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_AWAMAIL */
  (void)ctx;
  return tm_provider_awamail_generate();
}

static tm_email_info_t *tm_reg_gen_6(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_MAIL_TM */
  (void)ctx;
  return tm_provider_mail_tm_generate();
}

static tm_email_info_t *tm_reg_gen_7(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_DROPMAIL */
  (void)ctx;
  return tm_provider_dropmail_generate();
}

static tm_email_info_t *tm_reg_gen_8(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_GUERRILLAMAIL */
  (void)ctx;
  return tm_provider_guerrillamail_generate();
}

static tm_email_info_t *tm_reg_gen_9(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_MAILDROP */
  return tm_provider_maildrop_generate(ctx->domain);
}

static tm_email_info_t *tm_reg_gen_10(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_SMAIL_PW */
  (void)ctx;
  return tm_provider_smail_pw_generate();
}

static tm_email_info_t *tm_reg_gen_11(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_VIP_215 */
  (void)ctx;
  return tm_provider_vip215_generate();
}

static tm_email_info_t *tm_reg_gen_12(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_FAKE_LEGAL */
  return tm_provider_fake_legal_generate(ctx->domain);
}

static tm_email_info_t *tm_reg_gen_13(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_IMGUI_DE */
  (void)ctx;
  return tm_with_channel(tm_provider_fake_legal_generate("imgui.de"),
                               CHANNEL_IMGUI_DE);
}

static tm_email_info_t *tm_reg_gen_14(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_PULSEWEBMENU_DE */
  (void)ctx;
  return tm_with_channel(tm_provider_fake_legal_generate("pulsewebmenu.de"),
                          CHANNEL_PULSEWEBMENU_DE);
}

static tm_email_info_t *tm_reg_gen_15(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_MFFAC */
  (void)ctx;
  return tm_provider_mffac_generate();
}

static tm_email_info_t *tm_reg_gen_16(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_TA_EASY */
  (void)ctx;
  return tm_provider_ta_easy_generate();
}

static tm_email_info_t *tm_reg_gen_17(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_MOAKT */
  return tm_provider_moakt_generate(ctx->domain);
}

static tm_email_info_t *tm_reg_gen_18(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_DRMAIL_IN */
  (void)ctx;
  return tm_with_channel(tm_provider_moakt_generate("drmail.in"),
                               CHANNEL_DRMAIL_IN);
}

static tm_email_info_t *tm_reg_gen_19(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_TEML_NET */
  (void)ctx;
  return tm_with_channel(tm_provider_moakt_generate("teml.net"),
                               CHANNEL_TEML_NET);
}

static tm_email_info_t *tm_reg_gen_20(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_TMPEML_COM */
  (void)ctx;
  return tm_with_channel(tm_provider_moakt_generate("tmpeml.com"),
                               CHANNEL_TMPEML_COM);
}

static tm_email_info_t *tm_reg_gen_21(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_TMPBOX_NET */
  (void)ctx;
  return tm_with_channel(tm_provider_moakt_generate("tmpbox.net"),
                               CHANNEL_TMPBOX_NET);
}

static tm_email_info_t *tm_reg_gen_22(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_MOAKT_CC */
  (void)ctx;
  return tm_with_channel(tm_provider_moakt_generate("moakt.cc"),
                               CHANNEL_MOAKT_CC);
}

static tm_email_info_t *tm_reg_gen_23(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_DISBOX_NET */
  (void)ctx;
  return tm_with_channel(tm_provider_moakt_generate("disbox.net"),
                               CHANNEL_DISBOX_NET);
}

static tm_email_info_t *tm_reg_gen_24(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_TMPMAIL_ORG */
  (void)ctx;
  return tm_with_channel(tm_provider_moakt_generate("tmpmail.org"),
                               CHANNEL_TMPMAIL_ORG);
}

static tm_email_info_t *tm_reg_gen_25(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_TMPMAIL_NET */
  (void)ctx;
  return tm_with_channel(tm_provider_moakt_generate("tmpmail.net"),
                               CHANNEL_TMPMAIL_NET);
}

static tm_email_info_t *tm_reg_gen_26(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_TMAILS_NET */
  (void)ctx;
  return tm_with_channel(tm_provider_moakt_generate("tmails.net"),
                               CHANNEL_TMAILS_NET);
}

static tm_email_info_t *tm_reg_gen_27(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_DISBOX_ORG */
  (void)ctx;
  return tm_with_channel(tm_provider_moakt_generate("disbox.org"),
                               CHANNEL_DISBOX_ORG);
}

static tm_email_info_t *tm_reg_gen_28(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_MOAKT_CO */
  (void)ctx;
  return tm_with_channel(tm_provider_moakt_generate("moakt.co"),
                               CHANNEL_MOAKT_CO);
}

static tm_email_info_t *tm_reg_gen_29(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_MOAKT_WS */
  (void)ctx;
  return tm_with_channel(tm_provider_moakt_generate("moakt.ws"),
                               CHANNEL_MOAKT_WS);
}

static tm_email_info_t *tm_reg_gen_30(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_TMAIL_WS */
  (void)ctx;
  return tm_with_channel(tm_provider_moakt_generate("tmail.ws"),
                               CHANNEL_TMAIL_WS);
}

static tm_email_info_t *tm_reg_gen_31(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_BAREED_WS */
  (void)ctx;
  return tm_with_channel(tm_provider_moakt_generate("bareed.ws"),
                               CHANNEL_BAREED_WS);
}

static tm_email_info_t *tm_reg_gen_32(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_10MINUTE_ONE */
  return tm_provider_tenminute_one_generate(ctx->domain);
}

static tm_email_info_t *tm_reg_gen_33(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_XGHFF_COM */
  (void)ctx;
  return tm_with_channel(tm_provider_tenminute_one_generate("xghff.com"),
                               CHANNEL_XGHFF_COM);
}

static tm_email_info_t *tm_reg_gen_34(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_OQQAJ_COM */
  (void)ctx;
  return tm_with_channel(tm_provider_tenminute_one_generate("oqqaj.com"),
                               CHANNEL_OQQAJ_COM);
}

static tm_email_info_t *tm_reg_gen_35(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_PSOVV_COM */
  (void)ctx;
  return tm_with_channel(tm_provider_tenminute_one_generate("psovv.com"),
                               CHANNEL_PSOVV_COM);
}

static tm_email_info_t *tm_reg_gen_36(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_DBWOT_COM */
  (void)ctx;
  return tm_with_channel(tm_provider_tenminute_one_generate("dbwot.com"),
                               CHANNEL_DBWOT_COM);
}

static tm_email_info_t *tm_reg_gen_37(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_YGWPR_COM */
  (void)ctx;
  return tm_with_channel(tm_provider_tenminute_one_generate("ygwpr.com"),
                               CHANNEL_YGWPR_COM);
}

static tm_email_info_t *tm_reg_gen_38(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_IMXWE_COM */
  (void)ctx;
  return tm_with_channel(tm_provider_tenminute_one_generate("imxwe.com"),
                               CHANNEL_IMXWE_COM);
}

static tm_email_info_t *tm_reg_gen_39(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_EMAIL10MIN */
  (void)ctx;
  return tm_provider_email10min_generate();
}

static tm_email_info_t *tm_reg_gen_40(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_MJJ_CM */
  (void)ctx;
  return NULL;
}

static tm_email_info_t *tm_reg_gen_41(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_LINSHI_CO */
  (void)ctx;
  return NULL;
}

static tm_email_info_t *tm_reg_gen_42(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_HARAKIRIMAIL */
  (void)ctx;
  return tm_provider_harakirimail_generate();
}

static tm_email_info_t *tm_reg_gen_43(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_JQKJQK_XYZ */
  (void)ctx;
  return tm_provider_zhujump_generate("jqkjqk.xyz", CHANNEL_JQKJQK_XYZ);
}

static tm_email_info_t *tm_reg_gen_44(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_LYHLEVI_COM */
  (void)ctx;
  return tm_provider_moemail_generate("https://lyhlevi.com",
                                            "lyhlevi.com", CHANNEL_LYHLEVI_COM,
                                            24 * 60 * 60 * 1000);
}

static tm_email_info_t *tm_reg_gen_45(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_TEMPMAIL_PLUS */
  (void)ctx;
  return tm_provider_tempmail_plus_generate(NULL, CHANNEL_TEMPMAIL_PLUS);
}

static tm_email_info_t *tm_reg_gen_46(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_FEXPOST_COM */
  (void)ctx;
  return tm_provider_tempmail_plus_generate("fexpost.com",
                                                  CHANNEL_FEXPOST_COM);
}

static tm_email_info_t *tm_reg_gen_47(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_FEXBOX_ORG */
  (void)ctx;
  return tm_provider_tempmail_plus_generate("fexbox.org", CHANNEL_FEXBOX_ORG);
}

static tm_email_info_t *tm_reg_gen_48(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_MAILBOX_IN_UA */
  (void)ctx;
  return tm_provider_tempmail_plus_generate("mailbox.in.ua",
                                                  CHANNEL_MAILBOX_IN_UA);
}

static tm_email_info_t *tm_reg_gen_49(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_ROVER_INFO */
  (void)ctx;
  return tm_provider_tempmail_plus_generate("rover.info", CHANNEL_ROVER_INFO);
}

static tm_email_info_t *tm_reg_gen_50(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_CHITTHI_IN */
  (void)ctx;
  return tm_provider_tempmail_plus_generate("chitthi.in", CHANNEL_CHITTHI_IN);
}

static tm_email_info_t *tm_reg_gen_51(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_FEXTEMP_COM */
  (void)ctx;
  return tm_provider_tempmail_plus_generate("fextemp.com",
                                                  CHANNEL_FEXTEMP_COM);
}

static tm_email_info_t *tm_reg_gen_52(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_ANY_PINK */
  (void)ctx;
  return tm_provider_tempmail_plus_generate("any.pink", CHANNEL_ANY_PINK);
}

static tm_email_info_t *tm_reg_gen_53(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_MEREPOST_COM */
  (void)ctx;
  return tm_provider_tempmail_plus_generate("merepost.com",
                                                  CHANNEL_MEREPOST_COM);
}

static tm_email_info_t *tm_reg_gen_54(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_TEMPMAIL_LOL_V2 */
  (void)ctx;
  return tm_provider_tempmail_lol_v2_generate();
}

static tm_email_info_t *tm_reg_gen_55(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_TEMPGBOX */
  (void)ctx;
  return tm_provider_tempgbox_generate();
}

static tm_email_info_t *tm_reg_gen_56(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_EMAILNATOR */
  (void)ctx;
  return tm_provider_emailnator_generate();
}

static tm_email_info_t *tm_reg_gen_57(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_TEMPORAM */
  return tm_provider_temporam_generate(ctx->domain);
}

static tm_email_info_t *tm_reg_gen_58(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_NEIGHBOURS */
  return tm_provider_neighbours_generate(ctx->domain);
}

static tm_email_info_t *tm_reg_gen_59(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_M2U */
  (void)ctx;
  return tm_provider_m2u_generate();
}

static tm_email_info_t *tm_reg_gen_60(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_TEMPY_EMAIL */
  return tm_provider_tempy_email_generate(ctx->domain);
}

static tm_email_info_t *tm_reg_gen_61(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_FMAIL */
  return tm_provider_fmail_generate(ctx->domain);
}

static tm_email_info_t *tm_reg_gen_62(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_OCKITO */
  (void)ctx;
  return tm_provider_ockito_generate();
}

static tm_email_info_t *tm_reg_gen_63(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_ANONBOX */
  (void)ctx;
  return tm_provider_anonbox_generate();
}

static tm_email_info_t *tm_reg_gen_64(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_DUCKMAIL */
  (void)ctx;
  return tm_provider_duckmail_generate();
}

static tm_email_info_t *tm_reg_gen_65(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_MAILINATOR */
  (void)ctx;
  return tm_provider_mailinator_generate();
}

static tm_email_info_t *tm_reg_gen_66(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_TEMPMAIL365 */
  return tm_tempmail365_generate(ctx->domain);
}

static tm_email_info_t *tm_reg_gen_67(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_TEMPINBOX */
  return tm_provider_tempinbox_generate(ctx->domain);
}

static tm_email_info_t *tm_reg_gen_68(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_SHARKLASERS */
  (void)ctx;
  return tm_provider_guerrillamail_mirror_generate(
          CHANNEL_SHARKLASERS, "https://www.sharklasers.com/ajax.php");
}

static tm_email_info_t *tm_reg_gen_69(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_SHARKLASERS_COM */
  (void)ctx;
  return tm_provider_guerrillamail_mirror_generate(
          CHANNEL_SHARKLASERS_COM, "https://sharklasers.com/ajax.php");
}

static tm_email_info_t *tm_reg_gen_70(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_GRR_LA */
  (void)ctx;
  return tm_provider_guerrillamail_mirror_generate(
          CHANNEL_GRR_LA, "https://www.grr.la/ajax.php");
}

static tm_email_info_t *tm_reg_gen_71(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_GRR_LA_COM */
  (void)ctx;
  return tm_provider_guerrillamail_mirror_generate(
          CHANNEL_GRR_LA_COM, "https://grr.la/ajax.php");
}

static tm_email_info_t *tm_reg_gen_72(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_GUERRILLAMAIL_INFO */
  (void)ctx;
  return tm_provider_guerrillamail_mirror_generate(
          CHANNEL_GUERRILLAMAIL_INFO,
          "https://www.guerrillamail.info/ajax.php");
}

static tm_email_info_t *tm_reg_gen_73(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_GUERRILLAMAIL_COM */
  (void)ctx;
  return tm_provider_guerrillamail_mirror_generate(
          CHANNEL_GUERRILLAMAIL_COM, "https://guerrillamail.com/ajax.php");
}

static tm_email_info_t *tm_reg_gen_74(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_SPAM4ME */
  (void)ctx;
  return tm_provider_guerrillamail_mirror_generate(
          CHANNEL_SPAM4ME, "https://www.spam4.me/ajax.php");
}

static tm_email_info_t *tm_reg_gen_75(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_GUERRILLAMAIL_NET */
  (void)ctx;
  return tm_provider_guerrillamail_mirror_generate(
          CHANNEL_GUERRILLAMAIL_NET, "https://www.guerrillamail.net/ajax.php");
}

static tm_email_info_t *tm_reg_gen_76(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_GUERRILLAMAIL_ORG */
  (void)ctx;
  return tm_provider_guerrillamail_mirror_generate(
          CHANNEL_GUERRILLAMAIL_ORG, "https://www.guerrillamail.org/ajax.php");
}

static tm_email_info_t *tm_reg_gen_77(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_GUERRILLAMAILBLOCK */
  (void)ctx;
  return tm_provider_guerrillamail_mirror_generate(
          CHANNEL_GUERRILLAMAILBLOCK,
          "https://www.guerrillamailblock.com/ajax.php");
}

static tm_email_info_t *tm_reg_gen_78(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_GUERRILLAMAIL_COM_WWW */
  (void)ctx;
  return tm_provider_guerrillamail_mirror_generate(
          CHANNEL_GUERRILLAMAIL_COM_WWW,
          "https://www.guerrillamail.com/ajax.php");
}

static tm_email_info_t *tm_reg_gen_79(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_TEMP_MAIL_IO */
  (void)ctx;
  return tm_provider_temp_mail_io_generate();
}

static tm_email_info_t *tm_reg_gen_80(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_MAIL_CX */
  return tm_provider_mail_cx_generate(ctx->domain);
}

static tm_email_info_t *tm_reg_gen_81(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_DDKER_COM */
  (void)ctx;
  return tm_with_channel(tm_provider_mail_cx_generate("ddker.com"),
                               CHANNEL_DDKER_COM);
}

static tm_email_info_t *tm_reg_gen_82(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_CATCHMAIL */
  return tm_provider_catchmail_generate(ctx->domain);
}

static tm_email_info_t *tm_reg_gen_83(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_CATCHMAIL_MAILISTRY */
  (void)ctx;
  return tm_with_channel(tm_provider_catchmail_generate("mailistry.com"),
                               CHANNEL_CATCHMAIL_MAILISTRY);
}

static tm_email_info_t *tm_reg_gen_84(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_CATCHMAIL_ZEPPOST */
  (void)ctx;
  return tm_with_channel(tm_provider_catchmail_generate("zeppost.com"),
                               CHANNEL_CATCHMAIL_ZEPPOST);
}

static tm_email_info_t *tm_reg_gen_85(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_MAILFORSPAM */
  return tm_provider_mailforspam_generate(ctx->domain);
}

static tm_email_info_t *tm_reg_gen_86(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_MAILFORSPAM_TEMPMAIL_IO */
  (void)ctx;
  return tm_with_channel(tm_provider_mailforspam_generate("tempmail.io"),
                               CHANNEL_MAILFORSPAM_TEMPMAIL_IO);
}

static tm_email_info_t *tm_reg_gen_87(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_MAILFORSPAM_DISPOSABLE */
  (void)ctx;
  return tm_with_channel(tm_provider_mailforspam_generate("disposable.email"),
                          CHANNEL_MAILFORSPAM_DISPOSABLE);
}

static tm_email_info_t *tm_reg_gen_88(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_TEMPMAILC */
  (void)ctx;
  return tm_provider_tempmailc_generate();
}

static tm_email_info_t *tm_reg_gen_89(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_MAILNESIA */
  (void)ctx;
  return tm_provider_mailnesia_generate();
}

static tm_email_info_t *tm_reg_gen_90(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_THROWAWAYMAIL */
  (void)ctx;
  return tm_provider_throwawaymail_generate();
}

static tm_email_info_t *tm_reg_gen_91(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_TEMPMAIL_FISH */
  (void)ctx;
  return tm_provider_tempmail_fish_generate();
}

static tm_email_info_t *tm_reg_gen_92(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_NEIGHBOURS_SH */
  (void)ctx;
  return tm_provider_neighbours_sh_generate();
}

static tm_email_info_t *tm_reg_gen_93(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_SHITTY_EMAIL */
  (void)ctx;
  return tm_provider_shitty_email_generate();
}

static tm_email_info_t *tm_reg_gen_94(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_TEMPMAILPRO */
  (void)ctx;
  return tm_provider_tempmailpro_generate();
}

static tm_email_info_t *tm_reg_gen_95(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_DEVMAIL_UK */
  (void)ctx;
  return tm_provider_devmail_uk_generate();
}

static tm_email_info_t *tm_reg_gen_96(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_CLEANTEMPMAIL */
  (void)ctx;
  return tm_provider_cleantempmail_generate();
}

static tm_email_info_t *tm_reg_gen_97(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_INBOXKITTEN */
  (void)ctx;
  return tm_provider_inboxkitten_generate();
}

static tm_email_info_t *tm_reg_gen_98(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_GETNADA */
  return tm_provider_getnada_generate(ctx->domain);
}

static tm_email_info_t *tm_reg_gen_99(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_ONE_VPN_NET */
  (void)ctx;
  return tm_with_channel(tm_provider_getnada_generate("1vpn.net"),
                               CHANNEL_ONE_VPN_NET);
}

static tm_email_info_t *tm_reg_gen_100(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_ABEMATV_COM */
  (void)ctx;
  return tm_with_channel(tm_provider_getnada_generate("abematv.com"),
                               CHANNEL_ABEMATV_COM);
}

static tm_email_info_t *tm_reg_gen_101(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_ABEMATV_NET */
  (void)ctx;
  return tm_with_channel(tm_provider_getnada_generate("abematv.net"),
                               CHANNEL_ABEMATV_NET);
}

static tm_email_info_t *tm_reg_gen_102(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_ABEMATV_ORG */
  (void)ctx;
  return tm_with_channel(tm_provider_getnada_generate("abematv.org"),
                               CHANNEL_ABEMATV_ORG);
}

static tm_email_info_t *tm_reg_gen_103(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_ACEH_CC */
  (void)ctx;
  return tm_with_channel(tm_provider_getnada_generate("aceh.cc"),
                               CHANNEL_ACEH_CC);
}

static tm_email_info_t *tm_reg_gen_104(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_BANGKABELITUNG_NET */
  (void)ctx;
  return tm_with_channel(tm_provider_getnada_generate("bangkabelitung.net"),
                          CHANNEL_BANGKABELITUNG_NET);
}

static tm_email_info_t *tm_reg_gen_105(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_CCTRUYEN_COM */
  (void)ctx;
  return tm_with_channel(tm_provider_getnada_generate("cctruyen.com"),
                               CHANNEL_CCTRUYEN_COM);
}

static tm_email_info_t *tm_reg_gen_106(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_GETNADA_COM */
  (void)ctx;
  return tm_with_channel(tm_provider_getnada_generate("getnada.com"),
                               CHANNEL_GETNADA_COM);
}

static tm_email_info_t *tm_reg_gen_107(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_GETNADA_EMAIL */
  (void)ctx;
  return tm_with_channel(tm_provider_getnada_generate("getnada.email"),
                               CHANNEL_GETNADA_EMAIL);
}

static tm_email_info_t *tm_reg_gen_108(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_GETNADA_NET */
  (void)ctx;
  return tm_with_channel(tm_provider_getnada_generate("getnada.net"),
                               CHANNEL_GETNADA_NET);
}

static tm_email_info_t *tm_reg_gen_109(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_JAWATENGAH_NET */
  (void)ctx;
  return tm_with_channel(tm_provider_getnada_generate("jawatengah.net"),
                               CHANNEL_JAWATENGAH_NET);
}

static tm_email_info_t *tm_reg_gen_110(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_JAWATIMUR_NET */
  (void)ctx;
  return tm_with_channel(tm_provider_getnada_generate("jawatimur.net"),
                               CHANNEL_JAWATIMUR_NET);
}

static tm_email_info_t *tm_reg_gen_111(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_KALIMANTANBARAT_NET */
  (void)ctx;
  return tm_with_channel(tm_provider_getnada_generate("kalimantanbarat.net"),
                          CHANNEL_KALIMANTANBARAT_NET);
}

static tm_email_info_t *tm_reg_gen_112(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_KALIMANTANSELATAN_NET */
  (void)ctx;
  return tm_with_channel(tm_provider_getnada_generate("kalimantanselatan.net"),
                          CHANNEL_KALIMANTANSELATAN_NET);
}

static tm_email_info_t *tm_reg_gen_113(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_KALIMANTANTENGAH_NET */
  (void)ctx;
  return tm_with_channel(tm_provider_getnada_generate("kalimantantengah.net"),
                          CHANNEL_KALIMANTANTENGAH_NET);
}

static tm_email_info_t *tm_reg_gen_114(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_KALIMANTANTIMUR_NET */
  (void)ctx;
  return tm_with_channel(tm_provider_getnada_generate("kalimantantimur.net"),
                          CHANNEL_KALIMANTANTIMUR_NET);
}

static tm_email_info_t *tm_reg_gen_115(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_KALIMANTANUTARA_NET */
  (void)ctx;
  return tm_with_channel(tm_provider_getnada_generate("kalimantanutara.net"),
                          CHANNEL_KALIMANTANUTARA_NET);
}

static tm_email_info_t *tm_reg_gen_116(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_KEPULAUANRIAU_NET */
  (void)ctx;
  return tm_with_channel(tm_provider_getnada_generate("kepulauanriau.net"),
                          CHANNEL_KEPULAUANRIAU_NET);
}

static tm_email_info_t *tm_reg_gen_117(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_LUXURY345_COM */
  (void)ctx;
  return tm_with_channel(tm_provider_getnada_generate("luxury345.com"),
                               CHANNEL_LUXURY345_COM);
}

static tm_email_info_t *tm_reg_gen_118(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_MALUKUUTARA_NET */
  (void)ctx;
  return tm_with_channel(tm_provider_getnada_generate("malukuutara.net"),
                               CHANNEL_MALUKUUTARA_NET);
}

static tm_email_info_t *tm_reg_gen_119(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_NUSATENGGARABARAT_NET */
  (void)ctx;
  return tm_with_channel(tm_provider_getnada_generate("nusatenggarabarat.net"),
                          CHANNEL_NUSATENGGARABARAT_NET);
}

static tm_email_info_t *tm_reg_gen_120(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_NUSATENGGARATIMUR_NET */
  (void)ctx;
  return tm_with_channel(tm_provider_getnada_generate("nusatenggaratimur.net"),
                          CHANNEL_NUSATENGGARATIMUR_NET);
}

static tm_email_info_t *tm_reg_gen_121(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_PAPUABARAT_NET */
  (void)ctx;
  return tm_with_channel(tm_provider_getnada_generate("papuabarat.net"),
                               CHANNEL_PAPUABARAT_NET);
}

static tm_email_info_t *tm_reg_gen_122(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_PAPUABARATDAYA_NET */
  (void)ctx;
  return tm_with_channel(tm_provider_getnada_generate("papuabaratdaya.net"),
                          CHANNEL_PAPUABARATDAYA_NET);
}

static tm_email_info_t *tm_reg_gen_123(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_PAPUASELATAN_NET */
  (void)ctx;
  return tm_with_channel(tm_provider_getnada_generate("papuaselatan.net"),
                               CHANNEL_PAPUASELATAN_NET);
}

static tm_email_info_t *tm_reg_gen_124(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_PEHOL_COM */
  (void)ctx;
  return tm_with_channel(tm_provider_getnada_generate("pehol.com"),
                               CHANNEL_PEHOL_COM);
}

static tm_email_info_t *tm_reg_gen_125(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_PTRUYEN_COM */
  (void)ctx;
  return tm_with_channel(tm_provider_getnada_generate("ptruyen.com"),
                               CHANNEL_PTRUYEN_COM);
}

static tm_email_info_t *tm_reg_gen_126(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_PULAUBALI_NET */
  (void)ctx;
  return tm_with_channel(tm_provider_getnada_generate("pulaubali.net"),
                               CHANNEL_PULAUBALI_NET);
}

static tm_email_info_t *tm_reg_gen_127(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_RIAU_NET */
  (void)ctx;
  return tm_with_channel(tm_provider_getnada_generate("riau.net"),
                               CHANNEL_RIAU_NET);
}

static tm_email_info_t *tm_reg_gen_128(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_SEOKEY_ORG */
  (void)ctx;
  return tm_with_channel(tm_provider_getnada_generate("seokey.org"),
                               CHANNEL_SEOKEY_ORG);
}

static tm_email_info_t *tm_reg_gen_129(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_SULAWESIBARAT_NET */
  (void)ctx;
  return tm_with_channel(tm_provider_getnada_generate("sulawesibarat.net"),
                          CHANNEL_SULAWESIBARAT_NET);
}

static tm_email_info_t *tm_reg_gen_130(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_SULAWESISELATAN_NET */
  (void)ctx;
  return tm_with_channel(tm_provider_getnada_generate("sulawesiselatan.net"),
                          CHANNEL_SULAWESISELATAN_NET);
}

static tm_email_info_t *tm_reg_gen_131(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_SULAWESITENGAH_NET */
  (void)ctx;
  return tm_with_channel(tm_provider_getnada_generate("sulawesitengah.net"),
                          CHANNEL_SULAWESITENGAH_NET);
}

static tm_email_info_t *tm_reg_gen_132(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_SULAWESITENGGARA_NET */
  (void)ctx;
  return tm_with_channel(tm_provider_getnada_generate("sulawesitenggara.net"),
                          CHANNEL_SULAWESITENGGARA_NET);
}

static tm_email_info_t *tm_reg_gen_133(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_SUMATERABARAT_NET */
  (void)ctx;
  return tm_with_channel(tm_provider_getnada_generate("sumaterabarat.net"),
                          CHANNEL_SUMATERABARAT_NET);
}

static tm_email_info_t *tm_reg_gen_134(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_SUMATERASELATAN_NET */
  (void)ctx;
  return tm_with_channel(tm_provider_getnada_generate("sumateraselatan.net"),
                          CHANNEL_SUMATERASELATAN_NET);
}

static tm_email_info_t *tm_reg_gen_135(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_SUMATERAUTARA_NET */
  (void)ctx;
  return tm_with_channel(tm_provider_getnada_generate("sumaterautara.net"),
                          CHANNEL_SUMATERAUTARA_NET);
}

static tm_email_info_t *tm_reg_gen_136(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_VILLATOGEL_COM */
  (void)ctx;
  return tm_with_channel(tm_provider_getnada_generate("villatogel.com"),
                               CHANNEL_VILLATOGEL_COM);
}

static tm_email_info_t *tm_reg_gen_137(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_MAIL123 */
  (void)ctx;
  return tm_provider_mail123_generate();
}

static tm_email_info_t *tm_reg_gen_138(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_MAIL10S */
  (void)ctx;
  return tm_provider_mail10s_generate();
}

static tm_email_info_t *tm_reg_gen_139(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_WEBMAILTEMP */
  (void)ctx;
  return tm_provider_webmailtemp_generate();
}

static tm_email_info_t *tm_reg_gen_140(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_TEMPFASTMAIL */
  (void)ctx;
  return tm_provider_tempfastmail_generate();
}

static tm_email_info_t *tm_reg_gen_141(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_ONE_SEC_MAIL */
  (void)ctx;
  return tm_provider_one_sec_mail_generate();
}

static tm_email_info_t *tm_reg_gen_142(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_FAKEMAIL */
  (void)ctx;
  return tm_provider_fakemail_generate();
}

static tm_email_info_t *tm_reg_gen_143(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_OPENINBOX */
  (void)ctx;
  return tm_provider_openinbox_generate();
}

static tm_email_info_t *tm_reg_gen_144(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_INBOXES */
  return tm_provider_inboxes_generate(ctx->domain);
}

static tm_email_info_t *tm_reg_gen_145(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_UNCORREOTEMPORAL */
  (void)ctx;
  return tm_provider_uncorreotemporal_generate();
}

static tm_email_info_t *tm_reg_gen_146(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_WEB_LIBRARY_NET */
  (void)ctx;
  return tm_with_channel(tm_provider_mail_tm_generate(),
                               CHANNEL_WEB_LIBRARY_NET);
}

static tm_email_info_t *tm_reg_gen_147(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_BYOM */
  (void)ctx;
  return tm_provider_byom_generate();
}

static tm_email_info_t *tm_reg_gen_148(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_ANONYMMAIL */
  (void)ctx;
  return tm_provider_anonymmail_generate();
}

static tm_email_info_t *tm_reg_gen_149(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_EYEPASTE */
  (void)ctx;
  return tm_provider_eyepaste_generate();
}

static tm_email_info_t *tm_reg_gen_150(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_MAIL_SUNLS */
  (void)ctx;
  return tm_provider_mail_sunls_generate();
}

static tm_email_info_t *tm_reg_gen_151(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_EXPRESSINBOXHUB */
  (void)ctx;
  return tm_provider_expressinboxhub_generate();
}

static tm_email_info_t *tm_reg_gen_152(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_LROID */
  (void)ctx;
  return tm_provider_lroid_generate();
}

static tm_email_info_t *tm_reg_gen_153(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_HARIBU */
  (void)ctx;
  return tm_provider_haribu_generate();
}

static tm_email_info_t *tm_reg_gen_154(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_ROOTSH */
  (void)ctx;
  return tm_provider_rootsh_generate();
}

static tm_email_info_t *tm_reg_gen_155(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_FAKE_EMAIL_SITE */
  (void)ctx;
  return tm_provider_fake_email_site_generate();
}

static tm_email_info_t *tm_reg_gen_156(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_MOHMAL */
  (void)ctx;
  return tm_provider_mohmal_generate();
}

static tm_email_info_t *tm_reg_gen_157(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_MAILGOLEM */
  (void)ctx;
  return tm_provider_mailgolem_generate();
}

static tm_email_info_t *tm_reg_gen_158(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_BEST_TEMP_MAIL */
  (void)ctx;
  return tm_provider_best_temp_mail_generate();
}

static tm_email_info_t *tm_reg_gen_159(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_DISPOSABLEMAIL_APP */
  (void)ctx;
  return tm_provider_disposablemail_app_generate();
}

static tm_email_info_t *tm_reg_gen_160(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_MAILTEMP_CC */
  (void)ctx;
  return tm_provider_mailtemp_cc_generate();
}

static tm_email_info_t *tm_reg_gen_161(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_MINUTEINBOX */
  (void)ctx;
  return tm_provider_minuteinbox_generate();
}

static tm_email_info_t *tm_reg_gen_162(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_MAILCATCH */
  (void)ctx;
  return NULL;
}

static tm_email_info_t *tm_reg_gen_163(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_TEMPEMAIL_CO */
  (void)ctx;
  return tm_provider_tempemail_co_generate();
}

static tm_email_info_t *tm_reg_gen_164(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_TEMPEMAILS_NET */
  (void)ctx;
  return tm_provider_tempemails_net_generate();
}

static tm_email_info_t *tm_reg_gen_165(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_ALTMAILS */
  (void)ctx;
  return tm_provider_altmails_generate();
}

static tm_email_info_t *tm_reg_gen_166(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_TEMPEMAIL_INFO */
  (void)ctx;
  return tm_provider_tempemail_info_generate();
}

static tm_email_info_t *tm_reg_gen_167(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_SMAILPRO */
  (void)ctx;
  return tm_provider_smailpro_generate();
}

static tm_email_info_t *tm_reg_gen_168(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_TEMPMAILTEN */
  (void)ctx;
  return tm_provider_tempmailten_generate();
}

static tm_email_info_t *tm_reg_gen_169(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_MAILDROP_CC */
  (void)ctx;
  return tm_provider_maildrop_cc_generate();
}

static tm_email_info_t *tm_reg_gen_170(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_TENMINUTEMAIL_NET */
  (void)ctx;
  return tm_provider_tenminutemail_net_generate();
}

static tm_email_info_t *tm_reg_gen_171(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_LINSHIYOUXIANG_NET */
  (void)ctx;
  return tm_provider_linshiyouxiang_net_generate();
}

static tm_email_info_t *tm_reg_gen_172(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_TEMPMAIL_FYI */
  (void)ctx;
  return tm_provider_tempmail_fyi_generate();
}

static tm_email_info_t *tm_reg_gen_173(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_DISPOSABLEMAIL_COM */
  (void)ctx;
  return tm_provider_disposablemail_com_generate();
}

static tm_email_info_t *tm_reg_gen_174(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_TEMPP_MAILS */
  (void)ctx;
  return tm_provider_tempp_mails_generate();
}

static tm_email_info_t *tm_reg_gen_175(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_EMAILTEMP_ORG */
  (void)ctx;
  return tm_provider_emailtemp_org_generate();
}

static tm_email_info_t *tm_reg_gen_176(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_MYTEMPMAIL_CC */
  (void)ctx;
  return tm_provider_mytempmail_cc_generate();
}

static tm_email_info_t *tm_reg_gen_177(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_TEMP_MAIL_NOW */
  (void)ctx;
  return tm_provider_temp_mail_now_generate();
}

static tm_email_info_t *tm_reg_gen_178(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_MAIL_TD */
  (void)ctx;
  return tm_provider_mail_td_generate();
}

static tm_email_info_t *tm_reg_gen_179(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_MAILHOLE_DE */
  (void)ctx;
  return tm_provider_mailhole_de_generate();
}

static tm_email_info_t *tm_reg_gen_180(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_TMAIL_LINK */
  (void)ctx;
  return tm_provider_tmail_link_generate();
}

static tm_email_info_t *tm_reg_gen_181(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_24MAIL_CHACUO */
  (void)ctx;
  return tm_provider_24mail_chacuo_generate();
}

static tm_email_info_t *tm_reg_gen_182(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_NIMAIL */
  (void)ctx;
  return tm_provider_nimail_generate();
}

static tm_email_info_t *tm_reg_gen_183(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_FREECUSTOM */
  (void)ctx;
  return tm_provider_freecustom_generate();
}

static tm_email_info_t *tm_reg_gen_184(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_APIHZ */
  (void)ctx;
  return tm_provider_apihz_generate();
}

static tm_email_info_t *tm_reg_gen_185(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_SOGETTHIS_COM */
  (void)ctx;
  return tm_provider_sogetthis_com_generate();
}

static tm_email_info_t *tm_reg_gen_186(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_BOBMAIL_INFO */
  (void)ctx;
  return tm_provider_bobmail_info_generate();
}

static tm_email_info_t *tm_reg_gen_187(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_SUREMAIL_INFO */
  (void)ctx;
  return tm_provider_suremail_info_generate();
}

static tm_email_info_t *tm_reg_gen_188(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_BINKMAIL_COM */
  (void)ctx;
  return tm_provider_binkmail_com_generate();
}

static tm_email_info_t *tm_reg_gen_189(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_VERYREALEMAIL_COM */
  (void)ctx;
  return tm_provider_veryrealemail_com_generate();
}

static tm_email_info_t *tm_reg_gen_190(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_MAILMOMY */
  (void)ctx;
  return tm_provider_mailmomy_generate();
}

static tm_email_info_t *tm_reg_gen_191(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_CHAMMY_INFO */
  (void)ctx;
  return tm_provider_chammy_info_generate();
}

static tm_email_info_t *tm_reg_gen_192(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_THISISNOTMYREALEMAIL_COM */
  (void)ctx;
  return tm_provider_thisisnotmyrealemail_com_generate();
}

static tm_email_info_t *tm_reg_gen_193(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_NOTMAILINATOR_COM */
  (void)ctx;
  return tm_provider_notmailinator_com_generate();
}

static tm_email_info_t *tm_reg_gen_194(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_SPAMHEREPLEASE_COM */
  (void)ctx;
  return tm_provider_spamhereplease_com_generate();
}

static tm_email_info_t *tm_reg_gen_195(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_SENDSPAMHERE_COM */
  (void)ctx;
  return tm_provider_sendspamhere_com_generate();
}

static tm_email_info_t *tm_reg_gen_196(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_SENDFREE_ORG */
  (void)ctx;
  return tm_provider_sendfree_org_generate();
}

static tm_email_info_t *tm_reg_gen_197(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_JUNK_BEATS_ORG */
  (void)ctx;
  return tm_provider_junk_beats_org_generate();
}

static tm_email_info_t *tm_reg_gen_198(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_JUNK_IHMEHL_COM */
  (void)ctx;
  return tm_provider_junk_ihmehl_com_generate();
}

static tm_email_info_t *tm_reg_gen_199(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_JUNK_NOPLAY_ORG */
  (void)ctx;
  return tm_provider_junk_noplay_org_generate();
}

static tm_email_info_t *tm_reg_gen_200(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_JUNK_VANILLASYSTEM_COM */
  (void)ctx;
  return tm_provider_junk_vanillasystem_com_generate();
}

static tm_email_info_t *tm_reg_gen_201(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_SPAM_JASONPEARCE_COM */
  (void)ctx;
  return tm_provider_spam_jasonpearce_com_generate();
}

static tm_email_info_t *tm_reg_gen_202(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_FISH_SKYTALE_NET */
  (void)ctx;
  return tm_provider_fish_skytale_net_generate();
}

static tm_email_info_t *tm_reg_gen_203(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_SPAM_MCCREW_COM */
  (void)ctx;
  return tm_provider_spam_mccrew_com_generate();
}

static tm_email_info_t *tm_reg_gen_204(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_DROPMAIL_CLICK */
  (void)ctx;
  return tm_provider_dropmail_click_generate();
}

static tm_email_info_t *tm_reg_gen_205(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_SPAM_COROIU_COM */
  (void)ctx;
  return tm_provider_spam_coroiu_com_generate();
}

static tm_email_info_t *tm_reg_gen_206(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_SPAM_DELUSER_NET */
  (void)ctx;
  return tm_provider_spam_deluser_net_generate();
}

static tm_email_info_t *tm_reg_gen_207(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_SPAM_DHSF_NET */
  (void)ctx;
  return tm_provider_spam_dhsf_net_generate();
}

static tm_email_info_t *tm_reg_gen_208(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_SPAM_LUCATNT_COM */
  (void)ctx;
  return tm_provider_spam_lucatnt_com_generate();
}

static tm_email_info_t *tm_reg_gen_209(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_SPAM_LYCEUM_LIFE_COM_RU */
  (void)ctx;
  return tm_provider_spam_lyceum_life_com_ru_generate();
}

static tm_email_info_t *tm_reg_gen_210(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_SPAM_NETPIRATES_NET */
  (void)ctx;
  return tm_provider_spam_netpirates_net_generate();
}

static tm_email_info_t *tm_reg_gen_211(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_SPAM_NO_IP_NET */
  (void)ctx;
  return tm_provider_spam_no_ip_net_generate();
}

static tm_email_info_t *tm_reg_gen_212(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_SPAM_OZH_ORG */
  (void)ctx;
  return tm_provider_spam_ozh_org_generate();
}

static tm_email_info_t *tm_reg_gen_213(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_SPAM_PYPHUS_ORG */
  (void)ctx;
  return tm_provider_spam_pyphus_org_generate();
}

static tm_email_info_t *tm_reg_gen_214(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_SPAM_SHEP_PW */
  (void)ctx;
  return tm_provider_spam_shep_pw_generate();
}

static tm_email_info_t *tm_reg_gen_215(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_SPAM_WTF_AT */
  (void)ctx;
  return tm_provider_spam_wtf_at_generate();
}

static tm_email_info_t *tm_reg_gen_216(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_SPAM_WULCZER_ORG */
  (void)ctx;
  return tm_provider_spam_wulczer_org_generate();
}

static tm_email_info_t *tm_reg_gen_217(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_CRAP_KAKADUA_NET */
  (void)ctx;
  return tm_provider_crap_kakadua_net_generate();
}

static tm_email_info_t *tm_reg_gen_218(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_SPAM_JANLUGT_NL */
  (void)ctx;
  return tm_provider_spam_janlugt_nl_generate();
}

static tm_email_info_t *tm_reg_gen_219(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_MIN_BURNINGFISH_NET */
  (void)ctx;
  return tm_provider_min_burningfish_net_generate();
}

static tm_email_info_t *tm_reg_gen_220(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_SINK_FBLAY_COM */
  (void)ctx;
  return tm_provider_sink_fblay_com_generate();
}

static tm_email_info_t *tm_reg_gen_221(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_ETGDEV_DE */
  (void)ctx;
  return tm_provider_etgdev_de_generate();
}

static tm_email_info_t *tm_reg_gen_222(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_MTMDEV_COM */
  (void)ctx;
  return tm_provider_mtmdev_com_generate();
}

static tm_email_info_t *tm_reg_gen_223(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_TEST_UNERGIE_COM */
  (void)ctx;
  return tm_provider_test_unergie_com_generate();
}

static tm_email_info_t *tm_reg_gen_224(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_BLOCK_BDEA_CC */
  (void)ctx;
  return tm_provider_block_bdea_cc_generate();
}

static tm_email_info_t *tm_reg_gen_225(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_TORCH_YI_ORG */
  (void)ctx;
  return tm_provider_torch_yi_org_generate();
}

static tm_email_info_t *tm_reg_gen_226(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_CARLTON183_CHANGEIP_NET */
  (void)ctx;
  return tm_provider_carlton183_changeip_net_generate();
}

static tm_email_info_t *tm_reg_gen_227(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_MAIL_FSMASH_ORG */
  (void)ctx;
  return tm_provider_mail_fsmash_org_generate();
}

static tm_email_info_t *tm_reg_gen_228(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_EBS_COM_AR */
  (void)ctx;
  return tm_provider_ebs_com_ar_generate();
}

static tm_email_info_t *tm_reg_gen_229(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_JAMA_TRENET_EU */
  (void)ctx;
  return tm_provider_jama_trenet_eu_generate();
}

static tm_email_info_t *tm_reg_gen_230(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_BLACKHOLE_DJURBY_SE */
  (void)ctx;
  return tm_provider_blackhole_djurby_se_generate();
}

static tm_email_info_t *tm_reg_gen_231(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_M8R_DAVIDFUHR_DE */
  (void)ctx;
  return tm_provider_m8r_davidfuhr_de_generate();
}

static tm_email_info_t *tm_reg_gen_232(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_MI_MEON_BE */
  (void)ctx;
  return tm_provider_mi_meon_be_generate();
}

static tm_email_info_t *tm_reg_gen_233(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_M_NIK_ME */
  (void)ctx;
  return tm_provider_m_nik_me_generate();
}

static tm_email_info_t *tm_reg_gen_234(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_MAIL_BENTRASK_COM */
  (void)ctx;
  return tm_provider_mail_bentrask_com_generate();
}

static tm_email_info_t *tm_reg_gen_235(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_T_ZIBET_NET */
  (void)ctx;
  return tm_provider_t_zibet_net_generate();
}

static tm_email_info_t *tm_reg_gen_236(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_M8R_MCASAL_COM */
  (void)ctx;
  return tm_provider_m8r_mcasal_com_generate();
}

static tm_email_info_t *tm_reg_gen_237(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_RAMJANE_MOOO_COM */
  (void)ctx;
  return tm_provider_ramjane_mooo_com_generate();
}

static tm_email_info_t *tm_reg_gen_238(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_RAUXA_SENY_CAT */
  (void)ctx;
  return tm_provider_rauxa_seny_cat_generate();
}

static tm_email_info_t *tm_reg_gen_239(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_SP_WOOT_AT */
  (void)ctx;
  return tm_provider_sp_woot_at_generate();
}

static tm_email_info_t *tm_reg_gen_240(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_FWD2M_ESZETT_ES */
  (void)ctx;
  return tm_provider_fwd2m_eszett_es_generate();
}

static tm_email_info_t *tm_reg_gen_241(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_M_887_AT */
  (void)ctx;
  return tm_provider_m_887_at_generate();
}

static tm_email_info_t *tm_reg_gen_242(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_NOSPAM_THURSTONS_US */
  (void)ctx;
  return tm_provider_nospam_thurstons_us_generate();
}

static tm_email_info_t *tm_reg_gen_243(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_NULL_K3VIN_NET */
  (void)ctx;
  return tm_provider_null_k3vin_net_generate();
}

static tm_email_info_t *tm_reg_gen_244(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_REALLY_ISTRASH_COM */
  (void)ctx;
  return tm_provider_really_istrash_com_generate();
}

static tm_email_info_t *tm_reg_gen_245(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_SPAM_HORTUK_OVH */
  (void)ctx;
  return tm_provider_spam_hortuk_ovh_generate();
}

static tm_email_info_t *tm_reg_gen_246(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_16888888_CYOU */
  (void)ctx;
  return tm_provider_16888888_cyou_generate();
}

static tm_email_info_t *tm_reg_gen_247(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_17666688_SHOP */
  (void)ctx;
  return tm_provider_17666688_shop_generate();
}

static tm_email_info_t *tm_reg_gen_248(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_282MAIL_COM */
  (void)ctx;
  return tm_provider_282mail_com_generate();
}

static tm_email_info_t *tm_reg_gen_249(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_BSDU32_BUZZ */
  (void)ctx;
  return tm_provider_bsdu32_buzz_generate();
}

static tm_email_info_t *tm_reg_gen_250(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_DOXU243_BUZZ */
  (void)ctx;
  return tm_provider_doxu243_buzz_generate();
}

static tm_email_info_t *tm_reg_gen_251(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_EASYME_PRO */
  (void)ctx;
  return tm_provider_easyme_pro_generate();
}

static tm_email_info_t *tm_reg_gen_252(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_EVERGREENCO_SHOP */
  (void)ctx;
  return tm_provider_evergreenco_shop_generate();
}

static tm_email_info_t *tm_reg_gen_253(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_LAYUEMING_PICS */
  (void)ctx;
  return tm_provider_layueming_pics_generate();
}

static tm_email_info_t *tm_reg_gen_254(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_MINGYUEKEJI_ONLINE */
  (void)ctx;
  return tm_provider_mingyuekeji_online_generate();
}

static tm_email_info_t *tm_reg_gen_255(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_MINGYUEMING_CLICK */
  (void)ctx;
  return tm_provider_mingyueming_click_generate();
}

static tm_email_info_t *tm_reg_gen_256(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_MINGYUEMING_SHOP */
  (void)ctx;
  return tm_provider_mingyueming_shop_generate();
}

static tm_email_info_t *tm_reg_gen_257(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_MINGYUKEJI_LOL */
  (void)ctx;
  return tm_provider_mingyukeji_lol_generate();
}

static tm_email_info_t *tm_reg_gen_258(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_NUXH62_SPACE */
  (void)ctx;
  return tm_provider_nuxh62_space_generate();
}

static tm_email_info_t *tm_reg_gen_259(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_PROID_CLOUD_IP_CC */
  (void)ctx;
  return tm_provider_proid_cloud_ip_cc_generate();
}

static tm_email_info_t *tm_reg_gen_260(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_SBOOK_PICS */
  (void)ctx;
  return tm_provider_sbook_pics_generate();
}

static tm_email_info_t *tm_reg_gen_261(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_XUE32_BUZZ */
  (void)ctx;
  return tm_provider_xue32_buzz_generate();
}

static tm_email_info_t *tm_reg_gen_262(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_XKX_ME */
  (void)ctx;
  return tm_provider_xkx_me_generate();
}

static tm_email_info_t *tm_reg_gen_263(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_GONEBOX_EMAIL */
  (void)ctx;
  return tm_provider_gonebox_email_generate();
}

static tm_email_info_t *tm_reg_gen_264(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_MAILCAT_AI */
  (void)ctx;
  return tm_provider_mailcat_ai_generate();
}

static tm_email_info_t *tm_reg_gen_265(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_TEMPGO_EMAIL */
  (void)ctx;
  return tm_provider_tempgo_email_generate();
}

static tm_email_info_t *tm_reg_gen_266(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_RESTMAIL_NET */
  (void)ctx;
  return tm_provider_restmail_net_generate();
}

static tm_email_info_t *tm_reg_gen_267(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_B_SMELLY_CC */
  (void)ctx;
  return tm_provider_b_smelly_cc_generate();
}

static tm_email_info_t *tm_reg_gen_268(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_DEA_SOON_IT */
  (void)ctx;
  return tm_provider_dea_soon_it_generate();
}

static tm_email_info_t *tm_reg_gen_269(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_DISPOSABLE_AL_SUDANI_COM */
  (void)ctx;
  return tm_provider_disposable_al_sudani_com_generate();
}

static tm_email_info_t *tm_reg_gen_270(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_DISPOSABLE_NOGONAD_NL */
  (void)ctx;
  return tm_provider_disposable_nogonad_nl_generate();
}

static tm_email_info_t *tm_reg_gen_271(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_J_FAIRUSE_ORG */
  (void)ctx;
  return tm_provider_j_fairuse_org_generate();
}

static tm_email_info_t *tm_reg_gen_272(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_MN_CURPPA_COM */
  (void)ctx;
  return tm_provider_mn_curppa_com_generate();
}

static tm_email_info_t *tm_reg_gen_273(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_MAILINATORZZ_MOOO_COM */
  (void)ctx;
  return tm_provider_mailinatorzz_mooo_com_generate();
}

static tm_email_info_t *tm_reg_gen_274(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_NOTFOND_404_MN */
  (void)ctx;
  return tm_provider_notfond_404_mn_generate();
}

static tm_email_info_t *tm_reg_gen_275(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_TEMPGMAILER */
  (void)ctx;
  return NULL;
}

static tm_email_info_t *tm_reg_gen_276(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_TEMP_MAIL_ORG */
  (void)ctx;
  return NULL;
}

static tm_email_info_t *tm_reg_gen_277(const tm_gen_ctx_t *ctx) {
  /* default */
  (void)ctx;
  return NULL;
}

static tm_email_info_t *tm_reg_gen_278(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_DROPMAIL_ME */
  (void)ctx;
  return tm_provider_dropmail_me_generate();
}

static tm_email_info_t *tm_reg_gen_279(const tm_gen_ctx_t *ctx) {
  /* CHANNEL_TEN_MINUTE_MAIL_NET */
  (void)ctx;
  return tm_provider_ten_minute_mail_net_generate();
}

/* ========== 获取邮件 thunk（统一签名，逐字复刻原 switch 分支） ========== */

static tm_email_t *tm_reg_get_0(const char *email, const char *token, int *count) {
  /* CHANNEL_TEMPMAIL */
  (void)token;
  return tm_provider_tempmail_get_emails(email, count);
}

static tm_email_t *tm_reg_get_1(const char *email, const char *token, int *count) {
  /* CHANNEL_TEMPMAIL_CN */
  (void)token;
  return tm_provider_tempmail_cn_get_emails(email, count);
}

static tm_email_t *tm_reg_get_2(const char *email, const char *token, int *count) {
  /* CHANNEL_LINSHIYOU */
  if (!token) {
    *count = -1;
    return NULL;
  }
  return tm_provider_linshiyou_get_emails(token,
                                                email, count);
}

static tm_email_t *tm_reg_get_3(const char *email, const char *token, int *count) {
  /* CHANNEL_TEMPMAIL_LOL */
  return tm_provider_tempmail_lol_get_emails(token,
                                                   email, count);
}

static tm_email_t *tm_reg_get_4(const char *email, const char *token, int *count) {
  /* CHANNEL_CHATGPT_ORG_UK */
  return tm_provider_chatgpt_org_uk_get_emails(token,
                                                     email, count);
}

static tm_email_t *tm_reg_get_5(const char *email, const char *token, int *count) {
  /* CHANNEL_AWAMAIL */
  return tm_provider_awamail_get_emails(token,
                                              email, count);
}

static tm_email_t *tm_reg_get_6(const char *email, const char *token, int *count) {
  /* CHANNEL_MAIL_TM, CHANNEL_WEB_LIBRARY_NET */
  return tm_provider_mail_tm_get_emails(token,
                                              email, count);
}

static tm_email_t *tm_reg_get_7(const char *email, const char *token, int *count) {
  /* CHANNEL_DROPMAIL */
  return tm_provider_dropmail_get_emails(token,
                                               email, count);
}

static tm_email_t *tm_reg_get_8(const char *email, const char *token, int *count) {
  /* CHANNEL_GUERRILLAMAIL */
  return tm_provider_guerrillamail_get_emails(token,
                                                    email, count);
}

static tm_email_t *tm_reg_get_9(const char *email, const char *token, int *count) {
  /* CHANNEL_MAILDROP */
  return tm_provider_maildrop_get_emails(token,
                                               email, count);
}

static tm_email_t *tm_reg_get_10(const char *email, const char *token, int *count) {
  /* CHANNEL_SMAIL_PW */
  if (!token) {
    *count = -1;
    return NULL;
  }
  return tm_provider_smail_pw_get_emails(token,
                                               email, count);
}

static tm_email_t *tm_reg_get_11(const char *email, const char *token, int *count) {
  /* CHANNEL_VIP_215 */
  if (!token) {
    *count = -1;
    return NULL;
  }
  return tm_provider_vip215_get_emails(token,
                                             email, count);
}

static tm_email_t *tm_reg_get_12(const char *email, const char *token, int *count) {
  /* CHANNEL_FAKE_LEGAL, CHANNEL_IMGUI_DE, CHANNEL_PULSEWEBMENU_DE */
  (void)token;
  return tm_provider_fake_legal_get_emails(email, count);
}

static tm_email_t *tm_reg_get_13(const char *email, const char *token, int *count) {
  /* CHANNEL_MFFAC */
  return tm_provider_mffac_get_emails(token,
                                            email, count);
}

static tm_email_t *tm_reg_get_14(const char *email, const char *token, int *count) {
  /* CHANNEL_TA_EASY */
  if (!token) {
    *count = -1;
    return NULL;
  }
  return tm_provider_ta_easy_get_emails(token,
                                              email, count);
}

static tm_email_t *tm_reg_get_15(const char *email, const char *token, int *count) {
  /* CHANNEL_MOAKT, CHANNEL_DRMAIL_IN, CHANNEL_TEML_NET, CHANNEL_TMPEML_COM, CHANNEL_TMPBOX_NET, CHANNEL_MOAKT_CC, CHANNEL_DISBOX_NET, CHANNEL_TMPMAIL_ORG, CHANNEL_TMPMAIL_NET, CHANNEL_TMAILS_NET, CHANNEL_DISBOX_ORG, CHANNEL_MOAKT_CO, CHANNEL_MOAKT_WS, CHANNEL_TMAIL_WS, CHANNEL_BAREED_WS */
  if (!token) {
    *count = -1;
    return NULL;
  }
  return tm_provider_moakt_get_emails(token,
                                            email, count);
}

static tm_email_t *tm_reg_get_16(const char *email, const char *token, int *count) {
  /* CHANNEL_10MINUTE_ONE, CHANNEL_XGHFF_COM, CHANNEL_OQQAJ_COM, CHANNEL_PSOVV_COM, CHANNEL_DBWOT_COM, CHANNEL_YGWPR_COM, CHANNEL_IMXWE_COM */
  if (!token) {
    *count = -1;
    return NULL;
  }
  return tm_provider_tenminute_one_get_emails(token,
                                                    email, count);
}

static tm_email_t *tm_reg_get_17(const char *email, const char *token, int *count) {
  /* CHANNEL_EMAIL10MIN */
  if (!token) {
    *count = -1;
    return NULL;
  }
  return tm_provider_email10min_get_emails(token,
                                                 email, count);
}

static tm_email_t *tm_reg_get_18(const char *email, const char *token, int *count) {
  /* CHANNEL_MJJ_CM */
  (void)email;
  (void)token;
  *count = -1;
  return NULL;
}

static tm_email_t *tm_reg_get_19(const char *email, const char *token, int *count) {
  /* CHANNEL_LINSHI_CO */
  (void)email;
  (void)token;
  *count = -1;
  return NULL;
}

static tm_email_t *tm_reg_get_20(const char *email, const char *token, int *count) {
  /* CHANNEL_HARAKIRIMAIL */
  (void)token;
  return tm_provider_harakirimail_get_emails(email, count);
}

static tm_email_t *tm_reg_get_21(const char *email, const char *token, int *count) {
  /* CHANNEL_JQKJQK_XYZ, CHANNEL_LYHLEVI_COM */
  if (!token) {
    *count = -1;
    return NULL;
  }
  return tm_provider_zhujump_get_emails(token,
                                              email, count);
}

static tm_email_t *tm_reg_get_22(const char *email, const char *token, int *count) {
  /* CHANNEL_TEMPMAIL_PLUS, CHANNEL_FEXPOST_COM, CHANNEL_FEXBOX_ORG, CHANNEL_MAILBOX_IN_UA, CHANNEL_ROVER_INFO, CHANNEL_CHITTHI_IN, CHANNEL_FEXTEMP_COM, CHANNEL_ANY_PINK, CHANNEL_MEREPOST_COM */
  (void)token;
  return tm_provider_tempmail_plus_get_emails(email, count);
}

static tm_email_t *tm_reg_get_23(const char *email, const char *token, int *count) {
  /* CHANNEL_TEMPMAIL_LOL_V2 */
  if (!token) {
    *count = -1;
    return NULL;
  }
  return tm_provider_tempmail_lol_v2_get_emails(
          token, email, count);
}

static tm_email_t *tm_reg_get_24(const char *email, const char *token, int *count) {
  /* CHANNEL_TEMPGBOX */
  if (!token) {
    *count = -1;
    return NULL;
  }
  return tm_provider_tempgbox_get_emails(token,
                                               email, count);
}

static tm_email_t *tm_reg_get_25(const char *email, const char *token, int *count) {
  /* CHANNEL_EMAILNATOR */
  if (!token) {
    *count = -1;
    return NULL;
  }
  return tm_provider_emailnator_get_emails(token,
                                                 email, count);
}

static tm_email_t *tm_reg_get_26(const char *email, const char *token, int *count) {
  /* CHANNEL_TEMPORAM */
  return tm_provider_temporam_get_emails(token,
                                               email, count);
}

static tm_email_t *tm_reg_get_27(const char *email, const char *token, int *count) {
  /* CHANNEL_NEIGHBOURS */
  (void)token;
  return tm_provider_neighbours_get_emails(email, count);
}

static tm_email_t *tm_reg_get_28(const char *email, const char *token, int *count) {
  /* CHANNEL_M2U */
  if (!token) {
    *count = -1;
    return NULL;
  }
  return tm_provider_m2u_get_emails(token, email,
                                          count);
}

static tm_email_t *tm_reg_get_29(const char *email, const char *token, int *count) {
  /* CHANNEL_TEMPY_EMAIL */
  (void)token;
  return tm_provider_tempy_email_get_emails(email, count);
}

static tm_email_t *tm_reg_get_30(const char *email, const char *token, int *count) {
  /* CHANNEL_FMAIL */
  (void)token;
  return tm_provider_fmail_get_emails(email, count);
}

static tm_email_t *tm_reg_get_31(const char *email, const char *token, int *count) {
  /* CHANNEL_OCKITO */
  if (!token) {
    *count = -1;
    return NULL;
  }
  return tm_provider_ockito_get_emails(token,
                                             email, count);
}

static tm_email_t *tm_reg_get_32(const char *email, const char *token, int *count) {
  /* CHANNEL_ANONBOX */
  if (!token) {
    *count = -1;
    return NULL;
  }
  return tm_provider_anonbox_get_emails(token,
                                              email, count);
}

static tm_email_t *tm_reg_get_33(const char *email, const char *token, int *count) {
  /* CHANNEL_DUCKMAIL */
  if (!token) {
    *count = -1;
    return NULL;
  }
  return tm_provider_duckmail_get_emails(token,
                                               email, count);
}

static tm_email_t *tm_reg_get_34(const char *email, const char *token, int *count) {
  /* CHANNEL_MAILINATOR */
  (void)token;
  return tm_provider_mailinator_get_emails(email, count);
}

static tm_email_t *tm_reg_get_35(const char *email, const char *token, int *count) {
  /* CHANNEL_TEMPMAIL365 */
  (void)token;
  return tm_tempmail365_get_emails(email, count);
}

static tm_email_t *tm_reg_get_36(const char *email, const char *token, int *count) {
  /* CHANNEL_TEMPINBOX */
  (void)token;
  return tm_provider_tempinbox_get_emails(email, count);
}

static tm_email_t *tm_reg_get_37(const char *email, const char *token, int *count) {
  /* CHANNEL_SHARKLASERS */
  if (!token) {
    *count = -1;
    return NULL;
  }
  return tm_provider_guerrillamail_mirror_get_emails(
          "https://www.sharklasers.com/ajax.php", token,
          email, count);
}

static tm_email_t *tm_reg_get_38(const char *email, const char *token, int *count) {
  /* CHANNEL_SHARKLASERS_COM */
  if (!token) {
    *count = -1;
    return NULL;
  }
  return tm_provider_guerrillamail_mirror_get_emails(
          "https://sharklasers.com/ajax.php", token,
          email, count);
}

static tm_email_t *tm_reg_get_39(const char *email, const char *token, int *count) {
  /* CHANNEL_GRR_LA */
  if (!token) {
    *count = -1;
    return NULL;
  }
  return tm_provider_guerrillamail_mirror_get_emails(
          "https://www.grr.la/ajax.php", token, email,
          count);
}

static tm_email_t *tm_reg_get_40(const char *email, const char *token, int *count) {
  /* CHANNEL_GRR_LA_COM */
  if (!token) {
    *count = -1;
    return NULL;
  }
  return tm_provider_guerrillamail_mirror_get_emails(
          "https://grr.la/ajax.php", token, email,
          count);
}

static tm_email_t *tm_reg_get_41(const char *email, const char *token, int *count) {
  /* CHANNEL_GUERRILLAMAIL_INFO */
  if (!token) {
    *count = -1;
    return NULL;
  }
  return tm_provider_guerrillamail_mirror_get_emails(
          "https://www.guerrillamail.info/ajax.php", token,
          email, count);
}

static tm_email_t *tm_reg_get_42(const char *email, const char *token, int *count) {
  /* CHANNEL_GUERRILLAMAIL_COM */
  if (!token) {
    *count = -1;
    return NULL;
  }
  return tm_provider_guerrillamail_mirror_get_emails(
          "https://guerrillamail.com/ajax.php", token,
          email, count);
}

static tm_email_t *tm_reg_get_43(const char *email, const char *token, int *count) {
  /* CHANNEL_SPAM4ME */
  if (!token) {
    *count = -1;
    return NULL;
  }
  return tm_provider_guerrillamail_mirror_get_emails(
          "https://www.spam4.me/ajax.php", token, email,
          count);
}

static tm_email_t *tm_reg_get_44(const char *email, const char *token, int *count) {
  /* CHANNEL_GUERRILLAMAIL_NET */
  if (!token) {
    *count = -1;
    return NULL;
  }
  return tm_provider_guerrillamail_mirror_get_emails(
          "https://www.guerrillamail.net/ajax.php", token,
          email, count);
}

static tm_email_t *tm_reg_get_45(const char *email, const char *token, int *count) {
  /* CHANNEL_GUERRILLAMAIL_ORG */
  if (!token) {
    *count = -1;
    return NULL;
  }
  return tm_provider_guerrillamail_mirror_get_emails(
          "https://www.guerrillamail.org/ajax.php", token,
          email, count);
}

static tm_email_t *tm_reg_get_46(const char *email, const char *token, int *count) {
  /* CHANNEL_GUERRILLAMAILBLOCK */
  if (!token) {
    *count = -1;
    return NULL;
  }
  return tm_provider_guerrillamail_mirror_get_emails(
          "https://www.guerrillamailblock.com/ajax.php", token,
          email, count);
}

static tm_email_t *tm_reg_get_47(const char *email, const char *token, int *count) {
  /* CHANNEL_GUERRILLAMAIL_COM_WWW */
  if (!token) {
    *count = -1;
    return NULL;
  }
  return tm_provider_guerrillamail_mirror_get_emails(
          "https://www.guerrillamail.com/ajax.php", token,
          email, count);
}

static tm_email_t *tm_reg_get_48(const char *email, const char *token, int *count) {
  /* CHANNEL_TEMP_MAIL_IO */
  (void)token;
  return tm_provider_temp_mail_io_get_emails(email, count);
}

static tm_email_t *tm_reg_get_49(const char *email, const char *token, int *count) {
  /* CHANNEL_MAIL_CX, CHANNEL_DDKER_COM */
  if (!token) {
    *count = -1;
    return NULL;
  }
  return tm_provider_mail_cx_get_emails(token,
                                              email, count);
}

static tm_email_t *tm_reg_get_50(const char *email, const char *token, int *count) {
  /* CHANNEL_CATCHMAIL */
  (void)token;
  return tm_provider_catchmail_get_emails(email, count);
}

static tm_email_t *tm_reg_get_51(const char *email, const char *token, int *count) {
  /* CHANNEL_CATCHMAIL_MAILISTRY */
  (void)token;
  return tm_provider_catchmail_get_emails(email, count);
}

static tm_email_t *tm_reg_get_52(const char *email, const char *token, int *count) {
  /* CHANNEL_CATCHMAIL_ZEPPOST */
  (void)token;
  return tm_provider_catchmail_get_emails(email, count);
}

static tm_email_t *tm_reg_get_53(const char *email, const char *token, int *count) {
  /* CHANNEL_MAILFORSPAM */
  (void)token;
  return tm_provider_mailforspam_get_emails(email, count);
}

static tm_email_t *tm_reg_get_54(const char *email, const char *token, int *count) {
  /* CHANNEL_MAILFORSPAM_TEMPMAIL_IO */
  (void)token;
  return tm_provider_mailforspam_get_emails(email, count);
}

static tm_email_t *tm_reg_get_55(const char *email, const char *token, int *count) {
  /* CHANNEL_MAILFORSPAM_DISPOSABLE */
  (void)token;
  return tm_provider_mailforspam_get_emails(email, count);
}

static tm_email_t *tm_reg_get_56(const char *email, const char *token, int *count) {
  /* CHANNEL_TEMPMAILC */
  (void)token;
  return tm_provider_tempmailc_get_emails(email, count);
}

static tm_email_t *tm_reg_get_57(const char *email, const char *token, int *count) {
  /* CHANNEL_MAILNESIA */
  (void)token;
  return tm_provider_mailnesia_get_emails(email, count);
}

static tm_email_t *tm_reg_get_58(const char *email, const char *token, int *count) {
  /* CHANNEL_THROWAWAYMAIL */
  if (!token) {
    *count = -1;
    return NULL;
  }
  return tm_provider_throwawaymail_get_emails(token,
                                                    email, count);
}

static tm_email_t *tm_reg_get_59(const char *email, const char *token, int *count) {
  /* CHANNEL_TEMPMAIL_FISH */
  if (!token) {
    *count = -1;
    return NULL;
  }
  return tm_provider_tempmail_fish_get_emails(token,
                                                    email, count);
}

static tm_email_t *tm_reg_get_60(const char *email, const char *token, int *count) {
  /* CHANNEL_NEIGHBOURS_SH */
  return tm_provider_neighbours_sh_get_emails(token,
                                                    email, count);
}

static tm_email_t *tm_reg_get_61(const char *email, const char *token, int *count) {
  /* CHANNEL_SHITTY_EMAIL */
  if (!token) {
    *count = -1;
    return NULL;
  }
  return tm_provider_shitty_email_get_emails(token,
                                                   email, count);
}

static tm_email_t *tm_reg_get_62(const char *email, const char *token, int *count) {
  /* CHANNEL_TEMPMAILPRO */
  if (!token) {
    *count = -1;
    return NULL;
  }
  return tm_provider_tempmailpro_get_emails(token,
                                                  email, count);
}

static tm_email_t *tm_reg_get_63(const char *email, const char *token, int *count) {
  /* CHANNEL_DEVMAIL_UK */
  (void)token;
  return tm_provider_devmail_uk_get_emails(email, count);
}

static tm_email_t *tm_reg_get_64(const char *email, const char *token, int *count) {
  /* CHANNEL_CLEANTEMPMAIL */
  (void)token;
  return tm_provider_cleantempmail_get_emails(email, count);
}

static tm_email_t *tm_reg_get_65(const char *email, const char *token, int *count) {
  /* CHANNEL_INBOXKITTEN */
  (void)token;
  return tm_provider_inboxkitten_get_emails(email, count);
}

static tm_email_t *tm_reg_get_66(const char *email, const char *token, int *count) {
  /* CHANNEL_GETNADA, CHANNEL_ONE_VPN_NET, CHANNEL_ABEMATV_COM, CHANNEL_ABEMATV_NET, CHANNEL_ABEMATV_ORG, CHANNEL_ACEH_CC, CHANNEL_BANGKABELITUNG_NET, CHANNEL_CCTRUYEN_COM, CHANNEL_GETNADA_COM, CHANNEL_GETNADA_EMAIL, CHANNEL_GETNADA_NET, CHANNEL_JAWATENGAH_NET, CHANNEL_JAWATIMUR_NET, CHANNEL_KALIMANTANBARAT_NET, CHANNEL_KALIMANTANSELATAN_NET, CHANNEL_KALIMANTANTENGAH_NET, CHANNEL_KALIMANTANTIMUR_NET, CHANNEL_KALIMANTANUTARA_NET, CHANNEL_KEPULAUANRIAU_NET, CHANNEL_LUXURY345_COM, CHANNEL_MALUKUUTARA_NET, CHANNEL_NUSATENGGARABARAT_NET, CHANNEL_NUSATENGGARATIMUR_NET, CHANNEL_PAPUABARAT_NET, CHANNEL_PAPUABARATDAYA_NET, CHANNEL_PAPUASELATAN_NET, CHANNEL_PEHOL_COM, CHANNEL_PTRUYEN_COM, CHANNEL_PULAUBALI_NET, CHANNEL_RIAU_NET, CHANNEL_SEOKEY_ORG, CHANNEL_SULAWESIBARAT_NET, CHANNEL_SULAWESISELATAN_NET, CHANNEL_SULAWESITENGAH_NET, CHANNEL_SULAWESITENGGARA_NET, CHANNEL_SUMATERABARAT_NET, CHANNEL_SUMATERASELATAN_NET, CHANNEL_SUMATERAUTARA_NET, CHANNEL_VILLATOGEL_COM */
  if (!token) {
    *count = -1;
    return NULL;
  }
  return tm_provider_getnada_get_emails(token,
                                              email, count);
}

static tm_email_t *tm_reg_get_67(const char *email, const char *token, int *count) {
  /* CHANNEL_MAIL123 */
  (void)token;
  return tm_provider_mail123_get_emails(email, count);
}

static tm_email_t *tm_reg_get_68(const char *email, const char *token, int *count) {
  /* CHANNEL_MAIL10S */
  (void)token;
  return tm_provider_mail10s_get_emails(email, count);
}

static tm_email_t *tm_reg_get_69(const char *email, const char *token, int *count) {
  /* CHANNEL_WEBMAILTEMP */
  if (!token) {
    *count = -1;
    return NULL;
  }
  return tm_provider_webmailtemp_get_emails(token,
                                                  email, count);
}

static tm_email_t *tm_reg_get_70(const char *email, const char *token, int *count) {
  /* CHANNEL_TEMPFASTMAIL */
  if (!token) {
    *count = -1;
    return NULL;
  }
  return tm_provider_tempfastmail_get_emails(token,
                                                   email, count);
}

static tm_email_t *tm_reg_get_71(const char *email, const char *token, int *count) {
  /* CHANNEL_ONE_SEC_MAIL */
  if (!token) {
    *count = -1;
    return NULL;
  }
  return tm_provider_one_sec_mail_get_emails(token,
                                                   email, count);
}

static tm_email_t *tm_reg_get_72(const char *email, const char *token, int *count) {
  /* CHANNEL_FAKEMAIL */
  if (!token) {
    *count = -1;
    return NULL;
  }
  return tm_provider_fakemail_get_emails(token,
                                               email, count);
}

static tm_email_t *tm_reg_get_73(const char *email, const char *token, int *count) {
  /* CHANNEL_OPENINBOX */
  if (!token) {
    *count = -1;
    return NULL;
  }
  return tm_provider_openinbox_get_emails(token,
                                                email, count);
}

static tm_email_t *tm_reg_get_74(const char *email, const char *token, int *count) {
  /* CHANNEL_INBOXES */
  (void)token;
  return tm_provider_inboxes_get_emails(email, count);
}

static tm_email_t *tm_reg_get_75(const char *email, const char *token, int *count) {
  /* CHANNEL_UNCORREOTEMPORAL */
  if (!token) {
    *count = -1;
    return NULL;
  }
  return tm_provider_uncorreotemporal_get_emails(
          token, email, count);
}

static tm_email_t *tm_reg_get_76(const char *email, const char *token, int *count) {
  /* CHANNEL_BYOM */
  (void)token;
  return tm_provider_byom_get_emails(email, count);
}

static tm_email_t *tm_reg_get_77(const char *email, const char *token, int *count) {
  /* CHANNEL_ANONYMMAIL */
  (void)token;
  return tm_provider_anonymmail_get_emails(email, count);
}

static tm_email_t *tm_reg_get_78(const char *email, const char *token, int *count) {
  /* CHANNEL_EYEPASTE */
  (void)token;
  return tm_provider_eyepaste_get_emails(email, count);
}

static tm_email_t *tm_reg_get_79(const char *email, const char *token, int *count) {
  /* CHANNEL_MAIL_SUNLS */
  (void)token;
  return tm_provider_mail_sunls_get_emails(email, count);
}

static tm_email_t *tm_reg_get_80(const char *email, const char *token, int *count) {
  /* CHANNEL_EXPRESSINBOXHUB */
  if (!token) {
    *count = -1;
    return NULL;
  }
  return tm_provider_expressinboxhub_get_emails(
          token, email, count);
}

static tm_email_t *tm_reg_get_81(const char *email, const char *token, int *count) {
  /* CHANNEL_LROID */
  if (!token) {
    *count = -1;
    return NULL;
  }
  return tm_provider_lroid_get_emails(token,
                                            email, count);
}

static tm_email_t *tm_reg_get_82(const char *email, const char *token, int *count) {
  /* CHANNEL_HARIBU */
  if (!token) {
    *count = -1;
    return NULL;
  }
  return tm_provider_haribu_get_emails(token,
                                             email, count);
}

static tm_email_t *tm_reg_get_83(const char *email, const char *token, int *count) {
  /* CHANNEL_ROOTSH */
  if (!token) {
    *count = -1;
    return NULL;
  }
  return tm_provider_rootsh_get_emails(token,
                                             email, count);
}

static tm_email_t *tm_reg_get_84(const char *email, const char *token, int *count) {
  /* CHANNEL_FAKE_EMAIL_SITE */
  (void)token;
  return tm_provider_fake_email_site_get_emails(email, count);
}

static tm_email_t *tm_reg_get_85(const char *email, const char *token, int *count) {
  /* CHANNEL_MOHMAL */
  if (!token) {
    *count = -1;
    return NULL;
  }
  return tm_provider_mohmal_get_emails(token,
                                             email, count);
}

static tm_email_t *tm_reg_get_86(const char *email, const char *token, int *count) {
  /* CHANNEL_MAILGOLEM */
  if (!token) {
    *count = -1;
    return NULL;
  }
  return tm_provider_mailgolem_get_emails(token,
                                                email, count);
}

static tm_email_t *tm_reg_get_87(const char *email, const char *token, int *count) {
  /* CHANNEL_BEST_TEMP_MAIL */
  if (!token) {
    *count = -1;
    return NULL;
  }
  return tm_provider_best_temp_mail_get_emails(token,
                                                     email, count);
}

static tm_email_t *tm_reg_get_88(const char *email, const char *token, int *count) {
  /* CHANNEL_DISPOSABLEMAIL_APP */
  if (!token) {
    *count = -1;
    return NULL;
  }
  return tm_provider_disposablemail_app_get_emails(
          token, email, count);
}

static tm_email_t *tm_reg_get_89(const char *email, const char *token, int *count) {
  /* CHANNEL_MAILTEMP_CC */
  if (!token) {
    *count = -1;
    return NULL;
  }
  return tm_provider_mailtemp_cc_get_emails(token,
                                                  email, count);
}

static tm_email_t *tm_reg_get_90(const char *email, const char *token, int *count) {
  /* CHANNEL_MINUTEINBOX */
  if (!token) {
    *count = -1;
    return NULL;
  }
  return tm_provider_minuteinbox_get_emails(token,
                                                  email, count);
}

static tm_email_t *tm_reg_get_91(const char *email, const char *token, int *count) {
  /* CHANNEL_MAILCATCH */
  (void)email;
  (void)token;
  *count = -1;
  return NULL;
}

static tm_email_t *tm_reg_get_92(const char *email, const char *token, int *count) {
  /* CHANNEL_TEMPEMAIL_CO */
  if (!token) {
    *count = -1;
    return NULL;
  }
  return tm_provider_tempemail_co_get_emails(token,
                                                   email, count);
}

static tm_email_t *tm_reg_get_93(const char *email, const char *token, int *count) {
  /* CHANNEL_TEMPEMAILS_NET */
  if (!token) {
    *count = -1;
    return NULL;
  }
  return tm_provider_tempemails_net_get_emails(token,
                                                     email, count);
}

static tm_email_t *tm_reg_get_94(const char *email, const char *token, int *count) {
  /* CHANNEL_ALTMAILS */
  if (!token) {
    *count = -1;
    return NULL;
  }
  return tm_provider_altmails_get_emails(token,
                                               email, count);
}

static tm_email_t *tm_reg_get_95(const char *email, const char *token, int *count) {
  /* CHANNEL_TEMPEMAIL_INFO */
  if (!token) {
    *count = -1;
    return NULL;
  }
  return tm_provider_tempemail_info_get_emails(token,
                                                     email, count);
}

static tm_email_t *tm_reg_get_96(const char *email, const char *token, int *count) {
  /* CHANNEL_SMAILPRO */
  return tm_provider_smailpro_get_emails(token,
                                               email, count);
}

static tm_email_t *tm_reg_get_97(const char *email, const char *token, int *count) {
  /* CHANNEL_TEMPMAILTEN */
  if (!token) {
    *count = -1;
    return NULL;
  }
  return tm_provider_tempmailten_get_emails(token,
                                                  email, count);
}

static tm_email_t *tm_reg_get_98(const char *email, const char *token, int *count) {
  /* CHANNEL_MAILDROP_CC */
  return tm_provider_maildrop_cc_get_emails(token,
                                                  email, count);
}

static tm_email_t *tm_reg_get_99(const char *email, const char *token, int *count) {
  /* CHANNEL_TENMINUTEMAIL_NET */
  if (!token) {
    *count = -1;
    return NULL;
  }
  return tm_provider_tenminutemail_net_get_emails(
          token, email, count);
}

static tm_email_t *tm_reg_get_100(const char *email, const char *token, int *count) {
  /* CHANNEL_LINSHIYOUXIANG_NET */
  if (!token) {
    *count = -1;
    return NULL;
  }
  return tm_provider_linshiyouxiang_net_get_emails(
          token, email, count);
}

static tm_email_t *tm_reg_get_101(const char *email, const char *token, int *count) {
  /* CHANNEL_TEMPMAIL_FYI */
  if (!token) {
    *count = -1;
    return NULL;
  }
  return tm_provider_tempmail_fyi_get_emails(token,
                                                   email, count);
}

static tm_email_t *tm_reg_get_102(const char *email, const char *token, int *count) {
  /* CHANNEL_DISPOSABLEMAIL_COM */
  if (!token) {
    *count = -1;
    return NULL;
  }
  return tm_provider_disposablemail_com_get_emails(
          token, email, count);
}

static tm_email_t *tm_reg_get_103(const char *email, const char *token, int *count) {
  /* CHANNEL_TEMPP_MAILS */
  if (!token) {
    *count = -1;
    return NULL;
  }
  return tm_provider_tempp_mails_get_emails(token,
                                                  email, count);
}

static tm_email_t *tm_reg_get_104(const char *email, const char *token, int *count) {
  /* CHANNEL_EMAILTEMP_ORG */
  if (!token) {
    *count = -1;
    return NULL;
  }
  return tm_provider_emailtemp_org_get_emails(token,
                                                    email, count);
}

static tm_email_t *tm_reg_get_105(const char *email, const char *token, int *count) {
  /* CHANNEL_MYTEMPMAIL_CC */
  if (!token) {
    *count = -1;
    return NULL;
  }
  return tm_provider_mytempmail_cc_get_emails(token,
                                                    email, count);
}

static tm_email_t *tm_reg_get_106(const char *email, const char *token, int *count) {
  /* CHANNEL_TEMP_MAIL_NOW */
  if (!token) {
    *count = -1;
    return NULL;
  }
  return tm_provider_temp_mail_now_get_emails(token,
                                                    email, count);
}

static tm_email_t *tm_reg_get_107(const char *email, const char *token, int *count) {
  /* CHANNEL_MAIL_TD */
  if (!token) {
    *count = -1;
    return NULL;
  }
  return tm_provider_mail_td_get_emails(token,
                                              email, count);
}

static tm_email_t *tm_reg_get_108(const char *email, const char *token, int *count) {
  /* CHANNEL_MAILHOLE_DE */
  return tm_provider_mailhole_de_get_emails(token,
                                                  email, count);
}

static tm_email_t *tm_reg_get_109(const char *email, const char *token, int *count) {
  /* CHANNEL_TMAIL_LINK */
  if (!token) {
    *count = -1;
    return NULL;
  }
  return tm_provider_tmail_link_get_emails(token,
                                                 email, count);
}

static tm_email_t *tm_reg_get_110(const char *email, const char *token, int *count) {
  /* CHANNEL_24MAIL_CHACUO */
  (void)token;
  return tm_provider_24mail_chacuo_get_emails(email, count);
}

static tm_email_t *tm_reg_get_111(const char *email, const char *token, int *count) {
  /* CHANNEL_NIMAIL */
  return tm_provider_nimail_get_emails(token,
                                             email, count);
}

static tm_email_t *tm_reg_get_112(const char *email, const char *token, int *count) {
  /* CHANNEL_FREECUSTOM */
  return tm_provider_freecustom_get_emails(token,
                                                 email, count);
}

static tm_email_t *tm_reg_get_113(const char *email, const char *token, int *count) {
  /* CHANNEL_APIHZ */
  return tm_provider_apihz_get_emails(token,
                                            email, count);
}

static tm_email_t *tm_reg_get_114(const char *email, const char *token, int *count) {
  /* CHANNEL_SOGETTHIS_COM */
  (void)token;
  return tm_provider_sogetthis_com_get_emails(email, count);
}

static tm_email_t *tm_reg_get_115(const char *email, const char *token, int *count) {
  /* CHANNEL_BOBMAIL_INFO */
  (void)token;
  return tm_provider_bobmail_info_get_emails(email, count);
}

static tm_email_t *tm_reg_get_116(const char *email, const char *token, int *count) {
  /* CHANNEL_SUREMAIL_INFO */
  (void)token;
  return tm_provider_suremail_info_get_emails(email, count);
}

static tm_email_t *tm_reg_get_117(const char *email, const char *token, int *count) {
  /* CHANNEL_BINKMAIL_COM */
  (void)token;
  return tm_provider_binkmail_com_get_emails(email, count);
}

static tm_email_t *tm_reg_get_118(const char *email, const char *token, int *count) {
  /* CHANNEL_VERYREALEMAIL_COM */
  (void)token;
  return tm_provider_veryrealemail_com_get_emails(email, count);
}

static tm_email_t *tm_reg_get_119(const char *email, const char *token, int *count) {
  /* CHANNEL_MAILMOMY */
  return tm_provider_mailmomy_get_emails(token,
                                               email, count);
}

static tm_email_t *tm_reg_get_120(const char *email, const char *token, int *count) {
  /* CHANNEL_CHAMMY_INFO */
  (void)token;
  return tm_provider_chammy_info_get_emails(email, count);
}

static tm_email_t *tm_reg_get_121(const char *email, const char *token, int *count) {
  /* CHANNEL_THISISNOTMYREALEMAIL_COM */
  (void)token;
  return tm_provider_thisisnotmyrealemail_com_get_emails(
          email, count);
}

static tm_email_t *tm_reg_get_122(const char *email, const char *token, int *count) {
  /* CHANNEL_NOTMAILINATOR_COM */
  (void)token;
  return tm_provider_notmailinator_com_get_emails(email, count);
}

static tm_email_t *tm_reg_get_123(const char *email, const char *token, int *count) {
  /* CHANNEL_SPAMHEREPLEASE_COM */
  (void)token;
  return tm_provider_spamhereplease_com_get_emails(email, count);
}

static tm_email_t *tm_reg_get_124(const char *email, const char *token, int *count) {
  /* CHANNEL_SENDSPAMHERE_COM */
  (void)token;
  return tm_provider_sendspamhere_com_get_emails(email, count);
}

static tm_email_t *tm_reg_get_125(const char *email, const char *token, int *count) {
  /* CHANNEL_SENDFREE_ORG */
  (void)token;
  return tm_provider_sendfree_org_get_emails(email, count);
}

static tm_email_t *tm_reg_get_126(const char *email, const char *token, int *count) {
  /* CHANNEL_JUNK_BEATS_ORG */
  (void)token;
  return tm_provider_junk_beats_org_get_emails(email, count);
}

static tm_email_t *tm_reg_get_127(const char *email, const char *token, int *count) {
  /* CHANNEL_JUNK_IHMEHL_COM */
  (void)token;
  return tm_provider_junk_ihmehl_com_get_emails(email, count);
}

static tm_email_t *tm_reg_get_128(const char *email, const char *token, int *count) {
  /* CHANNEL_JUNK_NOPLAY_ORG */
  (void)token;
  return tm_provider_junk_noplay_org_get_emails(email, count);
}

static tm_email_t *tm_reg_get_129(const char *email, const char *token, int *count) {
  /* CHANNEL_JUNK_VANILLASYSTEM_COM */
  (void)token;
  return tm_provider_junk_vanillasystem_com_get_emails(email,
                                                             count);
}

static tm_email_t *tm_reg_get_130(const char *email, const char *token, int *count) {
  /* CHANNEL_SPAM_JASONPEARCE_COM */
  (void)token;
  return tm_provider_spam_jasonpearce_com_get_emails(email,
                                                           count);
}

static tm_email_t *tm_reg_get_131(const char *email, const char *token, int *count) {
  /* CHANNEL_FISH_SKYTALE_NET */
  (void)token;
  return tm_provider_fish_skytale_net_get_emails(email, count);
}

static tm_email_t *tm_reg_get_132(const char *email, const char *token, int *count) {
  /* CHANNEL_SPAM_MCCREW_COM */
  (void)token;
  return tm_provider_spam_mccrew_com_get_emails(email, count);
}

static tm_email_t *tm_reg_get_133(const char *email, const char *token, int *count) {
  /* CHANNEL_DROPMAIL_CLICK */
  return tm_provider_dropmail_click_get_emails(token,
                                                     email, count);
}

static tm_email_t *tm_reg_get_134(const char *email, const char *token, int *count) {
  /* CHANNEL_SPAM_COROIU_COM */
  (void)token;
  return tm_provider_spam_coroiu_com_get_emails(email, count);
}

static tm_email_t *tm_reg_get_135(const char *email, const char *token, int *count) {
  /* CHANNEL_SPAM_DELUSER_NET */
  (void)token;
  return tm_provider_spam_deluser_net_get_emails(email, count);
}

static tm_email_t *tm_reg_get_136(const char *email, const char *token, int *count) {
  /* CHANNEL_SPAM_DHSF_NET */
  (void)token;
  return tm_provider_spam_dhsf_net_get_emails(email, count);
}

static tm_email_t *tm_reg_get_137(const char *email, const char *token, int *count) {
  /* CHANNEL_SPAM_LUCATNT_COM */
  (void)token;
  return tm_provider_spam_lucatnt_com_get_emails(email, count);
}

static tm_email_t *tm_reg_get_138(const char *email, const char *token, int *count) {
  /* CHANNEL_SPAM_LYCEUM_LIFE_COM_RU */
  (void)token;
  return tm_provider_spam_lyceum_life_com_ru_get_emails(email,
                                                              count);
}

static tm_email_t *tm_reg_get_139(const char *email, const char *token, int *count) {
  /* CHANNEL_SPAM_NETPIRATES_NET */
  (void)token;
  return tm_provider_spam_netpirates_net_get_emails(email, count);
}

static tm_email_t *tm_reg_get_140(const char *email, const char *token, int *count) {
  /* CHANNEL_SPAM_NO_IP_NET */
  (void)token;
  return tm_provider_spam_no_ip_net_get_emails(email, count);
}

static tm_email_t *tm_reg_get_141(const char *email, const char *token, int *count) {
  /* CHANNEL_SPAM_OZH_ORG */
  (void)token;
  return tm_provider_spam_ozh_org_get_emails(email, count);
}

static tm_email_t *tm_reg_get_142(const char *email, const char *token, int *count) {
  /* CHANNEL_SPAM_PYPHUS_ORG */
  (void)token;
  return tm_provider_spam_pyphus_org_get_emails(email, count);
}

static tm_email_t *tm_reg_get_143(const char *email, const char *token, int *count) {
  /* CHANNEL_SPAM_SHEP_PW */
  (void)token;
  return tm_provider_spam_shep_pw_get_emails(email, count);
}

static tm_email_t *tm_reg_get_144(const char *email, const char *token, int *count) {
  /* CHANNEL_SPAM_WTF_AT */
  (void)token;
  return tm_provider_spam_wtf_at_get_emails(email, count);
}

static tm_email_t *tm_reg_get_145(const char *email, const char *token, int *count) {
  /* CHANNEL_SPAM_WULCZER_ORG */
  (void)token;
  return tm_provider_spam_wulczer_org_get_emails(email, count);
}

static tm_email_t *tm_reg_get_146(const char *email, const char *token, int *count) {
  /* CHANNEL_CRAP_KAKADUA_NET */
  (void)token;
  return tm_provider_crap_kakadua_net_get_emails(email, count);
}

static tm_email_t *tm_reg_get_147(const char *email, const char *token, int *count) {
  /* CHANNEL_SPAM_JANLUGT_NL */
  (void)token;
  return tm_provider_spam_janlugt_nl_get_emails(email, count);
}

static tm_email_t *tm_reg_get_148(const char *email, const char *token, int *count) {
  /* CHANNEL_MIN_BURNINGFISH_NET */
  (void)token;
  return tm_provider_min_burningfish_net_get_emails(email, count);
}

static tm_email_t *tm_reg_get_149(const char *email, const char *token, int *count) {
  /* CHANNEL_SINK_FBLAY_COM */
  (void)token;
  return tm_provider_sink_fblay_com_get_emails(email, count);
}

static tm_email_t *tm_reg_get_150(const char *email, const char *token, int *count) {
  /* CHANNEL_ETGDEV_DE */
  (void)token;
  return tm_provider_etgdev_de_get_emails(email, count);
}

static tm_email_t *tm_reg_get_151(const char *email, const char *token, int *count) {
  /* CHANNEL_MTMDEV_COM */
  (void)token;
  return tm_provider_mtmdev_com_get_emails(email, count);
}

static tm_email_t *tm_reg_get_152(const char *email, const char *token, int *count) {
  /* CHANNEL_TEST_UNERGIE_COM */
  (void)token;
  return tm_provider_test_unergie_com_get_emails(email, count);
}

static tm_email_t *tm_reg_get_153(const char *email, const char *token, int *count) {
  /* CHANNEL_BLOCK_BDEA_CC */
  (void)token;
  return tm_provider_block_bdea_cc_get_emails(email, count);
}

static tm_email_t *tm_reg_get_154(const char *email, const char *token, int *count) {
  /* CHANNEL_TORCH_YI_ORG */
  (void)token;
  return tm_provider_torch_yi_org_get_emails(email, count);
}

static tm_email_t *tm_reg_get_155(const char *email, const char *token, int *count) {
  /* CHANNEL_CARLTON183_CHANGEIP_NET */
  (void)token;
  return tm_provider_carlton183_changeip_net_get_emails(email,
                                                              count);
}

static tm_email_t *tm_reg_get_156(const char *email, const char *token, int *count) {
  /* CHANNEL_MAIL_FSMASH_ORG */
  (void)token;
  return tm_provider_mail_fsmash_org_get_emails(email, count);
}

static tm_email_t *tm_reg_get_157(const char *email, const char *token, int *count) {
  /* CHANNEL_EBS_COM_AR */
  (void)token;
  return tm_provider_ebs_com_ar_get_emails(email, count);
}

static tm_email_t *tm_reg_get_158(const char *email, const char *token, int *count) {
  /* CHANNEL_JAMA_TRENET_EU */
  (void)token;
  return tm_provider_jama_trenet_eu_get_emails(email, count);
}

static tm_email_t *tm_reg_get_159(const char *email, const char *token, int *count) {
  /* CHANNEL_BLACKHOLE_DJURBY_SE */
  (void)token;
  return tm_provider_blackhole_djurby_se_get_emails(email, count);
}

static tm_email_t *tm_reg_get_160(const char *email, const char *token, int *count) {
  /* CHANNEL_M8R_DAVIDFUHR_DE */
  (void)token;
  return tm_provider_m8r_davidfuhr_de_get_emails(email, count);
}

static tm_email_t *tm_reg_get_161(const char *email, const char *token, int *count) {
  /* CHANNEL_MI_MEON_BE */
  (void)token;
  return tm_provider_mi_meon_be_get_emails(email, count);
}

static tm_email_t *tm_reg_get_162(const char *email, const char *token, int *count) {
  /* CHANNEL_M_NIK_ME */
  (void)token;
  return tm_provider_m_nik_me_get_emails(email, count);
}

static tm_email_t *tm_reg_get_163(const char *email, const char *token, int *count) {
  /* CHANNEL_MAIL_BENTRASK_COM */
  (void)token;
  return tm_provider_mail_bentrask_com_get_emails(email, count);
}

static tm_email_t *tm_reg_get_164(const char *email, const char *token, int *count) {
  /* CHANNEL_T_ZIBET_NET */
  (void)token;
  return tm_provider_t_zibet_net_get_emails(email, count);
}

static tm_email_t *tm_reg_get_165(const char *email, const char *token, int *count) {
  /* CHANNEL_M8R_MCASAL_COM */
  (void)token;
  return tm_provider_m8r_mcasal_com_get_emails(email, count);
}

static tm_email_t *tm_reg_get_166(const char *email, const char *token, int *count) {
  /* CHANNEL_RAMJANE_MOOO_COM */
  (void)token;
  return tm_provider_ramjane_mooo_com_get_emails(email, count);
}

static tm_email_t *tm_reg_get_167(const char *email, const char *token, int *count) {
  /* CHANNEL_RAUXA_SENY_CAT */
  (void)token;
  return tm_provider_rauxa_seny_cat_get_emails(email, count);
}

static tm_email_t *tm_reg_get_168(const char *email, const char *token, int *count) {
  /* CHANNEL_SP_WOOT_AT */
  (void)token;
  return tm_provider_sp_woot_at_get_emails(email, count);
}

static tm_email_t *tm_reg_get_169(const char *email, const char *token, int *count) {
  /* CHANNEL_FWD2M_ESZETT_ES */
  (void)token;
  return tm_provider_fwd2m_eszett_es_get_emails(email, count);
}

static tm_email_t *tm_reg_get_170(const char *email, const char *token, int *count) {
  /* CHANNEL_M_887_AT */
  (void)token;
  return tm_provider_m_887_at_get_emails(email, count);
}

static tm_email_t *tm_reg_get_171(const char *email, const char *token, int *count) {
  /* CHANNEL_NOSPAM_THURSTONS_US */
  (void)token;
  return tm_provider_nospam_thurstons_us_get_emails(email, count);
}

static tm_email_t *tm_reg_get_172(const char *email, const char *token, int *count) {
  /* CHANNEL_NULL_K3VIN_NET */
  (void)token;
  return tm_provider_null_k3vin_net_get_emails(email, count);
}

static tm_email_t *tm_reg_get_173(const char *email, const char *token, int *count) {
  /* CHANNEL_REALLY_ISTRASH_COM */
  (void)token;
  return tm_provider_really_istrash_com_get_emails(email, count);
}

static tm_email_t *tm_reg_get_174(const char *email, const char *token, int *count) {
  /* CHANNEL_SPAM_HORTUK_OVH */
  (void)token;
  return tm_provider_spam_hortuk_ovh_get_emails(email, count);
}

static tm_email_t *tm_reg_get_175(const char *email, const char *token, int *count) {
  /* CHANNEL_16888888_CYOU */
  (void)token;
  return tm_provider_16888888_cyou_get_emails(email, count);
}

static tm_email_t *tm_reg_get_176(const char *email, const char *token, int *count) {
  /* CHANNEL_17666688_SHOP */
  (void)token;
  return tm_provider_17666688_shop_get_emails(email, count);
}

static tm_email_t *tm_reg_get_177(const char *email, const char *token, int *count) {
  /* CHANNEL_282MAIL_COM */
  (void)token;
  return tm_provider_282mail_com_get_emails(email, count);
}

static tm_email_t *tm_reg_get_178(const char *email, const char *token, int *count) {
  /* CHANNEL_BSDU32_BUZZ */
  (void)token;
  return tm_provider_bsdu32_buzz_get_emails(email, count);
}

static tm_email_t *tm_reg_get_179(const char *email, const char *token, int *count) {
  /* CHANNEL_DOXU243_BUZZ */
  (void)token;
  return tm_provider_doxu243_buzz_get_emails(email, count);
}

static tm_email_t *tm_reg_get_180(const char *email, const char *token, int *count) {
  /* CHANNEL_EASYME_PRO */
  (void)token;
  return tm_provider_easyme_pro_get_emails(email, count);
}

static tm_email_t *tm_reg_get_181(const char *email, const char *token, int *count) {
  /* CHANNEL_EVERGREENCO_SHOP */
  (void)token;
  return tm_provider_evergreenco_shop_get_emails(email, count);
}

static tm_email_t *tm_reg_get_182(const char *email, const char *token, int *count) {
  /* CHANNEL_LAYUEMING_PICS */
  (void)token;
  return tm_provider_layueming_pics_get_emails(email, count);
}

static tm_email_t *tm_reg_get_183(const char *email, const char *token, int *count) {
  /* CHANNEL_MINGYUEKEJI_ONLINE */
  (void)token;
  return tm_provider_mingyuekeji_online_get_emails(email, count);
}

static tm_email_t *tm_reg_get_184(const char *email, const char *token, int *count) {
  /* CHANNEL_MINGYUEMING_CLICK */
  (void)token;
  return tm_provider_mingyueming_click_get_emails(email, count);
}

static tm_email_t *tm_reg_get_185(const char *email, const char *token, int *count) {
  /* CHANNEL_MINGYUEMING_SHOP */
  (void)token;
  return tm_provider_mingyueming_shop_get_emails(email, count);
}

static tm_email_t *tm_reg_get_186(const char *email, const char *token, int *count) {
  /* CHANNEL_MINGYUKEJI_LOL */
  (void)token;
  return tm_provider_mingyukeji_lol_get_emails(email, count);
}

static tm_email_t *tm_reg_get_187(const char *email, const char *token, int *count) {
  /* CHANNEL_NUXH62_SPACE */
  (void)token;
  return tm_provider_nuxh62_space_get_emails(email, count);
}

static tm_email_t *tm_reg_get_188(const char *email, const char *token, int *count) {
  /* CHANNEL_PROID_CLOUD_IP_CC */
  (void)token;
  return tm_provider_proid_cloud_ip_cc_get_emails(email, count);
}

static tm_email_t *tm_reg_get_189(const char *email, const char *token, int *count) {
  /* CHANNEL_SBOOK_PICS */
  (void)token;
  return tm_provider_sbook_pics_get_emails(email, count);
}

static tm_email_t *tm_reg_get_190(const char *email, const char *token, int *count) {
  /* CHANNEL_XUE32_BUZZ */
  (void)token;
  return tm_provider_xue32_buzz_get_emails(email, count);
}

static tm_email_t *tm_reg_get_191(const char *email, const char *token, int *count) {
  /* CHANNEL_XKX_ME */
  if (!token) {
    *count = -1;
    return NULL;
  }
  return tm_provider_xkx_me_get_emails(token,
                                              email, count);
}

static tm_email_t *tm_reg_get_192(const char *email, const char *token, int *count) {
  /* CHANNEL_GONEBOX_EMAIL */
  (void)token;
  return tm_provider_gonebox_email_get_emails(email, count);
}

static tm_email_t *tm_reg_get_193(const char *email, const char *token, int *count) {
  /* CHANNEL_MAILCAT_AI */
  if (!token) {
    *count = -1;
    return NULL;
  }
  return tm_provider_mailcat_ai_get_emails(token,
                                                 email, count);
}

static tm_email_t *tm_reg_get_194(const char *email, const char *token, int *count) {
  /* CHANNEL_TEMPGO_EMAIL */
  if (!token) {
    *count = -1;
    return NULL;
  }
  return tm_provider_tempgo_email_get_emails(token,
                                                   email, count);
}

static tm_email_t *tm_reg_get_195(const char *email, const char *token, int *count) {
  /* CHANNEL_RESTMAIL_NET */
  (void)token;
  return tm_provider_restmail_net_get_emails(email, count);
}

static tm_email_t *tm_reg_get_196(const char *email, const char *token, int *count) {
  /* CHANNEL_B_SMELLY_CC */
  (void)token;
  return tm_provider_b_smelly_cc_get_emails(email, count);
}

static tm_email_t *tm_reg_get_197(const char *email, const char *token, int *count) {
  /* CHANNEL_DEA_SOON_IT */
  (void)token;
  return tm_provider_dea_soon_it_get_emails(email, count);
}

static tm_email_t *tm_reg_get_198(const char *email, const char *token, int *count) {
  /* CHANNEL_DISPOSABLE_AL_SUDANI_COM */
  (void)token;
  return tm_provider_disposable_al_sudani_com_get_emails(
          email, count);
}

static tm_email_t *tm_reg_get_199(const char *email, const char *token, int *count) {
  /* CHANNEL_DISPOSABLE_NOGONAD_NL */
  (void)token;
  return tm_provider_disposable_nogonad_nl_get_emails(email,
                                                            count);
}

static tm_email_t *tm_reg_get_200(const char *email, const char *token, int *count) {
  /* CHANNEL_J_FAIRUSE_ORG */
  (void)token;
  return tm_provider_j_fairuse_org_get_emails(email, count);
}

static tm_email_t *tm_reg_get_201(const char *email, const char *token, int *count) {
  /* CHANNEL_MN_CURPPA_COM */
  (void)token;
  return tm_provider_mn_curppa_com_get_emails(email, count);
}

static tm_email_t *tm_reg_get_202(const char *email, const char *token, int *count) {
  /* CHANNEL_MAILINATORZZ_MOOO_COM */
  (void)token;
  return tm_provider_mailinatorzz_mooo_com_get_emails(email,
                                                            count);
}

static tm_email_t *tm_reg_get_203(const char *email, const char *token, int *count) {
  /* CHANNEL_NOTFOND_404_MN */
  (void)token;
  return tm_provider_notfond_404_mn_get_emails(email, count);
}

static tm_email_t *tm_reg_get_204(const char *email, const char *token, int *count) {
  /* CHANNEL_TEMPGMAILER */
  (void)email;
  (void)token;
  *count = -1;
  return NULL;
}

static tm_email_t *tm_reg_get_205(const char *email, const char *token, int *count) {
  /* CHANNEL_TEMP_MAIL_ORG */
  (void)email;
  (void)token;
  *count = -1;
  return NULL;
}

static tm_email_t *tm_reg_get_206(const char *email, const char *token, int *count) {
  /* default */
  (void)email;
  (void)token;
  *count = -1;
  return NULL;
}

static tm_email_t *tm_reg_get_207(const char *email, const char *token, int *count) {
  /* CHANNEL_DROPMAIL_ME */
  if (!token) {
    *count = -1;
    return NULL;
  }
  return tm_provider_dropmail_me_get_emails(token, email, count);
}

static tm_email_t *tm_reg_get_208(const char *email, const char *token, int *count) {
  /* CHANNEL_TEN_MINUTE_MAIL_NET */
  if (!token) {
    *count = -1;
    return NULL;
  }
  return tm_provider_ten_minute_mail_net_get_emails(token, email, count);
}

/* ========== 有序渠道注册表（顺序即 baseline / listChannels 顺序） ========== */
static const tm_channel_spec_t g_channel_registry[] = {
    {CHANNEL_TEMPMAIL, "TempMail", "tempmail.ing", tm_reg_gen_0, tm_reg_get_0},
    {CHANNEL_TEMPMAIL_CN, "TempMail CN", "tempmail.cn", tm_reg_gen_1, tm_reg_get_1},
    {CHANNEL_TA_EASY, "TA Easy", "ta-easy.com", tm_reg_gen_16, tm_reg_get_14},
    {CHANNEL_10MINUTE_ONE, "10 Minute Email", "10minutemail.one", tm_reg_gen_32, tm_reg_get_16},
    {CHANNEL_XGHFF_COM, "xghff.com", "10minutemail.one", tm_reg_gen_33, tm_reg_get_16},
    {CHANNEL_OQQAJ_COM, "oqqaj.com", "10minutemail.one", tm_reg_gen_34, tm_reg_get_16},
    {CHANNEL_PSOVV_COM, "psovv.com", "10minutemail.one", tm_reg_gen_35, tm_reg_get_16},
    {CHANNEL_DBWOT_COM, "dbwot.com", "10minutemail.one", tm_reg_gen_36, tm_reg_get_16},
    {CHANNEL_YGWPR_COM, "ygwpr.com", "10minutemail.one", tm_reg_gen_37, tm_reg_get_16},
    {CHANNEL_IMXWE_COM, "imxwe.com", "10minutemail.one", tm_reg_gen_38, tm_reg_get_16},
    {CHANNEL_LINSHIYOU, "临时邮", "linshiyou.com", tm_reg_gen_2, tm_reg_get_2},
    {CHANNEL_MFFAC, "MFFAC", "mffac.com", tm_reg_gen_15, tm_reg_get_13},
    {CHANNEL_TEMPMAIL_LOL, "TempMail LOL", "tempmail.lol", tm_reg_gen_3, tm_reg_get_3},
    {CHANNEL_CHATGPT_ORG_UK, "ChatGPT Mail", "mail.chatgpt.org.uk", tm_reg_gen_4, tm_reg_get_4},
    {CHANNEL_TEMP_MAIL_IO, "Temp-Mail.io", "temp-mail.io", tm_reg_gen_79, tm_reg_get_48},
    {CHANNEL_MAIL_CX, "Mail.cx", "mail.cx", tm_reg_gen_80, tm_reg_get_49},
    {CHANNEL_DDKER_COM, "ddker.com", "mail.cx", tm_reg_gen_81, tm_reg_get_49},
    {CHANNEL_CATCHMAIL, "Catchmail", "catchmail.io", tm_reg_gen_82, tm_reg_get_50},
    {CHANNEL_CATCHMAIL_MAILISTRY, "Catchmail Mailistry", "mailistry.com", tm_reg_gen_83, tm_reg_get_51},
    {CHANNEL_CATCHMAIL_ZEPPOST, "Catchmail Zeppost", "zeppost.com", tm_reg_gen_84, tm_reg_get_52},
    {CHANNEL_MAILFORSPAM, "MailForSpam", "mailforspam.com", tm_reg_gen_85, tm_reg_get_53},
    {CHANNEL_MAILFORSPAM_TEMPMAIL_IO, "MailForSpam TempMail.io", "tempmail.io", tm_reg_gen_86, tm_reg_get_54},
    {CHANNEL_MAILFORSPAM_DISPOSABLE, "MailForSpam Disposable", "disposable.email", tm_reg_gen_87, tm_reg_get_55},
    {CHANNEL_TEMPMAILC, "TempMailC", "tempmailc.com", tm_reg_gen_88, tm_reg_get_56},
    {CHANNEL_MAILNESIA, "Mailnesia", "mailnesia.com", tm_reg_gen_89, tm_reg_get_57},
    {CHANNEL_THROWAWAYMAIL, "ThrowawayMail", "throwawaymail.app", tm_reg_gen_90, tm_reg_get_58},
    {CHANNEL_TEMPMAIL_FISH, "TempMail Fish", "tempmail.fish", tm_reg_gen_91, tm_reg_get_59},
    {CHANNEL_NEIGHBOURS_SH, "Neighbours", "neighbours.sh", tm_reg_gen_92, tm_reg_get_60},
    {CHANNEL_SHITTY_EMAIL, "shitty.email", "shitty.email", tm_reg_gen_93, tm_reg_get_61},
    {CHANNEL_TEMPMAILPRO, "TempMail Pro", "tempmailpro.us", tm_reg_gen_94, tm_reg_get_62},
    {CHANNEL_DEVMAIL_UK, "DevMail UK", "devmail.uk", tm_reg_gen_95, tm_reg_get_63},
    {CHANNEL_INBOXKITTEN, "InboxKitten", "inboxkitten.com", tm_reg_gen_97, tm_reg_get_65},
    {CHANNEL_CLEANTEMPMAIL, "CleanTempMail", "cleantempmail.com", tm_reg_gen_96, tm_reg_get_64},
    {CHANNEL_GETNADA, "GetNada", "getnada.net", tm_reg_gen_98, tm_reg_get_66},
    {CHANNEL_ONE_VPN_NET, "1vpn.net", "getnada.net", tm_reg_gen_99, tm_reg_get_66},
    {CHANNEL_ABEMATV_COM, "abematv.com", "getnada.net", tm_reg_gen_100, tm_reg_get_66},
    {CHANNEL_ABEMATV_NET, "abematv.net", "getnada.net", tm_reg_gen_101, tm_reg_get_66},
    {CHANNEL_ABEMATV_ORG, "abematv.org", "getnada.net", tm_reg_gen_102, tm_reg_get_66},
    {CHANNEL_ACEH_CC, "aceh.cc", "getnada.net", tm_reg_gen_103, tm_reg_get_66},
    {CHANNEL_BANGKABELITUNG_NET, "bangkabelitung.net", "getnada.net", tm_reg_gen_104, tm_reg_get_66},
    {CHANNEL_CCTRUYEN_COM, "cctruyen.com", "getnada.net", tm_reg_gen_105, tm_reg_get_66},
    {CHANNEL_GETNADA_COM, "getnada.com", "getnada.net", tm_reg_gen_106, tm_reg_get_66},
    {CHANNEL_GETNADA_EMAIL, "getnada.email", "getnada.net", tm_reg_gen_107, tm_reg_get_66},
    {CHANNEL_GETNADA_NET, "getnada.net", "getnada.net", tm_reg_gen_108, tm_reg_get_66},
    {CHANNEL_JAWATENGAH_NET, "jawatengah.net", "getnada.net", tm_reg_gen_109, tm_reg_get_66},
    {CHANNEL_JAWATIMUR_NET, "jawatimur.net", "getnada.net", tm_reg_gen_110, tm_reg_get_66},
    {CHANNEL_KALIMANTANBARAT_NET, "kalimantanbarat.net", "getnada.net", tm_reg_gen_111, tm_reg_get_66},
    {CHANNEL_KALIMANTANSELATAN_NET, "kalimantanselatan.net", "getnada.net", tm_reg_gen_112, tm_reg_get_66},
    {CHANNEL_KALIMANTANTENGAH_NET, "kalimantantengah.net", "getnada.net", tm_reg_gen_113, tm_reg_get_66},
    {CHANNEL_KALIMANTANTIMUR_NET, "kalimantantimur.net", "getnada.net", tm_reg_gen_114, tm_reg_get_66},
    {CHANNEL_KALIMANTANUTARA_NET, "kalimantanutara.net", "getnada.net", tm_reg_gen_115, tm_reg_get_66},
    {CHANNEL_KEPULAUANRIAU_NET, "kepulauanriau.net", "getnada.net", tm_reg_gen_116, tm_reg_get_66},
    {CHANNEL_LUXURY345_COM, "luxury345.com", "getnada.net", tm_reg_gen_117, tm_reg_get_66},
    {CHANNEL_MALUKUUTARA_NET, "malukuutara.net", "getnada.net", tm_reg_gen_118, tm_reg_get_66},
    {CHANNEL_NUSATENGGARABARAT_NET, "nusatenggarabarat.net", "getnada.net", tm_reg_gen_119, tm_reg_get_66},
    {CHANNEL_NUSATENGGARATIMUR_NET, "nusatenggaratimur.net", "getnada.net", tm_reg_gen_120, tm_reg_get_66},
    {CHANNEL_PAPUABARAT_NET, "papuabarat.net", "getnada.net", tm_reg_gen_121, tm_reg_get_66},
    {CHANNEL_PAPUABARATDAYA_NET, "papuabaratdaya.net", "getnada.net", tm_reg_gen_122, tm_reg_get_66},
    {CHANNEL_PAPUASELATAN_NET, "papuaselatan.net", "getnada.net", tm_reg_gen_123, tm_reg_get_66},
    {CHANNEL_PEHOL_COM, "pehol.com", "getnada.net", tm_reg_gen_124, tm_reg_get_66},
    {CHANNEL_PTRUYEN_COM, "ptruyen.com", "getnada.net", tm_reg_gen_125, tm_reg_get_66},
    {CHANNEL_PULAUBALI_NET, "pulaubali.net", "getnada.net", tm_reg_gen_126, tm_reg_get_66},
    {CHANNEL_RIAU_NET, "riau.net", "getnada.net", tm_reg_gen_127, tm_reg_get_66},
    {CHANNEL_SEOKEY_ORG, "seokey.org", "getnada.net", tm_reg_gen_128, tm_reg_get_66},
    {CHANNEL_SULAWESIBARAT_NET, "sulawesibarat.net", "getnada.net", tm_reg_gen_129, tm_reg_get_66},
    {CHANNEL_SULAWESISELATAN_NET, "sulawesiselatan.net", "getnada.net", tm_reg_gen_130, tm_reg_get_66},
    {CHANNEL_SULAWESITENGAH_NET, "sulawesitengah.net", "getnada.net", tm_reg_gen_131, tm_reg_get_66},
    {CHANNEL_SULAWESITENGGARA_NET, "sulawesitenggara.net", "getnada.net", tm_reg_gen_132, tm_reg_get_66},
    {CHANNEL_SUMATERABARAT_NET, "sumaterabarat.net", "getnada.net", tm_reg_gen_133, tm_reg_get_66},
    {CHANNEL_SUMATERASELATAN_NET, "sumateraselatan.net", "getnada.net", tm_reg_gen_134, tm_reg_get_66},
    {CHANNEL_SUMATERAUTARA_NET, "sumaterautara.net", "getnada.net", tm_reg_gen_135, tm_reg_get_66},
    {CHANNEL_VILLATOGEL_COM, "villatogel.com", "getnada.net", tm_reg_gen_136, tm_reg_get_66},
    {CHANNEL_MAIL123, "Mail123", "mail123.fr", tm_reg_gen_137, tm_reg_get_67},
    {CHANNEL_MAIL10S, "Mail10s", "mail10s.com", tm_reg_gen_138, tm_reg_get_68},
    {CHANNEL_WEBMAILTEMP, "WebMailTemp", "webmailtemp.com", tm_reg_gen_139, tm_reg_get_69},
    {CHANNEL_TEMPFASTMAIL, "TempFastMail", "tempfastmail.com", tm_reg_gen_140, tm_reg_get_70},
    {CHANNEL_ONE_SEC_MAIL, "1SecMail", "1sec-mail.com", tm_reg_gen_141, tm_reg_get_71},
    {CHANNEL_FAKEMAIL, "FakeMail", "fakemail.net", tm_reg_gen_142, tm_reg_get_72},
    {CHANNEL_OPENINBOX, "OpenInbox", "openinbox.io", tm_reg_gen_143, tm_reg_get_73},
    {CHANNEL_INBOXES, "Inboxes", "inboxes.com", tm_reg_gen_144, tm_reg_get_74},
    {CHANNEL_UNCORREOTEMPORAL, "UnCorreoTemporal", "uncorreotemporal.com", tm_reg_gen_145, tm_reg_get_75},
    {CHANNEL_AWAMAIL, "AwaMail", "awamail.com", tm_reg_gen_5, tm_reg_get_5},
    {CHANNEL_MAIL_TM, "Mail.tm", "mail.tm", tm_reg_gen_6, tm_reg_get_6},
    {CHANNEL_WEB_LIBRARY_NET, "web-library.net", "mail.tm", tm_reg_gen_146, tm_reg_get_6},
    {CHANNEL_DROPMAIL, "DropMail", "dropmail.me", tm_reg_gen_7, tm_reg_get_7},
    {CHANNEL_GUERRILLAMAIL, "Guerrilla Mail", "guerrillamail.com", tm_reg_gen_8, tm_reg_get_8},
    {CHANNEL_GUERRILLAMAIL_COM, "GuerrillaMail Root", "guerrillamail.com", tm_reg_gen_73, tm_reg_get_42},
    {CHANNEL_MAILDROP, "Maildrop", "maildrop.cx", tm_reg_gen_9, tm_reg_get_9},
    {CHANNEL_SMAIL_PW, "Smail.pw", "smail.pw", tm_reg_gen_10, tm_reg_get_10},
    {CHANNEL_VIP_215, "VIP 215", "vip.215.im", tm_reg_gen_11, tm_reg_get_11},
    {CHANNEL_FAKE_LEGAL, "Fake Legal", "fake.legal", tm_reg_gen_12, tm_reg_get_12},
    {CHANNEL_IMGUI_DE, "imgui.de", "fake.legal", tm_reg_gen_13, tm_reg_get_12},
    {CHANNEL_PULSEWEBMENU_DE, "pulsewebmenu.de", "fake.legal", tm_reg_gen_14, tm_reg_get_12},
    {CHANNEL_MOAKT, "Moakt", "moakt.com", tm_reg_gen_17, tm_reg_get_15},
    {CHANNEL_DRMAIL_IN, "drmail.in", "moakt.com", tm_reg_gen_18, tm_reg_get_15},
    {CHANNEL_TEML_NET, "teml.net", "moakt.com", tm_reg_gen_19, tm_reg_get_15},
    {CHANNEL_TMPEML_COM, "tmpeml.com", "moakt.com", tm_reg_gen_20, tm_reg_get_15},
    {CHANNEL_TMPBOX_NET, "tmpbox.net", "moakt.com", tm_reg_gen_21, tm_reg_get_15},
    {CHANNEL_MOAKT_CC, "moakt.cc", "moakt.com", tm_reg_gen_22, tm_reg_get_15},
    {CHANNEL_DISBOX_NET, "disbox.net", "moakt.com", tm_reg_gen_23, tm_reg_get_15},
    {CHANNEL_TMPMAIL_ORG, "tmpmail.org", "moakt.com", tm_reg_gen_24, tm_reg_get_15},
    {CHANNEL_TMPMAIL_NET, "tmpmail.net", "moakt.com", tm_reg_gen_25, tm_reg_get_15},
    {CHANNEL_TMAILS_NET, "tmails.net", "moakt.com", tm_reg_gen_26, tm_reg_get_15},
    {CHANNEL_DISBOX_ORG, "disbox.org", "moakt.com", tm_reg_gen_27, tm_reg_get_15},
    {CHANNEL_MOAKT_CO, "moakt.co", "moakt.com", tm_reg_gen_28, tm_reg_get_15},
    {CHANNEL_MOAKT_WS, "moakt.ws", "moakt.com", tm_reg_gen_29, tm_reg_get_15},
    {CHANNEL_TMAIL_WS, "tmail.ws", "moakt.com", tm_reg_gen_30, tm_reg_get_15},
    {CHANNEL_BAREED_WS, "bareed.ws", "moakt.com", tm_reg_gen_31, tm_reg_get_15},
    {CHANNEL_EMAIL10MIN, "Email10Min", "email10min.com", tm_reg_gen_39, tm_reg_get_17},
    {CHANNEL_MJJ_CM, "MJJ Mail", "mjj.cm", tm_reg_gen_40, tm_reg_get_18},
    {CHANNEL_LINSHI_CO, "临时Co", "linshi.co", tm_reg_gen_41, tm_reg_get_19},
    {CHANNEL_HARAKIRIMAIL, "HarakiriMail", "harakirimail.com", tm_reg_gen_42, tm_reg_get_20},
    {CHANNEL_JQKJQK_XYZ, "jqkjqk.xyz", "mail.zhujump.com", tm_reg_gen_43, tm_reg_get_21},
    {CHANNEL_LYHLEVI_COM, "LyhLevi MoeMail", "lyhlevi.com", tm_reg_gen_44, tm_reg_get_21},
    {CHANNEL_TEMPMAIL_PLUS, "TempMail Plus", "tempmail.plus", tm_reg_gen_45, tm_reg_get_22},
    {CHANNEL_FEXPOST_COM, "fexpost.com", "tempmail.plus", tm_reg_gen_46, tm_reg_get_22},
    {CHANNEL_FEXBOX_ORG, "fexbox.org", "tempmail.plus", tm_reg_gen_47, tm_reg_get_22},
    {CHANNEL_MAILBOX_IN_UA, "mailbox.in.ua", "tempmail.plus", tm_reg_gen_48, tm_reg_get_22},
    {CHANNEL_ROVER_INFO, "rover.info", "tempmail.plus", tm_reg_gen_49, tm_reg_get_22},
    {CHANNEL_CHITTHI_IN, "chitthi.in", "tempmail.plus", tm_reg_gen_50, tm_reg_get_22},
    {CHANNEL_FEXTEMP_COM, "fextemp.com", "tempmail.plus", tm_reg_gen_51, tm_reg_get_22},
    {CHANNEL_ANY_PINK, "any.pink", "tempmail.plus", tm_reg_gen_52, tm_reg_get_22},
    {CHANNEL_MEREPOST_COM, "merepost.com", "tempmail.plus", tm_reg_gen_53, tm_reg_get_22},
    {CHANNEL_TEMPMAIL_LOL_V2, "TempMail LOL V2", "tempmail.lol", tm_reg_gen_54, tm_reg_get_23},
    {CHANNEL_TEMPGBOX, "TempGBox", "tempgbox.net", tm_reg_gen_55, tm_reg_get_24},
    {CHANNEL_EMAILNATOR, "Emailnator", "emailnator.com", tm_reg_gen_56, tm_reg_get_25},
    {CHANNEL_TEMPORAM, "Temporam", "temporam.com", tm_reg_gen_57, tm_reg_get_26},
    {CHANNEL_NEIGHBOURS, "Neighbours", "neighbours.sh", tm_reg_gen_58, tm_reg_get_27},
    {CHANNEL_SHARKLASERS, "SharkLasers", "sharklasers.com", tm_reg_gen_68, tm_reg_get_37},
    {CHANNEL_SHARKLASERS_COM, "SharkLasers Root", "sharklasers.com", tm_reg_gen_69, tm_reg_get_38},
    {CHANNEL_GRR_LA, "Grr.la", "grr.la", tm_reg_gen_70, tm_reg_get_39},
    {CHANNEL_GRR_LA_COM, "Grr.la Root", "grr.la", tm_reg_gen_71, tm_reg_get_40},
    {CHANNEL_GUERRILLAMAIL_INFO, "GuerrillaMail Info", "guerrillamail.info", tm_reg_gen_72, tm_reg_get_41},
    {CHANNEL_SPAM4ME, "Spam4.me", "spam4.me", tm_reg_gen_74, tm_reg_get_43},
    {CHANNEL_GUERRILLAMAIL_NET, "GuerrillaMail Net", "guerrillamail.net", tm_reg_gen_75, tm_reg_get_44},
    {CHANNEL_GUERRILLAMAIL_ORG, "GuerrillaMail Org", "guerrillamail.org", tm_reg_gen_76, tm_reg_get_45},
    {CHANNEL_GUERRILLAMAILBLOCK, "GuerrillaMailBlock", "guerrillamailblock.com", tm_reg_gen_77, tm_reg_get_46},
    {CHANNEL_GUERRILLAMAIL_COM_WWW, "GuerrillaMail WWW", "guerrillamail.com", tm_reg_gen_78, tm_reg_get_47},
    {CHANNEL_M2U, "MailToYou", "m2u.io", tm_reg_gen_59, tm_reg_get_28},
    {CHANNEL_TEMPY_EMAIL, "Tempy Email", "tempy.email", tm_reg_gen_60, tm_reg_get_29},
    {CHANNEL_FMAIL, "FMail", "fmail.men", tm_reg_gen_61, tm_reg_get_30},
    {CHANNEL_OCKITO, "Ockito", "ockito.com", tm_reg_gen_62, tm_reg_get_31},
    {CHANNEL_ANONBOX, "Anonbox", "anonbox.net", tm_reg_gen_63, tm_reg_get_32},
    {CHANNEL_DUCKMAIL, "DuckMail", "duckmail.sbs", tm_reg_gen_64, tm_reg_get_33},
    {CHANNEL_MAILINATOR, "Mailinator", "mailinator.com", tm_reg_gen_65, tm_reg_get_34},
    {CHANNEL_TEMPMAIL365, "Tempmail365", "https://tempmail365.cn", tm_reg_gen_66, tm_reg_get_35},
    {CHANNEL_TEMPINBOX, "TempInbox", "https://www.tempinbox.xyz", tm_reg_gen_67, tm_reg_get_36},
    {CHANNEL_BYOM, "Byom", "byom.de", tm_reg_gen_147, tm_reg_get_76},
    {CHANNEL_ANONYMMAIL, "AnonymMail", "anonymmail.net", tm_reg_gen_148, tm_reg_get_77},
    {CHANNEL_EYEPASTE, "EyePaste", "eyepaste.com", tm_reg_gen_149, tm_reg_get_78},
    {CHANNEL_MAIL_SUNLS, "Mail Sunls", "mail.sunls.de", tm_reg_gen_150, tm_reg_get_79},
    {CHANNEL_EXPRESSINBOXHUB, "ExpressInboxHub", "expressinboxhub.com", tm_reg_gen_151, tm_reg_get_80},
    {CHANNEL_LROID, "Lroid", "lroid.com", tm_reg_gen_152, tm_reg_get_81},
    {CHANNEL_HARIBU, "Haribu", "haribu.net", tm_reg_gen_153, tm_reg_get_82},
    {CHANNEL_ROOTSH, "Rootsh(BccTo)", "rootsh.com", tm_reg_gen_154, tm_reg_get_83},
    {CHANNEL_FAKE_EMAIL_SITE, "FakeEmailSite", "fake-email.site", tm_reg_gen_155, tm_reg_get_84},
    {CHANNEL_MOHMAL, "Mohmal", "mohmal.com", tm_reg_gen_156, tm_reg_get_85},
    {CHANNEL_MAILGOLEM, "MailGolem", "mailgolem.com", tm_reg_gen_157, tm_reg_get_86},
    {CHANNEL_BEST_TEMP_MAIL, "BestTempMail", "best-temp-mail.com", tm_reg_gen_158, tm_reg_get_87},
    {CHANNEL_DISPOSABLEMAIL_APP, "DisposableMail", "disposablemail.app", tm_reg_gen_159, tm_reg_get_88},
    {CHANNEL_MAILTEMP_CC, "MailTemp.cc", "mailtemp.cc", tm_reg_gen_160, tm_reg_get_89},
    {CHANNEL_MINUTEINBOX, "MinuteInbox", "minuteinbox.com", tm_reg_gen_161, tm_reg_get_90},
    {CHANNEL_MAILCATCH, "MailCatch", "mailcatch.com", tm_reg_gen_162, tm_reg_get_91},
    {CHANNEL_TEMPEMAIL_CO, "TempEmail.co", "tempemail.co", tm_reg_gen_163, tm_reg_get_92},
    {CHANNEL_TEMPEMAILS_NET, "TempEmails.net", "tempemails.net", tm_reg_gen_164, tm_reg_get_93},
    {CHANNEL_ALTMAILS, "AltMails", "tempmail.altmails.com", tm_reg_gen_165, tm_reg_get_94},
    {CHANNEL_TEMPEMAIL_INFO, "TempEmailInfo", "tempemail.info", tm_reg_gen_166, tm_reg_get_95},
    {CHANNEL_SMAILPRO, "SmailPro", "smailpro.com", tm_reg_gen_167, tm_reg_get_96},
    {CHANNEL_TEMPMAILTEN, "TempMailTen", "tempmailten.com", tm_reg_gen_168, tm_reg_get_97},
    {CHANNEL_MAILDROP_CC, "MailDrop.cc", "maildrop.cc", tm_reg_gen_169, tm_reg_get_98},
    {CHANNEL_TENMINUTEMAIL_NET, "10MinuteMail.net", "10minutemail.net", tm_reg_gen_170, tm_reg_get_99},
    {CHANNEL_LINSHIYOUXIANG_NET, "LinshiYouxiang", "linshiyouxiang.net", tm_reg_gen_171, tm_reg_get_100},
    {CHANNEL_TEMPMAIL_FYI, "TempMail.fyi", "temp-mail.fyi", tm_reg_gen_172, tm_reg_get_101},
    {CHANNEL_DISPOSABLEMAIL_COM, "DisposableMail.com", "disposablemail.com", tm_reg_gen_173, tm_reg_get_102},
    {CHANNEL_TEMPP_MAILS, "TemppMails", "tempp-mails.com", tm_reg_gen_174, tm_reg_get_103},
    {CHANNEL_EMAILTEMP_ORG, "EmailTemp", "emailtemp.org", tm_reg_gen_175, tm_reg_get_104},
    {CHANNEL_MYTEMPMAIL_CC, "MyTempMail.cc", "mytempmail.cc", tm_reg_gen_176, tm_reg_get_105},
    {CHANNEL_TEMP_MAIL_NOW, "TempMailNow", "temp-mail.now", tm_reg_gen_177, tm_reg_get_106},
    {CHANNEL_MAIL_TD, "MailTd", "mail.td", tm_reg_gen_178, tm_reg_get_107},
    {CHANNEL_MAILHOLE_DE, "Mailhole.de", "mailhole.de", tm_reg_gen_179, tm_reg_get_108},
    {CHANNEL_TMAIL_LINK, "TMail.link", "tmail.link", tm_reg_gen_180, tm_reg_get_109},
    {CHANNEL_24MAIL_CHACUO, "24Mail Chacuo", "24mail.chacuo.net", tm_reg_gen_181, tm_reg_get_110},
    {CHANNEL_NIMAIL, "Nimail", "nimail.cn", tm_reg_gen_182, tm_reg_get_111},
    {CHANNEL_FREECUSTOM, "FreeCustom.Email", "freecustom.email", tm_reg_gen_183, tm_reg_get_112},
    {CHANNEL_16888888_CYOU, "16888888.cyou", "mailmomy.com", tm_reg_gen_246, tm_reg_get_175},
    {CHANNEL_17666688_SHOP, "17666688.shop", "mailmomy.com", tm_reg_gen_247, tm_reg_get_176},
    {CHANNEL_282MAIL_COM, "282mail.com", "mailmomy.com", tm_reg_gen_248, tm_reg_get_177},
    {CHANNEL_BLACKHOLE_DJURBY_SE, "Mailinator (blackhole.djurby.se)", "mailinator.com", tm_reg_gen_230, tm_reg_get_159},
    {CHANNEL_BLOCK_BDEA_CC, "Mailinator (block.bdea.cc)", "mailinator.com", tm_reg_gen_224, tm_reg_get_153},
    {CHANNEL_BSDU32_BUZZ, "bsdu32.buzz", "mailmomy.com", tm_reg_gen_249, tm_reg_get_178},
    {CHANNEL_B_SMELLY_CC, "Mailinator (b.smelly.cc)", "mailinator.com", tm_reg_gen_267, tm_reg_get_196},
    {CHANNEL_CARLTON183_CHANGEIP_NET, "Mailinator (183carlton.changeip.net)", "mailinator.com", tm_reg_gen_226, tm_reg_get_155},
    {CHANNEL_DEA_SOON_IT, "Mailinator (dea.soon.it)", "mailinator.com", tm_reg_gen_268, tm_reg_get_197},
    {CHANNEL_DISPOSABLE_AL_SUDANI_COM, "Mailinator (disposable.al-sudani.com)", "mailinator.com", tm_reg_gen_269, tm_reg_get_198},
    {CHANNEL_DISPOSABLE_NOGONAD_NL, "Mailinator (disposable.nogonad.nl)", "mailinator.com", tm_reg_gen_270, tm_reg_get_199},
    {CHANNEL_DOXU243_BUZZ, "doxu243.buzz", "mailmomy.com", tm_reg_gen_250, tm_reg_get_179},
    {CHANNEL_EASYME_PRO, "easyme.pro", "mailmomy.com", tm_reg_gen_251, tm_reg_get_180},
    {CHANNEL_EBS_COM_AR, "Mailinator (ebs.com.ar)", "mailinator.com", tm_reg_gen_228, tm_reg_get_157},
    {CHANNEL_ETGDEV_DE, "Mailinator (etgdev.de)", "mailinator.com", tm_reg_gen_221, tm_reg_get_150},
    {CHANNEL_EVERGREENCO_SHOP, "evergreenco.shop", "mailmomy.com", tm_reg_gen_252, tm_reg_get_181},
    {CHANNEL_FWD2M_ESZETT_ES, "Mailinator (fwd2m.eszett.es)", "mailinator.com", tm_reg_gen_240, tm_reg_get_169},
    {CHANNEL_JAMA_TRENET_EU, "Mailinator (jama.trenet.eu)", "mailinator.com", tm_reg_gen_229, tm_reg_get_158},
    {CHANNEL_J_FAIRUSE_ORG, "Mailinator (j.fairuse.org)", "mailinator.com", tm_reg_gen_271, tm_reg_get_200},
    {CHANNEL_LAYUEMING_PICS, "layueming.pics", "mailmomy.com", tm_reg_gen_253, tm_reg_get_182},
    {CHANNEL_M_887_AT, "Mailinator (m.887.at)", "mailinator.com", tm_reg_gen_241, tm_reg_get_170},
    {CHANNEL_M8R_DAVIDFUHR_DE, "Mailinator (m8r.davidfuhr.de)", "mailinator.com", tm_reg_gen_231, tm_reg_get_160},
    {CHANNEL_M8R_MCASAL_COM, "Mailinator (m8r.mcasal.com)", "mailinator.com", tm_reg_gen_236, tm_reg_get_165},
    {CHANNEL_MAIL_BENTRASK_COM, "Mailinator (mail.bentrask.com)", "mailinator.com", tm_reg_gen_234, tm_reg_get_163},
    {CHANNEL_MAIL_FSMASH_ORG, "Mailinator (mail.fsmash.org)", "mailinator.com", tm_reg_gen_227, tm_reg_get_156},
    {CHANNEL_MAILINATORZZ_MOOO_COM, "Mailinator (mailinatorzz.mooo.com)", "mailinator.com", tm_reg_gen_273, tm_reg_get_202},
    {CHANNEL_MI_MEON_BE, "Mailinator (mi.meon.be)", "mailinator.com", tm_reg_gen_232, tm_reg_get_161},
    {CHANNEL_MINGYUEKEJI_ONLINE, "mingyuekeji.online", "mailmomy.com", tm_reg_gen_254, tm_reg_get_183},
    {CHANNEL_MINGYUEMING_CLICK, "mingyueming.click", "mailmomy.com", tm_reg_gen_255, tm_reg_get_184},
    {CHANNEL_MINGYUEMING_SHOP, "mingyueming.shop", "mailmomy.com", tm_reg_gen_256, tm_reg_get_185},
    {CHANNEL_MINGYUKEJI_LOL, "mingyukeji.lol", "mailmomy.com", tm_reg_gen_257, tm_reg_get_186},
    {CHANNEL_MN_CURPPA_COM, "Mailinator (mn.curppa.com)", "mailinator.com", tm_reg_gen_272, tm_reg_get_201},
    {CHANNEL_M_NIK_ME, "Mailinator (m.nik.me)", "mailinator.com", tm_reg_gen_233, tm_reg_get_162},
    {CHANNEL_MTMDEV_COM, "Mailinator (mtmdev.com)", "mailinator.com", tm_reg_gen_222, tm_reg_get_151},
    {CHANNEL_NOSPAM_THURSTONS_US, "Mailinator (nospam.thurstons.us)", "mailinator.com", tm_reg_gen_242, tm_reg_get_171},
    {CHANNEL_NOTFOND_404_MN, "Mailinator (notfond.404.mn)", "mailinator.com", tm_reg_gen_274, tm_reg_get_203},
    {CHANNEL_NULL_K3VIN_NET, "Mailinator (null.k3vin.net)", "mailinator.com", tm_reg_gen_243, tm_reg_get_172},
    {CHANNEL_NUXH62_SPACE, "nuxh62.space", "mailmomy.com", tm_reg_gen_258, tm_reg_get_187},
    {CHANNEL_PROID_CLOUD_IP_CC, "proid.cloud-ip.cc", "mailmomy.com", tm_reg_gen_259, tm_reg_get_188},
    {CHANNEL_RAMJANE_MOOO_COM, "Mailinator (ramjane.mooo.com)", "mailinator.com", tm_reg_gen_237, tm_reg_get_166},
    {CHANNEL_RAUXA_SENY_CAT, "Mailinator (rauxa.seny.cat)", "mailinator.com", tm_reg_gen_238, tm_reg_get_167},
    {CHANNEL_REALLY_ISTRASH_COM, "Mailinator (really.istrash.com)", "mailinator.com", tm_reg_gen_244, tm_reg_get_173},
    {CHANNEL_SBOOK_PICS, "sbook.pics", "mailmomy.com", tm_reg_gen_260, tm_reg_get_189},
    {CHANNEL_SPAM_HORTUK_OVH, "Mailinator (spam.hortuk.ovh)", "mailinator.com", tm_reg_gen_245, tm_reg_get_174},
    {CHANNEL_SP_WOOT_AT, "Mailinator (sp.woot.at)", "mailinator.com", tm_reg_gen_239, tm_reg_get_168},
    {CHANNEL_TEST_UNERGIE_COM, "Mailinator (test.unergie.com)", "mailinator.com", tm_reg_gen_223, tm_reg_get_152},
    {CHANNEL_TORCH_YI_ORG, "Mailinator (torch.yi.org)", "mailinator.com", tm_reg_gen_225, tm_reg_get_154},
    {CHANNEL_T_ZIBET_NET, "Mailinator (t.zibet.net)", "mailinator.com", tm_reg_gen_235, tm_reg_get_164},
    {CHANNEL_XUE32_BUZZ, "xue32.buzz", "mailmomy.com", tm_reg_gen_261, tm_reg_get_190},
    {CHANNEL_APIHZ, "ApiHz TempMail", "apihz.cn", tm_reg_gen_184, tm_reg_get_113},
    {CHANNEL_SOGETTHIS_COM, "Mailinator (sogetthis.com)", "mailinator.com", tm_reg_gen_185, tm_reg_get_114},
    {CHANNEL_BOBMAIL_INFO, "Mailinator (bobmail.info)", "mailinator.com", tm_reg_gen_186, tm_reg_get_115},
    {CHANNEL_SUREMAIL_INFO, "Mailinator (suremail.info)", "mailinator.com", tm_reg_gen_187, tm_reg_get_116},
    {CHANNEL_BINKMAIL_COM, "Mailinator (binkmail.com)", "mailinator.com", tm_reg_gen_188, tm_reg_get_117},
    {CHANNEL_VERYREALEMAIL_COM, "Mailinator (veryrealemail.com)", "mailinator.com", tm_reg_gen_189, tm_reg_get_118},
    {CHANNEL_MAILMOMY, "Mailmomy", "mailmomy.com", tm_reg_gen_190, tm_reg_get_119},
    {CHANNEL_CHAMMY_INFO, "Mailinator (chammy.info)", "mailinator.com", tm_reg_gen_191, tm_reg_get_120},
    {CHANNEL_THISISNOTMYREALEMAIL_COM, "Mailinator (thisisnotmyrealemail.com)", "mailinator.com", tm_reg_gen_192, tm_reg_get_121},
    {CHANNEL_NOTMAILINATOR_COM, "Mailinator (notmailinator.com)", "mailinator.com", tm_reg_gen_193, tm_reg_get_122},
    {CHANNEL_SPAMHEREPLEASE_COM, "Mailinator (spamhereplease.com)", "mailinator.com", tm_reg_gen_194, tm_reg_get_123},
    {CHANNEL_SENDSPAMHERE_COM, "Mailinator (sendspamhere.com)", "mailinator.com", tm_reg_gen_195, tm_reg_get_124},
    {CHANNEL_SENDFREE_ORG, "Mailinator (sendfree.org)", "mailinator.com", tm_reg_gen_196, tm_reg_get_125},
    {CHANNEL_JUNK_BEATS_ORG, "Mailinator (junk.beats.org)", "mailinator.com", tm_reg_gen_197, tm_reg_get_126},
    {CHANNEL_JUNK_IHMEHL_COM, "Mailinator (junk.ihmehl.com)", "mailinator.com", tm_reg_gen_198, tm_reg_get_127},
    {CHANNEL_JUNK_NOPLAY_ORG, "Mailinator (junk.noplay.org)", "mailinator.com", tm_reg_gen_199, tm_reg_get_128},
    {CHANNEL_JUNK_VANILLASYSTEM_COM, "Mailinator (junk.vanillasystem.com)", "mailinator.com", tm_reg_gen_200, tm_reg_get_129},
    {CHANNEL_SPAM_JASONPEARCE_COM, "Mailinator (spam.jasonpearce.com)", "mailinator.com", tm_reg_gen_201, tm_reg_get_130},
    {CHANNEL_FISH_SKYTALE_NET, "Mailinator (fish.skytale.net)", "mailinator.com", tm_reg_gen_202, tm_reg_get_131},
    {CHANNEL_SPAM_MCCREW_COM, "Mailinator (spam.mccrew.com)", "mailinator.com", tm_reg_gen_203, tm_reg_get_132},
    {CHANNEL_DROPMAIL_CLICK, "DropMail.click", "dropmail.click", tm_reg_gen_204, tm_reg_get_133},
    {CHANNEL_SPAM_COROIU_COM, "Mailinator (spam.coroiu.com)", "mailinator.com", tm_reg_gen_205, tm_reg_get_134},
    {CHANNEL_SPAM_DELUSER_NET, "Mailinator (spam.deluser.net)", "mailinator.com", tm_reg_gen_206, tm_reg_get_135},
    {CHANNEL_SPAM_DHSF_NET, "Mailinator (spam.dhsf.net)", "mailinator.com", tm_reg_gen_207, tm_reg_get_136},
    {CHANNEL_SPAM_LUCATNT_COM, "Mailinator (spam.lucatnt.com)", "mailinator.com", tm_reg_gen_208, tm_reg_get_137},
    {CHANNEL_SPAM_LYCEUM_LIFE_COM_RU, "Mailinator (spam.lyceum-life.com.ru)", "mailinator.com", tm_reg_gen_209, tm_reg_get_138},
    {CHANNEL_SPAM_NETPIRATES_NET, "Mailinator (spam.netpirates.net)", "mailinator.com", tm_reg_gen_210, tm_reg_get_139},
    {CHANNEL_SPAM_NO_IP_NET, "Mailinator (spam.no-ip.net)", "mailinator.com", tm_reg_gen_211, tm_reg_get_140},
    {CHANNEL_SPAM_OZH_ORG, "Mailinator (spam.ozh.org)", "mailinator.com", tm_reg_gen_212, tm_reg_get_141},
    {CHANNEL_SPAM_PYPHUS_ORG, "Mailinator (spam.pyphus.org)", "mailinator.com", tm_reg_gen_213, tm_reg_get_142},
    {CHANNEL_SPAM_SHEP_PW, "Mailinator (spam.shep.pw)", "mailinator.com", tm_reg_gen_214, tm_reg_get_143},
    {CHANNEL_SPAM_WTF_AT, "Mailinator (spam.wtf.at)", "mailinator.com", tm_reg_gen_215, tm_reg_get_144},
    {CHANNEL_SPAM_WULCZER_ORG, "Mailinator (spam.wulczer.org)", "mailinator.com", tm_reg_gen_216, tm_reg_get_145},
    {CHANNEL_CRAP_KAKADUA_NET, "Mailinator (crap.kakadua.net)", "mailinator.com", tm_reg_gen_217, tm_reg_get_146},
    {CHANNEL_SPAM_JANLUGT_NL, "Mailinator (spam.janlugt.nl)", "mailinator.com", tm_reg_gen_218, tm_reg_get_147},
    {CHANNEL_MIN_BURNINGFISH_NET, "Mailinator (min.burningfish.net)", "mailinator.com", tm_reg_gen_219, tm_reg_get_148},
    {CHANNEL_SINK_FBLAY_COM, "Mailinator (sink.fblay.com)", "mailinator.com", tm_reg_gen_220, tm_reg_get_149},
    {CHANNEL_TEMPGMAILER, "TempGmailer", "tempgmailer.com", tm_reg_gen_275, tm_reg_get_204},
    {CHANNEL_TEMP_MAIL_ORG, "Temp-Mail.org", "temp-mail.org", tm_reg_gen_276, tm_reg_get_205},
    {CHANNEL_XKX_ME, "XKX.me", "xkx.me", tm_reg_gen_262, tm_reg_get_191},
    {CHANNEL_GONEBOX_EMAIL, "Gonebox Email", "gonebox.email", tm_reg_gen_263, tm_reg_get_192},
    {CHANNEL_MAILCAT_AI, "Mailcat AI", "mailcat.ai", tm_reg_gen_264, tm_reg_get_193},
    {CHANNEL_TEMPGO_EMAIL, "TempGo Email", "tempgo.email", tm_reg_gen_265, tm_reg_get_194},
    {CHANNEL_RESTMAIL_NET, "Restmail.net", "restmail.net", tm_reg_gen_266, tm_reg_get_195},
    {CHANNEL_DROPMAIL_ME, "DropMail.me", "dropmail.me", tm_reg_gen_278, tm_reg_get_207},
    {CHANNEL_TEN_MINUTE_MAIL_NET, "10MinuteMail.net", "10minutemail.net", tm_reg_gen_279, tm_reg_get_208},
};

#define TM_REGISTRY_N ((int)(sizeof(g_channel_registry) / sizeof(g_channel_registry[0])))

const tm_channel_spec_t *tm_registry_all(int *count) {
  if (count)
    *count = TM_REGISTRY_N;
  return g_channel_registry;
}

int tm_registry_count(void) { return TM_REGISTRY_N; }

const tm_channel_spec_t *tm_registry_find(tm_channel_t channel) {
  for (int i = 0; i < TM_REGISTRY_N; i++) {
    if (g_channel_registry[i].channel == channel)
      return &g_channel_registry[i];
  }
  return NULL;
}
