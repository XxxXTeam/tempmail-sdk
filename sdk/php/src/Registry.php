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
use ChanhanzhanX\TempMail\Providers\HarakiriMail;
use ChanhanzhanX\TempMail\Providers\Inboxes;
use ChanhanzhanX\TempMail\Providers\InboxKitten;
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
use ChanhanzhanX\TempMail\Providers\Moakt;
use ChanhanzhanX\TempMail\Providers\Neighbours;
use ChanhanzhanX\TempMail\Providers\NeighboursSh;
use ChanhanzhanX\TempMail\Providers\Nimail;
use ChanhanzhanX\TempMail\Providers\Ockito;
use ChanhanzhanX\TempMail\Providers\RestmailNet;
use ChanhanzhanX\TempMail\Providers\Smailpro;
use ChanhanzhanX\TempMail\Providers\LinshiCo;
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
use ChanhanzhanX\TempMail\Providers\TempMailOrg;
use ChanhanzhanX\TempMail\Providers\TempEmailCo;
use ChanhanzhanX\TempMail\Providers\TempEmailInfo;
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
use ChanhanzhanX\TempMail\Providers\XkxMe;
use ChanhanzhanX\TempMail\Providers\Firetempmail;
use ChanhanzhanX\TempMail\Providers\Huskmail;
use ChanhanzhanX\TempMail\Providers\Mtempmail;
use ChanhanzhanX\TempMail\Providers\Nullmail;
use ChanhanzhanX\TempMail\Providers\ShitpostEmail;
use ChanhanzhanX\TempMail\Providers\Smails;
use ChanhanzhanX\TempMail\Providers\TempmailEe;
use ChanhanzhanX\TempMail\Providers\Tempmailportal;
use ChanhanzhanX\TempMail\Providers\TempmailsIo;
use ChanhanzhanX\TempMail\Providers\TenminApp;
use ChanhanzhanX\TempMail\Providers\Zerodrop;
// 第四轮新增渠道 provider 导入
use ChanhanzhanX\TempMail\Providers\TemporarymailCom;
use ChanhanzhanX\TempMail\Providers\ThirtyMinEmail;
use ChanhanzhanX\TempMail\Providers\LinshiXyz;
use ChanhanzhanX\TempMail\Providers\Crazymailing;
use ChanhanzhanX\TempMail\Providers\Nukemail;
use ChanhanzhanX\TempMail\Providers\Shadowmail;
use ChanhanzhanX\TempMail\Providers\Flybymail;
use ChanhanzhanX\TempMail\Providers\Nowtempmail;
use ChanhanzhanX\TempMail\Providers\Clawdemail;
use ChanhanzhanX\TempMail\Providers\Tempmail100;
use ChanhanzhanX\TempMail\Providers\Mailticking;
use ChanhanzhanX\TempMail\Providers\Tempmailto;
use ChanhanzhanX\TempMail\Providers\TempMailGg;
use ChanhanzhanX\TempMail\Providers\Tmpkit;
use ChanhanzhanX\TempMail\Providers\Internxt;
use ChanhanzhanX\TempMail\Providers\GeneratorEmail;

