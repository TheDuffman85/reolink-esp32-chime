#!/usr/bin/env node
// Capture the current embedded HTML with isolated synthetic API fixtures.
// All requests are intercepted; no board, doorbell or external service is used.
const { chromium } = require('playwright');
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const root = path.resolve(__dirname, '..');
const source = fs.readFileSync(path.join(root, 'include/WebUi.h'), 'utf8');
const embeddedUiHash = crypto.createHash('sha256').update(source).digest('hex');
const firmwareVersion = JSON.parse(fs.readFileSync(path.join(root, 'version.json'), 'utf8')).version;
const nextPatchVersion = firmwareVersion.replace(/\d+$/, patch => String(Number(patch) + 1));
const match = source.match(/WebUi\[\] PROGMEM = R"HTML\(([\s\S]*?)\)HTML";/);
const login = source.match(/LoginUi\[\] PROGMEM = R"HTML\(([\s\S]*?)\)HTML";/);
if (!match || !login) throw Error('Embedded WebUi/LoginUi HTML not found');
const images = path.join(root, 'docs/images');
fs.mkdirSync(images, { recursive: true });

// Demonstration identity is signed-positive, like newly paired firmware nodes.
const nodeId = '21B2C3D4', doorbellId = '11223344';
function packetHex(header, plain) {
  const cipher = crypto.createCipheriv('aes-128-ctr', Buffer.alloc(16), Buffer.alloc(16));
  const mac = Buffer.concat([Buffer.from(header, 'hex'), cipher.update(Buffer.from(plain)), cipher.final()]);
  const body = Buffer.concat([Buffer.from([mac.length]), mac]);
  let crc = 0xFFFF;
  for (const byte of body) {
    crc ^= byte << 8;
    for (let bit = 0; bit < 8; ++bit) crc = ((crc << 1) ^ ((crc & 0x8000) ? 0x8005 : 0)) & 0xFFFF;
  }
  return Buffer.concat([body, Buffer.from([crc >> 8, crc & 255])]).toString('hex').toUpperCase();
}
const ringHex = packetHex('14033412D4C3B2214433221104030201', [9, 0, 0, 0]);
const joinHex = packetHex('18020000D4C3B22144332211', [3, 6, 0x34, 0x12, 0, 0, 0, 0, 0x30, 0x30, 0x41, 0x32]);
const status = {
  csrf: 'documentation-demo-token', uptime_ms: 60000,
  wifi_connected: true, wifi_configured: true, wifi_rssi: -53,
  ssid: 'Demo-WiFi', ip: '192.168.1.77', hostname: 'reolink-chime.local',
  ap_active: false, ap_name: 'Reolink-Chime-DEMO', wifi_mode: 'client',
  radio_ok: true, radio_error: '', radio_part: 0, radio_version: 20, marcstate: 13,
  rf_frequency_hz: 868000000, rf_trim: 0, rssi_dbm: -99,
  tx_polarity: 'normal (locked)', sweep_step: null, sweep_steps: 22,
  pairing: 'joined', profile: true, paired: true, joined: true, setup_visible: false, setup_required: false,
  node_id: nodeId, identity_compatible: true, doorbell_id: doorbellId,
  chime_name: 'Demo ESP32 Chime', transmitted: 8, transmit_errors: 0,
  packets: 4, crc_ti: 4, crc_bad: 0, crc_ccitt: 0, dropped: 0, invalid_lengths: 0,
  rings: 1, last_ring_ms: 52000, gong_active: false, gong_ready: true,
  storage_ok: true, learned_polarity: 1, free_heap: 172032,
  firmware: 'Synthetic documentation build', version: firmwareVersion, radio_resets: 0,
  ring_seq: 5, auth_enabled: true, auth_user: 'admin', mute: { active: false, permanent: false, remaining_s: 0 }, time_synced: true, epoch: 1791900000, mqtt: 'connected',
};
// Ring log fixture: newest first, epoch-stamped like a synchronised clock.
const ring = (id, ago, source, outcome, extra = {}) => ({ id, source, outcome, type: 1, sound: 0,
  ms: 60000 - ago * 1000, epoch: status.epoch - ago, previous_boot: false, volume: 4, audio_level: 35,
  preview: false, ...(source === 'doorbell' ? { tone: 0, rssi_dbm: -62 } : {}), ...extra });
const rings = { uptime_ms: 60000, epoch: status.epoch, last_id: 5, rings_since_start: 1, rings: [
  ring(5, 8, 'doorbell', 'played', { tone: 9, sound: 9 }),
  ring(4, 25, 'mqtt', 'played', { sound: 1, volume: 3 }),
  ring(3, 40, 'web', 'played', { sound: 2, preview: true }),
  ring(2, 3600 * 5, 'doorbell', 'chime_muted', { previous_boot: true, tone: 3, sound: 3, rssi_dbm: -70 }),
  ring(1, 86400 + 120, 'doorbell', 'played', { previous_boot: true, rssi_dbm: -65 }),
] };
const authInfo = { enabled: true, username: 'admin', password_set: true };
const integrations = { enabled: true, host: 'homeassistant.local', port: 1883, username: 'chime',
  password_set: true, topic: '', default_topic: 'reolink_chime/b2c3d4', discovery: true,
  prefix: 'homeassistant', state: 'connected', dropped: 0, ntp: 'pool.ntp.org',
  time_synced: true, epoch: status.epoch };
const settings = {
  melody: 0, volume: 35, relay: false, relay_ms: 300,
  frequency_hz: 868000000, trim: 0, polarity: 0, rx_mode: 0, ssid: 'Demo-WiFi',
  wifi_password_set: true, profile: true, paired: true, node_id: nodeId,
  source: doorbellId, destination: nodeId, pair: '1234',
};
const chime = { name: status.chime_name, volume: 4, led: 1, joined: true,
  doorbell_id: doorbellId, node_id: nodeId, session: '1234', requests: 3,
  last_opcode: 0x15, last_request_ms: 50000, uptime_ms: 60000 };
const gong = { type: 1, audio_output: 0, pattern: 0, pulse_ms: 300, gap_ms: 700,
  relay_available: true, pwm_available: true, dac_available: true };
const pinMap = { sck: 18, miso: 19, mosi: 23, cs: 5, clock: 4, data: 16,
  audio: 25, relay: 26, led: 2, button: 0, relay_active_low: false };
const issues = [
  { role: 'cs', level: 'info', text: 'Boot strap pin: an optional 10 kOhm pull-up to 3.3 V can keep CS high at reset.' },
  { role: 'data', level: 'info', text: 'On WROVER modules GPIO16/17 belong to PSRAM; fine on WROOM-32.' },
];
const packets = [
  { id: 1, ms: 12000, epoch: status.epoch - 48, epoch_ms: 0, rssi_dbm: -61, inverted: false, crc: 'TI-8005',
    result: 'pairing response accepted', hex: '[pairing frame redacted]' },
  { id: 2, ms: 12100, epoch: status.epoch - 48, epoch_ms: 100, rssi_dbm: -61, inverted: false, crc: 'TI-8005',
    result: 'join response accepted', hex: joinHex },
  { id: 3, ms: 52000, epoch: status.epoch - 8, epoch_ms: 0, rssi_dbm: -62, inverted: false, crc: 'TI-8005',
    result: 'ring parameter 0', hex: ringHex },
  { id: 4, ms: 52100, epoch: status.epoch - 8, epoch_ms: 100, rssi_dbm: -63, inverted: false, crc: 'TI-8005',
    result: 'duplicate ring', hex: ringHex },
];
const log = '    12.100 [link] joined\n    50.000 [app] config: name=\'Demo ESP32 Chime\' volume=4 led=1\n    52.000 [rx] #3 -62.0 dBm normal crc=TI-8005: ring parameter 0\n    52.100 [rx] #4 -63.0 dBm normal crc=TI-8005: duplicate ring\n    55.000 [diag] radio=ok pkts=4 ti=4 bad=0 drop=0\n';

