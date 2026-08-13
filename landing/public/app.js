/**
 * GrokLink OS research hub - interactive demos (browser-only education).
 * No live RF, no TX, no device control. Authorized research framing only.
 */
(function () {
  'use strict';

  var EDU = 'I_WILL_USE_ONLY_AUTHORIZED_TARGETS';
  var VAULT_KEY = 'glk_hub_vault_v1';
  var reduced =
    window.matchMedia &&
    window.matchMedia('(prefers-reduced-motion: reduce)').matches;

  function $(sel, root) {
    return (root || document).querySelector(sel);
  }
  function $$(sel, root) {
    return Array.prototype.slice.call((root || document).querySelectorAll(sel));
  }

  function el(tag, cls, text) {
    var n = document.createElement(tag);
    if (cls) n.className = cls;
    if (text != null) n.textContent = text;
    return n;
  }

  function copyText(text, btn) {
    function done(ok) {
      if (!btn) return;
      var prev = btn.getAttribute('data-label') || btn.textContent;
      btn.setAttribute('data-label', prev);
      btn.textContent = ok ? 'Copied' : 'Copy failed';
      setTimeout(function () {
        btn.textContent = btn.getAttribute('data-label') || 'Copy';
      }, 1400);
    }
    if (navigator.clipboard && navigator.clipboard.writeText) {
      navigator.clipboard.writeText(text).then(
        function () {
          done(true);
        },
        function () {
          done(false);
        }
      );
    } else {
      var ta = document.createElement('textarea');
      ta.value = text;
      ta.style.position = 'fixed';
      ta.style.left = '-9999px';
      document.body.appendChild(ta);
      ta.select();
      try {
        done(document.execCommand('copy'));
      } catch (e) {
        done(false);
      }
      document.body.removeChild(ta);
    }
  }

  /* ---------- simple FNV-1a 32 (educational; matches storage integrity style) ---------- */
  function fnv1a32(str) {
    var h = 0x811c9dc5;
    for (var i = 0; i < str.length; i++) {
      h ^= str.charCodeAt(i);
      h = Math.imul(h, 0x01000193);
    }
    return ('00000000' + (h >>> 0).toString(16)).slice(-8);
  }

  /* ===================== Architecture explorer ===================== */
  var LAYERS = [
    {
      id: 'llm',
      name: 'PC / LLM tools',
      short: 'Bridge + observe API',
      accent: '#60a5fa',
      status: 'shipped',
      body:
        'Python package groklink-os: USB CDC or host-sim GrokRPC, multi-LLM observe tools (passive only), craft, vault, plug-sync. Optional localhost tools HTTP on :8741.',
      bits: ['observe_rx / spectrum', 'tools-schema (OpenAI tools)', 'plug-sync after field', 'lab GLK1 education'],
    },
    {
      id: 'apps',
      name: 'Apps / GUI',
      short: 'ST7567 pages + shell',
      accent: '#22d3ee',
      status: 'shipped',
      body:
        'On-device UI: HOME, RADIO, SAFETY, STORAGE, ABOUT. Shell / factory diagnostics on host paths. Field arm from SAFETY (hold OK) for unplugged passive explore.',
      bits: ['ST7567 monochrome', 'STORAGE page (v3.8)', 'FIELD ACTIVE indicator'],
    },
    {
      id: 'services',
      name: 'Services',
      short: 'Policy, agent, skills, audit',
      accent: '#34d399',
      status: 'shipped',
      body:
        'Safety/policy engine on the actuator path, GrokAgent mission IR, skill host, RPC, audit, storage, power, radio arbiter. ML (if present) is advisory only.',
      bits: ['default-deny policy', 'hash-chained audit', 'agent resume tokens', 'spectrum circuit breaker'],
    },
    {
      id: 'drivers',
      name: 'Drivers',
      short: 'Radio + bus I/O',
      accent: '#a78bfa',
      status: 'shipped',
      body:
        'Async drivers: SubGHz (CC1101), IR, NFC wrappers, GPIO, iButton, UART/SPI/I2C, BLE wrappers. TX and drive paths still gate through policy.',
      bits: ['CC1101 VERSION 0x14', 'bit-banged SPI_R path', 'BLE via safety wrappers'],
    },
    {
      id: 'hal',
      name: 'HAL',
      short: 'Board-portable',
      accent: '#fbbf24',
      status: 'shipped',
      body:
        'Clock, GPIO, DMA, timer, USB, flash, SDMMC hooks, radio phy. Keeps kernel and services portable across host-sim and STM32WB55 BSP.',
      bits: ['host-sim first-class', 'WB55 primary target'],
    },
    {
      id: 'kernel',
      name: 'Kernel',
      short: 'GrokLink RTOS',
      accent: '#fb7185',
      status: 'shipped',
      body:
        'Lightweight preemptive RTOS: tasks, mutex (priority inheritance), queues, events, software timers, memory pools, tick, power. M4 owns app; M0+ reserved for vendor BLE stack via IPCC.',
      bits: ['priority map 0..31', 'fail-closed budgets', 'deep-sleep aware'],
    },
    {
      id: 'bsp',
      name: 'Platform BSP',
      short: 'host sim | stm32wb55',
      accent: '#94a3b8',
      status: 'shipped',
      body:
        'Board support: host simulator for CI and bring-up, STM32WB55 field board. Future MCUs plug under the same HAL boundary.',
      bits: ['USB CDC 0483:5740 app', 'DFU 0483:DF11'],
    },
  ];

  function initArchitecture() {
    var list = $('#arch-layers');
    var detail = $('#arch-detail');
    if (!list || !detail) return;

    LAYERS.forEach(function (L, i) {
      var btn = el('button', 'arch-layer');
      btn.type = 'button';
      btn.setAttribute('data-id', L.id);
      btn.setAttribute('aria-pressed', i === 2 ? 'true' : 'false');
      btn.style.setProperty('--layer-accent', L.accent);
      btn.innerHTML =
        '<span class="arch-layer-name">' +
        L.name +
        '</span><span class="arch-layer-short">' +
        L.short +
        '</span><span class="arch-layer-status">' +
        L.status +
        '</span>';
      btn.addEventListener('click', function () {
        $$('.arch-layer', list).forEach(function (b) {
          b.setAttribute('aria-pressed', 'false');
        });
        btn.setAttribute('aria-pressed', 'true');
        renderArchDetail(L);
      });
      list.appendChild(btn);
    });
    renderArchDetail(LAYERS[2]);
  }

  function renderArchDetail(L) {
    var detail = $('#arch-detail');
    if (!detail) return;
    detail.innerHTML = '';
    var h = el('h3', null, L.name);
    h.style.color = L.accent;
    detail.appendChild(h);
    detail.appendChild(el('p', 'muted', L.body));
    var ul = el('ul', 'chip-list');
    L.bits.forEach(function (b) {
      var li = el('li', 'chip', b);
      ul.appendChild(li);
    });
    detail.appendChild(ul);
    var note = el(
      'p',
      'tiny muted',
      'Honest stack: native OS owns these layers end-to-end. Not a Flipper / Furi / Momentum overlay.'
    );
    detail.appendChild(note);
  }

  /* ===================== Safety policy simulator ===================== */
  var MODES = {
    info: {
      label: 'info',
      risk: 'low',
      color: '#94a3b8',
      desc: 'Status, skill list, non-actuator reads. Lowest friction; still audited when configured.',
      gates: ['session optional', 'audit (optional)'],
      allow: true,
    },
    passive_rx: {
      label: 'passive_rx',
      risk: 'medium',
      color: '#34d399',
      desc: 'SubGHz / IR / NFC passive RX. Needs edu session, frequency window, rate limit, blacklist check.',
      gates: ['edu-ack', 'freq window', 'rate limit', 'blacklist'],
      allow: null,
    },
    active_tx: {
      label: 'active_tx',
      risk: 'high',
      color: '#fbbf24',
      desc: 'SubGHz / IR TX. Default-deny until edu + single-use confirm token + duty + power cap + blacklist all pass.',
      gates: ['edu-ack', 'confirm token TTL', 'blacklist', 'duty cycle', 'power cap', 'audit'],
      allow: null,
    },
    gpio: {
      label: 'gpio / contact',
      risk: 'high',
      color: '#fb923c',
      desc: 'GPIO drive and contact write. Confirm-scoped; pin blacklists; agent cannot self-mint tokens by default.',
      gates: ['edu-ack', 'confirm', 'pin blacklist', 'audit'],
      allow: null,
    },
    system: {
      label: 'system',
      risk: 'critical',
      color: '#fb7185',
      desc: 'Reboot, unlock, rekey. Requires edu + physical on-device OK for system class.',
      gates: ['edu-ack', 'physical confirm', 'audit'],
      allow: null,
    },
  };

  var safetyState = {
    mode: 'passive_rx',
    edu: false,
    token: null,
    tokenExpires: 0,
    blacklistOk: true,
    dutyOk: true,
    log: [],
  };

  function pushSafetyLog(msg, kind) {
    safetyState.log.unshift({ t: Date.now(), msg: msg, kind: kind || 'info' });
    if (safetyState.log.length > 12) safetyState.log.pop();
    renderSafetyLog();
  }

  function renderSafetyLog() {
    var box = $('#safety-log');
    if (!box) return;
    box.innerHTML = '';
    safetyState.log.forEach(function (row) {
      var line = el('div', 'sim-log-line sim-log-' + row.kind, row.msg);
      box.appendChild(line);
    });
  }

  function tokenValid() {
    return safetyState.token && Date.now() < safetyState.tokenExpires;
  }

  function evaluatePolicy() {
    var m = MODES[safetyState.mode];
    var reasons = [];
    var decision = 'ALLOW';

    if (safetyState.mode === 'info') {
      decision = 'ALLOW';
      reasons.push('info path - low friction');
    } else if (!safetyState.edu) {
      decision = 'DENY';
      reasons.push('missing edu-ack session');
    } else if (!safetyState.blacklistOk) {
      decision = 'DENY';
      reasons.push('blacklist fail-closed');
    } else if (safetyState.mode === 'passive_rx') {
      decision = 'ALLOW';
      reasons.push('edu + blacklist + rate windows OK (sim)');
    } else if (safetyState.mode === 'active_tx' || safetyState.mode === 'gpio') {
      if (!tokenValid()) {
        decision = 'DENY';
        reasons.push('confirm token missing or expired (TTL)');
      } else if (!safetyState.dutyOk && safetyState.mode === 'active_tx') {
        decision = 'DENY';
        reasons.push('duty-cycle / spectrum planner circuit breaker');
      } else {
        decision = 'ALLOW (single-use then burn)';
        reasons.push('all high-risk gates passed (browser mock only)');
      }
    } else if (safetyState.mode === 'system') {
      decision = 'DENY';
      reasons.push('system requires physical OK on device - web cannot satisfy');
    }

    return { decision: decision, reasons: reasons, mode: m };
  }

  function renderSafety() {
    var m = MODES[safetyState.mode];
    var evalR = evaluatePolicy();
    var modeEl = $('#safety-mode-label');
    var desc = $('#safety-mode-desc');
    var gates = $('#safety-gates');
    var decision = $('#safety-decision');
    var eduBadge = $('#safety-edu-badge');
    var tokBadge = $('#safety-token-badge');
    var blBadge = $('#safety-bl-badge');
    var dutyBadge = $('#safety-duty-badge');

    if (modeEl) {
      modeEl.textContent = m.label;
      modeEl.style.color = m.color;
    }
    if (desc) desc.textContent = m.desc;
    if (gates) {
      gates.innerHTML = '';
      m.gates.forEach(function (g) {
        gates.appendChild(el('span', 'gate', g));
      });
    }
    if (decision) {
      decision.textContent = evalR.decision;
      decision.className =
        'decision-pill ' +
        (evalR.decision.indexOf('ALLOW') === 0 ? 'allow' : 'deny');
      decision.title = evalR.reasons.join('; ');
    }
    if (eduBadge) {
      eduBadge.textContent = safetyState.edu ? 'edu: ACK' : 'edu: none';
      eduBadge.className = 'pill ' + (safetyState.edu ? 'ok' : 'bad');
    }
    if (tokBadge) {
      tokBadge.textContent = tokenValid()
        ? 'token: live ' + Math.max(0, Math.ceil((safetyState.tokenExpires - Date.now()) / 1000)) + 's'
        : 'token: none';
      tokBadge.className = 'pill ' + (tokenValid() ? 'ok' : 'muted');
    }
    if (blBadge) {
      blBadge.textContent = safetyState.blacklistOk ? 'blacklist: OK' : 'blacklist: FAIL';
      blBadge.className = 'pill ' + (safetyState.blacklistOk ? 'ok' : 'bad');
    }
    if (dutyBadge) {
      dutyBadge.textContent = safetyState.dutyOk ? 'duty: OK' : 'breaker OPEN';
      dutyBadge.className = 'pill ' + (safetyState.dutyOk ? 'ok' : 'bad');
    }

    var why = $('#safety-why');
    if (why) {
      why.innerHTML = '';
      evalR.reasons.forEach(function (r) {
        why.appendChild(el('li', null, r));
      });
    }

    drawPolicyCanvas(evalR);
  }

  function drawPolicyCanvas(evalR) {
    var c = $('#policy-canvas');
    if (!c || !c.getContext) return;
    var ctx = c.getContext('2d');
    var w = c.width;
    var h = c.height;
    ctx.clearRect(0, 0, w, h);
    ctx.fillStyle = '#060a12';
    ctx.fillRect(0, 0, w, h);

    var nodes = [
      { x: 50, y: h / 2, label: 'caller' },
      { x: 160, y: h / 2, label: 'edu?' },
      { x: 270, y: h / 2, label: 'class' },
      { x: 380, y: h / 2, label: 'confirm' },
      { x: 490, y: h / 2, label: 'duty' },
      { x: 600, y: h / 2, label: 'decision' },
    ];
    ctx.strokeStyle = 'rgba(34,211,238,0.35)';
    ctx.lineWidth = 2;
    for (var i = 0; i < nodes.length - 1; i++) {
      ctx.beginPath();
      ctx.moveTo(nodes[i].x + 28, nodes[i].y);
      ctx.lineTo(nodes[i + 1].x - 28, nodes[i + 1].y);
      ctx.stroke();
    }
    nodes.forEach(function (n, idx) {
      var on =
        (idx === 1 && safetyState.edu) ||
        (idx === 3 && tokenValid()) ||
        (idx === 4 && safetyState.dutyOk) ||
        idx === 0 ||
        idx === 2 ||
        idx === 5;
      ctx.beginPath();
      ctx.arc(n.x, n.y, 26, 0, Math.PI * 2);
      ctx.fillStyle = on ? 'rgba(34,211,238,0.2)' : 'rgba(30,58,95,0.5)';
      ctx.fill();
      ctx.strokeStyle = on ? '#22d3ee' : '#1e3a5f';
      ctx.stroke();
      ctx.fillStyle = '#e8eef8';
      ctx.font = '11px IBM Plex Mono, monospace';
      ctx.textAlign = 'center';
      ctx.fillText(n.label, n.x, n.y + 4);
    });
    ctx.fillStyle = evalR.decision.indexOf('ALLOW') === 0 ? '#34d399' : '#fb7185';
    ctx.font = 'bold 13px IBM Plex Mono, monospace';
    ctx.textAlign = 'left';
    ctx.fillText(evalR.decision, 40, h - 16);
  }

  function initSafety() {
    $$('[data-mode]').forEach(function (btn) {
      btn.addEventListener('click', function () {
        safetyState.mode = btn.getAttribute('data-mode');
        $$('[data-mode]').forEach(function (b) {
          b.setAttribute('aria-pressed', b === btn ? 'true' : 'false');
        });
        pushSafetyLog('mode -> ' + safetyState.mode, 'info');
        renderSafety();
      });
    });
    var eduBtn = $('#btn-edu-ack');
    if (eduBtn) {
      eduBtn.addEventListener('click', function () {
        safetyState.edu = true;
        pushSafetyLog('edu-ack: ' + EDU, 'ok');
        renderSafety();
      });
    }
    var tokBtn = $('#btn-issue-token');
    if (tokBtn) {
      tokBtn.addEventListener('click', function () {
        if (!safetyState.edu) {
          pushSafetyLog('confirm-issue DENY: edu required', 'deny');
          renderSafety();
          return;
        }
        safetyState.token = 'mock-' + fnv1a32(String(Date.now()));
        safetyState.tokenExpires = Date.now() + 60000;
        pushSafetyLog('confirm issued TTL=60s action-scoped (mock)', 'ok');
        renderSafety();
        if (!reduced) {
          var iv = setInterval(function () {
            if (!tokenValid()) {
              clearInterval(iv);
              pushSafetyLog('confirm token expired', 'deny');
            }
            renderSafety();
          }, 1000);
        }
      });
    }
    var tryBtn = $('#btn-try-action');
    if (tryBtn) {
      tryBtn.addEventListener('click', function () {
        var r = evaluatePolicy();
        if (r.decision.indexOf('ALLOW') === 0) {
          pushSafetyLog(
            'policy ALLOW for ' + safetyState.mode + ' (browser mock - no hardware)',
            'ok'
          );
          if (safetyState.mode === 'active_tx' || safetyState.mode === 'gpio') {
            safetyState.token = null;
            safetyState.tokenExpires = 0;
            pushSafetyLog('confirm token burned (single-use)', 'info');
          }
        } else {
          pushSafetyLog('policy DENY: ' + r.reasons.join('; '), 'deny');
        }
        renderSafety();
      });
    }
    var blBtn = $('#btn-toggle-bl');
    if (blBtn) {
      blBtn.addEventListener('click', function () {
        safetyState.blacklistOk = !safetyState.blacklistOk;
        pushSafetyLog(
          'blacklist ' + (safetyState.blacklistOk ? 'OK' : 'FAIL CLOSED'),
          safetyState.blacklistOk ? 'ok' : 'deny'
        );
        renderSafety();
      });
    }
    var dutyBtn = $('#btn-toggle-duty');
    if (dutyBtn) {
      dutyBtn.addEventListener('click', function () {
        safetyState.dutyOk = !safetyState.dutyOk;
        pushSafetyLog(
          'spectrum duty ' + (safetyState.dutyOk ? 'OK' : 'circuit breaker OPEN'),
          safetyState.dutyOk ? 'ok' : 'deny'
        );
        renderSafety();
      });
    }
    var resetBtn = $('#btn-safety-reset');
    if (resetBtn) {
      resetBtn.addEventListener('click', function () {
        safetyState.edu = false;
        safetyState.token = null;
        safetyState.tokenExpires = 0;
        safetyState.blacklistOk = true;
        safetyState.dutyOk = true;
        safetyState.mode = 'passive_rx';
        pushSafetyLog('session reset to default-deny baseline', 'info');
        $$('[data-mode]').forEach(function (b) {
          b.setAttribute(
            'aria-pressed',
            b.getAttribute('data-mode') === 'passive_rx' ? 'true' : 'false'
          );
        });
        renderSafety();
      });
    }
    pushSafetyLog('simulator ready - default deny on elevated actions', 'info');
    renderSafety();
  }

  /* ===================== Skills + Agent + Vault ===================== */
  var SKILLS = [
    {
      id: 'lab_passive_listen',
      name: 'Lab Passive Listen',
      source: 'ROM',
      risk: 'passive_rx',
      hw: 'subghz',
      desc: 'Owned-lab passive RX watch. Always available without SD.',
    },
    {
      id: 'lab_passive_watch',
      name: 'Lab Passive Watch',
      source: 'ROM',
      risk: 'passive_rx',
      hw: 'subghz',
      desc: 'Field mission for unplugged passive ticks (~600 ms cadence when armed).',
    },
    {
      id: 'medsec_lab_passive_ism',
      name: 'MedSec Passive ISM',
      source: 'ROM',
      risk: 'passive_rx',
      hw: 'subghz',
      desc: 'MedSec pack: passive facility/lab research under written RoE. Not clinical.',
    },
    {
      id: 'sd_custom_scan',
      name: 'Custom SD Skill (example)',
      source: 'SD',
      risk: 'passive_rx',
      hw: 'subghz',
      desc: 'Hot-load example: manifest on SD skills/<id>/ with optional GLKSIG1. Educational catalog only.',
    },
    {
      id: 'sd_tx_gated',
      name: 'TX Plan Skill (example)',
      source: 'SD',
      risk: 'active_tx',
      hw: 'subghz',
      desc: 'Risk-classed active_tx package. Still cannot fire without human confirm token on device.',
    },
  ];

  var agentState = {
    unplugged: false,
    running: false,
    mission: 'lab_passive_watch',
    steps: 0,
    selected: 'lab_passive_listen',
    timer: null,
  };

  function loadVault() {
    try {
      var raw = localStorage.getItem(VAULT_KEY);
      if (!raw) return { chain: [], root: '00000000' };
      return JSON.parse(raw);
    } catch (e) {
      return { chain: [], root: '00000000' };
    }
  }

  function saveVault(v) {
    try {
      localStorage.setItem(VAULT_KEY, JSON.stringify(v));
    } catch (e) {
      /* quota / private mode */
    }
  }

  function appendVault(actor, action, detail) {
    var v = loadVault();
    var prev = v.root || '00000000';
    var ts = new Date().toISOString();
    var payload = [prev, ts, actor, action, detail].join('|');
    var hash = fnv1a32(payload);
    var entry = {
      prev: prev,
      ts: ts,
      actor: actor,
      action: action,
      detail: detail,
      hash: hash,
    };
    v.chain.push(entry);
    if (v.chain.length > 40) v.chain = v.chain.slice(-40);
    v.root = hash;
    saveVault(v);
    renderVault();
    return entry;
  }

  function renderVault() {
    var v = loadVault();
    var list = $('#vault-list');
    var root = $('#vault-root');
    if (root) root.textContent = 'chain root: ' + (v.root || '00000000');
    if (!list) return;
    list.innerHTML = '';
    v.chain
      .slice()
      .reverse()
      .slice(0, 12)
      .forEach(function (e) {
        var row = el(
          'div',
          'vault-row',
          e.hash.slice(0, 8) +
            '  ' +
            e.action +
            '  ' +
            e.detail +
            '  <- ' +
            e.prev.slice(0, 8)
        );
        list.appendChild(row);
      });
    drawVaultCanvas(v);
  }

  function drawVaultCanvas(v) {
    var c = $('#vault-canvas');
    if (!c || !c.getContext) return;
    var ctx = c.getContext('2d');
    var w = c.width;
    var h = c.height;
    ctx.clearRect(0, 0, w, h);
    ctx.fillStyle = '#060a12';
    ctx.fillRect(0, 0, w, h);
    var n = Math.min(v.chain.length, 10);
    var start = Math.max(0, v.chain.length - n);
    ctx.font = '10px IBM Plex Mono, monospace';
    ctx.textAlign = 'left';
    for (var i = 0; i < n; i++) {
      var e = v.chain[start + i];
      var x = 20 + i * 58;
      var y = h / 2;
      ctx.beginPath();
      ctx.arc(x, y, 14, 0, Math.PI * 2);
      ctx.fillStyle = 'rgba(52,211,153,0.2)';
      ctx.fill();
      ctx.strokeStyle = '#34d399';
      ctx.stroke();
      if (i < n - 1) {
        ctx.beginPath();
        ctx.moveTo(x + 14, y);
        ctx.lineTo(x + 44, y);
        ctx.strokeStyle = 'rgba(34,211,238,0.4)';
        ctx.stroke();
      }
      ctx.fillStyle = '#a5f3fc';
      ctx.fillText(e.hash.slice(0, 4), x - 12, y + 32);
    }
    if (n === 0) {
      ctx.fillStyle = '#8b9bb8';
      ctx.fillText('empty vault - run a passive mission step', 16, h / 2);
    }
  }

  function renderSkills() {
    var grid = $('#skills-grid');
    if (!grid) return;
    grid.innerHTML = '';
    var filter = ($('#skills-filter') && $('#skills-filter').value) || 'all';
    SKILLS.forEach(function (s) {
      if (filter === 'rom' && s.source !== 'ROM') return;
      if (filter === 'sd' && s.source !== 'SD') return;
      if (filter === 'passive' && s.risk !== 'passive_rx') return;
      if (filter === 'tx' && s.risk !== 'active_tx') return;
      var card = el('button', 'skill-card');
      card.type = 'button';
      card.setAttribute('aria-pressed', agentState.selected === s.id ? 'true' : 'false');
      card.innerHTML =
        '<div class="skill-top"><span class="skill-id">' +
        s.id +
        '</span><span class="skill-src src-' +
        s.source.toLowerCase() +
        '">' +
        s.source +
        '</span></div>' +
        '<div class="skill-name">' +
        s.name +
        '</div>' +
        '<div class="skill-meta"><span class="risk-' +
        s.risk +
        '">' +
        s.risk +
        '</span> Â· ' +
        s.hw +
        '</div>' +
        '<p class="skill-desc">' +
        s.desc +
        '</p>';
      card.addEventListener('click', function () {
        agentState.selected = s.id;
        agentState.mission = s.risk === 'passive_rx' ? s.id : agentState.mission;
        renderSkills();
        renderAgentStatus();
      });
      grid.appendChild(card);
    });
  }

  function renderAgentStatus() {
    var st = $('#agent-status');
    if (!st) return;
    st.innerHTML =
      '<div>mission: <strong>' +
      agentState.mission +
      '</strong></div>' +
      '<div>unplugged autonomy: <strong>' +
      (agentState.unplugged ? 'ARMED' : 'off') +
      '</strong></div>' +
      '<div>steps this session: <strong>' +
      agentState.steps +
      '</strong></div>' +
      '<div class="tiny muted">Web demo only - passive. Device uses policy + vault on hardware.</div>';
  }

  function missionStep() {
    var sk = SKILLS.filter(function (s) {
      return s.id === agentState.mission;
    })[0];
    if (!sk || sk.risk !== 'passive_rx') {
      appendVault('agent', 'deny', 'non-passive mission blocked in hub demo');
      return;
    }
    agentState.steps += 1;
    var rssi = -40 - Math.floor(Math.random() * 50);
    appendVault(
      agentState.unplugged ? 'agent-offline' : 'agent',
      'passive_rx_step',
      agentState.mission + ' rssi~' + rssi + 'dBm (sim)'
    );
    renderAgentStatus();
  }

  function initSkills() {
    var filt = $('#skills-filter');
    if (filt) filt.addEventListener('change', renderSkills);
    var loadBtn = $('#btn-hotload');
    if (loadBtn) {
      loadBtn.addEventListener('click', function () {
        appendVault('skill-host', 'scan', 'ROM + SD skills root (sim hot-load)');
        pushToast('Skill scan complete (catalog refresh - educational mock)');
        renderSkills();
      });
    }
    var stepBtn = $('#btn-mission-step');
    if (stepBtn) {
      stepBtn.addEventListener('click', function () {
        missionStep();
      });
    }
    var runBtn = $('#btn-mission-run');
    if (runBtn) {
      runBtn.addEventListener('click', function () {
        var n = 3;
        function tick() {
          if (n-- <= 0) return;
          missionStep();
          if (!reduced) setTimeout(tick, 350);
          else tick();
        }
        tick();
      });
    }
    var unplug = $('#btn-unplug-toggle');
    if (unplug) {
      unplug.addEventListener('click', function () {
        agentState.unplugged = !agentState.unplugged;
        appendVault(
          'operator',
          agentState.unplugged ? 'prepare_unplugged' : 'disarm_offline',
          agentState.mission
        );
        if (agentState.timer) {
          clearInterval(agentState.timer);
          agentState.timer = null;
        }
        if (agentState.unplugged && !reduced) {
          agentState.timer = setInterval(function () {
            missionStep();
          }, 2000);
        }
        unplug.textContent = agentState.unplugged
          ? 'Disarm unplugged autonomy'
          : 'Arm unplugged autonomy';
        unplug.setAttribute('aria-pressed', agentState.unplugged ? 'true' : 'false');
        renderAgentStatus();
      });
    }
    var clearV = $('#btn-vault-clear');
    if (clearV) {
      clearV.addEventListener('click', function () {
        saveVault({ chain: [], root: '00000000' });
        renderVault();
        pushToast('Browser vault mock cleared (device vault unchanged)');
      });
    }
    renderSkills();
    renderAgentStatus();
    renderVault();
  }

  function pushToast(msg) {
    var t = $('#toast');
    if (!t) return;
    t.textContent = msg;
    t.hidden = false;
    clearTimeout(pushToast._tm);
    pushToast._tm = setTimeout(function () {
      t.hidden = true;
    }, 2400);
  }

  /* ===================== Quickstart wizard ===================== */
  var WIZARD = [
    {
      title: '1. Legal and education phrase',
      body:
        'Authorized research and owned equipment only. Not a medical device. Type or paste the education phrase before elevated work on device.',
      cmd: EDU,
      tips: [
        'Unauthorized RF / IR / RFID / NFC / access interference may be illegal.',
        'Web wizard never talks to hardware.',
      ],
    },
    {
      title: '2. Enter STM32 DFU',
      body:
        'Hold BACK+OK while plugging USB to enter DFU (0483:DF11). Install the latest GrokLink-OS radio DFU from GitHub Releases.',
      cmd: '# qFlipper: Install from file  |  or flash_os_dfu_only.ps1 from the repo',
      tips: [
        'qFlipper can flash DFU in bootloader mode only.',
        'It cannot manage GrokLink OS like Flipper firmware (no protobuf).',
      ],
    },
    {
      title: '3. Install PC bridge',
      body: 'Clone the repo and install the Python bridge with serial support.',
      cmd:
        'git clone https://github.com/Pitchfork-and-Torch/GrokLink-OS.git\ncd GrokLink-OS/bridge\npy -3 -m pip install -e ".[serial]"',
      tips: ['Set GLK_SERIAL_PORT=COMx on Windows after flash.', 'Host-sim works without hardware.'],
    },
    {
      title: '4. edu-ack + status',
      body: 'Open the USB serial at 230400 baud (GrokRPC JSON-line). Acknowledge education and check status.',
      cmd: 'groklink-os edu-ack\ngroklink-os status\ngroklink-os ping',
      tips: ['App CDC identity: VID_0483 / PID_5740 product GrokLink OS.', 'GrokRPC JSON API 6.'],
    },
    {
      title: '5. First passive observe',
      body: 'Prefer packaged observe tools (passive only). Never invent TX or third-party decode.',
      cmd: 'groklink-os observe-rx --freq 433920000 --ms 400\ngroklink-os observe-spectrum --freqs 315000000,433920000',
      tips: [
        'Observation sets safety.tx=false and decode=false.',
        'Captures stay on the operator machine by default.',
      ],
    },
    {
      title: '6. Plug-sync after field',
      body: 'If the device ran unplugged passive explore, ingest vault lessons on reconnect before other work.',
      cmd: 'groklink-os plug-sync\ngroklink-os vault-tail',
      tips: [
        'Unplugged: on-device agent can passive-explore; PC LLM needs USB.',
        'v3.8 durable vault when storage mode is ok.',
      ],
    },
  ];

  var wizStep = 0;

  function renderWizard() {
    var s = WIZARD[wizStep];
    var title = $('#wiz-title');
    var body = $('#wiz-body');
    var cmd = $('#wiz-cmd');
    var tips = $('#wiz-tips');
    var prog = $('#wiz-progress');
    if (title) title.textContent = s.title;
    if (body) body.textContent = s.body;
    if (cmd) cmd.textContent = s.cmd;
    if (tips) {
      tips.innerHTML = '';
      s.tips.forEach(function (t) {
        tips.appendChild(el('li', null, t));
      });
    }
    if (prog) {
      prog.textContent = 'Step ' + (wizStep + 1) + ' / ' + WIZARD.length;
      prog.setAttribute('aria-valuenow', String(wizStep + 1));
      prog.setAttribute('aria-valuemax', String(WIZARD.length));
    }
    var prev = $('#wiz-prev');
    var next = $('#wiz-next');
    if (prev) prev.disabled = wizStep === 0;
    if (next) next.textContent = wizStep === WIZARD.length - 1 ? 'Done' : 'Next';
    $$('.wiz-dot').forEach(function (d, i) {
      d.classList.toggle('on', i <= wizStep);
      d.classList.toggle('current', i === wizStep);
    });
  }

  function initWizard() {
    var dots = $('#wiz-dots');
    if (dots) {
      WIZARD.forEach(function (_, i) {
        var d = el('span', 'wiz-dot');
        d.setAttribute('aria-hidden', 'true');
        dots.appendChild(d);
      });
    }
    var prev = $('#wiz-prev');
    var next = $('#wiz-next');
    var copy = $('#wiz-copy');
    if (prev) {
      prev.addEventListener('click', function () {
        if (wizStep > 0) {
          wizStep--;
          renderWizard();
        }
      });
    }
    if (next) {
      next.addEventListener('click', function () {
        if (wizStep < WIZARD.length - 1) {
          wizStep++;
          renderWizard();
        } else {
          pushToast('Wizard complete - stay on authorized targets only');
        }
      });
    }
    if (copy) {
      copy.addEventListener('click', function () {
        copyText(WIZARD[wizStep].cmd, copy);
      });
    }
    renderWizard();
  }

  /* ===================== Roadmap ===================== */
  var ROADMAP = [
    {
      ver: 'v3.8.0',
      status: 'shipped',
      title: 'USB-stable field unit',
      items: ['USB-first boot + CDC GrokRPC', 'CC1101 light RX', 'ST7567 GUI', 'multi-LLM observe'],
    },
    {
      ver: 'v3.8.0',
      status: 'shipped',
      title: 'Storage, skills, persistent autonomy',
      items: [
        'Crash-safe storage layout',
        'Hot-load skills + GLKSIG1 hook',
        'Durable hash-chained vault',
        'Agent resume + STORAGE page',
      ],
    },
    {
      ver: 'v3.9',
      status: 'planned',
      title: 'Sketch (roadmap)',
      items: [
        'littlefs on SDMMC',
        'BLE status channel (M0+ / IPCC)',
        'Ed25519 skill verification',
        'Minimal Desktop (DFU + serial + observe)',
        'USB soak automation + CI host tests',
      ],
    },
  ];

  function initRoadmap() {
    var root = $('#roadmap-track');
    if (!root) return;
    ROADMAP.forEach(function (r) {
      var card = el('article', 'road-card road-' + r.status);
      card.innerHTML =
        '<div class="road-ver">' +
        r.ver +
        '</div><div class="road-status">' +
        r.status +
        '</div><h3>' +
        r.title +
        '</h3>';
      var ul = el('ul');
      r.items.forEach(function (it) {
        ul.appendChild(el('li', null, it));
      });
      card.appendChild(ul);
      root.appendChild(card);
    });
  }

  /* ===================== Docs filter ===================== */
  function initDocs() {
    var input = $('#docs-filter');
    if (!input) return;
    input.addEventListener('input', function () {
      var q = input.value.toLowerCase().trim();
      $$('#docs-list details').forEach(function (d) {
        var text = d.textContent.toLowerCase();
        d.hidden = q && text.indexOf(q) === -1;
      });
    });
  }

  /* ===================== Copy buttons ===================== */
  function initCopyButtons() {
    $$('[data-copy]').forEach(function (btn) {
      btn.addEventListener('click', function () {
        var sel = btn.getAttribute('data-copy');
        var node = sel ? $(sel) : null;
        var text = node ? node.textContent : btn.getAttribute('data-copy-text') || '';
        copyText(text, btn);
      });
    });
  }

  /* ===================== Sticky safety dismiss (session only) ===================== */
  function initSticky() {
    var bar = $('#safety-sticky');
    var btn = $('#sticky-ack');
    if (!bar || !btn) return;
    btn.addEventListener('click', function () {
      bar.classList.add('acked');
      btn.textContent = 'Acknowledged this session';
      btn.disabled = true;
      appendVault('web-operator', 'edu_phrase_seen', EDU);
    });
  }

  /* ===================== Nav active section ===================== */
  function initScrollSpy() {
    var links = $$('.site-nav a[href^="#"]');
    if (!links.length || !('IntersectionObserver' in window)) return;
    var map = {};
    links.forEach(function (a) {
      var id = a.getAttribute('href').slice(1);
      var sec = document.getElementById(id);
      if (sec) map[id] = a;
    });
    var io = new IntersectionObserver(
      function (entries) {
        entries.forEach(function (en) {
          if (!en.isIntersecting) return;
          var id = en.target.id;
          links.forEach(function (a) {
            a.classList.toggle('active', a.getAttribute('href') === '#' + id);
          });
        });
      },
      { rootMargin: '-30% 0px -55% 0px', threshold: 0.01 }
    );
    Object.keys(map).forEach(function (id) {
      var sec = document.getElementById(id);
      if (sec) io.observe(sec);
    });
  }

  /* ===================== Mobile nav ===================== */
  function initMobileNav() {
    var toggle = $('#nav-toggle');
    var nav = $('#site-nav-links');
    if (!toggle || !nav) return;
    toggle.addEventListener('click', function () {
      var open = nav.classList.toggle('open');
      toggle.setAttribute('aria-expanded', open ? 'true' : 'false');
    });
  }

  document.addEventListener('DOMContentLoaded', function () {
    initSticky();
    initArchitecture();
    initSafety();
    initSkills();
    initWizard();
    initRoadmap();
    initDocs();
    initCopyButtons();
    initScrollSpy();
    initMobileNav();
  });
})();
