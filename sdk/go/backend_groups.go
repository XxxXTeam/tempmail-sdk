package tempemail

import (
	"sync"
	"time"
)

var backendGroups = map[string][]Channel{
	"mailinator": {
		ChannelMailinator,
		"sogetthis-com", "bobmail-info", "suremail-info", "binkmail-com",
		"veryrealemail-com", "chammy-info", "thisisnotmyrealemail-com",
		"notmailinator-com", "spamhereplease-com", "sendspamhere-com",
		"sendfree-org",
	},
	"getnada": {
		ChannelGetnada,
		"getnada-com", "getnada-email", "getnada-net",
	},
	"guerrillamail": {
		ChannelGuerrillaMail,
		"sharklasers",
	},
	"moakt": {
		ChannelMoakt,
		"drmail-in", "teml-net", "tmpeml-com", "tmpbox-net",
		"moakt-cc", "disbox-net", "tmpmail-org", "tmpmail-net",
		"tmails-net", "disbox-org", "moakt-co", "moakt-ws",
		"tmail-ws", "bareed-ws",
	},
	"catchmail": {
		ChannelCatchmail,
		"catchmail-mailistry", "catchmail-zeppost",
	},
	"mailforspam": {
		ChannelMailforspam,
		"mailforspam-tempmail-io", "mailforspam-disposable",
	},
	"tempmail-plus": {
		ChannelTempmailPlus,
		"fexpost-com", "fexbox-org", "mailbox-in-ua",
		"rover-info", "chitthi-in", "fextemp-com",
		"any-pink", "merepost-com",
	},
	"mail-cx": {
		ChannelMailCx,
		"ddker-com",
	},
	"fake-legal": {
		ChannelFakeLegal,
		"imgui-de", "pulsewebmenu-de",
	},
	"neighbours": {
		"neighbours-sh", "neighbours",
	},
}

var channelToBackend map[Channel]string

func init() {
	channelToBackend = make(map[Channel]string)
	for backend, channels := range backendGroups {
		for _, ch := range channels {
			channelToBackend[ch] = backend
		}
	}
}

type circuitState struct {
	lastFailure time.Time
	failCount   int
}

var (
	circuitMu  sync.Mutex
	circuitMap = make(map[string]*circuitState)
)

const defaultCooldown = 60 * time.Second
const maxCooldown = 5 * time.Minute

func isBackendOpen(backend string) bool {
	circuitMu.Lock()
	defer circuitMu.Unlock()
	state, ok := circuitMap[backend]
	if !ok {
		return true
	}
	cooldown := defaultCooldown * time.Duration(1<<(state.failCount-1))
	if cooldown > maxCooldown {
		cooldown = maxCooldown
	}
	if time.Since(state.lastFailure) >= cooldown {
		delete(circuitMap, backend)
		return true
	}
	return false
}

func recordBackendFailure(backend string) {
	circuitMu.Lock()
	defer circuitMu.Unlock()
	state, ok := circuitMap[backend]
	if ok {
		state.lastFailure = time.Now()
		state.failCount++
	} else {
		circuitMap[backend] = &circuitState{lastFailure: time.Now(), failCount: 1}
	}
}

func recordBackendSuccess(backend string) {
	circuitMu.Lock()
	defer circuitMu.Unlock()
	delete(circuitMap, backend)
}