(async () => {
  const browser = await chromium.launch({ headless: true,
    ...(process.env.SCREENSHOT_BROWSER_EXE ? { executablePath: process.env.SCREENSHOT_BROWSER_EXE } : {}),
    args: ['--no-sandbox'],
  });
  const results = [], errors = [], unexpected = [], previews = [], setupSaves = [], authSaves = [];
  const setupVisibilitySaves = [], wifiConnections = [], pairActions = [];
  const savedPinMap = { ...pinMap }, defaultPinMap = { ...pinMap };
  let failNextAuthSave=false, expectedAuthErrors=0, failNextSetupSave=false, expectedSetupErrors=0;
  let statusGate=null;
  const assert = require('node:assert/strict');
  try {
    const page = await browser.newPage({ viewport: { width: 1100, height: 1000 },
      deviceScaleFactor: 1, colorScheme: 'dark' });
    page.on('pageerror', error => errors.push(error.message));
    page.on('console', msg => { if (msg.type() !== 'error') return;
      if (expectedAuthErrors && msg.location().url.endsWith('/api/auth') && /status of 500/.test(msg.text())) { --expectedAuthErrors; return; }
      if (expectedSetupErrors && msg.location().url.endsWith('/api/setup') && /status of 500/.test(msg.text())) { --expectedSetupErrors; return; }
      errors.push(msg.text());
    });
    await page.route('**/*', async route => {
      const request = route.request(), url = new URL(request.url());
      if (url.origin !== 'http://reolink-docs.test') {
        unexpected.push(request.url()); return route.abort();
      }
      if (url.pathname === '/' && request.method() === 'GET')
        return route.fulfill({ contentType: 'text/html', body: match[1] });
      if (url.pathname === '/login' && request.method() === 'GET')
        return route.fulfill({ contentType: 'text/html', body: login[1] });
      if (url.pathname === '/api/log' && request.method() === 'GET') {
        const from = Number(url.searchParams.get('from') || 0);
        return route.fulfill({ contentType: 'text/plain', body: log.slice(from),
          headers: { 'X-Log-Next': String(log.length) } });
      }
      if (url.pathname === '/api/gong/test' && request.method() === 'POST') {
        previews.push(request.postDataJSON());
        return route.fulfill({ json: { message: 'Playing preview' } });
      }
      // The UI validates its GPIO map on load. This POST is a mocked check only.
      if (url.pathname === '/api/pins/check' && request.method() === 'POST') {
        const map = request.postDataJSON();
        const invalid = gong.type === 0 && gong.audio_output === 1 && map.audio >= 0 && ![25, 26].includes(map.audio);
        return route.fulfill({ json: { ok: !invalid, issues: invalid ? [...issues, { role: 'audio', level: 'error', text: 'DAC audio requires GPIO25 or GPIO26' }] : issues } });
      }
      if (url.pathname === '/api/setup' && request.method() === 'POST') {
        const body=request.postDataJSON();
        assert.deepEqual(Object.keys(body),['visible']);assert.equal(typeof body.visible,'boolean');
        if(failNextSetupSave){failNextSetupSave=false;++expectedSetupErrors;return route.fulfill({status:500,json:{error:'Synthetic setup storage failure'}})}
        setupVisibilitySaves.push(body.visible);status.setup_visible=body.visible;
        if(!body.visible)status.setup_required=false;
        return route.fulfill({json:{visible:status.setup_visible}});
      }
      if (url.pathname === '/api/wifi/connect' && request.method() === 'POST') {
        const body=request.postDataJSON();wifiConnections.push(body);
        status.ssid=body.ssid;status.wifi_connected=true;status.wifi_configured=true;
        return route.fulfill({json:{message:'Synthetic WiFi connected'}});
      }
      if (['/api/pair/start','/api/pair/cancel'].includes(url.pathname) && request.method() === 'POST') {
        pairActions.push(url.pathname);status.joined=false;chime.joined=false;
        status.pairing=url.pathname.endsWith('/start')?'discovering':'idle';
        return route.fulfill({json:{message:'Synthetic pairing action'}});
      }
      if (url.pathname === '/api/auth' && request.method() === 'POST') {
        const body=request.postDataJSON();
        if(failNextAuthSave){failNextAuthSave=false;++expectedAuthErrors;return route.fulfill({status:500,json:{error:'Synthetic storage failure'}})}
        if (body.enabled && !authInfo.password_set && !body.password) return route.fulfill({ status:400,json:{error:'Choose a password to turn sign-in on'} });
        authSaves.push({enabled:body.enabled,username:body.username,password_changed:!!body.password});
        if ('enabled' in body) authInfo.enabled=body.enabled;
        if ('username' in body) authInfo.username=body.username;
        if (body.password) authInfo.password_set=true;
        status.auth_enabled=authInfo.enabled;status.auth_user=authInfo.username;
        return route.fulfill({json:{message:'Synthetic sign-in settings saved'}});
      }
      if (['/api/setup/sound/check', '/api/setup/sound'].includes(url.pathname) && request.method() === 'POST') {
        const body = request.postDataJSON(), proposed = { ...savedPinMap, ...body.pins }, config = { ...gong, ...body.chime };
        const problems = [];
        const pinsOnly = Object.entries(proposed).filter(([key, value]) => key !== 'relay_active_low' && value >= 0);
        for (const [role, pin] of pinsOnly)
          if (pinsOnly.some(([other, otherPin]) => role !== other && pin === otherPin)) problems.push({ role, level: 'error', text: 'Already used by another function.' });
        if (config.type === 1 && proposed.relay < 0) problems.push({ role: 'relay', level: 'error', text: 'Assign a relay GPIO before choosing Gong.' });
        if (config.type === 0 && (proposed.audio < 0 || config.audio_output === 1 && ![25, 26].includes(proposed.audio))) problems.push({ role: 'audio', level: 'error', text: 'DAC audio requires GPIO25 or GPIO26.' });
        const restart = Object.keys(proposed).some(key => proposed[key] !== pinMap[key]);
        if (url.pathname.endsWith('/check')) return route.fulfill({ json: { ok: !problems.length, issues: problems, pins_changed: restart, restart_required: false } });
        if (problems.length) return route.fulfill({ status: 400, json: { error: 'Fix the wiring errors first' } });
        setupSaves.push(body); Object.assign(savedPinMap, proposed); Object.assign(pinMap, proposed);
        for (const key of ['type', 'audio_output', 'pattern']) if (key in body.chime) gong[key] = body.chime[key];
        if ('name' in body.chime) chime.name = body.chime.name;
        if ('melody' in body.chime) settings.melody = body.chime.melody;
        if ('extra_relay' in body.chime) settings.relay = body.chime.extra_relay;
        return route.fulfill({ json: { ok: true, issues: [], pins_changed: restart, restart_required: false, message: 'Synthetic setup saved' } });
      }
      if (url.pathname === '/api/pins' && request.method() === 'POST') {
        const proposed = { ...savedPinMap, ...request.postDataJSON() };
        Object.assign(savedPinMap, proposed); Object.assign(pinMap, proposed);
        return route.fulfill({ json: { ok: true, message: 'GPIO settings saved and applied.' } });
      }
      const savedFixtures = { '/api/gong': gong, '/api/settings': settings, '/api/chime': chime };
      if (request.method() === 'POST' && savedFixtures[url.pathname]) {
        Object.assign(savedFixtures[url.pathname], request.postDataJSON());
        return route.fulfill({ json: { message: 'Synthetic settings saved' } });
      }
      const fixtures = {
        '/api/status': status, '/api/settings': settings, '/api/chime': chime,
        '/api/gong': gong, '/api/pins': { pins: pinMap, saved: savedPinMap, defaults: defaultPinMap },
        '/api/packets': { packets }, '/api/rings': rings, '/api/auth': authInfo,
        '/api/integrations': integrations,
        '/api/wifi': { mode: 'client', active_mode: 'client', ap_ssid: '', ap_password_set: false,
          default_ap_ssid: 'Reolink-Chime-DEMO', button: true },
        '/api/wifi/scan': { scanning: false, networks: [
          { ssid: 'Demo-WiFi', rssi: -53, secure: true },
          { ssid: 'Demo-Guest', rssi: -72, secure: true },
        ] },
      };
      if(url.pathname==='/api/status'&&statusGate)await statusGate;
      if (request.method() === 'GET' && fixtures[url.pathname])
        return route.fulfill({ json: fixtures[url.pathname] });
      unexpected.push(request.method() + ' ' + url.pathname); return route.abort();
    });
    await page.goto('http://reolink-docs.test/');
    await page.waitForFunction(() => document.getElementById('heroTitle').textContent === 'Ready' &&
      document.querySelectorAll('#pinRows tr').length === 10 &&
      document.getElementById('pinState').textContent === 'Valid' &&
      document.querySelectorAll('#rings li').length === 5);
    // Stabilise the documentation fixture; normal firmware polling is unchanged.
    const stabilise = async () => page.evaluate(() => {
      for (let id = 1; id < 100; ++id) clearInterval(id);
    });
    await stabilise();
    // Setup exists only during initial setup, so the fixture re-enters that state.
    const enableSetup = async () => {
      if (await page.evaluate(()=>initialSetup())) return;
      const step=await page.evaluate(()=>wizStep);
      status.setup_visible=true;status.setup_required=true;
      await page.evaluate(async s=>{await refresh();wizStep=s;show('setup')},step);
      await page.waitForFunction(()=>activeTab==='setup');
    };
    const leaveSetup = async () => {
      if (!await page.evaluate(()=>initialSetup())) return;
      status.setup_visible=false;status.setup_required=false;
      await page.evaluate(async()=>{await refresh()});
    };
    // Finish closes Setup; reopen it on the sound step to change choices again.
    const reopenSetup = async () => {
      assert.equal(await page.evaluate(()=>activeTab),'home');
      await enableSetup();
      await page.evaluate(async()=>{await loadSettings();await refresh();wizStep=2;show('setup');});
    };
    const select = async tab => {
      if(tab==='setup')await enableSetup();
      else{await leaveSetup();await page.locator(`#nav [data-tab="${tab}"]`).click();}
      await page.locator(`#tab-${tab}`).waitFor({ state: 'visible' });
    };
    const capture = async (name, tab, width = 1100, height = 1000, section, fullPage) => {
      await page.setViewportSize({ width, height });
      if (await page.evaluate(() => activeTab) !== tab) await select(tab);
      if (tab === 'setup' && await page.evaluate(() => wizStep) === 2) await page.waitForFunction(() => !wizWireBusy);
      if (section) await page.evaluate(s => openSection(s), section);
      if (section === 'debug') await page.waitForFunction(() => document.getElementById('packets').children.length === 4);
      if (tab === 'setup' && await page.evaluate(() => wizStep) === 0) await page.waitForFunction(() => document.querySelectorAll('#networks button[data-ssid]').length === 2);
      await page.evaluate(() => { scrollTo(0, 0); clearTimeout($("toast").t); $("toast").className = "toast"; });
      // Complete phone captures use the page height as their viewport so the
      // fixed navigation lands at the bottom instead of covering a control.
      if (width < 860 && fullPage === true) {
        height = Math.max(height, await page.evaluate(() => document.documentElement.scrollHeight));
        await page.setViewportSize({ width, height });
      }
      if (await page.evaluate(() => document.documentElement.scrollWidth > innerWidth)) throw Error(`${name} has horizontal overflow`);
      // A phone capture uses the viewport so its fixed bottom navigation stays
      // at the bottom of the image instead of overlaying a full-page capture.
      await page.screenshot({ path: path.join(images, name), fullPage: fullPage === undefined ? width >= 860 : fullPage, animations: 'disabled' });
      results.push({ file: name, tab, viewport: { width, height } });
    };
    // Configured devices hide Setup, while every choice remains available.
    assert.equal(await page.locator('#nav').isVisible(),true);
    assert.equal(await page.evaluate(()=>activeTab),'home');
    await capture('ui-setup-hidden-desktop.png','home',1100,700);
    await capture('ui-setup-hidden-mobile.png','home',390,900);
    await capture('ui-settings-setup-hidden-desktop.png','settings',1100,900,'general',true);
    await capture('ui-settings-setup-hidden-mobile.png','settings',390,900,'general',true);
    await page.evaluate(()=>openSection('network'));
    assert.deepEqual(await page.locator('#sec-network > .card h3').allTextContents(), ['WiFi mode','WiFi connection','Clock']);
    await page.locator('#cfgWifiChange').click();
    // Leaving Client mode hides its controls and closes the connection editor.
    for (const mode of ['ap','off']) {
      await page.locator(`#wmSeg [data-m="${mode}"]`).click();
      assert.equal(await page.locator('#cfgWifiCard').isVisible(),false);
      assert.equal(await page.locator('#cfgWifiTask').isVisible(),false);
      assert.equal(await page.evaluate(()=>settingsStep),-1);
      assert.equal(await page.locator('#wmAp').isVisible(),mode==='ap');
      await capture(`ui-settings-wifi-${mode}-desktop.png`,'settings',1100,1100,'network',true);
      await capture(`ui-settings-wifi-${mode}-mobile.png`,'settings',390,900,'network',true);
    }
    await page.locator('#wmSeg [data-m="client"]').click();
    assert.equal(await page.locator('#cfgWifiCard').isVisible(),true);
    // Selecting a mode alone must neither save nor restart.
    assert.equal(wifiConnections.length,0);
    await page.evaluate(()=>{wmDirty=false;fillWifiMode()});
    await page.locator('#cfgWifiChange').click();
    assert.equal(await page.evaluate(()=>activeTab),'settings');
    assert.equal(await page.locator('#nav').isVisible(),true);
    assert.equal(await page.locator('#wiz0 .stepper-title').isVisible(),false);
    await page.waitForFunction(()=>document.querySelectorAll('#networks button[data-ssid]').length===2);
    await capture('ui-settings-wifi-desktop.png','settings',1100,1100,'network',true);
    await capture('ui-settings-wifi-mobile.png','settings',390,900,'network',true);
    await page.locator('#wSsid').fill('Demo-WiFi');
    await page.locator('#wPass').fill('example-password');
    await page.locator('#wConnect').click();
    await page.waitForFunction(()=>document.getElementById('wifiProgressText').textContent.startsWith('Connected.'));
    assert.equal(wifiConnections.length,1);
    assert.equal(await page.evaluate(()=>activeTab),'settings');
    await page.locator('#wNext').click();
    assert.equal(await page.locator('#cfgWifiTask').isVisible(),false);
    await page.evaluate(()=>openSection('doorbell'));
    await page.locator('#cfgRepair').click();
    assert.equal(await page.evaluate(()=>activeTab),'settings');
    assert.equal(await page.locator('#pBack').isVisible(),false);
    await capture('ui-settings-pair-desktop.png','settings',1100,1100,'doorbell',true);
    await capture('ui-settings-pair-mobile.png','settings',390,900,'doorbell',true);
    await page.locator('#pStart').click();
    await page.locator('#pCancel').waitFor();
    assert.equal(await page.locator('#nav').isVisible(),true);
    await page.locator('#pCancel').click();
    await page.locator('#pStart').waitFor();
    assert.deepEqual(pairActions,['/api/pair/start','/api/pair/cancel']);
    await page.locator('#pNext').click();
    assert.equal(await page.locator('#cfgPairTask').isVisible(),false);
    status.joined=true;status.pairing='joined';chime.joined=true;
    await page.evaluate(async()=>{await refresh()});
    // Configured devices offer no way back into Setup.
    assert.deepEqual(await page.locator('#nav button:visible').evaluateAll(buttons=>buttons.map(b=>b.dataset.tab)),['home','sound','settings']);
    assert.equal((await page.locator('#sec-general').textContent()).includes('Setup'),false);
    await capture('web-status.png', 'home', 1100, 700);
    await select('sound');
    assert.deepEqual(await page.locator('#gongTypes .choice b').allTextContents(), ['Gong', 'Audio', 'None']);
    assert.equal(await page.locator('#gongTypes .choice.on b').textContent(), 'Gong');
    assert.equal(await page.locator('#audioCards').isVisible(), false);
    await page.locator('#gongTypes .choice').filter({ has: page.locator('b', { hasText: /^Audio$/ }) }).click();
    assert.equal(await page.locator('#audioCards').isVisible(), true);
    assert.equal(await page.locator('#relayCards').isVisible(), false);
    assert.equal(await page.locator('#melodies .choice').count(), 10);
    assert.equal(await page.locator('#patterns .choice').count(), 10);
    for (const [name, output] of [['PWM', 0], ['DAC', 1]]) {
      await page.locator('#audioOutputs .choice').filter({ has: page.locator('b', { hasText: new RegExp('^' + name + '$') }) }).click();
      await Promise.all([page.waitForResponse(r => r.url().endsWith('/api/gong/test')), page.locator('#melodies button[data-play="1"]').click()]);
      assert.equal(previews.at(-1).type, 0);
      assert.equal(previews.at(-1).audio_output, output);
      assert.equal(previews.at(-1).melody, 1);
      assert.equal(previews.at(-1).extra_relay, false);
      assert.equal(previews.at(-1).extra_relay_ms, settings.relay_ms);
      await Promise.all([page.waitForResponse(r => r.url().endsWith('/api/gong/test')), page.locator('#melodies button[data-play="9"]').click()]);
      assert.equal(previews.at(-1).melody, 9);
      assert.equal(previews.at(-1).audio_output, output);
    }
    // Switching between audio and gong keeps the numbered doorbell slot.
    await page.locator('#melodies .choice[data-i="9"]').click();
    await page.locator('#gongTypes .choice').filter({ has: page.locator('b', { hasText: /^Gong$/ }) }).click();
    assert.equal(await page.evaluate(() => patternPick), 9);
    await Promise.all([page.waitForResponse(r => r.url().endsWith('/api/gong/test')), page.locator('#patterns button[data-play="9"]').click()]);
    assert.equal(previews.at(-1).pattern, 9);
    await page.locator('#gongTypes .choice').filter({ has: page.locator('b', { hasText: /^Audio$/ }) }).click();
    assert.equal(await page.evaluate(() => soundPick), 9);
    await page.locator('#melodies .choice[data-i="0"]').click();
    // Audio previews use the unsaved relay choice and pulse length.
    await page.locator('#relay').check();await page.locator('#relayMs').fill('450');
    await Promise.all([page.waitForResponse(r=>r.url().endsWith('/api/gong/test')),page.locator('#melodies button[data-play="0"]').click()]);
    assert.equal(previews.at(-1).extra_relay,true);assert.equal(previews.at(-1).extra_relay_ms,450);
    await page.locator('#relayMs').fill('300');await page.locator('#relay').uncheck();
    // Saving and reloading retains DAC, even when switching through Gong/None.
    await page.locator('#soundSave').click();
    await page.waitForFunction(() => !dirty);
    assert.equal(gong.type, 0); assert.equal(gong.audio_output, 1);
    for (const mode of ['Gong', 'None', 'Audio']) {
      await page.locator('#gongTypes .choice').filter({ has: page.locator('b', { hasText: new RegExp('^' + mode + '$') }) }).click();
      await page.locator('#soundSave').click();
      await page.waitForFunction(() => !dirty);
      assert.equal(gong.audio_output, 1);
      assert.equal(await page.locator('#audioCards').isVisible(), mode === 'Audio');
    }
    assert.equal(await page.locator('#audioOutputs .choice.on b').textContent(), 'DAC');
    // Invalid DAC wiring keeps PWM selected, so the owner can still use Audio.
    gong.dac_available = false; gong.audio_output = 0;
    await page.evaluate(async () => { await loadSettings(); });
    await page.locator('#audioOutputs .choice').filter({ hasText: 'DAC' }).click();
    assert.equal(await page.locator('#audioOutputs .choice.on b').textContent(), 'PWM');
    assert.equal(await page.locator('#gongTypes .choice.on b').textContent(), 'Audio');
    gong.dac_available = true;
    gong.type = 0; gong.audio_output = 0;
    await page.evaluate(async () => { await loadSettings(); render(); });
    await capture('web-sound.png', 'sound');
    gong.type = 0; gong.audio_output = 1;
    await page.evaluate(async () => { await loadSettings(); render(); });
    await capture('web-sound-dac.png', 'sound');
    gong.type = 1; gong.pattern = 1;
    await page.evaluate(async () => { await loadSettings(); render(); });
    await capture('web-sound-relay.png', 'sound');
    gong.type = 1; gong.pattern = 0;
    await page.evaluate(async () => { await loadSettings(); render(); });
    await page.evaluate(()=>{wizStep=0});
    await capture('web-setup-wifi.png', 'setup', 1100, 800);
    status.joined = false; status.pairing = 'idle'; chime.joined = false;
    await page.evaluate(async () => { await refresh(); });
    await page.locator('#wNext').click();
    await capture('web-setup-pairing.png', 'setup', 1100, 800);
    await page.locator('#pNext').click();
    await capture('web-setup-sound.png', 'setup', 1100, 800);
    assert.deepEqual(await page.locator('#wizTypes .choice b').allTextContents(), ['Gong', 'Audio', 'None']);
    assert.equal(await page.locator('#wizTypes .choice.on b').textContent(), 'Gong');
    assert.equal(await page.locator('#wizAudioField').isVisible(), false);
    // Polling must preserve Setup drafts separately from the Sound tab.
    await page.locator('#melodies2 .choice[data-i="4"]').click();
    await page.waitForFunction(()=>!wizWireBusy&&wizWireOk);
    await page.evaluate(async()=>{await refresh();await loadSettings();render()});
    assert.equal(await page.evaluate(()=>wizardSoundBody().chime.pattern),4);
    assert.match(await page.locator('#melodies2 .choice.on b').textContent(),/Insistent/);
    // Phones pick the ten patterns from a dropdown with a play button.
    await page.setViewportSize({ width: 390, height: 900 });
    assert.equal(await page.locator('#melodies2 .choice:visible').count(),0);
    assert.equal(await page.locator('#melodies2 select').inputValue(),'4');
    assert.equal(await page.locator('#melodies2 .pickm small').textContent(),'Five quick strikes');
    await page.locator('#melodies2 select').selectOption('1');
    assert.match(await page.locator('#melodies2 .choice.on b').textContent(),/Double/);
    assert.equal(await page.evaluate(()=>wizardSoundBody().chime.pattern),1);
    await page.waitForFunction(()=>!wizWireBusy&&wizWireOk);
    await Promise.all([page.waitForResponse(r=>r.url().endsWith('/api/gong/test')),page.locator('#melodies2 [data-mplay]').click()]);
    assert.equal(previews.at(-1).pattern,1);
    await page.setViewportSize({ width: 1100, height: 1000 });
    await page.locator('#melodies2 .choice[data-i="0"]').click();
    await page.waitForFunction(()=>!wizWireBusy&&wizWireOk);

    await page.locator('#wizTypes .choice').filter({ has: page.locator('b', { hasText: /^Audio$/ }) }).click();
    await page.locator('#wizAudioOutputs .choice').filter({ hasText: 'DAC' }).click();
    await page.waitForFunction(() => wizAudioPick === 1 && !wizWireBusy && wizWireOk);
    await page.locator('#melodies2 .choice[data-i="2"]').click();
    await page.waitForFunction(()=>!wizWireBusy&&wizWireOk);
    await page.evaluate(async()=>{await refresh();await loadSettings();render()});
    assert.equal(await page.evaluate(()=>wizardSoundBody().chime.melody),2);
    assert.match(await page.locator('#melodies2 .choice.on b').textContent(),/Double beep/);
    await page.locator('#melodies2 .choice[data-i="0"]').click();
    await page.waitForFunction(()=>!wizWireBusy&&wizWireOk);

    await Promise.all([page.waitForResponse(r => r.url().endsWith('/api/gong/test')), page.locator('#melodies2 button[data-play="0"]').click()]);
    assert.equal(previews.at(-1).type, 0); assert.equal(previews.at(-1).audio_output, 1);
    assert.equal(previews.at(-1).extra_relay,false);
    const savesBeforeReview = setupSaves.length;
    await page.locator('#sNext').click();
    await page.locator('#wizAuthNext').click();
    await page.locator('#wiz4').waitFor();
    assert.equal(setupSaves.length, savesBeforeReview);
    assert.equal(status.setup_visible,true);
    assert.equal(await page.locator('#doneTitle').textContent(), 'Ready to finish');
    await page.locator('#dHome').click();
    await page.waitForFunction(() => wizSetupSaved);
    assert.equal(setupSaves.length, savesBeforeReview + 1);
    assert.equal(status.setup_visible,false);
    assert.equal(await page.locator('#nav').isVisible(),true);
    assert.equal(gong.type, 0); assert.equal(gong.audio_output, 1);
    assert.match(await page.locator('#doneSummary').textContent(), /Audio · DAC/);
    await reopenSetup();
    await page.waitForFunction(() => !wizWireBusy && wizWireOk);
    assert.deepEqual(await page.locator('#wizAudioPin option').evaluateAll(options => options.map(o => o.value)), ['25', '26']);
    await page.locator('#wizAudioPin').selectOption('26');
    await page.waitForFunction(() => !wizWireBusy && wizWireOk);
    assert.equal(await page.locator('#sNext').textContent(), 'Next');
    assert.equal(await page.locator('#melodies2 button[data-play="0"]').isDisabled(), true);
    const savesBeforeApply = setupSaves.length;
    await page.locator('#sNext').click();
    await page.locator('#wizAuthNext').click();
    assert.equal(setupSaves.length, savesBeforeApply);
    assert.equal(await page.locator('#dHome').textContent(), 'Finish');
    assert.equal(await page.locator('#dTest').isDisabled(), true);
    await capture('web-setup-review.png', 'setup', 1100, 850);
    await page.locator('#dBack').click();
    if (await page.locator('#wiz3').isVisible()) await page.locator('#wizAuthBack').click();
    assert.equal(setupSaves.length, savesBeforeApply);
    await page.waitForFunction(() => !wizWireBusy && wizWireOk);
    await page.locator('#sNext').click();
    await page.locator('#wizAuthNext').click();
    await page.locator('#dHome').click();
    await page.waitForFunction(() => wizSetupSaved);
    assert.equal(setupSaves.length, savesBeforeApply + 1);
    assert.equal(savedPinMap.audio, 26); assert.equal(savedPinMap.relay, -1);
    assert.equal(pinMap.audio, 26); assert.equal(pinMap.relay, -1);
    assert.equal(await page.locator('#waitDlg').evaluate(dialog => dialog.open), false);
    assert.equal(await page.evaluate(()=>activeTab),'home');
    assert.equal(await page.locator('#toast').textContent(),'Setup complete');
    await capture('web-setup-complete.png', 'home', 1100, 850);
    await capture('ui-complete-mobile.png', 'home', 390, 900, undefined, true);
    Object.assign(pinMap, defaultPinMap); Object.assign(savedPinMap, defaultPinMap);
    gong.type = 0; gong.audio_output = 1;
    await page.evaluate(async () => { await loadSettings(); });
    // Capture wiring with DAC selected and check the shared pin choices.
    await select('settings');
    await page.evaluate(() => openSection('advanced'));
    await page.evaluate(async () => { await loadPins(); });
    assert.deepEqual(await page.locator('#pin-audio option').evaluateAll(options => options.map(o => o.value)), ['-1', '25', '26']);
    assert.match(await page.locator('#pinAudioHint').textContent(), /DAC/);
    assert.match(await page.locator('#row-relay').textContent(), /Gong mode/);
    gong.type = 1;
    await page.evaluate(async () => { await loadSettings(); });
    status.joined = true; status.pairing = 'joined'; chime.joined = true;
    await page.evaluate(async () => { await refresh(); });
    for (const [section, name] of [['general', 'web-settings.png'], ['ha', 'web-settings-ha.png'],
                                   ['security', 'web-settings-security.png'], ['advanced', 'web-settings-advanced.png']]) {
      await page.evaluate(s => openSection(s), section);
      await capture(name, 'settings');
    }
    await page.evaluate(() => openSection('general'));
    // Confirmation modal: Esc must cancel without sending any request.
    await page.setViewportSize({ width: 1100, height: 700 });
    await select('settings');
    await page.evaluate(() => openSection('network'));
    await page.locator('#cfgWifiForget').click();
    await page.locator('#dlg[open]').waitFor();
    await page.screenshot({ path: path.join(images, 'web-dialog.png'), animations: 'disabled' });
    results.push({ file: 'web-dialog.png', tab: 'settings', viewport: { width: 1100, height: 700 } });
    await page.keyboard.press('Escape');
    await page.waitForFunction(() => !document.getElementById('dlg').open);
    await page.evaluate(() => openSection('general'));
    await capture('web-maintenance.png', 'settings', 1100, 1000, 'debug');
    await capture('web-mobile.png', 'home', 390, 1000);
    await capture('web-settings-mobile.png', 'settings', 390, 1000);
    status.mute = { active: true, permanent: false, remaining_s: 2700 };
    await page.evaluate(async () => { await refresh(); });
    await capture('web-muted.png', 'home', 1100, 700);
    status.mute = { active: false, permanent: false, remaining_s: 0 };
    await page.evaluate(async () => { await refresh(); setTheme('light'); });
    await capture('web-status-light.png', 'home', 1100, 700);
    await page.evaluate(() => setTheme('system'));
    gong.type = 2;
    await page.evaluate(async () => { await loadSettings(); render(); });
    await capture('web-sound-none.png', 'sound', 1100, 700);
    gong.type = 0;
    await page.evaluate(async () => { await loadSettings(); render(); });
    // Check every tab at phone, tablet and desktop widths, including the full
    // GPIO form. Horizontal scrolling inside the packet table is intentional.
    for (const width of [390, 768, 1100]) {
      await page.setViewportSize({ width, height: 900 });
      for (const tab of ['home', 'sound', 'setup', 'settings']) {
        await select(tab);
        if (await page.evaluate(() => document.documentElement.scrollWidth > innerWidth))
          throw Error(`${tab} page has horizontal overflow at ${width}px`);
      }
      await select('sound');
      for (const [type, output] of [[1, 0], [0, 0], [0, 1], [2, 1]]) {
        gong.type = type; gong.audio_output = output;
        await page.evaluate(async () => { await loadSettings(); });
        if (await page.evaluate(() => document.documentElement.scrollWidth > innerWidth))
          throw Error(`sound type ${type} has horizontal overflow at ${width}px`);
      }
      await select('settings');
      for (const section of ['general', 'network', 'doorbell', 'ha', 'security', 'system', 'advanced', 'debug']) {
        await page.evaluate(s => openSection(s), section);
        if (await page.evaluate(() => document.documentElement.scrollWidth > innerWidth))
          throw Error(`settings ${section} has horizontal overflow at ${width}px`);
      }
    }
    // Optional authentication is staged until the fifth and final step.
    const authBefore={...authInfo},statusAuthBefore={enabled:status.auth_enabled,user:status.auth_user};
    authInfo.enabled=false;authInfo.password_set=false;status.auth_enabled=false;
    await enableSetup();
    await page.evaluate(async()=>{await loadSettings();await refresh();wizStep=2;show('setup');});
    await page.waitForFunction(()=>!wizWireBusy&&wizWireOk);
    const authCount=authSaves.length;
    await page.locator('#sNext').click();
    assert.equal(await page.locator('#steps > div').count(),5);
    assert.equal(await page.locator('#wizAuthModes .choice.on b').textContent(),'No sign-in');
    assert.equal(await page.locator('#wizAuthFields').isVisible(),false);
    await capture('ui-auth-off-desktop.png','setup',1100,850,undefined,true);
    await capture('ui-auth-off-mobile.png','setup',390,900,undefined,true);
    await page.locator('#wizAuthNext').click();
    assert.equal(authSaves.length,authCount);
    assert.match(await page.locator('#doneSummary').textContent(),/Sign-inOff/);
    status.ap_active=true;
    await page.locator('#dHome').click();
    // A still-running setup hotspot offers a restart once Setup has closed.
    await page.locator('#dlg[open]').waitFor();
    assert.equal(await page.locator('#dlgOk').textContent(),'Restart now');
    await page.locator('#dlgCancel').click();
    status.ap_active=false;
    assert.equal(await page.evaluate(()=>wizSetupSaved),true);
    assert.equal(authSaves.length,authCount);assert.equal(status.auth_enabled,false);
    await reopenSetup();
    await page.waitForFunction(()=>!wizWireBusy&&wizWireOk);
    await page.locator('#sNext').click();
    await page.locator('#wizAuthModes .choice').filter({hasText:'Require sign-in'}).click();
    assert.equal(await page.locator('#wizAuthNext').isDisabled(),true);
    await page.locator('#wizAuthUser').fill('bad:user');
    assert.match(await page.locator('#wizAuthError').textContent(),/without a colon/);
    await page.locator('#wizAuthUser').fill('demo-owner');
    await page.locator('#wizAuthPass').fill('example-password');
    await page.locator('#wizAuthConfirm').fill('different-password');
    assert.equal(await page.locator('#wizAuthNext').isDisabled(),true);
    assert.match(await page.locator('#wizAuthError').textContent(),/do not match/);
    await page.locator('#wizAuthConfirm').fill('example-password');
    assert.equal(await page.locator('#wizAuthNext').isDisabled(),false);
    await capture('ui-auth-enabled-desktop.png','setup',1100,850,undefined,true);
    await capture('ui-auth-enabled-mobile.png','setup',390,900,undefined,true);
    await page.locator('#wizAuthNext').click();
    assert.equal(authSaves.length,authCount);
    assert.match(await page.locator('#doneSummary').textContent(),/On · demo-owner/);
    assert.doesNotMatch(await page.locator('#doneSummary').textContent(),/example-password/);
    await capture('ui-auth-review-desktop.png','setup',1100,850,undefined,true);
    await capture('ui-auth-review-mobile.png','setup',390,900,undefined,true);
    failNextAuthSave=true;
    await page.locator('#dHome').click();
    await page.waitForFunction(()=>!document.getElementById('dHome').disabled);
    assert.equal(await page.evaluate(()=>wizSetupSaved),false);
    assert.equal(status.auth_enabled,false);
    assert.match(await page.locator('#toast').textContent(),/setup could not finish/);
    await page.locator('#dHome').click();
    await page.waitForFunction(()=>wizSetupSaved);
    assert.equal(authSaves.length,authCount+1);
    assert.equal(status.auth_enabled,true);assert.equal(status.auth_user,'demo-owner');
    assert.equal(new URL(page.url()).pathname,'/');
    assert.equal(await page.locator('#wizAuthPass').inputValue(),'');
    assert.equal(await page.locator('#wizAuthConfirm').inputValue(),'');
    // Keeping a saved password needs no new password, and Next without changes preserves auth.
    await reopenSetup();
    await page.waitForFunction(()=>!wizWireBusy&&wizWireOk);
    await page.locator('#sNext').click();
    assert.equal(await page.locator('#wizAuthModes .choice.on b').textContent(),'Require sign-in');
    assert.equal(await page.locator('#wizAuthNext').isDisabled(),false);
    assert.match(await page.locator('#wizAuthPassLabel').textContent(),/keep the current/);
    await capture('ui-auth-existing-desktop.png','setup',1100,850,undefined,true);
    await capture('ui-auth-existing-mobile.png','setup',390,900,undefined,true);
    await page.locator('#wizAuthNext').click();
    await page.locator('#dHome').click();
    await page.waitForFunction(()=>wizSetupSaved);
    assert.equal(authSaves.length,authCount+1);
    assert.equal(status.auth_enabled,true);
    // Explicitly choosing off is also saved only on Finish.
    await reopenSetup();
    await page.waitForFunction(()=>!wizWireBusy&&wizWireOk);
    await page.locator('#sNext').click();
    await page.locator('#wizAuthModes .choice').filter({hasText:'No sign-in'}).click();
    await page.locator('#wizAuthNext').click();
    assert.equal(status.auth_enabled,true);
    await page.locator('#dHome').click();
    await page.waitForFunction(()=>wizSetupSaved);
    assert.equal(status.auth_enabled,false);
    assert.equal(authSaves.length,authCount+2);
    Object.assign(authInfo,authBefore);status.auth_enabled=statusAuthBefore.enabled;status.auth_user=statusAuthBefore.user;
    await page.evaluate(async()=>{await loadSettings();await refresh();});
    // All changed UI states, including the complete output wiring on phones.
    for (const [mode, output, slug] of [[1, 0, 'gong'], [0, 0, 'pwm'], [0, 1, 'dac'], [2, 0, 'none']]) {
      gong.type = mode; gong.audio_output = output;
      await leaveSetup();
      await page.evaluate(async () => { await loadSettings(); });
      await capture(`ui-sound-${slug}-desktop.png`, 'sound', 1100, 1000, undefined, true);
      await capture(`ui-sound-${slug}-mobile.png`, 'sound', 390, 900, undefined, true);
      await enableSetup();
      await page.evaluate(() => { wizStep=2; show('setup'); });
      await page.waitForFunction(() => !wizWireBusy && wizWireOk);
      await capture(`ui-setup-${slug}-desktop.png`, 'setup', 1100, 1000, undefined, true);
      await capture(`ui-setup-${slug}-mobile.png`, 'setup', 390, 900, undefined, true);
      if (mode === 0 && output === 0) {
        await page.locator('#wizAudioPin').selectOption('18');
        await page.waitForFunction(() => !wizWireBusy);
        assert.equal(await page.locator('#sNext').isDisabled(), true);
        assert.match(await page.locator('#wizWireIssues').textContent(), /Already used/);
        await page.locator('#wizAudioPin').selectOption('25');
        await page.waitForFunction(() => !wizWireBusy && wizWireOk);
      }
      await page.locator('#sNext').click();
    await page.locator('#wizAuthNext').click();
      await capture(`ui-review-${slug}-desktop.png`, 'setup', 1100, 850, undefined, true);
      await capture(`ui-review-${slug}-mobile.png`, 'setup', 390, 900, undefined, true);
    }
    gong.type=0; gong.audio_output=1;
    await page.evaluate(async () => { await loadSettings(); });
    await capture('ui-wiring-desktop.png', 'settings', 1100, 1000, 'advanced', true);
    await capture('ui-wiring-mobile.png', 'settings', 390, 900, 'advanced', true);
    // Advanced wiring uses the same live map path, with no reboot dialog.
    gong.audio_output=0;
    await page.evaluate(async () => { await loadSettings(); });
    await page.locator('#pin-audio').selectOption('27');
    await page.waitForFunction(() => !document.getElementById('pinSave').disabled);
    await page.locator('#pinSave').click();
    await page.waitForFunction(() => P.pins.audio === 27 && P.saved.audio === 27);
    assert.equal(pinMap.audio, 27);
    assert.equal(await page.locator('#waitDlg').evaluate(dialog => dialog.open), false);
    assert.equal(await page.locator('#dlg').evaluate(dialog => dialog.open), false);
    Object.assign(pinMap, defaultPinMap); Object.assign(savedPinMap, defaultPinMap);
    // A DAC method remembered in Gong may have a PWM-only saved pin.
    gong.type=1;gong.audio_output=1;pinMap.audio=27;savedPinMap.audio=27;
    await enableSetup();await page.evaluate(async()=>{await loadSettings();wizStep=2;show('setup')});
    await page.waitForFunction(()=>!wizWireBusy&&wizWireOk);
    await page.locator('#wizTypes .choice').filter({has:page.locator('b',{hasText:/^Audio$/})}).click();
    await page.waitForFunction(()=>wizType===0&&wizAudioPin===25&&!wizWireBusy&&wizWireOk);
    assert.equal(await page.locator('#wizAudioPin').inputValue(),'25');
    assert.equal(await page.evaluate(()=>wizardSoundBody().pins.audio),25);
    Object.assign(pinMap,defaultPinMap);Object.assign(savedPinMap,defaultPinMap);
    settings.relay=true; gong.type=0; gong.audio_output=0;
    await enableSetup();
    await page.evaluate(async () => { await loadSettings(); wizStep=2; show('setup'); });
    await page.waitForFunction(() => !wizWireBusy && wizWireOk);
    assert.equal(await page.locator('#wizRelayPinField').isVisible(), true);
    await capture('ui-setup-extra-relay-desktop.png', 'setup', 1100, 1000, undefined, true);
    await capture('ui-setup-extra-relay-mobile.png', 'setup', 390, 900, undefined, true);
    await page.locator('#wizExtraRelay').uncheck();
    await page.waitForFunction(()=>!wizWireBusy&&wizWireOk);
    assert.equal(await page.locator('#melodies2 select').inputValue(),'0');
    await Promise.all([page.waitForResponse(r=>r.url().endsWith('/api/gong/test')),page.locator('#melodies2 [data-mplay]').click()]);
    assert.equal(settings.relay,true);assert.equal(previews.at(-1).extra_relay,false);

    settings.relay=false;
    // A stale saved Setup tab opens Home once initial setup is complete.
    await leaveSetup();
    await page.evaluate(()=>localStorage.setItem('tab','setup'));
    await page.reload();
    await page.waitForFunction(()=>S&&P&&activeTab==='home');await stabilise();
    assert.equal(await page.locator('#nav').isVisible(),true);
    // Losing pairing must still offer a direct recovery path without Setup.
    status.paired=false;status.joined=false;status.pairing='idle';chime.joined=false;
    await page.evaluate(async()=>{await refresh()});
    await page.locator('#heroSetup').click();
    assert.equal(await page.evaluate(()=>activeTab),'settings');
    assert.equal(await page.locator('#cfgPairTask').isVisible(),true);
    assert.equal(await page.locator('#nav').isVisible(),true);
    await page.locator('#pNext').click();
    status.paired=true;status.joined=true;status.pairing='joined';chime.joined=true;
    await page.evaluate(async()=>{await refresh()});
    // Initial setup overrides every stored page and exposes no pages while
    // loading status. Pairing, polling and reloads cannot unlock navigation.
    const initialAuthBefore={...authInfo};
    status.setup_visible=true;status.setup_required=true;status.paired=false;status.joined=false;status.pairing='idle';
    status.wifi_configured=false;status.wifi_connected=false;status.ssid='';status.ap_active=true;
    status.auth_enabled=false;status.auth_user='';authInfo.enabled=false;authInfo.password_set=false;chime.joined=false;
    gong.type=1;gong.audio_output=0;
    let releaseStatus;
    statusGate=new Promise(resolve=>{releaseStatus=resolve});
    await page.evaluate(()=>localStorage.setItem('tab','home'));await page.reload();
    assert.equal(await page.locator('#nav').isVisible(),false);
    assert.equal(await page.locator('main').isVisible(),false);
    statusGate=null;releaseStatus();
    await page.waitForFunction(()=>S&&P&&activeTab==='setup');await stabilise();
    assert.equal(await page.evaluate(()=>wizStep),0);
    for(const stored of ['home','sound','settings','debug','setup']) {
      await page.evaluate(tab=>localStorage.setItem('tab',tab),stored);await page.reload();
      await page.waitForFunction(()=>S&&P&&activeTab==='setup');await stabilise();
      assert.equal(await page.evaluate(()=>wizStep),0);
      assert.equal(await page.locator('#nav').isVisible(),false);
      for(const tab of ['home','sound','settings']) {
        await page.evaluate(tab=>show(tab),tab);
        assert.equal(await page.evaluate(()=>activeTab),'setup');
        assert.equal(await page.locator('#tab-'+tab).isVisible(),false);
      }
    }
    await capture('ui-initial-setup-desktop.png','setup',1100,800);
    await capture('ui-initial-setup-mobile.png','setup',390,900,undefined,true);
    status.wifi_configured=true;status.wifi_connected=true;status.ssid='Demo-WiFi';status.ap_active=false;
    await page.evaluate(async()=>{await refresh()});await page.locator('#wNext').click();
    await capture('ui-initial-pairing-desktop.png','setup',1100,800);
    await capture('ui-initial-pairing-mobile.png','setup',390,900,undefined,true);
    status.paired=true;status.joined=true;status.pairing='joined';chime.joined=true;
    await page.evaluate(async()=>{await refresh()});
    assert.equal(await page.locator('#nav').isVisible(),false);
    await page.evaluate(()=>localStorage.setItem('tab','settings'));await page.reload();
    await page.waitForFunction(()=>S&&P&&activeTab==='setup');await stabilise();
    assert.equal(await page.evaluate(()=>wizStep),1);
    await page.locator('#pNext').click();
    await page.waitForFunction(()=>!wizWireBusy&&wizWireOk);
    await capture('ui-initial-wiring-desktop.png','setup',1100,1000,undefined,true);
    await capture('ui-initial-wiring-mobile.png','setup',390,900,undefined,true);
    await page.locator('#sName').fill('Initial setup draft');
    await page.evaluate(()=>{show('settings');openSettingsFlow(0)});
    assert.equal(await page.evaluate(()=>activeTab),'setup');
    assert.equal(await page.evaluate(()=>settingsStep),-1);
    assert.equal(await page.locator('#sName').inputValue(),'Initial setup draft');
    await page.locator('#sName').fill('Demo ESP32 Chime');
    await page.waitForFunction(()=>!wizWireBusy&&wizWireOk);
    await page.locator('#sNext').click();
    await page.locator('#wizAuthModes .choice').filter({hasText:'Require sign-in'}).click();await page.locator('#wizAuthUser').fill('demo-owner');
    await page.locator('#wizAuthPass').fill('example-password');await page.locator('#wizAuthConfirm').fill('example-password');
    await capture('ui-initial-signin-desktop.png','setup',1100,850,undefined,true);
    await capture('ui-initial-signin-mobile.png','setup',390,900,undefined,true);
    await page.locator('#wizAuthNext').click();
    await capture('ui-initial-review-desktop.png','setup',1100,850,undefined,true);
    await capture('ui-initial-review-mobile.png','setup',390,900,undefined,true);
    assert.equal(status.setup_required,true);
    failNextSetupSave=true;
    await page.locator('#dHome').click();
    await page.waitForFunction(()=>document.getElementById('toast').textContent.includes('Synthetic setup storage failure')&&!document.getElementById('dHome').disabled);
    assert.equal(status.setup_required,true);
    assert.equal(await page.locator('#nav').isVisible(),false);
    await page.locator('#dHome').click();
    await page.waitForFunction(()=>wizSetupSaved&&S.setup_required===false);
    assert.equal(status.setup_visible,false);
    assert.deepEqual(await page.locator('#nav button:visible').evaluateAll(buttons=>buttons.map(b=>b.dataset.tab)),['home','sound','settings']);
    assert.equal(await page.evaluate(()=>activeTab),'home');
    await capture('ui-initial-complete-desktop.png','home',1100,850,undefined,true);
    await capture('ui-initial-complete-mobile.png','home',390,900,undefined,true);
    await page.reload();await page.waitForFunction(()=>S&&P&&activeTab==='home');await stabilise();
    await select('sound');
    assert.equal(await page.evaluate(()=>activeTab),'sound');
    assert.equal(status.setup_required,false);
    Object.assign(authInfo,initialAuthBefore);status.auth_enabled=authInfo.enabled;status.auth_user=authInfo.username;
    await page.evaluate(async()=>{await loadSettings();await refresh()});
    // Firmware file check: ESP32 image magic and embedded version marker.
    const fileChecks = await page.evaluate(async () => {
      const file = bytes => new File([new Uint8Array(bytes)], 'firmware.bin');
      const marker = [...new TextEncoder().encode('xxREOLINK_CHIME_VERSION=1.2.3\0')];
      // A decoy without a version (like this page's own search text) comes first.
      const decoy = [...new TextEncoder().encode("'REOLINK_CHIME_VERSION=');")];
      return [await fileVersion(file([0xE9, 0, ...decoy, ...marker])), await fileVersion(file([0xE9, 1, 2, 3])),
              await fileVersion(file([0x50, 0x4B, ...marker]))];
    });
    if (JSON.stringify(fileChecks) !== JSON.stringify(['1.2.3', null, false]))
      throw Error('fileVersion checks failed: ' + JSON.stringify(fileChecks));
    // Restart spinner (captured before it starts polling the chime).
    await page.setViewportSize({ width: 1100, height: 700 });
    await select('settings');
    await page.evaluate(version => { openSection('system'); waitForRestart('Installing version '+version, version); }, nextPatchVersion);
    await page.waitForTimeout(1200);
    await page.screenshot({ path: path.join(images, 'web-update-countdown.png'), animations: 'disabled' });
    results.push({ file: 'web-update-countdown.png', tab: 'settings', viewport: { width: 1100, height: 700 } });
    // Sign-in page, desktop and phone; it makes no requests before submit.
    for (const [name, width, height] of [['web-login.png', 1100, 700], ['web-login-mobile.png', 390, 760]]) {
      await page.setViewportSize({ width, height });
      await page.goto('http://reolink-docs.test/login');
      await page.locator('#pass').waitFor();
      if (await page.evaluate(() => document.documentElement.scrollWidth > innerWidth))
        throw Error(`login page has horizontal overflow at ${width}px`);
      await page.screenshot({ path: path.join(images, name), animations: 'disabled' });
      results.push({ file: name, tab: 'login', viewport: { width, height } });
    }
    assert.equal(expectedAuthErrors,0);
    assert.equal(expectedSetupErrors,0);
    if (errors.length || unexpected.length)
      throw Error(JSON.stringify({ page_errors: errors, unexpected_requests: unexpected }));
    // Include the UI hash so updated screenshots get fresh URLs even when the
    // firmware version is unchanged. Keep them tied to the tested captures.
    const readmeImages = {
      home:'ui-setup-hidden-desktop.png', mobile:'ui-setup-hidden-mobile.png',
      'sound-gong':'ui-sound-gong-desktop.png','sound-pwm':'ui-sound-pwm-desktop.png','sound-dac':'ui-sound-dac-desktop.png',
      'setup-wifi':'ui-initial-setup-desktop.png','setup-mobile':'ui-initial-setup-mobile.png',
      'setup-pairing':'ui-initial-pairing-desktop.png','setup-wiring':'ui-initial-wiring-desktop.png',
      'setup-signin':'ui-initial-signin-desktop.png','setup-review':'ui-initial-review-desktop.png','setup-complete':'ui-initial-complete-desktop.png',
      'settings-general':'ui-settings-setup-hidden-desktop.png','settings-wifi':'ui-settings-wifi-desktop.png',
      'settings-security':'web-settings-security.png',wiring:'ui-wiring-desktop.png',mqtt:'web-settings-ha.png',
      debug:'web-maintenance.png',update:'web-update-countdown.png',
    };
    const readmeScreenshots={};
    for(const [view,original] of Object.entries(readmeImages)) {
      const file=`readme-${view}.png`,capture=results.find(item=>item.file===original);
      assert.ok(capture,original);fs.copyFileSync(path.join(images,original),path.join(images,file));
      readmeScreenshots[view]=file;results.push({...capture,file,source_file:original});
    }
    const report = { firmware_version:firmwareVersion,readme_screenshots:readmeScreenshots,embedded_ui_sha256: embeddedUiHash,
      data: 'Synthetic API fixtures; no device connection', screenshots: results,
      checked_widths: [390, 768, 1100], checked_output_types: ['Gong', 'Audio', 'None'], checked_audio_outputs: ['PWM', 'DAC'],
      checked_sound_slots: 10, mode_switch_preserves_sound_slot: true,
      audio_preview_outputs: previews.map(p => p.audio_output), setup_live_pins_only_after_finish: true, setup_auth_only_after_finish: true, expected_auth_storage_failure_tested: true, setup_only_until_initial_setup_finishes: true, settings_wifi_pair_without_setup: true, wifi_mode_first_client_only_connection: true, setup_sound_drafts_survive_refresh: true, audio_previews_match_relay_draft: true, page_errors: errors, unexpected_requests: unexpected,
      initial_setup_only_until_finish: true, initial_setup_overrides_saved_tabs: true,
      initial_setup_stays_locked_after_pairing_and_save_failure: true,
      page_horizontal_overflow: false };
    fs.writeFileSync(path.join(images, 'screenshot-manifest.json'), JSON.stringify(report, null, 2) + '\n');
    console.log(JSON.stringify(report));
  } finally { await browser.close(); }
})().catch(error => { console.error(error); process.exitCode = 1; });