/**
 * 渠道注册表：单一事实来源
 *
 * 按 ChannelData 定义的顺序（与 baseline 逐行一致）注册全部 290 个渠道。
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
            'kalimantantimur-net' => 'kalimantantimur.net',
            'luxury345-com' => 'luxury345.com',
            'pehol-com' => 'pehol.com',
            'ptruyen-com' => 'ptruyen.com',
            'riau-net' => 'riau.net',
            'seokey-org' => 'seokey.org',
            'sulawesiselatan-net' => 'sulawesiselatan.net',
            'sumaterabarat-net' => 'sumaterabarat.net',
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
            '17666688-shop' => '17666688.shop',
            '282mail-com' => '282mail.com',
            'bsdu32-buzz' => 'bsdu32.buzz',
            'doxu243-buzz' => 'doxu243.buzz',
            'easyme-pro' => 'easyme.pro',
            'layueming-pics' => 'layueming.pics',
            'mingyuekeji-online' => 'mingyuekeji.online',
            'mingyueming-click' => 'mingyueming.click',
            'mingyueming-shop' => 'mingyueming.shop',
            'mingyukeji-lol' => 'mingyukeji.lol',
            'nuxh62-space' => 'nuxh62.space',
            'proid-cloud-ip-cc' => 'proid.cloud-ip.cc',
            'sbook-pics' => 'sbook.pics',
            'xue32-buzz' => 'xue32.buzz',
            '2famail-com' => '2famail.com',
            'xikemail-com' => 'xikemail.com',
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

        // inboxes.com 镜像域名渠道：固定域名生成邮箱，回填各自渠道标识
        $ov['blondmail-com'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => Inboxes::generate('blondmail.com', 'blondmail-com'),
            static fn (string $e, ?string $t): array => Inboxes::getEmails($e),
        ];
        $ov['chapsmail-com'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => Inboxes::generate('chapsmail.com', 'chapsmail-com'),
            static fn (string $e, ?string $t): array => Inboxes::getEmails($e),
        ];
        $ov['clowmail-com'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => Inboxes::generate('clowmail.com', 'clowmail-com'),
            static fn (string $e, ?string $t): array => Inboxes::getEmails($e),
        ];
        $ov['dropjar-com'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => Inboxes::generate('dropjar.com', 'dropjar-com'),
            static fn (string $e, ?string $t): array => Inboxes::getEmails($e),
        ];
        $ov['fivermail-com'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => Inboxes::generate('fivermail.com', 'fivermail-com'),
            static fn (string $e, ?string $t): array => Inboxes::getEmails($e),
        ];
        $ov['getairmail-com'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => Inboxes::generate('getairmail.com', 'getairmail-com'),
            static fn (string $e, ?string $t): array => Inboxes::getEmails($e),
        ];
        $ov['getmule-com'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => Inboxes::generate('getmule.com', 'getmule-com'),
            static fn (string $e, ?string $t): array => Inboxes::getEmails($e),
        ];
        $ov['inboxbear-com'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => Inboxes::generate('inboxbear.com', 'inboxbear-com'),
            static fn (string $e, ?string $t): array => Inboxes::getEmails($e),
        ];
        $ov['replyloop-com'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => Inboxes::generate('replyloop.com', 'replyloop-com'),
            static fn (string $e, ?string $t): array => Inboxes::getEmails($e),
        ];
        $ov['temptami-com'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => Inboxes::generate('temptami.com', 'temptami-com'),
            static fn (string $e, ?string $t): array => Inboxes::getEmails($e),
        ];

        // inboxkitten：本地生成邮箱，storage 补全正文
        $ov['inboxkitten'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => InboxKitten::generate(),
            static fn (string $e, ?string $t): array => InboxKitten::getEmails($e),
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

        // xkx-me：CSRF + session cookie，POST /mailbox/create/random（禁重定向）+ GET messages
        $ov['xkx-me'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => XkxMe::generate(),
            static fn (string $e, ?string $t): array => XkxMe::getEmails($e, $t),
        ];

        // === Go 端新增渠道同步绑定 ===

        // tempmails-io：POST /api/temp-mail/generate 建箱，读信前先 POST fetch-emails 触发上游同步
        $ov['tempmails-io'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => TempmailsIo::generate(),
            static fn (string $e, ?string $t): array => TempmailsIo::getEmails($e, $t ?? ''),
        ];

        // shitpost-email：POST /api/create 建箱（本地随机 username），GET /api/inbox 读信
        $ov['shitpost-email'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => ShitpostEmail::generate(),
            static fn (string $e, ?string $t): array => ShitpostEmail::getEmails($e, $t ?? ''),
        ];

        // smails：POST /api/mailbox 建箱，Bearer 读信（列表 + 逐封详情合并）
        $ov['smails'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => Smails::generate(),
            static fn (string $e, ?string $t): array => Smails::getEmails($e, $t ?? ''),
        ];

        // tempmailportal：POST /api/v2/inbox 建箱，Bearer 读信（列表 + 逐封详情合并）
        $ov['tempmailportal'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => Tempmailportal::generate(),
            static fn (string $e, ?string $t): array => Tempmailportal::getEmails($e, $t ?? ''),
        ];

        // huskmail：POST /api/v1/accounts 建箱（JWT），GET /v1/messages 读信（列表 + 详情合并）
        $ov['huskmail'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => Huskmail::generate(),
            static fn (string $e, ?string $t): array => Huskmail::getEmails($e, $t ?? ''),
        ];

        // zerodrop：本地生成地址（@zerodrop-sandbox.online），GET /api/inbox/{name} 读信
        $ov['zerodrop'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => Zerodrop::generate(),
            static fn (string $e, ?string $t): array => Zerodrop::getEmails($e, $t),
        ];

        // firetempmail：本地生成地址，GET mail.firetempmail.com/mail/get 读信（必带 Origin）
        $ov['firetempmail'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => Firetempmail::generate(),
            static fn (string $e, ?string $t): array => Firetempmail::getEmails($e, $t),
        ];

        // nullmail：POST /api/emails 建箱，GET /api/emails/{addr} 读信（正文逐封二拉 body 端点）
        $ov['nullmail'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => Nullmail::generate(),
            static fn (string $e, ?string $t): array => Nullmail::getEmails($e, $t),
        ];

        // tenmin-app：GET /api/inbox/{localpart} 首访即建箱（6 位十六进制 localpart）
        $ov['tenmin-app'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => TenminApp::generate(),
            static fn (string $e, ?string $t): array => TenminApp::getEmails($e, $t),
        ];

        // mtempmail：POST /api/emails/{apiKey} 建箱（公共 key），GET /api/messages/{apiKey}/{email} 读信
        $ov['mtempmail'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => Mtempmail::generate(),
            static fn (string $e, ?string $t): array => Mtempmail::getEmails($e, $t),
        ];

        // tempmail-ee：GET / 建立会话 → POST /api/mailbox/change（sec-ch-ua + browserIntegrity）
        // 提取 Set-Cookie（temp_email + temp_mail_session），读信 POST /api/mails + 单封 MIME 解析
        $ov['tempmail-ee'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => TempmailEe::generate(),
            static fn (string $e, ?string $t): array => TempmailEe::getEmails($e, $t ?? ''),
        ];

        // === Go 端第四轮新增渠道同步绑定 ===

        // temporarymail-com：GET /api/?action=requestEmailAccess 建箱（secretKey 鉴权），checkInbox 读信
        // 列表主题常为 "[No Subject]"：详情覆盖真实主题 + /view/ 渲染端点全文剥标签还原
        $ov['temporarymail-com'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => TemporarymailCom::generate(),
            static fn (string $e, ?string $t): array => TemporarymailCom::getEmails($e, $t ?? ''),
        ];

        // 30minemail：GET /?generate 返回 HTML 页面解析 16 位 hex 本地名，messages.php 读信
        $ov['30minemail'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => ThirtyMinEmail::generate(),
            static fn (string $e, ?string $t): array => ThirtyMinEmail::getEmails($e, $t ?? ''),
        ];

        // linshi-xyz：本地生成 6 位 hex 前缀（无建箱请求），GET /api/mails/{前缀} 读信
        $ov['linshi-xyz'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => LinshiXyz::generate(),
            static fn (string $e, ?string $t): array => LinshiXyz::getEmails($e, $t ?? ''),
        ];

        // crazymailing：POST /api/mailbox 建箱（Origin/Referer），REST 读信 + 逐封二拉 body 端点
        $ov['crazymailing'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => Crazymailing::generate(),
            static fn (string $e, ?string $t): array => Crazymailing::getEmails($e, $t ?? ''),
        ];

        // nukemail：SHA-256 PoW（前缀 4 零）建箱 + nukemail_token Cookie 会话读信
        $ov['nukemail'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => Nukemail::generate(),
            static fn (string $e, ?string $t): array => Nukemail::getEmails($e, $t ?? ''),
        ];

        // shadowmail：register/login/new-address 会话（sessionId Cookie），get-emails 读信
        $ov['shadowmail'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => Shadowmail::generate(),
            static fn (string $e, ?string $t): array => Shadowmail::getEmails($e, $t ?? ''),
        ];

        // flybymail：POST /api/recipients 建箱，GET /api/recipients/{email}/emails 读信
        $ov['flybymail'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => Flybymail::generate(),
            static fn (string $e, ?string $t): array => Flybymail::getEmails($e, $t ?? ''),
        ];

        // nowtempmail：POST /mailbox 建箱，Bearer JWT 读信（列表 + 逐封详情合并）
        $ov['nowtempmail'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => Nowtempmail::generate(),
            static fn (string $e, ?string $t): array => Nowtempmail::getEmails($e, $t ?? ''),
        ];

        // clawdemail：POST /register 建箱（api.clawdemail.com），Bearer 读信（列表 + 详情合并）
        $ov['clawdemail'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => Clawdemail::generate(),
            static fn (string $e, ?string $t): array => Clawdemail::getEmails($e, $t ?? ''),
        ];

        // tempmail100：POST /init 取 JWT + POST /web/generate 建箱，GET /web/emails（裸 token）读信
        $ov['tempmail100'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => Tempmail100::generate(),
            static fn (string $e, ?string $t): array => Tempmail100::getEmails($e, $t ?? ''),
        ];

        // tempmailto：Laravel Cookie 会话，GET 首页建箱 + POST /get_messages 读信（_token + captcha 空）
        $ov['tempmailto'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => Tempmailto::generate(),
            static fn (string $e, ?string $t): array => Tempmailto::getEmails($e, $t ?? ''),
        ];

        // temp-mail-gg：Livewire v3 会话，POST /livewire/update 建箱/读信
        $ov['temp-mail-gg'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => TempMailGg::generate(),
            static fn (string $e, ?string $t): array => TempMailGg::getEmails($e, $t ?? ''),
        ];

        // mailticking：POST /get-mailbox 建箱（type=4 独立域名）+ activate-email 激活，get-emails 列信
        $ov['mailticking'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => Mailticking::generate(),
            static fn (string $e, ?string $t): array => Mailticking::getEmails($e, $t ?? ''),
        ];

        // tmpkit：tRPC 会话（initSession 建箱 + getEmails/getEmailDetail 读信，Cookie tempmail_session）
        $ov['tmpkit'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => Tmpkit::generate(),
            static fn (string $e, ?string $t): array => Tmpkit::getEmails($e, $t ?? ''),
        ];

        // internxt：Cookie 会话 + csrf-token 头（create-email 建箱，get-inbox/get-message 读信）
        $ov['internxt'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => Internxt::generate(),
            static fn (string $e, ?string $t): array => Internxt::getEmails($e, $t ?? ''),
        ];

        // generator-email：SSR 网页型（SITE_DATA 快照建箱 + 收信页 list-group-item2 行解析）
        $ov['generator-email'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => GeneratorEmail::generate(),
            static fn (string $e, ?string $t): array => GeneratorEmail::getEmails($e, $t ?? ''),
        ];

        // inboxes.com 镜像域名渠道（固定域名生成 + 回填渠道标识）
        $ov['gimpmail-com'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => Inboxes::generate('gimpmail.com', 'gimpmail-com'),
            static fn (string $e, ?string $t): array => Inboxes::getEmails($e),
        ];
        $ov['givmail-com'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => Inboxes::generate('givmail.com', 'givmail-com'),
            static fn (string $e, ?string $t): array => Inboxes::getEmails($e),
        ];
        $ov['guysmail-com'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => Inboxes::generate('guysmail.com', 'guysmail-com'),
            static fn (string $e, ?string $t): array => Inboxes::getEmails($e),
        ];
        $ov['robot-mail-com'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => Inboxes::generate('robot-mail.com', 'robot-mail-com'),
            static fn (string $e, ?string $t): array => Inboxes::getEmails($e),
        ];
        $ov['tupmail-com'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => Inboxes::generate('tupmail.com', 'tupmail-com'),
            static fn (string $e, ?string $t): array => Inboxes::getEmails($e),
        ];
        $ov['vomoto-com'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => Inboxes::generate('vomoto.com', 'vomoto-com'),
            static fn (string $e, ?string $t): array => Inboxes::getEmails($e),
        ];

        // mail.cx 镜像域名渠道（固定域名 + 回填渠道标识）
        $ov['uqu-me'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => MailCx::generate('uqu.me', 'uqu-me'),
            static fn (string $e, ?string $t): array => MailCx::getEmails($e, $t),
        ];
        $ov['9k3r-com'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => MailCx::generate('9k3r.com', '9k3r-com'),
            static fn (string $e, ?string $t): array => MailCx::getEmails($e, $t),
        ];

        // mail.tm 镜像域名渠道（指定域名建账号 + 回填渠道标识）
        $ov['fldubai-com'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => MailTm::generate('fldubai-com', 'fldubai.com'),
            static fn (string $e, ?string $t): array => MailTm::getEmails($e, $t),
        ];

        // duckmail 镜像域名渠道（指定域名建账号 + 回填渠道标识）
        $ov['niceground-shop'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => DuckMail::generate('niceground-shop', 'niceground.shop'),
            static fn (string $e, ?string $t): array => DuckMail::getEmails($e, $t),
        ];
        $ov['glasswhitehub-com'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => DuckMail::generate('glasswhitehub-com', 'glasswhitehub.com'),
            static fn (string $e, ?string $t): array => DuckMail::getEmails($e, $t),
        ];
        $ov['hubaiclass-org'] = [
            static fn (GenerateEmailOptions $o): EmailInfo => DuckMail::generate('hubaiclass-org', 'hubaiclass.org'),
            static fn (string $e, ?string $t): array => DuckMail::getEmails($e, $t),
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
