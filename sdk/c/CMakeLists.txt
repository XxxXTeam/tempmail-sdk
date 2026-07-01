cmake_minimum_required(VERSION 3.14)
project(tempmail_sdk VERSION 1.0.3 LANGUAGES C)

set(CMAKE_C_STANDARD 11)

# 查找 libcurl
find_package(CURL REQUIRED)

# SDK 库
add_library(tempmail_sdk STATIC
    src/types.c
    src/logger.c
    src/retry.c
    src/normalize.c
    src/client.c
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
    src/providers/segamail.c
    src/providers/mail_sunls.c
    src/providers/expressinboxhub.c
    src/providers/lroid.c
    src/providers/haribu.c
    src/providers/pleasenospam.c
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
    vendor/cJSON.c
)

target_include_directories(tempmail_sdk PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor
)

target_compile_definitions(tempmail_sdk PRIVATE "TEMPMAIL_SDK_VERSION=\"${PROJECT_VERSION}\"")

find_package(Threads REQUIRED)
target_link_libraries(tempmail_sdk PUBLIC CURL::libcurl PRIVATE Threads::Threads)

# 示例程序
add_executable(tempmail_example example/main.c)
target_link_libraries(tempmail_example tempmail_sdk)
