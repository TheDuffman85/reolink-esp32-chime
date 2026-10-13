#!/usr/bin/env node
// Capture the current embedded HTML with isolated synthetic API fixtures.
// All requests are intercepted; no board, doorbell or external service is used.
const { chromium } = require('playwright');
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const root = path.resolve(__dirname, '..');
const source = fs.readFileSync(path.join(root, 'include/WebUi.h'), 'utf8');
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
  pairing: 'joined', profile: true, paired: true, joined: true,
  node_id: nodeId, identity_compatible: true, doorbell_id: doorbellId,
  chime_name: 'Demo ESP32 Chime', transmitted: 8, transmit_errors: 0,
  packets: 4, crc_ti: 4, crc_bad: 0, crc_ccitt: 0, dropped: 0, invalid_lengths: 0,
  rings: 1, last_ring_ms: 52000, gong_active: false, gong_ready: true,
  storage_ok: true, learned_polarity: 1, free_heap: 172032,
  firmware: 'Oct 13 2026 · demo build', version: '1.0.0', radio_resets: 0,
  ring_seq: 5, auth_enabled: true, auth_user: 'admin', mute: { active: false, permanent: false, remaining_s: 0 }, time_synced: true, epoch: 1791900000, mqtt: 'connected',
};
// Ring log fixture: newest first, epoch-stamped like a synchronised clock.
const ring = (id, ago, source, outcome, extra = {}) => ({ id, source, outcome, type: 0, sound: 0,
  ms: 60000 - ago * 1000, epoch: status.epoch - ago, previous_boot: false, volume: 4, buzzer_level: 35,
  preview: false, ...(source === 'doorbell' ? { tone: 0, rssi_dbm: -62 } : {}), ...extra });
const rings = { uptime_ms: 60000, epoch: status.epoch, last_id: 5, rings_since_start: 1, rings: [
  ring(5, 8, 'doorbell', 'played'),
  ring(4, 25, 'mqtt', 'played', { sound: 1, volume: 3 }),
  ring(3, 40, 'web', 'played', { sound: 2, preview: true }),
  ring(2, 3600 * 5, 'doorbell', 'chime_muted', { previous_boot: true, tone: 3, rssi_dbm: -70 }),
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
const gong = { type: 0, pattern: 0, pulse_ms: 300, gap_ms: 700,
  relay_available: true, buzzer_available: true };
const pinMap = { sck: 18, miso: 19, mosi: 23, cs: 5, clock: 4, data: 16,
  buzzer: 25, relay: 26, led: 2, button: 0, relay_active_low: false };
const issues = [
  { role: 'cs', level: 'info', text: 'Boot strap pin: keep the recommended 10 kOhm pull-up to 3.3 V.' },
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
  const results = [], errors = [], unexpected = [];
  try {
    const page = await browser.newPage({ viewport: { width: 1100, height: 1000 },
      deviceScaleFactor: 1, colorScheme: 'dark' });
    page.on('pageerror', error => errors.push(error.message));
    page.on('console', msg => { if (msg.type() === 'error') errors.push(msg.text()); });
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
      // The UI validates its GPIO map on load. This POST is a mocked check only.
      if (url.pathname === '/api/pins/check' && request.method() === 'POST')
        return route.fulfill({ json: { ok: true, issues } });
      const fixtures = {
        '/api/status': status, '/api/settings': settings, '/api/chime': chime,
        '/api/gong': gong, '/api/pins': { pins: pinMap, saved: pinMap, defaults: pinMap },
        '/api/packets': { packets }, '/api/rings': rings, '/api/auth': authInfo,
        '/api/integrations': integrations,
        '/api/wifi': { mode: 'client', active_mode: 'client', ap_ssid: '', ap_password_set: false,
          default_ap_ssid: 'Reolink-Chime-DEMO', button: true },
        '/api/wifi/scan': { scanning: false, networks: [
          { ssid: 'Demo-WiFi', rssi: -53, secure: true },
          { ssid: 'Demo-Guest', rssi: -72, secure: true },
        ] },
      };
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
    await page.evaluate(() => {
      for (let id = 1; id < 100; ++id) clearInterval(id);
      const label = document.createElement('div');
      label.textContent = 'DOCUMENTATION DEMO · Synthetic data · No hardware connected';
      label.style.cssText = 'padding:10px 16px;background:#273847;color:#ffd78d;font:13px/1.5 system-ui;border-bottom:1px solid #506778';
      document.querySelector('header').prepend(label);
    });
    const select = async tab => {
      await page.locator(`#nav [data-tab="${tab}"]`).click();
      await page.locator(`#tab-${tab}`).waitFor({ state: 'visible' });
    };
    const capture = async (name, tab, width = 1100, height = 1000, section) => {
      await page.setViewportSize({ width, height });
      await select(tab);
      if (section) await page.evaluate(s => openSection(s), section);
      if (section === 'debug') await page.waitForFunction(() => document.getElementById('packets').children.length === 4);
      if (tab === 'setup') await page.waitForFunction(() => document.querySelectorAll('#networks button[data-ssid]').length === 2);
      await page.evaluate(() => scrollTo(0, 0));
      // A phone capture uses the viewport so its fixed bottom navigation stays
      // at the bottom of the image instead of overlaying a full-page capture.
      await page.screenshot({ path: path.join(images, name), fullPage: width >= 860, animations: 'disabled' });
      results.push({ file: name, tab, viewport: { width, height } });
    };
    await capture('web-status.png', 'home', 1100, 700);
    await capture('web-sound.png', 'sound');
    gong.type = 1; gong.pattern = 1;
    await page.evaluate(async () => { await loadSettings(); render(); });
    await capture('web-sound-relay.png', 'sound');
    gong.type = 0; gong.pattern = 0;
    await page.evaluate(async () => { await loadSettings(); render(); });
    await capture('web-setup-wifi.png', 'setup', 1100, 800);
    status.joined = false; status.pairing = 'idle'; chime.joined = false;
    await page.evaluate(async () => { await refresh(); });
    await page.locator('#wNext').click();
    await capture('web-setup-pairing.png', 'setup', 1100, 800);
    await page.locator('#pNext').click();
    await capture('web-setup-sound.png', 'setup', 1100, 800);
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
      for (const section of ['general', 'network', 'doorbell', 'ha', 'security', 'system', 'advanced', 'debug']) {
        await page.evaluate(s => openSection(s), section);
        if (await page.evaluate(() => document.documentElement.scrollWidth > innerWidth))
          throw Error(`settings ${section} has horizontal overflow at ${width}px`);
      }
    }
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
    await page.evaluate(() => { openSection('system'); waitForRestart('Installing version 1.0.1', '1.0.1'); });
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
    if (errors.length || unexpected.length)
      throw Error(JSON.stringify({ page_errors: errors, unexpected_requests: unexpected }));
    const report = { embedded_ui_sha256: crypto.createHash('sha256').update(source).digest('hex'),
      data: 'Synthetic API fixtures; no device connection', screenshots: results,
      checked_widths: [390, 768, 1100], page_errors: errors, unexpected_requests: unexpected,
      page_horizontal_overflow: false };
    fs.writeFileSync(path.join(images, 'screenshot-manifest.json'), JSON.stringify(report, null, 2) + '\n');
    console.log(JSON.stringify(report));
  } finally { await browser.close(); }
})().catch(error => { console.error(error); process.exitCode = 1; });
