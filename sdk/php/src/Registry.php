<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail;

use ChanhanzhanX\TempMail\Providers\Catchmail;
use ChanhanzhanX\TempMail\Providers\Anonymmail;
use ChanhanzhanX\TempMail\Providers\Byom;
use ChanhanzhanX\TempMail\Providers\ChatgptOrgUk;
use ChanhanzhanX\TempMail\Providers\CleanTempMail;
use ChanhanzhanX\TempMail\Providers\DevmailUk;
use ChanhanzhanX\TempMail\Providers\DropmailClick;
use ChanhanzhanX\TempMail\Providers\DuckMail;
use ChanhanzhanX\TempMail\Providers\FakeEmailSite;
use ChanhanzhanX\TempMail\Providers\Fmail;
use ChanhanzhanX\TempMail\Providers\Freecustom;
use ChanhanzhanX\TempMail\Providers\GoneboxEmail;
use ChanhanzhanX\TempMail\Providers\HarakiriMail;
use ChanhanzhanX\TempMail\Providers\Inboxes;
use ChanhanzhanX\TempMail\Providers\InboxKitten;
use ChanhanzhanX\TempMail\Providers\M2u;
use ChanhanzhanX\TempMail\Providers\Mail10s;
use ChanhanzhanX\TempMail\Providers\Mail123;
use ChanhanzhanX\TempMail\Providers\FakeLegal;
use ChanhanzhanX\TempMail\Providers\GetNada;
use ChanhanzhanX\TempMail\Providers\GuerrillaMail;
use ChanhanzhanX\TempMail\Providers\MailCx;
use ChanhanzhanX\TempMail\Providers\MailForSpam;
use ChanhanzhanX\TempMail\Providers\Maildrop;
use ChanhanzhanX\TempMail\Providers\MailholeDe;
use ChanhanzhanX\TempMail\Providers\Mailinator;
use ChanhanzhanX\TempMail\Providers\Mailmomy;
use ChanhanzhanX\TempMail\Providers\MailSunls;
use ChanhanzhanX\TempMail\Providers\MailTm;
use ChanhanzhanX\TempMail\Providers\Mffac;
use ChanhanzhanX\TempMail\Providers\Moakt;
use ChanhanzhanX\TempMail\Providers\Neighbours;
use ChanhanzhanX\TempMail\Providers\NeighboursSh;
use ChanhanzhanX\TempMail\Providers\Nimail;
use ChanhanzhanX\TempMail\Providers\Ockito;
use ChanhanzhanX\TempMail\Providers\RestmailNet;
use ChanhanzhanX\TempMail\Providers\Smailpro;
use ChanhanzhanX\TempMail\Providers\LinshiCo;
use ChanhanzhanX\TempMail\Providers\MjjCm;
use ChanhanzhanX\TempMail\Providers\TempmailCn;
use ChanhanzhanX\TempMail\Providers\Vip215;
use ChanhanzhanX\TempMail\Providers\Stub;
use ChanhanzhanX\TempMail\Providers\Tempinbox;
use ChanhanzhanX\TempMail\Providers\Tempmail;
use ChanhanzhanX\TempMail\Providers\Tempmail365;
use ChanhanzhanX\TempMail\Providers\Tempmailc;
use ChanhanzhanX\TempMail\Providers\TempMailIo;
use ChanhanzhanX\TempMail\Providers\TempMailLol;
use ChanhanzhanX\TempMail\Providers\TempMailPlus;
use ChanhanzhanX\TempMail\Providers\Temporam;
use ChanhanzhanX\TempMail\Providers\TempyEmail;
use ChanhanzhanX\TempMail\Providers\TenminuteOne;
use ChanhanzhanX\TempMail\Providers\Zhujump;
// shard-07-independent-complex 真实现 provider 导入
use ChanhanzhanX\TempMail\Providers\Anonbox;
use ChanhanzhanX\TempMail\Providers\DropMail;
use ChanhanzhanX\TempMail\Providers\DropMailMe;
use ChanhanzhanX\TempMail\Providers\Eyepaste;
use ChanhanzhanX\TempMail\Providers\Haribu;
use ChanhanzhanX\TempMail\Providers\Linshiyou;
use ChanhanzhanX\TempMail\Providers\Lroid;
use ChanhanzhanX\TempMail\Providers\MailCatch;
use ChanhanzhanX\TempMail\Providers\MaildropCc;
use ChanhanzhanX\TempMail\Providers\Mailnesia;
use ChanhanzhanX\TempMail\Providers\Mohmal;
use ChanhanzhanX\TempMail\Providers\SmailPw;
use ChanhanzhanX\TempMail\Providers\Tempgbox;
use ChanhanzhanX\TempMail\Providers\TenMinuteMailNet;
// shard-08-independent-session-a 真实现 provider 导入
use ChanhanzhanX\TempMail\Providers\Altmails;
use ChanhanzhanX\TempMail\Providers\Apihz;
use ChanhanzhanX\TempMail\Providers\Awamail;
use ChanhanzhanX\TempMail\Providers\BestTempMail;
use ChanhanzhanX\TempMail\Providers\Chacuo24mail;
use ChanhanzhanX\TempMail\Providers\DisposablemailApp;
use ChanhanzhanX\TempMail\Providers\DisposablemailCom;
use ChanhanzhanX\TempMail\Providers\Email10min;
use ChanhanzhanX\TempMail\Providers\Emailnator;
use ChanhanzhanX\TempMail\Providers\EmailtempOrg;
use ChanhanzhanX\TempMail\Providers\Expressinboxhub;
use ChanhanzhanX\TempMail\Providers\FakeMail;
use ChanhanzhanX\TempMail\Providers\LinshiyouxiangNet;
use ChanhanzhanX\TempMail\Providers\MailTd;
use ChanhanzhanX\TempMail\Providers\MailcatAi;
use ChanhanzhanX\TempMail\Providers\MailGolem;
use ChanhanzhanX\TempMail\Providers\MailtempCc;
use ChanhanzhanX\TempMail\Providers\Minuteinbox;
use ChanhanzhanX\TempMail\Providers\MytempmailCc;
use ChanhanzhanX\TempMail\Providers\OneSecMail;
use ChanhanzhanX\TempMail\Providers\Openinbox;
// shard-09-independent-session-b 真实现 provider 导入
use ChanhanzhanX\TempMail\Providers\Rootsh;
use ChanhanzhanX\TempMail\Providers\ShittyEmail;
use ChanhanzhanX\TempMail\Providers\TaEasy;
use ChanhanzhanX\TempMail\Providers\TempMailNow;
use ChanhanzhanX\TempMail\Providers\TempMailOrg;
use ChanhanzhanX\TempMail\Providers\TempEmailCo;
use ChanhanzhanX\TempMail\Providers\TempEmailInfo;
use ChanhanzhanX\TempMail\Providers\TempEmailsNet;
use ChanhanzhanX\TempMail\Providers\TempFastMail;
use ChanhanzhanX\TempMail\Providers\TempGmailer;
use ChanhanzhanX\TempMail\Providers\TempgoEmail;
use ChanhanzhanX\TempMail\Providers\TempMailFish;
use ChanhanzhanX\TempMail\Providers\TempMailFyi;
use ChanhanzhanX\TempMail\Providers\TempMailLolV2;
use ChanhanzhanX\TempMail\Providers\TempMailPro;
use ChanhanzhanX\TempMail\Providers\TempMailTen;
use ChanhanzhanX\TempMail\Providers\TemppMails;
use ChanhanzhanX\TempMail\Providers\TenMinuteMailNetApi;
use ChanhanzhanX\TempMail\Providers\ThrowawayMail;
use ChanhanzhanX\TempMail\Providers\TmailLink;
use ChanhanzhanX\TempMail\Providers\UnCorreoTemporal;
use ChanhanzhanX\TempMail\Providers\WebMailTemp;
use ChanhanzhanX\TempMail\Providers\XkxMe;

