use std::collections::HashMap;
use std::sync::{LazyLock, Mutex};
use std::time::Instant;

use crate::types::Channel;

static BACKEND_GROUPS: LazyLock<HashMap<&'static str, Vec<Channel>>> = LazyLock::new(|| {
    let mut m = HashMap::new();
    m.insert("mailinator", vec![
        Channel::Mailinator,
        Channel::SogetthisCom,
        Channel::BobmailInfo,
        Channel::SuremailInfo,
        Channel::BinkmailCom,
        Channel::VeryrealemailCom,
        Channel::ChammyInfo,
        Channel::ThisisnotmyrealemailCom,
        Channel::NotmailinatorCom,
        Channel::SpamherepleaseCom,
        Channel::SendspamhereCom,
        Channel::SendfreeOrg,
    ]);
    m.insert("getnada", vec![
        Channel::Getnada,
        Channel::GetnadaCom,
        Channel::GetnadaEmail,
        Channel::GetnadaNet,
    ]);
    m.insert("guerrillamail", vec![
        Channel::GuerrillaMail,
        Channel::Sharklasers,
    ]);
    m.insert("moakt", vec![
        Channel::Moakt,
        Channel::DrmailIn,
        Channel::TemlNet,
        Channel::TmpemlCom,
        Channel::TmpboxNet,
        Channel::MoaktCc,
        Channel::DisboxNet,
        Channel::TmpmailOrg,
        Channel::TmpmailNet,
        Channel::TmailsNet,
        Channel::DisboxOrg,
        Channel::MoaktCo,
        Channel::MoaktWs,
        Channel::TmailWs,
        Channel::BareedWs,
    ]);
    m.insert("catchmail", vec![
        Channel::Catchmail,
        Channel::CatchmailMailistry,
        Channel::CatchmailZeppost,
    ]);
    m.insert("mailforspam", vec![
        Channel::Mailforspam,
        Channel::MailforspamTempmailIo,
        Channel::MailforspamDisposable,
    ]);
    m.insert("tempmail-plus", vec![
        Channel::TempmailPlus,
        Channel::FexpostCom,
        Channel::FexboxOrg,
        Channel::MailboxInUa,
        Channel::RoverInfo,
        Channel::ChitthiIn,
        Channel::FextempCom,
        Channel::AnyPink,
        Channel::MerepostCom,
    ]);
    m.insert("mail-cx", vec![
        Channel::MailCx,
        Channel::DdkerCom,
    ]);
    m.insert("fake-legal", vec![
        Channel::FakeLegal,
        Channel::ImguiDe,
        Channel::PulsewebmenuDe,
    ]);
    m.insert("neighbours", vec![
        Channel::NeighboursSh,
        Channel::Neighbours,
    ]);
    m
});

static CHANNEL_TO_BACKEND: LazyLock<HashMap<Channel, &'static str>> = LazyLock::new(|| {
    let mut m = HashMap::new();
    for (backend, channels) in BACKEND_GROUPS.iter() {
        for ch in channels {
            m.insert(ch.clone(), *backend);
        }
    }
    m
});

pub fn get_backend(channel: &Channel) -> Option<&'static str> {
    CHANNEL_TO_BACKEND.get(channel).copied()
}

const DEFAULT_COOLDOWN_SECS: f64 = 60.0;
const MAX_COOLDOWN_SECS: f64 = 300.0;

struct CircuitState {
    last_failure: Instant,
    fail_count: u32,
}

static CIRCUIT_MAP: LazyLock<Mutex<HashMap<&'static str, CircuitState>>> =
    LazyLock::new(|| Mutex::new(HashMap::new()));

pub fn is_backend_open(backend: &str) -> bool {
    let mut map = CIRCUIT_MAP.lock().unwrap();
    if let Some(state) = map.get(backend) {
        let cooldown_secs =
            (DEFAULT_COOLDOWN_SECS * 2f64.powi((state.fail_count as i32) - 1)).min(MAX_COOLDOWN_SECS);
        let elapsed = state.last_failure.elapsed().as_secs_f64();
        if elapsed >= cooldown_secs {
            map.remove(backend);
            true
        } else {
            false
        }
    } else {
        true
    }
}

pub fn record_backend_failure(backend: &'static str) {
    let mut map = CIRCUIT_MAP.lock().unwrap();
    if let Some(state) = map.get_mut(backend) {
        state.last_failure = Instant::now();
        state.fail_count += 1;
    } else {
        map.insert(backend, CircuitState {
            last_failure: Instant::now(),
            fail_count: 1,
        });
    }
}

pub fn record_backend_success(backend: &str) {
    let mut map = CIRCUIT_MAP.lock().unwrap();
    map.remove(backend);
}
