cmake_minimum_required(VERSION 3.14)
project(tempmail_sdk VERSION 1.0.3 LANGUAGES C)

set(CMAKE_C_STANDARD 11)

# 查找 libcurl
find_package(CURL REQUIRED)

# 查找 OpenSSL（mail-td 渠道的 SHA-256 Proof-of-Work 需要）
find_package(OpenSSL REQUIRED)

# SDK 库
add_library(tempmail_sdk STATIC
    src/types.c
    src/logger.c
    src/retry.c
    src/normalize.c
    src/client.c
    src/registry.c
    src/http.c
    src/telemetry.c
    src/providers/tempmail.c
    src/providers/tempmail_cn.c
    src/providers/linshiyou.c
    src/providers/tempmail_lol.c
    src/providers/chatgpt_org_uk.c
    src/providers/awamail.c
    src/providers/mail_tm.c
    src/providers/dropmail.c
    src/providers/guerrillamail.c
    src/providers/maildrop.c
    src/providers/duckmail.c
    src/providers/smail_pw.c
    src/providers/vip_215.c
    src/providers/fake_legal.c
    src/providers/mffac.c
    src/providers/ta_easy.c
    src/providers/moakt.c
    src/providers/tenminute_one.c
    src/providers/email10min.c
    src/providers/harakirimail.c
    src/providers/tempmail_plus.c
    src/providers/tempmail_lol_v2.c
    src/providers/tempgbox.c
    src/providers/emailnator.c
    src/providers/temporam.c
    src/providers/neighbours.c
    src/providers/guerrillamail_mirrors.c
    src/providers/temp_mail_io.c
    src/providers/mail_cx.c
    src/providers/catchmail.c
    src/providers/mailforspam.c
    src/providers/tempmailc.c
    src/providers/mailnesia.c
    src/providers/throwawaymail.c
    src/providers/tempmail_fish.c
    src/providers/neighbours_sh.c
    src/providers/shitty_email.c
    src/providers/tempmailpro.c
    src/providers/devmail_uk.c
    src/providers/cleantempmail.c
    src/providers/inboxkitten.c
    src/providers/getnada.c
    src/providers/mail123.c
    src/providers/mail10s.c
    src/providers/webmailtemp.c
    src/providers/tempfastmail.c
    src/providers/one_sec_mail.c
    src/providers/fakemail.c
    src/providers/openinbox.c
    src/providers/inboxes.c
    src/providers/uncorreotemporal.c
    src/providers/zhujump.c
    src/providers/m2u.c
    src/providers/tempy_email.c
    src/providers/fmail.c
    src/providers/ockito.c
    src/providers/anonbox.c
    src/providers/duckmail.c
    src/providers/mailinator.c
    src/providers/tempmail365.c
    src/providers/tempinbox.c
    src/providers/byom.c
    src/providers/anonymmail.c
    src/providers/eyepaste.c
    src/providers/mail_sunls.c
    src/providers/expressinboxhub.c
    src/providers/lroid.c
    src/providers/haribu.c
    src/providers/rootsh.c
    src/providers/fake_email_site.c
    src/providers/mohmal.c
    src/providers/mailgolem.c
    src/providers/best_temp_mail.c
    src/providers/disposablemail_app.c
    src/providers/mailtemp_cc.c
    src/providers/minuteinbox.c
    src/providers/tempemail_co.c
    src/providers/tempemails_net.c
    src/providers/altmails.c
    src/providers/tempemail_info.c
    src/providers/smailpro.c
    src/providers/tempmailten.c
    src/providers/maildrop_cc.c
    src/providers/tenminutemail_net.c
    src/providers/linshiyouxiang_net.c
    src/providers/tempmail_fyi.c
    src/providers/disposablemail_com.c
    src/providers/tempp_mails.c
    src/providers/emailtemp_org.c
    src/providers/mytempmail_cc.c
    src/providers/temp_mail_now.c
    src/providers/mail_td.c
    src/providers/mailhole_de.c
    src/providers/tmail_link.c
    src/providers/twentyfourmail_chacuo.c
    src/providers/nimail.c
    src/providers/freecustom.c
    src/providers/apihz.c
    src/providers/sogetthis_com.c
    src/providers/bobmail_info.c
    src/providers/suremail_info.c
    src/providers/binkmail_com.c
    src/providers/veryrealemail_com.c
    src/providers/mailmomy.c
    src/providers/chammy_info.c
    src/providers/thisisnotmyrealemail_com.c
    src/providers/notmailinator_com.c
    src/providers/spamhereplease_com.c
    src/providers/sendspamhere_com.c
    src/providers/sendfree_org.c
    src/providers/junk_beats_org.c
    src/providers/junk_ihmehl_com.c
    src/providers/junk_noplay_org.c
    src/providers/junk_vanillasystem_com.c
    src/providers/spam_jasonpearce_com.c
    src/providers/fish_skytale_net.c
    src/providers/spam_mccrew_com.c
    src/providers/dropmail_click.c
    src/providers/spam_coroiu_com.c
    src/providers/spam_deluser_net.c
    src/providers/spam_dhsf_net.c
    src/providers/spam_lucatnt_com.c
    src/providers/spam_lyceum_life_com_ru.c
    src/providers/spam_netpirates_net.c
    src/providers/spam_no_ip_net.c
    src/providers/spam_ozh_org.c
    src/providers/spam_pyphus_org.c
    src/providers/spam_shep_pw.c
    src/providers/spam_wtf_at.c
    src/providers/spam_wulczer_org.c
    src/providers/crap_kakadua_net.c
    src/providers/spam_janlugt_nl.c
    src/providers/min_burningfish_net.c
    src/providers/sink_fblay_com.c
    src/providers/etgdev_de.c
    src/providers/mtmdev_com.c
    src/providers/test_unergie_com.c
    src/providers/block_bdea_cc.c
    src/providers/torch_yi_org.c
    src/providers/carlton183_changeip_net.c
    src/providers/mail_fsmash_org.c
    src/providers/ebs_com_ar.c
    src/providers/jama_trenet_eu.c
    src/providers/blackhole_djurby_se.c
    src/providers/m8r_davidfuhr_de.c
    src/providers/mi_meon_be.c
    src/providers/m_nik_me.c
    src/providers/mail_bentrask_com.c
    src/providers/t_zibet_net.c
    src/providers/m8r_mcasal_com.c
    src/providers/ramjane_mooo_com.c
    src/providers/rauxa_seny_cat.c
    src/providers/sp_woot_at.c
    src/providers/fwd2m_eszett_es.c
    src/providers/m_887_at.c
    src/providers/nospam_thurstons_us.c
    src/providers/null_k3vin_net.c
    src/providers/really_istrash_com.c
    src/providers/spam_hortuk_ovh.c
    src/providers/16888888_cyou.c
    src/providers/17666688_shop.c
    src/providers/282mail_com.c
    src/providers/bsdu32_buzz.c
    src/providers/doxu243_buzz.c
    src/providers/easyme_pro.c
    src/providers/evergreenco_shop.c
    src/providers/layueming_pics.c
    src/providers/mingyuekeji_online.c
    src/providers/mingyueming_click.c
    src/providers/mingyueming_shop.c
    src/providers/mingyukeji_lol.c
    src/providers/nuxh62_space.c
    src/providers/proid_cloud_ip_cc.c
    src/providers/sbook_pics.c
    src/providers/xue32_buzz.c
    src/providers/xkx_me.c
    src/providers/gonebox_email.c
    src/providers/mailcat_ai.c
    src/providers/tempgo_email.c
    src/providers/restmail_net.c
    src/providers/b_smelly_cc.c
    src/providers/dea_soon_it.c
    src/providers/disposable_al_sudani_com.c
    src/providers/disposable_nogonad_nl.c
    src/providers/j_fairuse_org.c
    src/providers/mn_curppa_com.c
    src/providers/mailinatorzz_mooo_com.c
    src/providers/notfond_404_mn.c
    src/providers/dropmail_me.c
    src/providers/ten_minute_mail_net.c
    src/webui.c
    vendor/cJSON.c
)

target_include_directories(tempmail_sdk PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor
)

target_compile_definitions(tempmail_sdk PRIVATE "TEMPMAIL_SDK_VERSION=\"${PROJECT_VERSION}\"")

find_package(Threads REQUIRED)
target_link_libraries(tempmail_sdk PUBLIC CURL::libcurl PRIVATE Threads::Threads OpenSSL::Crypto)

# 示例程序
add_executable(tempmail_example example/main.c)
target_link_libraries(tempmail_example tempmail_sdk)