/**
 * 渠道注册表：单一事实来源
 *
 * 按 ChannelData 定义的顺序（与 baseline 逐行一致）注册全部 279 个渠道。
 * 有真实现的渠道绑定对应 provider 闭包，其余绑定桩实现。
 * listChannels / getChannelInfo / 两个 dispatch 全部由注册表派生。
 */
final class Registry
{
    /** @var ChannelSpec[] 有序渠道注册表，注册顺序即枚举顺序（硬约束） */
    private static array $registry = [];

    /** @var array<string,ChannelSpec> 渠道标识到规格的映射，O(1) 查找分发 */
    private static array $map = [];

    /** @var string[]|null 缓存的有序渠道标识列表，避免每次建邮箱重复 array_map 提取 */
    private static ?array $channelNames = null;

    private static bool $initialized = false;

    /**
     * 注册一个渠道：追加到有序列表并建立映射；重复注册视为编程错误。
     */
    public static function register(ChannelSpec $spec): void
    {
        if (isset(self::$map[$spec->channel])) {
            throw new \LogicException('duplicate channel registration: ' . $spec->channel);
        }
        self::$registry[] = $spec;
        self::$map[$spec->channel] = $spec;
    }

    /**
     * 获取有序注册表（惰性初始化）。
     *
     * @return ChannelSpec[]
     */
    public static function all(): array
    {
        self::ensureInitialized();
        return self::$registry;
    }

    /**
     * 按渠道标识查找规格。
     */
    public static function get(string $channel): ?ChannelSpec
    {
        self::ensureInitialized();
        return self::$map[$channel] ?? null;
    }

    /**
     * 获取有序渠道标识列表（惰性初始化并缓存）。
     *
     * 渠道集合在运行期不变，缓存后避免每次建邮箱都对全部渠道执行 array_map 提取。
     * 返回列表副本，防止调用方（如需 shuffle）改动内部缓存。
     *
     * @return string[]
     */
    public static function channelNames(): array
    {
        self::ensureInitialized();
        if (self::$channelNames === null) {
            self::$channelNames = array_map(
                static fn (ChannelSpec $s): string => $s->channel,
                self::$registry,
            );
        }
        return self::$channelNames;
    }

    /**
     * 构建全部真实现渠道的分发表。
     *
     * @return array<string,array{0:callable,1:callable}>
     */
    private static function overrides(): array
    {
        $ov = [];

        // DuckMail
        $ov['duckmail'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => DuckMail::generate(),
            static fn (string $e, ?string $t): array => DuckMail::getEmails($e, $t),
        ];

        // tempmail.lol
        $ov['tempmail-lol'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => TempMailLol::generate($o->domain),
            static fn (string $e, ?string $t): array => TempMailLol::getEmails($e, $t),
        ];

        // temp-mail.io
        $ov['temp-mail-io'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => TempMailIo::generate(),
            static fn (string $e, ?string $t): array => TempMailIo::getEmails($e, $t),
        ];

        // mail.cx
        // 见下方 "mail.cx 及其别名" 统一块

        // maildrop
        $ov['maildrop'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => Maildrop::generate($o->domain),
            static fn (string $e, ?string $t): array => Maildrop::getEmails($e, $t),
        ];

        // chatgpt-org-uk
        $ov['chatgpt-org-uk'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => ChatgptOrgUk::generate(),
            static fn (string $e, ?string $t): array => ChatgptOrgUk::getEmails($e, $t),
        ];

        // tempmail-plus
        // 见下方 "tempmail-plus 及其全部固定域别名" 统一块

        // mail.cx 及其别名（共用匿名 X-Client-ID API，仅固定域名与渠道标识不同）
        $mailCxChannels = [
            'mail-cx' => null,
            'ddker-com' => 'ddker.com',
        ];
        foreach ($mailCxChannels as $slug => $fixedDomain) {
            $ov[$slug] = [
                static fn (GenerateEmailOptions $o): EmailInfo => MailCx::generate($fixedDomain ?? $o->domain, $slug),
                static fn (string $e, ?string $t): array => MailCx::getEmails($e, $t),
            ];
        }

        // mail.tm 及其别名 web-library-net（共用 api.mail.tm）
        $mailTmChannels = ['mail-tm', 'web-library-net'];
        foreach ($mailTmChannels as $slug) {
            $ov[$slug] = [
                static fn (GenerateEmailOptions $o): EmailInfo => MailTm::generate($slug),
                static fn (string $e, ?string $t): array => MailTm::getEmails($e, $t),
            ];
        }

        // tempmail-plus 及其全部固定域别名（共用 tempmail.plus API）
        $tempMailPlusChannels = [
            'tempmail-plus' => null,
            'fexpost-com' => 'fexpost.com',
            'fexbox-org' => 'fexbox.org',
            'mailbox-in-ua' => 'mailbox.in.ua',
            'rover-info' => 'rover.info',
            'chitthi-in' => 'chitthi.in',
            'fextemp-com' => 'fextemp.com',
            'any-pink' => 'any.pink',
            'merepost-com' => 'merepost.com',
        ];
        foreach ($tempMailPlusChannels as $slug => $fixedDomain) {
            $ov[$slug] = [
                static fn (GenerateEmailOptions $o): EmailInfo => TempMailPlus::generate($fixedDomain ?? $o->domain, $slug),
                static fn (string $e, ?string $t): array => TempMailPlus::getEmails($e, $t),
            ];
        }

        // Catchmail 及其别名（共用 api.catchmail.io，仅固定域名不同）
        $catchmailChannels = [
            'catchmail' => null,
            'catchmail-mailistry' => 'mailistry.com',
            'catchmail-zeppost' => 'zeppost.com',
        ];
        foreach ($catchmailChannels as $slug => $fixedDomain) {
            $ov[$slug] = [
                static fn (GenerateEmailOptions $o): EmailInfo => Catchmail::generate($fixedDomain ?? $o->domain, $slug),
                static fn (string $e, ?string $t): array => Catchmail::getEmails($e, $t),
            ];
        }

        // MailForSpam 及其别名（共用 api.mailforspam.com，仅固定域名不同）
        $mailForSpamChannels = [
            'mailforspam' => null,
            'mailforspam-tempmail-io' => 'tempmail.io',
            'mailforspam-disposable' => 'disposable.email',
        ];
        foreach ($mailForSpamChannels as $slug => $fixedDomain) {
            $ov[$slug] = [
                static fn (GenerateEmailOptions $o): EmailInfo => MailForSpam::generate($fixedDomain ?? $o->domain, $slug),
                static fn (string $e, ?string $t): array => MailForSpam::getEmails($e, $t),
            ];
        }

        // Fake Legal 及其别名（共用 imgui.de，imgui-de / pulsewebmenu-de 走 POST 保根域）
        $fakeLegalChannels = [
            'fake-legal' => null,
            'imgui-de' => 'imgui.de',
            'pulsewebmenu-de' => 'pulsewebmenu.de',
        ];
        foreach ($fakeLegalChannels as $slug => $fixedDomain) {
            $ov[$slug] = [
                static fn (GenerateEmailOptions $o): EmailInfo => FakeLegal::generate($fixedDomain ?? $o->domain, $slug),
                static fn (string $e, ?string $t): array => FakeLegal::getEmails($e, $t),
            ];
        }

        // 10minutemail.one 及其 6 个固定域别名（共用 web.10minutemail.one API，需 JWT Bearer）
        $tenminuteOneChannels = [
            '10minute-one' => null,
            'xghff-com' => 'xghff.com',
            'oqqaj-com' => 'oqqaj.com',
            'psovv-com' => 'psovv.com',
            'dbwot-com' => 'dbwot.com',
            'ygwpr-com' => 'ygwpr.com',
            'imxwe-com' => 'imxwe.com',
        ];
        foreach ($tenminuteOneChannels as $slug => $fixedDomain) {
            $ov[$slug] = [
                static fn (GenerateEmailOptions $o): EmailInfo => TenminuteOne::generate($fixedDomain ?? $o->domain, $slug),
                static fn (string $e, ?string $t): array => TenminuteOne::getEmails($e, $t),
            ];
        }

        // zhujump 系列固定域渠道（jqkjqk.xyz 用主实例，lyhlevi.com 用独立实例 URL）
        $ov['jqkjqk-xyz'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => Zhujump::generate('jqkjqk.xyz', 'jqkjqk-xyz'),
            static fn (string $e, ?string $t): array => Zhujump::getEmails($e, $t),
        ];
        $ov['lyhlevi-com'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => Zhujump::generateForInstance('https://lyhlevi.com', 'lyhlevi.com', 'lyhlevi-com', 86400000),
            static fn (string $e, ?string $t): array => Zhujump::getEmails($e, $t),
        ];

        // GuerrillaMail 及其全部镜像（共用一套 ajax.php API）
        $mirrors = [
            'guerrillamail' => 'https://api.guerrillamail.com/ajax.php',
            'guerrillamail-com' => 'https://guerrillamail.com/ajax.php',
            'sharklasers' => 'https://www.sharklasers.com/ajax.php',
            'sharklasers-com' => 'https://sharklasers.com/ajax.php',
            'grr-la' => 'https://www.grr.la/ajax.php',
            'grr-la-com' => 'https://grr.la/ajax.php',
            'guerrillamail-info' => 'https://www.guerrillamail.info/ajax.php',
            'spam4me' => 'https://www.spam4.me/ajax.php',
            'guerrillamail-net' => 'https://www.guerrillamail.net/ajax.php',
            'guerrillamail-org' => 'https://www.guerrillamail.org/ajax.php',
            'guerrillamailblock' => 'https://www.guerrillamailblock.com/ajax.php',
            'guerrillamail-com-www' => 'https://www.guerrillamail.com/ajax.php',
        ];
        foreach ($mirrors as $slug => $baseUrl) {
            $gen = GuerrillaMail::makeGenerate($slug, $baseUrl);
            $getE = GuerrillaMail::makeGetEmails($baseUrl);
            $ov[$slug] = [
                static fn (GenerateEmailOptions $o): EmailInfo => $gen(),
                static fn (string $e, ?string $t): array => $getE($e, $t),
            ];
        }

        // GetNada 及其全部别名域名（共用 getnada.net/api，仅固定域名与渠道标识不同）
        // 主渠道 getnada 使用用户传入域名（fixedDomain 为 null），其余为固定别名域名。
        $getnadaChannels = [
            'getnada' => null,
            '1vpn-net' => '1vpn.net',
            'abematv-com' => 'abematv.com',
            'abematv-net' => 'abematv.net',
            'abematv-org' => 'abematv.org',
            'aceh-cc' => 'aceh.cc',
            'bangkabelitung-net' => 'bangkabelitung.net',
            'cctruyen-com' => 'cctruyen.com',
            'getnada-com' => 'getnada.com',
            'getnada-email' => 'getnada.email',
            'getnada-net' => 'getnada.net',
            'jawatengah-net' => 'jawatengah.net',
            'jawatimur-net' => 'jawatimur.net',
            'kalimantanbarat-net' => 'kalimantanbarat.net',
            'kalimantanselatan-net' => 'kalimantanselatan.net',
            'kalimantantengah-net' => 'kalimantantengah.net',
            'kalimantantimur-net' => 'kalimantantimur.net',
            'kalimantanutara-net' => 'kalimantanutara.net',
            'kepulauanriau-net' => 'kepulauanriau.net',
            'luxury345-com' => 'luxury345.com',
            'malukuutara-net' => 'malukuutara.net',
            'nusatenggarabarat-net' => 'nusatenggarabarat.net',
            'nusatenggaratimur-net' => 'nusatenggaratimur.net',
            'papuabarat-net' => 'papuabarat.net',
            'papuabaratdaya-net' => 'papuabaratdaya.net',
            'papuaselatan-net' => 'papuaselatan.net',
            'pehol-com' => 'pehol.com',
            'ptruyen-com' => 'ptruyen.com',
            'pulaubali-net' => 'pulaubali.net',
            'riau-net' => 'riau.net',
            'seokey-org' => 'seokey.org',
            'sulawesibarat-net' => 'sulawesibarat.net',
            'sulawesiselatan-net' => 'sulawesiselatan.net',
            'sulawesitengah-net' => 'sulawesitengah.net',
            'sulawesitenggara-net' => 'sulawesitenggara.net',
            'sumaterabarat-net' => 'sumaterabarat.net',
            'sumateraselatan-net' => 'sumateraselatan.net',
            'sumaterautara-net' => 'sumaterautara.net',
            'villatogel-com' => 'villatogel.com',
        ];
        foreach ($getnadaChannels as $slug => $fixedDomain) {
            $gen = GetNada::makeGenerate($slug, $fixedDomain);
            $getE = GetNada::makeGetEmails();
            $ov[$slug] = [
                static fn (GenerateEmailOptions $o): EmailInfo => $gen($o->domain),
                static fn (string $e, ?string $t): array => $getE($e, $t),
            ];
        }

        // mailmomy 及其全部固定域名变体（共用 mailmomy.com 后端 API）
        // 主渠道 mailmomy 从 /api/domains/active 随机选域名（fixedDomain 为 null），其余为固定别名域名。
        $mailmomyChannels = [
            '16888888-cyou' => '16888888.cyou',
            '17666688-shop' => '17666688.shop',
            '282mail-com' => '282mail.com',
            'bsdu32-buzz' => 'bsdu32.buzz',
            'doxu243-buzz' => 'doxu243.buzz',
            'easyme-pro' => 'easyme.pro',
            'evergreenco-shop' => 'evergreenco.shop',
            'layueming-pics' => 'layueming.pics',
            'mingyuekeji-online' => 'mingyuekeji.online',
            'mingyueming-click' => 'mingyueming.click',
            'mingyueming-shop' => 'mingyueming.shop',
            'mingyukeji-lol' => 'mingyukeji.lol',
            'nuxh62-space' => 'nuxh62.space',
            'proid-cloud-ip-cc' => 'proid.cloud-ip.cc',
            'sbook-pics' => 'sbook.pics',
            'xue32-buzz' => 'xue32.buzz',
            'mailmomy' => null,
        ];
        foreach ($mailmomyChannels as $slug => $fixedDomain) {
            $gen = Mailmomy::makeGenerate($slug, $fixedDomain);
            $getE = Mailmomy::makeGetEmails();
            $ov[$slug] = [
                static fn (GenerateEmailOptions $o): EmailInfo => $gen(),
                static fn (string $e, ?string $t): array => $getE($e, $t),
            ];
        }

        // Mailinator 及其全部姊妹域名（MX 均指向 mail.mailinator.com，收信复用 domain=public API）
        // slug => 邮箱生成域名；收信逻辑全部共用。
        $mailinatorChannels = [
            'mailinator' => 'mailinator.com',
            'blackhole-djurby-se' => 'blackhole.djurby.se',
            'block-bdea-cc' => 'block.bdea.cc',
            'b-smelly-cc' => 'b.smelly.cc',
            'carlton183-changeip-net' => '183carlton.changeip.net',
            'dea-soon-it' => 'dea.soon.it',
            'disposable-al-sudani-com' => 'disposable.al-sudani.com',
            'disposable-nogonad-nl' => 'disposable.nogonad.nl',
            'ebs-com-ar' => 'ebs.com.ar',
            'etgdev-de' => 'etgdev.de',
            'fwd2m-eszett-es' => 'fwd2m.eszett.es',
            'jama-trenet-eu' => 'jama.trenet.eu',
            'j-fairuse-org' => 'j.fairuse.org',
            'm-887-at' => 'm.887.at',
            'm8r-davidfuhr-de' => 'm8r.davidfuhr.de',
            'm8r-mcasal-com' => 'm8r.mcasal.com',
            'mail-bentrask-com' => 'mail.bentrask.com',
            'mail-fsmash-org' => 'mail.fsmash.org',
            'mailinatorzz-mooo-com' => 'mailinatorzz.mooo.com',
            'mi-meon-be' => 'mi.meon.be',
            'mn-curppa-com' => 'mn.curppa.com',
            'm-nik-me' => 'm.nik.me',
            'mtmdev-com' => 'mtmdev.com',
            'nospam-thurstons-us' => 'nospam.thurstons.us',
            'notfond-404-mn' => 'notfond.404.mn',
            'null-k3vin-net' => 'null.k3vin.net',
            'ramjane-mooo-com' => 'ramjane.mooo.com',
            'rauxa-seny-cat' => 'rauxa.seny.cat',
            'really-istrash-com' => 'really.istrash.com',
            'spam-hortuk-ovh' => 'spam.hortuk.ovh',
            'sp-woot-at' => 'sp.woot.at',
            'test-unergie-com' => 'test.unergie.com',
            'torch-yi-org' => 'torch.yi.org',
            't-zibet-net' => 't.zibet.net',
            'sogetthis-com' => 'sogetthis.com',
            'bobmail-info' => 'bobmail.info',
            'suremail-info' => 'suremail.info',
            'binkmail-com' => 'binkmail.com',
            'veryrealemail-com' => 'veryrealemail.com',
            'chammy-info' => 'chammy.info',
            'thisisnotmyrealemail-com' => 'thisisnotmyrealemail.com',
            'notmailinator-com' => 'notmailinator.com',
            'spamhereplease-com' => 'spamhereplease.com',
            'sendspamhere-com' => 'sendspamhere.com',
            'sendfree-org' => 'sendfree.org',
            'junk-beats-org' => 'junk.beats.org',
            'junk-ihmehl-com' => 'junk.ihmehl.com',
            'junk-noplay-org' => 'junk.noplay.org',
            'junk-vanillasystem-com' => 'junk.vanillasystem.com',
            'spam-jasonpearce-com' => 'spam.jasonpearce.com',
            'fish-skytale-net' => 'fish.skytale.net',
            'spam-mccrew-com' => 'spam.mccrew.com',
            'spam-coroiu-com' => 'spam.coroiu.com',
            'spam-deluser-net' => 'spam.deluser.net',
            'spam-dhsf-net' => 'spam.dhsf.net',
            'spam-lucatnt-com' => 'spam.lucatnt.com',
            'spam-lyceum-life-com-ru' => 'spam.lyceum-life.com.ru',
            'spam-netpirates-net' => 'spam.netpirates.net',
            'spam-no-ip-net' => 'spam.no-ip.net',
            'spam-ozh-org' => 'spam.ozh.org',
            'spam-pyphus-org' => 'spam.pyphus.org',
            'spam-shep-pw' => 'spam.shep.pw',
            'spam-wtf-at' => 'spam.wtf.at',
            'spam-wulczer-org' => 'spam.wulczer.org',
            'crap-kakadua-net' => 'crap.kakadua.net',
            'spam-janlugt-nl' => 'spam.janlugt.nl',
            'min-burningfish-net' => 'min.burningfish.net',
            'sink-fblay-com' => 'sink.fblay.com',
        ];
        foreach ($mailinatorChannels as $slug => $domain) {
            $gen = Mailinator::makeGenerate($slug, $domain);
            $getE = Mailinator::makeGetEmails();
            $ov[$slug] = [
                static fn (GenerateEmailOptions $o): EmailInfo => $gen(),
                static fn (string $e, ?string $t): array => $getE($e, $t),
            ];
        }

        // moakt.com 及其全部镜像域名（HTTP 全打到 www.moakt.com，仅邮箱域名不同）
        // slug => 强制邮箱域名；null 表示随机域名。收信逻辑全部共用。
        $moaktChannels = [
            'moakt' => null,
            'drmail-in' => 'drmail.in',
            'teml-net' => 'teml.net',
            'tmpeml-com' => 'tmpeml.com',
            'tmpbox-net' => 'tmpbox.net',
            'moakt-cc' => 'moakt.cc',
            'disbox-net' => 'disbox.net',
            'tmpmail-org' => 'tmpmail.org',
            'tmpmail-net' => 'tmpmail.net',
            'tmails-net' => 'tmails.net',
            'disbox-org' => 'disbox.org',
            'moakt-co' => 'moakt.co',
            'moakt-ws' => 'moakt.ws',
            'tmail-ws' => 'tmail.ws',
            'bareed-ws' => 'bareed.ws',
        ];
        foreach ($moaktChannels as $slug => $fixedDomain) {
            $gen = Moakt::makeGenerate($slug, $fixedDomain);
            $getE = Moakt::makeGetEmails();
            $ov[$slug] = [
                static fn (GenerateEmailOptions $o): EmailInfo => $gen(),
                static fn (string $e, ?string $t): array => $getE($e, $t),
            ];
        }

        // === shard-11-independent-simple-b：独立简单渠道真实现 ===
        // mail-sunls：拉域名后本地生成地址
        $ov['mail-sunls'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => MailSunls::generate(),
            static fn (string $e, ?string $t): array => MailSunls::getEmails($e, $t),
        ];

        // mailhole-de：/api/random 抽取地址，/json/{email} 收信
        $ov['mailhole-de'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => MailholeDe::generate(),
            static fn (string $e, ?string $t): array => MailholeDe::getEmails($t !== null && $t !== '' ? $t : $e, $t),
        ];

        // mffac：POST 创建邮箱 + 详情补全
        $ov['mffac'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => Mffac::generate(),
            static fn (string $e, ?string $t): array => Mffac::getEmails($e, $t ?? ''),
        ];

        // neighbours：拉域名后本地生成地址
        $ov['neighbours'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => Neighbours::generate($o->domain),
            static fn (string $e, ?string $t): array => Neighbours::getEmails($e, $t),
        ];

        // neighbours-sh：固定域名公共收件箱
        $ov['neighbours-sh'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => NeighboursSh::generate(),
            static fn (string $e, ?string $t): array => NeighboursSh::getEmails($e, $t),
        ];

        // nimail：POST 表单 API
        $ov['nimail'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => Nimail::generate(),
            static fn (string $e, ?string $t): array => Nimail::getEmails($t !== null && $t !== '' ? $t : $e, $t),
        ];

        // ockito：Bearer + refresh token
        $ov['ockito'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => Ockito::generate(),
            static fn (string $e, ?string $t): array => Ockito::getEmails($e, $t),
        ];

        // restmail-net：ad-hoc 模式，本地生成 username
        $ov['restmail-net'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => RestmailNet::generate(),
            static fn (string $e, ?string $t): array => RestmailNet::getEmails($e, $t),
        ];

        // smailpro：payload JWT 两段式调用 sonjj API
        $ov['smailpro'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => Smailpro::generate(),
            static fn (string $e, ?string $t): array => Smailpro::getEmails($e, $t ?? ''),
        ];

        // tempinbox：随机或指定域名
        $ov['tempinbox'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => Tempinbox::generate($o->domain),
            static fn (string $e, ?string $t): array => Tempinbox::getEmails($e, $t),
        ];

        // tempmail（tempmail.ing）：POST 创建，支持有效期
        $ov['tempmail'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => Tempmail::generate($o->duration),
            static fn (string $e, ?string $t): array => Tempmail::getEmails($e, $t),
        ];

        // tempmail365：拉域名 + HTML content 提取
        $ov['tempmail365'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => Tempmail365::generate($o->domain),
            static fn (string $e, ?string $t): array => Tempmail365::getEmails($e, $t),
        ];

        // tempmailc：GET /new + 详情补全
        $ov['tempmailc'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => Tempmailc::generate(),
            static fn (string $e, ?string $t): array => Tempmailc::getEmails($e, $t),
        ];

        // temporam：拉域名后本地生成地址（token 存地址），收信按 token 优先
        $ov['temporam'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => Temporam::generate($o->domain),
            static fn (string $e, ?string $t): array => Temporam::getEmails($e, $t),
        ];

        // tempy-email：POST 创建 mailbox
        $ov['tempy-email'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => TempyEmail::generate($o->domain),
            static fn (string $e, ?string $t): array => TempyEmail::getEmails($e, $t),
        ];

        // ===== WebSocket/Socket.IO 渠道 =====

        // tempmail-cn：Socket.IO generate + REST API getEmails
        $ov['tempmail-cn'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => TempmailCn::generate($o->domain),
            static fn (string $e, ?string $t): array => TempmailCn::getEmails($e, $t),
        ];

        // mjj-cm：Socket.IO shortid 协议
        $ov['mjj-cm'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => MjjCm::generate(),
            static fn (string $e, ?string $t): array => MjjCm::getEmails($e, $t),
        ];

        // linshi-co：Socket.IO shortid 协议
        $ov['linshi-co'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => LinshiCo::generate(),
            static fn (string $e, ?string $t): array => LinshiCo::getEmails($e, $t),
        ];

        // vip-215：HTTP POST generate + WebSocket getEmails
        $ov['vip-215'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => Vip215::generate(),
            static fn (string $e, ?string $t): array => Vip215::getEmails($e, $t),
        ];

        // ===== shard-10-independent-simple-a 渠道绑定 =====

        // anonymmail：POST 表单 API，先取域名再创建
        $ov['anonymmail'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => Anonymmail::generate(),
            static fn (string $e, ?string $t): array => Anonymmail::getEmails($e),
        ];

        // byom：纯 GET 无认证，本地生成邮箱
        $ov['byom'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => Byom::generate(),
            static fn (string $e, ?string $t): array => Byom::getEmails($e),
        ];

        // cleantempmail：X-API-Key 鉴权 JSON API
        $ov['cleantempmail'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => CleanTempMail::generate(),
            static fn (string $e, ?string $t): array => CleanTempMail::getEmails($e),
        ];

        // devmail-uk：GET /api/new 创建
        $ov['devmail-uk'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => DevmailUk::generate(),
            static fn (string $e, ?string $t): array => DevmailUk::getEmails($e),
        ];

        // dropmail-click：免鉴权，token 即邮箱
        $ov['dropmail-click'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => DropmailClick::generate(),
            static fn (string $e, ?string $t): array => DropmailClick::getEmails($e, $t),
        ];

        // fake-email-site：POST 空 JSON 创建
        $ov['fake-email-site'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => FakeEmailSite::generate(),
            static fn (string $e, ?string $t): array => FakeEmailSite::getEmails($e, $t),
        ];

        // fmail：GET /v1/random 创建，支持指定域名
        $ov['fmail'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => Fmail::generate($o->domain),
            static fn (string $e, ?string $t): array => Fmail::getEmails($e),
        ];

        // freecustom：匿名 JWT 读信，token 即邮箱
        $ov['freecustom'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => Freecustom::generate(),
            static fn (string $e, ?string $t): array => Freecustom::getEmails($e, $t),
        ];

        // gonebox-email：POST 创建，无需鉴权
        $ov['gonebox-email'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => GoneboxEmail::generate(),
            static fn (string $e, ?string $t): array => GoneboxEmail::getEmails($e, $t),
        ];

        // harakirimail：无鉴权 REST API
        $ov['harakirimail'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => HarakiriMail::generate(),
            static fn (string $e, ?string $t): array => HarakiriMail::getEmails($e),
        ];

        // inboxes：取域名后本地拼邮箱
        $ov['inboxes'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => Inboxes::generate($o->domain),
            static fn (string $e, ?string $t): array => Inboxes::getEmails($e),
        ];

        // inboxkitten：本地生成邮箱，storage 补全正文
        $ov['inboxkitten'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => InboxKitten::generate(),
            static fn (string $e, ?string $t): array => InboxKitten::getEmails($e),
        ];

        // m2u：token+view_token 打包鉴权
        $ov['m2u'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => M2u::generate(),
            static fn (string $e, ?string $t): array => M2u::getEmails($e, $t),
        ];

        // mail10s：本地生成邮箱，token 即邮箱
        $ov['mail10s'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => Mail10s::generate(),
            static fn (string $e, ?string $t): array => Mail10s::getEmails($e, $t),
        ];

        // mail123：GET 创建，逐封补全正文
        $ov['mail123'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => Mail123::generate(),
            static fn (string $e, ?string $t): array => Mail123::getEmails($e, $t),
        ];

        // ===== shard-07-independent-complex 真实现绑定 =====

        // anonbox：GET 首页解析邮箱，mbox 文本解析收信
        $ov['anonbox'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => Anonbox::generate(),
            static fn (string $e, ?string $t): array => Anonbox::getEmails($e, $t),
        ];

        // dropmail：GraphQL af_ 令牌会话
        $ov['dropmail'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => DropMail::generate(),
            static fn (string $e, ?string $t): array => DropMail::getEmails($e, $t),
        ];

        // dropmail-me：GraphQL introduceSession（data-k 派生令牌）
        $ov['dropmail-me'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => DropMailMe::generate(),
            static fn (string $e, ?string $t): array => DropMailMe::getEmails($e, $t),
        ];

        // eyepaste：RSS XML 收信（getEmails 仅需邮箱）
        $ov['eyepaste'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => Eyepaste::generate(),
            static fn (string $e, ?string $t): array => Eyepaste::getEmails($e),
        ];

        // haribu：HTML 页面解析 + session cookie
        $ov['haribu'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => Haribu::generate(),
            static fn (string $e, ?string $t): array => Haribu::getEmails($e, $t),
        ];

        // linshiyou：自定义分隔符 HTML 分片解析
        $ov['linshiyou'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => Linshiyou::generate(),
            static fn (string $e, ?string $t): array => Linshiyou::getEmails($e, $t),
        ];

        // lroid：kontrol JSON API 优先，回退 HTML 解析
        $ov['lroid'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => Lroid::generate(),
            static fn (string $e, ?string $t): array => Lroid::getEmails($e, $t),
        ];

        // mailcatch：公开收件箱，列表 + 正文两段 HTML
        $ov['mailcatch'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => MailCatch::generate(),
            static fn (string $e, ?string $t): array => MailCatch::getEmails($e, $t),
        ];

        // maildrop-cc：GraphQL 公共邮箱
        $ov['maildrop-cc'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => MaildropCc::generate(),
            static fn (string $e, ?string $t): array => MaildropCc::getEmails($e, $t),
        ];

        // mailnesia：公开收件箱表格 + 详情页解析（getEmails 仅需邮箱）
        $ov['mailnesia'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => Mailnesia::generate(),
            static fn (string $e, ?string $t): array => Mailnesia::getEmails($e, $t),
        ];

        // mohmal：connect.sid 会话 + HTML 解析
        $ov['mohmal'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => Mohmal::generate(),
            static fn (string $e, ?string $t): array => Mohmal::getEmails($e, $t),
        ];

        // smail-pw：Remix/Flight loader 数据流解析
        $ov['smail-pw'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => SmailPw::generate(),
            static fn (string $e, ?string $t): array => SmailPw::getEmails($e, $t),
        ];

        // tempgbox：/api/proxy base64 载荷
        $ov['tempgbox'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => Tempgbox::generate(),
            static fn (string $e, ?string $t): array => Tempgbox::getEmails($e, $t),
        ];

        // 10minutemail-net：PHPSESSID 会话 + HTML 表格解析
        $ov['10minutemail-net'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => TenMinuteMailNet::generate(),
            static fn (string $e, ?string $t): array => TenMinuteMailNet::getEmails($e, $t),
        ];

        // ===== shard-08-independent-session-a 真实现绑定 =====
        // 注：duckmail / maildrop / chatgpt-org-uk 已在上文绑定，此处不重复。

        // altmails：Cookie session + CSRF + REST JSON
        $ov['altmails'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => Altmails::generate(),
            static fn (string $e, ?string $t): array => Altmails::getEmails($e, $t),
        ];

        // apihz（接口盒子）：id/key 凭据，读信仅需 token（含 mail/pwd）
        $ov['apihz'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => Apihz::generate(),
            static fn (string $e, ?string $t): array => Apihz::getEmails($t),
        ];

        // awamail：session token 读信
        $ov['awamail'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => Awamail::generate(),
            static fn (string $e, ?string $t): array => Awamail::getEmails($e, $t),
        ];

        // best-temp-mail：session token 读信
        $ov['best-temp-mail'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => BestTempMail::generate(),
            static fn (string $e, ?string $t): array => BestTempMail::getEmails($e, $t),
        ];

        // 24mail-chacuo：chacuo.net 后端，创建支持指定域名
        $ov['24mail-chacuo'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => Chacuo24mail::generate($o->domain),
            static fn (string $e, ?string $t): array => Chacuo24mail::getEmails($e, $t),
        ];

        // disposablemail-app：session token 读信
        $ov['disposablemail-app'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => DisposablemailApp::generate(),
            static fn (string $e, ?string $t): array => DisposablemailApp::getEmails($e, $t),
        ];

        // disposablemail-com：CSRF + PHP 后端，base64 token 存 CSRF/cookie
        $ov['disposablemail-com'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => DisposablemailCom::generate(),
            static fn (string $e, ?string $t): array => DisposablemailCom::getEmails($e, $t),
        ];

        // email10min：Laravel + CSRF，base64 token 存 cookie/csrf
        $ov['email10min'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => Email10min::generate(),
            static fn (string $e, ?string $t): array => Email10min::getEmails($e, $t),
        ];

        // emailnator：XSRF-TOKEN session，JSON token 存 xsrfToken/cookies
        $ov['emailnator'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => Emailnator::generate(),
            static fn (string $e, ?string $t): array => Emailnator::getEmails($e, $t),
        ];

        // emailtemp-org：Laravel + CSRF，base64 token 存 csrf/cookie
        $ov['emailtemp-org'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => EmailtempOrg::generate(),
            static fn (string $e, ?string $t): array => EmailtempOrg::getEmails($e, $t),
        ];

        // expressinboxhub：CSRF + session cookie，JSON token 复用 POST /messages
        $ov['expressinboxhub'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => Expressinboxhub::generate(),
            static fn (string $e, ?string $t): array => Expressinboxhub::getEmails($e, $t),
        ];

        // fakemail：CSRF + PHP 后端，token 存合并 cookie 串
        $ov['fakemail'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => FakeMail::generate(),
            static fn (string $e, ?string $t): array => FakeMail::getEmails($e, $t),
        ];

        // linshiyouxiang-net：session token 读信
        $ov['linshiyouxiang-net'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => LinshiyouxiangNet::generate(),
            static fn (string $e, ?string $t): array => LinshiyouxiangNet::getEmails($e, $t),
        ];

        // mail-td：SHA-256 PoW + session token
        $ov['mail-td'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => MailTd::generate(),
            static fn (string $e, ?string $t): array => MailTd::getEmails($e, $t),
        ];

        // mailcat-ai：session token 读信
        $ov['mailcat-ai'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => MailcatAi::generate(),
            static fn (string $e, ?string $t): array => MailcatAi::getEmails($e, $t),
        ];

        // mailgolem：Laravel session + CSRF，base64 token 存 csrf/cookies
        $ov['mailgolem'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => MailGolem::generate(),
            static fn (string $e, ?string $t): array => MailGolem::getEmails($e, $t),
        ];

        // mailtemp-cc：session token 读信
        $ov['mailtemp-cc'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => MailtempCc::generate(),
            static fn (string $e, ?string $t): array => MailtempCc::getEmails($e, $t),
        ];

        // minuteinbox：CSRF + PHP 后端，JSON token 存 csrf/cookies
        $ov['minuteinbox'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => Minuteinbox::generate(),
            static fn (string $e, ?string $t): array => Minuteinbox::getEmails($e, $t),
        ];

        // mytempmail-cc：session token 读信
        $ov['mytempmail-cc'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => MytempmailCc::generate(),
            static fn (string $e, ?string $t): array => MytempmailCc::getEmails($e, $t),
        ];

        // 1sec-mail：session token 读信
        $ov['1sec-mail'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => OneSecMail::generate(),
            static fn (string $e, ?string $t): array => OneSecMail::getEmails($e, $t),
        ];

        // openinbox：session token 读信
        $ov['openinbox'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => Openinbox::generate(),
            static fn (string $e, ?string $t): array => Openinbox::getEmails($e, $t),
        ];

        // ===== shard-09-independent-session-b 渠道绑定 =====
        // 注：tempmail-lol 已在上文绑定，此处不重复。

        // rootsh：GET 首页 + POST 表单多步（CSRF + cookie session）
        $ov['rootsh'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => Rootsh::generate(),
            static fn (string $e, ?string $t): array => Rootsh::getEmails($e, $t),
        ];

        // shitty-email：POST 创建 + X-Session-Token 读信
        $ov['shitty-email'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => ShittyEmail::generate(),
            static fn (string $e, ?string $t): array => ShittyEmail::getEmails($e, $t),
        ];

        // ta-easy：POST 空 body 创建 + token/email JSON 读信
        $ov['ta-easy'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => TaEasy::generate(),
            static fn (string $e, ?string $t): array => TaEasy::getEmails($e, $t),
        ];

        // temp-mail-now：CSRF + session cookie，POST /change_email + GET /fetch_emails
        $ov['temp-mail-now'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => TempMailNow::generate(),
            static fn (string $e, ?string $t): array => TempMailNow::getEmails($e, $t),
        ];

        // temp-mail-org：JWT Bearer 认证，POST /mailbox + GET /messages
        $ov['temp-mail-org'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => TempMailOrg::generate(),
            static fn (string $e, ?string $t): array => TempMailOrg::getEmails($e, $t),
        ];

        // tempemail-co：首页随机邮箱 + regex data-id 收信
        $ov['tempemail-co'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => TempEmailCo::generate(),
            static fn (string $e, ?string $t): array => TempEmailCo::getEmails($e, $t),
        ];

        // tempemail-info：base64 emailEncoded + PHPSESSID，POST checker + GET /view
        $ov['tempemail-info'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => TempEmailInfo::generate(),
            static fn (string $e, ?string $t): array => TempEmailInfo::getEmails($e, $t),
        ];

        // tempemails-net：CSRF meta + session cookie，POST /get_messages + GET /view
        $ov['tempemails-net'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => TempEmailsNet::generate(),
            static fn (string $e, ?string $t): array => TempEmailsNet::getEmails($e, $t),
        ];

        // tempfastmail：uuid token，POST /api/email-box + GET 读信
        $ov['tempfastmail'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => TempFastMail::generate(),
            static fn (string $e, ?string $t): array => TempFastMail::getEmails($e, $t),
        ];

        // tempgmailer：CSRF + Laravel session，POST /get-gmail + POST /get-inbox
        $ov['tempgmailer'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => TempGmailer::generate(),
            static fn (string $e, ?string $t): array => TempGmailer::getEmails($e, $t),
        ];

        // tempgo-email：POST /api/generate + GET /api/inbox
        $ov['tempgo-email'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => TempgoEmail::generate(),
            static fn (string $e, ?string $t): array => TempgoEmail::getEmails($e, $t),
        ];

        // tempmail-fish：Authorization header（无 Bearer），JSON token 含 email+authKey
        $ov['tempmail-fish'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => TempMailFish::generate(),
            static fn (string $e, ?string $t): array => TempMailFish::getEmails($e, $t),
        ];

        // tempmail-fyi：CSRF + PHPSESSID，POST /api/generate_email + /api/get_emails
        $ov['tempmail-fyi'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => TempMailFyi::generate(),
            static fn (string $e, ?string $t): array => TempMailFyi::getEmails($e, $t),
        ];

        // tempmail-lol-v2：GET /generate + GET /auth/{token} 读信
        $ov['tempmail-lol-v2'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => TempMailLolV2::generate(),
            static fn (string $e, ?string $t): array => TempMailLolV2::getEmails($e, $t),
        ];

        // tempmailpro：POST /mailbox/create + GET /mailbox/{token}/emails
        $ov['tempmailpro'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => TempMailPro::generate(),
            static fn (string $e, ?string $t): array => TempMailPro::getEmails($e, $t),
        ];

        // tempmailten：Laravel CSRF，POST /messages + GET /view/{id}
        $ov['tempmailten'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => TempMailTen::generate(),
            static fn (string $e, ?string $t): array => TempMailTen::getEmails($e, $t),
        ];

        // tempp-mails：与 tempmailten 共享模板（不同域名/token 前缀）
        $ov['tempp-mails'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => TemppMails::generate(),
            static fn (string $e, ?string $t): array => TemppMails::getEmails($e, $t),
        ];

        // ten-minute-mail-net：PHPSESSID + address.api.php + mail.api.php
        $ov['ten-minute-mail-net'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => TenMinuteMailNetApi::generate(),
            static fn (string $e, ?string $t): array => TenMinuteMailNetApi::getEmails($e, $t),
        ];

        // throwawaymail：POST /mailboxes + GET messages + detail
        $ov['throwawaymail'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => ThrowawayMail::generate(),
            static fn (string $e, ?string $t): array => ThrowawayMail::getEmails($e, $t),
        ];

        // tmail-link：Django csrftoken，GET 首页 + POST /inbox/{email}/
        $ov['tmail-link'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => TmailLink::generate(),
            static fn (string $e, ?string $t): array => TmailLink::getEmails($e, $t),
        ];

        // uncorreotemporal：POST /mailboxes + X-Session-Token 读信
        $ov['uncorreotemporal'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => UnCorreoTemporal::generate(),
            static fn (string $e, ?string $t): array => UnCorreoTemporal::getEmails($e, $t),
        ];

        // webmailtemp：GET /api/create + GET /api/check/{user}，token 含 cookie
        $ov['webmailtemp'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => WebMailTemp::generate(),
            static fn (string $e, ?string $t): array => WebMailTemp::getEmails($e, $t),
        ];

        // xkx-me：CSRF + session cookie，POST /mailbox/create/random（禁重定向）+ GET messages
        $ov['xkx-me'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => XkxMe::generate(),
            static fn (string $e, ?string $t): array => XkxMe::getEmails($e, $t),
        ];

        return $ov;
    }

    /**
     * 惰性初始化：按 ChannelData 顺序注册全部渠道。
     */
    private static function ensureInitialized(): void
    {
        if (self::$initialized) {
            return;
        }
        self::$initialized = true;

        $overrides = self::overrides();

        foreach (ChannelData::CHANNELS as [$channel, $name, $website]) {
            if (isset($overrides[$channel])) {
                [$generate, $getEmails] = $overrides[$channel];
            } else {
                $generate = Stub::makeGenerate($channel);
                $getEmails = Stub::makeGetEmails($channel);
            }
            self::register(new ChannelSpec($channel, $name, $website, $generate, $getEmails));
        }
    }
}
