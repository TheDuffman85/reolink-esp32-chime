#pragma once
#include <Arduino.h>

// Single-page web app served at "/". Tabs: Home, Sound, Setup (onboarding
// wizard), Settings and Debug. All data comes from the JSON API in Web.cpp/WebAuth.cpp.
static const char WebUi[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#0f1720"><title>Chime</title>
<link rel="icon" href="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24'%3E%3Cpath fill='%2338bdf8' d='M12 22a2.5 2.5 0 0 0 2.45-2h-4.9A2.5 2.5 0 0 0 12 22Zm7-6V11a7 7 0 0 0-5.5-6.84V3.5a1.5 1.5 0 0 0-3 0v.66A7 7 0 0 0 5 11v5l-2 2v1h18v-1Z'/%3E%3C/svg%3E">
<script>try{const t=localStorage.getItem('theme');if(t==='light'||t==='dark')document.documentElement.dataset.theme=t}catch(e){}</script>
<style>
:root{--bg:#0f1720;--panel:#17222d;--panel2:#1e2b38;--line:#2b3b4a;--text:#e6eef5;--muted:#93a6b6;--accent:#38bdf8;--accent2:#0ea5e9;--ok:#34d399;--warn:#fbbf24;--bad:#f87171;--shadow:0 8px 30px rgba(0,0,0,.35);--r:16px;color-scheme:dark}
@media (prefers-color-scheme:light){:root:not([data-theme=dark]){--bg:#f3f6f9;--panel:#fff;--panel2:#f1f5f9;--line:#dbe3ea;--text:#0f1d2a;--muted:#5b6f80;--accent:#0284c7;--accent2:#0369a1;--ok:#059669;--warn:#b45309;--bad:#dc2626;--shadow:0 6px 24px rgba(15,29,42,.08);color-scheme:light}}
:root[data-theme=light]{--bg:#f3f6f9;--panel:#fff;--panel2:#f1f5f9;--line:#dbe3ea;--text:#0f1d2a;--muted:#5b6f80;--accent:#0284c7;--accent2:#0369a1;--ok:#059669;--warn:#b45309;--bad:#dc2626;--shadow:0 6px 24px rgba(15,29,42,.08);color-scheme:light}
*{box-sizing:border-box}html,body{margin:0}body{font:15px/1.5 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif;background:var(--bg);color:var(--text);padding-bottom:84px}
a{color:var(--accent)}h1,h2,h3{margin:0;line-height:1.25}h2{font-size:1.15rem}h3{font-size:1rem}
header{position:sticky;top:0;z-index:5;background:var(--bg);border-bottom:1px solid var(--line)}
.bar{max-width:1080px;margin:auto;padding:12px 16px;display:flex;align-items:center;gap:12px}
.brand{display:flex;align-items:center;gap:10px;flex:1 1 0;min-width:0}.brand>div{min-width:0}
.hdr-right{display:flex;flex:none;align-items:center;gap:8px;margin-left:auto}.logo{width:36px;height:36px;border-radius:11px;display:grid;place-items:center;background:linear-gradient(135deg,var(--accent),#6366f1);color:#fff;flex:none}
.brand b{display:block;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;font-size:1.25rem;letter-spacing:-.01em}
.pill{display:inline-flex;align-items:center;gap:6px;padding:4px 10px;border-radius:999px;background:var(--panel2);border:1px solid var(--line);font-size:.8rem;color:var(--muted);white-space:nowrap;max-width:46vw;overflow:hidden;text-overflow:ellipsis}
.dot{width:8px;height:8px;border-radius:50%;background:var(--muted);flex:none}.dot.ok{background:var(--ok)}.dot.warn{background:var(--warn)}.dot.bad{background:var(--bad)}
.dot.pulse,.sicon.pulse{animation:pulse 1.4s infinite}
.sicon{width:15px;height:15px;flex:none;color:var(--muted)}.sicon.ok{color:var(--ok)}.sicon.warn{color:var(--warn)}.sicon.bad{color:var(--bad)}
@media (max-width:759px){.pill.status>span{display:none}.pill.status{padding:6px;gap:0}}@keyframes pulse{50%{opacity:.35}}
nav{position:fixed;bottom:0;left:0;right:0;z-index:6;background:var(--panel);border-top:1px solid var(--line);display:flex;justify-content:center;padding:6px 6px calc(6px + env(safe-area-inset-bottom))}
nav button{flex:1;max-width:140px;background:none;border:0;color:var(--muted);display:flex;flex-direction:column;align-items:center;gap:2px;padding:6px 4px;font:inherit;font-size:.75rem;cursor:pointer;border-radius:12px}
nav button.hidden{display:none}.settings-flow .stepper-title,.settings-flow #pBack{display:none}
.ui-loading #nav,.ui-loading main{display:none}
nav button svg{width:22px;height:22px}nav button.on{color:var(--accent);background:var(--panel2)}
@media (min-width:860px){body{padding-bottom:24px}nav{position:static;border:0;border-bottom:1px solid var(--line);background:var(--bg);gap:6px}nav button{flex-direction:row;gap:8px;font-size:.9rem;max-width:none;flex:none;padding:8px 14px}nav button svg{width:18px;height:18px}}
main{max-width:1080px;margin:auto;padding:16px}.tab{display:none;animation:fade .2s}.tab.on{display:block}@keyframes fade{from{opacity:0;transform:translateY(4px)}}
.card{background:var(--panel);border:1px solid var(--line);border-radius:var(--r);padding:18px;margin-bottom:16px;box-shadow:var(--shadow);min-width:0}
.card>h2{margin-bottom:4px}.sub{color:var(--muted);margin:0 0 14px;font-size:.9rem}
.grid{display:grid;gap:16px;grid-template-columns:repeat(auto-fit,minmax(min(100%,260px),1fr))}.grid .card{margin:0}
.hero{display:flex;gap:18px;align-items:center;flex-wrap:wrap;background:linear-gradient(135deg,color-mix(in srgb,var(--accent) 16%,var(--panel)),var(--panel))}
.hero .icon{width:64px;height:64px;border-radius:20px;display:grid;place-items:center;background:var(--panel2);flex:none}.hero .icon svg{width:34px;height:34px}
.hero h1{font-size:1.5rem}.hero p{margin:4px 0 0;color:var(--muted)}.hero .text{flex:1;min-width:200px}.hero .actions{display:flex;gap:8px;flex-wrap:wrap}
.stat .label{color:var(--muted);font-size:.8rem;text-transform:uppercase;letter-spacing:.05em}.stat .value{font-size:1.35rem;font-weight:600;margin:4px 0 2px;overflow-wrap:anywhere}.stat .hint{color:var(--muted);font-size:.85rem;overflow-wrap:anywhere}
button{font:inherit;border-radius:12px;border:1px solid var(--line);background:var(--panel2);color:var(--text);padding:10px 16px;cursor:pointer;display:inline-flex;align-items:center;gap:8px;justify-content:center}
button:hover{border-color:var(--accent)}button:disabled{opacity:.5;cursor:default}
.primary{background:var(--accent2);border-color:var(--accent2);color:#fff;font-weight:600}.primary:hover{filter:brightness(1.08)}
.danger{color:var(--bad);border-color:color-mix(in srgb,var(--bad) 45%,var(--line))}.ghost{background:none}.small{padding:6px 12px;font-size:.85rem}
.row{display:flex;gap:10px;flex-wrap:wrap;align-items:center}.spacer{flex:1}
.btnbar{display:flex;gap:10px;flex-wrap:wrap;align-items:center;justify-content:flex-end;margin-top:16px}.btnbar .left{margin-right:auto}
@media (max-width:479px){.btnbar{gap:8px}.btnbar button{padding:10px 12px;white-space:nowrap}.btnbar button svg{display:none}.btnbar.wiz{flex-wrap:nowrap}.btnbar.wiz button:last-child{flex:1}.btnbar.wiz.authbar{display:grid;grid-template-columns:auto 1fr}.authbar .left{margin-right:0}.authbar button:last-child{grid-column:1/-1}.stepact button{flex:1}}
.btnbar.wiz{border-top:1px solid var(--line);padding-top:16px;margin-top:20px}.btnbar.wiz button{min-width:96px}.stepact{display:flex;gap:10px;flex-wrap:wrap;align-items:center;margin-top:12px}.stepact .sub{margin:0;flex:1 1 200px}
label.field,div.field{display:block;margin:12px 0}label.field>span,div.field>span{display:block;font-size:.85rem;color:var(--muted);margin-bottom:6px}
input,select{font:inherit;width:100%;padding:10px 12px;border-radius:12px;border:1px solid var(--line);background:var(--panel2);color:var(--text)}
select{appearance:none;-webkit-appearance:none;padding-right:42px;cursor:pointer;background-image:url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24'%3E%3Cpath fill='none' stroke='%230ea5e9' stroke-width='2.5' stroke-linecap='round' stroke-linejoin='round' d='m6 9 6 6 6-6'/%3E%3C/svg%3E");background-repeat:no-repeat;background-position:right 12px center;background-size:20px}select:disabled{cursor:default;opacity:.6}
input:focus,select:focus{outline:2px solid color-mix(in srgb,var(--accent) 60%,transparent);border-color:var(--accent)}
input[type=range]{padding:0;accent-color:var(--accent);background:none;border:0}
.switch{display:flex;align-items:center;justify-content:space-between;gap:16px;padding:12px 0;border-top:1px solid var(--line)}.switch:first-of-type{border-top:0}
.switch input{appearance:none;-webkit-appearance:none;width:46px;height:26px;border-radius:99px;background:var(--line);position:relative;cursor:pointer;flex:none;padding:0;border:0;transition:.2s}
.switch input::after{content:"";position:absolute;top:3px;left:3px;width:20px;height:20px;border-radius:50%;background:#fff;transition:.2s}
.switch input:checked{background:var(--accent2)}.switch input:checked::after{left:23px}.switch small{display:block;color:var(--muted)}
.choices{display:grid;gap:10px;grid-template-columns:repeat(auto-fit,minmax(min(100%,170px),1fr))}
.choice{border:1px solid var(--line);border-radius:14px;padding:14px;background:var(--panel2);cursor:pointer;display:flex;align-items:center;gap:10px}
.choice.on{border-color:var(--accent);box-shadow:0 0 0 1px var(--accent) inset}.pickm{display:none;grid-column:1/-1}.pickm .row{flex-wrap:nowrap}.pickm select{flex:1;min-width:0}.pickm small{display:block;color:var(--muted);margin-top:6px}
@media (max-width:599px){.picklist>.choice{display:none}.picklist>.pickm{display:block}}.choice b{display:block}.choice small{color:var(--muted)}
.seg{display:flex;flex-wrap:wrap;background:var(--panel2);border:1px solid var(--line);border-radius:12px;padding:3px;gap:3px;width:max-content;max-width:100%}.seg button{border:0;background:none;padding:7px 14px;border-radius:9px}.seg button.on{background:var(--accent2);color:#fff}
.list{border:1px solid var(--line);border-radius:14px;overflow:hidden;max-height:260px;overflow-y:auto}.list button{width:100%;border:0;border-radius:0;border-top:1px solid var(--line);justify-content:flex-start;background:var(--panel);text-align:left}.list button:first-child{border-top:0}
.list button.on{background:var(--panel2);color:var(--accent)}.bars{margin-left:auto;display:inline-flex;gap:2px;align-items:flex-end;height:14px;vertical-align:middle}.bars i{width:3px;background:var(--line);border-radius:1px}.bars i.on{background:var(--accent)}
.steps{display:flex;gap:6px;margin-bottom:16px}.steps div{flex:1;height:6px;border-radius:6px;background:var(--line)}.steps div.on{background:var(--accent)}.steps div.done{background:var(--ok)}
.stepper-title{font-size:.8rem;color:var(--muted);text-transform:uppercase;letter-spacing:.06em}
ol.howto{margin:8px 0 16px;padding-left:20px}ol.howto li{margin:6px 0}
.phases{display:grid;gap:8px;margin:14px 0}.phase{display:flex;align-items:center;gap:10px;color:var(--muted)}.phase.on{color:var(--text)}.phase.done{color:var(--ok)}
.phase .dot{width:10px;height:10px}.progress{height:6px;background:var(--line);border-radius:6px;overflow:hidden}.progress div{height:100%;background:var(--accent);width:0;transition:width .5s}
.success{text-align:center;padding:12px 0}.success .big{width:72px;height:72px;margin:0 auto 12px;border-radius:50%;display:grid;place-items:center;background:color-mix(in srgb,var(--ok) 18%,transparent);color:var(--ok)}
.kv{display:grid;grid-template-columns:max-content 1fr;gap:6px 16px;font-size:.9rem;margin:0}.kv dt{color:var(--muted)}.kv dd{margin:0;overflow-wrap:anywhere;font-variant-numeric:tabular-nums;min-width:0}
pre.log{background:#0a0f14;color:#cfe3f1;border-radius:12px;padding:12px;height:360px;overflow:auto;font:12px/1.45 ui-monospace,SFMono-Regular,Menlo,monospace;margin:0;white-space:pre-wrap;overflow-wrap:anywhere}
.log .rx{color:#7dd3fc}.log .tx{color:#a5b4fc}.log .app{color:#86efac}.log .err{color:#fca5a5}.log .diag{color:#6b7f90}
table{border-collapse:collapse;width:100%;font-size:.82rem}td,th{text-align:left;border-bottom:1px solid var(--line);padding:8px;vertical-align:top}th{color:var(--muted);font-weight:500}
table.pins{table-layout:fixed}table.pins th:nth-child(1){width:38%}table.pins th:nth-child(2){width:200px}table.pins select{width:100%}@media (max-width:640px){table.pins thead{display:none}table.pins tr{display:block;border-bottom:1px solid var(--line);padding:10px 0 10px 8px}table.pins td{display:block;border:0;padding:3px 0}table.pins select{width:100%}}.issue{display:flex;gap:6px;align-items:flex-start;font-size:.85rem;margin:2px 0}.issue svg{width:14px;height:14px;flex:none;margin-top:.2em}.issue.ok{color:var(--ok)}.issue.error{color:var(--bad)}.issue.warning{color:var(--warn)}.issue.info{color:var(--muted)}.changed td:first-child{box-shadow:inset 3px 0 var(--accent)}
td.hex{font-family:ui-monospace,monospace;overflow-wrap:anywhere;min-width:200px}.scroll{overflow:auto}
details{border-top:1px solid var(--line);padding-top:10px;margin-top:14px}summary{cursor:pointer;color:var(--muted);font-weight:500}
.notice{border-radius:12px;padding:12px 14px;background:color-mix(in srgb,var(--warn) 14%,var(--panel));border:1px solid color-mix(in srgb,var(--warn) 40%,var(--line));font-size:.9rem;margin-bottom:12px}
.toast{position:fixed;left:50%;bottom:96px;transform:translateX(-50%) translateY(20px);opacity:0;pointer-events:none;background:var(--text);color:var(--bg);padding:10px 16px;border-radius:12px;box-shadow:var(--shadow);transition:.25s;z-index:20;max-width:calc(100% - 32px)}
.toast.show{opacity:1;transform:translateX(-50%)}.toast.bad{background:var(--bad);color:#fff}@media (min-width:860px){.toast{bottom:24px}}
.rings{list-style:none;margin:12px 0 0;padding:0;max-height:480px;overflow-y:auto}.rings li{display:flex;gap:12px;align-items:center;padding:10px 2px;border-top:1px solid var(--line)}.rings li:first-child{border-top:0}
.ring-ic{width:36px;height:36px;border-radius:12px;display:grid;place-items:center;background:var(--panel2);flex:none;color:var(--accent)}.ring-ic.door{color:var(--ok)}
.ring-main{flex:1;min-width:0}.ring-main b{display:block;font-weight:600}.ring-main small{color:var(--muted);display:block;overflow-wrap:anywhere}.ring-side{text-align:right;flex:none;font-size:.85rem;color:var(--muted);font-variant-numeric:tabular-nums}
.badge{display:inline-block;padding:1px 8px;border-radius:99px;font-size:.75rem;border:1px solid var(--line);margin-top:3px}.badge.played{color:var(--ok)}.badge.muted{color:var(--warn)}.badge.busy{color:var(--bad)}.badge.silent{color:var(--muted)}.badge.chime_muted{color:var(--warn)}
.mutemenu{flex-basis:100%;display:flex;flex-wrap:wrap;gap:8px;align-items:center;padding-top:4px}.mutemenu b{font-weight:500;color:var(--muted);margin-right:4px}
.pill.click{cursor:pointer;color:var(--warn);border-color:color-mix(in srgb,var(--warn) 45%,var(--line))}.empty{color:var(--muted);padding:18px 4px;text-align:center}
.settings{display:grid;grid-template-columns:270px minmax(0,1fr);gap:20px;align-items:start}
.snav{position:sticky;top:124px;display:flex;flex-direction:column;gap:2px;background:var(--panel);border:1px solid var(--line);border-radius:var(--r);padding:8px;box-shadow:var(--shadow)}
.snav button{justify-content:flex-start;text-align:left;border:0;background:none;padding:10px 12px;gap:12px;width:100%;border-radius:12px}.snav button:hover{background:var(--panel2)}
.snav button>svg{width:20px;height:20px;flex:none;color:var(--muted)}.snav span{min-width:0;flex:1}.snav b{display:block;font-weight:600}
.snav small{display:block;color:var(--muted);font-size:.8rem;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}.snav button.on{background:var(--panel2);color:var(--accent)}.snav button.on>svg{color:var(--accent)}.snav .chev{display:none}
.spanel{min-width:0}.ssec{display:none;max-width:760px}.ssec.wide{max-width:none}.ssec.on{display:block;animation:fade .2s}.sback{display:none}
.card>h3,.card .row>h3{font-size:1.02rem}.card>h3{margin-bottom:4px}.pair2{display:grid;grid-template-columns:repeat(auto-fit,minmax(min(100%,200px),1fr));gap:0 14px}
.inline{display:flex;gap:10px;align-items:center}.inline input{flex:1;min-width:0}.dangerzone{border-color:color-mix(in srgb,var(--bad) 30%,var(--line))}
details.plain{border:0;padding:0;margin:4px 0 0}
@media (max-width:859px){.settings{display:block}.snav{position:static}.settings.detail .snav{display:none}.settings:not(.detail) .spanel{display:none}.sback{display:inline-flex;margin:-8px 0 6px -10px;border:0;background:none;color:var(--accent);font-weight:600;padding:6px 10px}.sback:hover{border:0;background:var(--panel2)}.snav .chev{display:block;width:16px;height:16px;color:var(--muted)}.snav button.on{background:none;color:inherit}.snav button.on>svg{color:var(--muted)}.snav button{padding:12px}}
.userbar{display:flex;align-items:center;gap:2px;border:1px solid var(--line);border-radius:999px;padding:3px;background:var(--panel2);font-size:.85rem;max-width:50vw}
.userchip{display:flex;align-items:center;gap:6px;padding:0 8px 0 2px;min-width:0}.userchip span:last-child{overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.avatar{width:24px;height:24px;border-radius:50%;display:grid;place-items:center;background:color-mix(in srgb,var(--accent) 18%,transparent);color:var(--accent);flex:none}
.signout{border:0;border-left:1px solid var(--line);border-radius:0 999px 999px 0;background:none;padding:4px 10px;font-size:.85rem;gap:6px;color:var(--muted)}.signout:hover{color:var(--bad);border-color:var(--line)}
@media (max-width:479px){.brand b{font-size:1.15rem}.userchip,.signout span{display:none}.userbar{padding:0;border:0;background:none}.signout{border:1px solid var(--line);border-radius:50%;padding:7px;background:var(--panel2)}}
dialog.modal{border:1px solid var(--line);border-radius:18px;background:var(--panel);color:var(--text);padding:0;width:min(420px,calc(100% - 32px));box-shadow:0 24px 70px rgba(0,0,0,.45)}
dialog.modal::backdrop{background:rgba(5,10,15,.55);backdrop-filter:blur(2px)}dialog.modal[open]{animation:pop .16s ease-out}@keyframes pop{from{transform:scale(.96);opacity:0}}
dialog.modal form{padding:22px}dialog.modal h2{font-size:1.15rem;margin:0 0 6px}dialog.modal p+.btnbar{margin-top:0}dialog.modal p{color:var(--muted);margin:0 0 20px;white-space:pre-line;overflow-wrap:anywhere}
.dlg-icon{width:44px;height:44px;border-radius:14px;display:grid;place-items:center;margin-bottom:14px;background:color-mix(in srgb,var(--accent) 15%,transparent);color:var(--accent)}
.dlg-icon.danger{background:color-mix(in srgb,var(--bad) 15%,transparent);color:var(--bad)}.dlg-icon.ok{background:color-mix(in srgb,var(--ok) 15%,transparent);color:var(--ok)}
.spinner{width:40px;height:40px;border:4px solid var(--line);border-top-color:var(--accent);border-radius:50%;margin:2px auto 4px;animation:spin .9s linear infinite}@keyframes spin{to{transform:rotate(360deg)}}
button.dangerbtn{background:#dc2626;border-color:#dc2626;color:#fff;font-weight:600}button.dangerbtn:hover{filter:brightness(1.08)}
.hidden{display:none!important}.muted{color:var(--muted)}.mono{font-family:ui-monospace,monospace}.check{display:inline-flex;gap:6px;align-items:center;color:var(--muted);font-size:.85rem}.check input{width:auto}
</style></head><body class="ui-loading">
<svg width="0" height="0" style="position:absolute"><defs>
<symbol id="i-bell" viewBox="0 0 24 24"><path fill="currentColor" d="M12 22a2.5 2.5 0 0 0 2.45-2h-4.9A2.5 2.5 0 0 0 12 22Zm7-6V11a7 7 0 0 0-5.5-6.84V3.5a1.5 1.5 0 0 0-3 0v.66A7 7 0 0 0 5 11v5l-2 2v1h18v-1Z"/></symbol>
<symbol id="i-home" viewBox="0 0 24 24"><path fill="none" stroke="currentColor" stroke-width="2" stroke-linejoin="round" d="M3 11 12 4l9 7v9a1 1 0 0 1-1 1h-5v-6H9v6H4a1 1 0 0 1-1-1Z"/></symbol>
<symbol id="i-sound" viewBox="0 0 24 24"><path fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" d="M4 9v6h4l5 4V5L8 9Zm12.5-.5a5 5 0 0 1 0 7M19 6a8.5 8.5 0 0 1 0 12"/></symbol>
<symbol id="i-setup" viewBox="0 0 24 24"><path fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" d="m3 21 10-10M15 4V2m0 14v-2M8 9h2m10 0h2m-4.2 2.8L19 13m-1.2-6.8L19 5m-6.8 1.2L11 5"/><circle cx="15" cy="9" r="1.2" fill="currentColor"/></symbol>
<symbol id="i-gear" viewBox="0 0 24 24"><path fill="none" stroke="currentColor" stroke-width="2" stroke-linejoin="round" d="M10.3 3h3.4l.5 2.6 1.9 1.1 2.5-.9 1.7 3-2 1.7v2.2l2 1.7-1.7 3-2.5-.9-1.9 1.1-.5 2.6h-3.4l-.5-2.6-1.9-1.1-2.5.9-1.7-3 2-1.7v-2.2l-2-1.7 1.7-3 2.5.9 1.9-1.1Z"/><circle cx="12" cy="12" r="3" fill="none" stroke="currentColor" stroke-width="2"/></symbol>
<symbol id="i-bug" viewBox="0 0 24 24"><path fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" d="M8 8a4 4 0 0 1 8 0v1H8Zm-2 3h12v4a6 6 0 0 1-12 0Zm6 0v10M3 13h3m12 0h3M4 7l3 2m13-2-3 2M4 20l3-2m13 2-3-2"/></symbol>
<symbol id="i-belloff" viewBox="0 0 24 24"><path fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" d="M9.5 20a2.5 2.5 0 0 0 5 0M18 13.5V11a6 6 0 0 0-8.3-5.5M6.4 7.7A6 6 0 0 0 6 11v5l-2 2h14M3 3l18 18"/></symbol>
<symbol id="i-lock" viewBox="0 0 24 24"><rect x="5" y="11" width="14" height="10" rx="2" fill="none" stroke="currentColor" stroke-width="2"/><path fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" d="M8 11V8a4 4 0 0 1 8 0v3"/></symbol>
<symbol id="i-chip" viewBox="0 0 24 24"><rect x="6" y="6" width="12" height="12" rx="2" fill="none" stroke="currentColor" stroke-width="2"/><path fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" d="M9 2v4m6-4v4M9 18v4m6-4v4M2 9h4m-4 6h4m12-6h4m-4 6h4"/></symbol>
<symbol id="i-chev" viewBox="0 0 24 24"><path fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" d="m9 6 6 6-6 6"/></symbol>
<symbol id="i-warn" viewBox="0 0 24 24"><path fill="none" stroke="currentColor" stroke-width="2" stroke-linejoin="round" d="M12 3 2 20h20Z"/><path fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" d="M12 10v4m0 3v.01"/></symbol>
<symbol id="i-info" viewBox="0 0 24 24"><circle cx="12" cy="12" r="9" fill="none" stroke="currentColor" stroke-width="2"/><path fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" d="M12 11v6m0-9.5v.01"/></symbol>
<symbol id="i-x" viewBox="0 0 24 24"><circle cx="12" cy="12" r="9" fill="none" stroke="currentColor" stroke-width="2"/><path fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" d="m9 9 6 6m0-6-6 6"/></symbol>
<symbol id="i-user" viewBox="0 0 24 24"><circle cx="12" cy="8" r="4" fill="none" stroke="currentColor" stroke-width="2"/><path fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" d="M4 21a8 8 0 0 1 16 0"/></symbol>
<symbol id="i-logout" viewBox="0 0 24 24"><path fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" d="M15 4h3a2 2 0 0 1 2 2v12a2 2 0 0 1-2 2h-3M10 17l-5-5 5-5M5 12h11"/></symbol>
<symbol id="i-restart" viewBox="0 0 24 24"><path fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" d="M20 12a8 8 0 1 1-2.34-5.66M20 4v5h-5"/></symbol>
<symbol id="i-play" viewBox="0 0 24 24"><path fill="currentColor" d="M8 5.5v13a1 1 0 0 0 1.5.86l10.5-6.5a1 1 0 0 0 0-1.72L9.5 4.64A1 1 0 0 0 8 5.5Z"/></symbol>
<symbol id="i-check" viewBox="0 0 24 24"><path fill="none" stroke="currentColor" stroke-width="3" stroke-linecap="round" stroke-linejoin="round" d="m5 12.5 4.5 4.5L19 7.5"/></symbol>
<symbol id="i-wifi" viewBox="0 0 24 24"><path fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" d="M2 9a15 15 0 0 1 20 0M5 12.5a10 10 0 0 1 14 0M8.5 16a5 5 0 0 1 7 0"/><circle cx="12" cy="19.5" r="1.3" fill="currentColor"/></symbol>
<symbol id="i-link" viewBox="0 0 24 24"><path fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" d="M10 14a4 4 0 0 0 5.7 0l3-3a4 4 0 0 0-5.7-5.7l-1 1M14 10a4 4 0 0 0-5.7 0l-3 3a4 4 0 0 0 5.7 5.7l1-1"/></symbol>
</defs></svg>
<header><div class="bar"><div class="brand"><div class="logo"><svg width="20" height="20"><use href="#i-bell"/></svg></div><div style="min-width:0"><b id="hdrName">Chime</b></div></div>
<div class="hdr-right"><span class="pill status click hidden" id="hdrMute" title="Muted · open Home to unmute"><svg class="sicon" style="color:var(--warn)"><use href="#i-belloff"/></svg><span>Muted</span></span><span class="pill status" id="hdrLinkPill"><svg class="sicon" data-base="sicon" id="hdrLinkDot"><use href="#i-link"/></svg><span id="hdrLink">…</span></span><span class="pill status" id="hdrWifiPill"><svg class="sicon" data-base="sicon" id="hdrWifiDot"><use href="#i-wifi"/></svg><span id="hdrWifi">…</span></span>
<div class="userbar hidden" id="userWrap"><span class="userchip" title="Signed in"><span class="avatar"><svg width="14" height="14"><use href="#i-user"/></svg></span><span id="userName"></span></span><button class="signout" id="umOut" title="Sign out" aria-label="Sign out"><svg width="16" height="16"><use href="#i-logout"/></svg><span>Sign out</span></button></div></div></div>
<nav id="nav">
<button data-tab="home" class="on"><svg><use href="#i-home"/></svg>Home</button>
<button data-tab="sound"><svg><use href="#i-sound"/></svg>Sound</button>
<button data-tab="setup" id="setupTab" class="hidden"><svg><use href="#i-setup"/></svg>Setup</button>
<button data-tab="settings"><svg><use href="#i-gear"/></svg>Settings</button></nav></header>
<main>
<!-- HOME -->
<section class="tab on" id="tab-home">
<div class="notice hidden" id="identityNotice">This chime needs to be paired again before the doorbell can ring it. Remove its old entry in the Reolink app, then use Settings → Doorbell → Pair again.</div>
<div class="card hero"><div class="icon" id="heroIcon"><svg><use href="#i-bell"/></svg></div><div class="text"><h1 id="heroTitle">Connecting…</h1><p id="heroText">Reading chime status.</p></div>
<div class="actions"><button class="hidden" id="heroSetup">Start setup</button><button id="heroMute"><svg width="16" height="16"><use href="#i-belloff"/></svg><span id="heroMuteText">Mute</span></button><button class="primary" id="heroTest"><svg width="16" height="16"><use href="#i-play"/></svg>Play chime</button></div>
<div class="mutemenu hidden" id="muteMenu"><b>Mute for</b><button class="small" data-m="30">30 min</button><button class="small" data-m="60">1 h</button><button class="small" data-m="120">2 h</button><button class="small" data-m="240">4 h</button><button class="small" data-m="480">8 h</button><button class="small" data-m="0">Until I unmute</button></div></div>
<div class="card"><div class="row"><div style="min-width:0"><h2>Ring log</h2><p class="sub" id="ringSummary" style="margin:2px 0 0">Loading…</p></div><span class="spacer"></span><div class="seg" id="ringFilter"><button type="button" data-f="all">All</button><button type="button" data-f="doorbell">Doorbell only</button></div><button class="small ghost" id="ringClear">Clear</button></div>
<ul class="rings" id="rings"></ul></div></section>
<!-- SOUND -->
<section class="tab" id="tab-sound">
<div class="card"><h2>Sound mode</h2><p class="sub">Choose what makes the sound when someone rings. Doorbell rings follow the selected Reolink ringtone: ten numbered slots map to ten melodies or gong patterns. The choices below set the default for local tests and MQTT plays.</p><div class="choices" id="gongTypes"></div></div>
<div id="audioCards" class="hidden">
<div class="card"><h2>Audio output</h2><p class="sub">Choose the output method for your amplifier. Both play the same melodies.</p><div class="choices" id="audioOutputs"></div><p class="sub" id="audioOutputHint" style="margin:12px 0 0"></p></div>
<div class="card"><h2>Melody for local tests</h2><p class="sub">Synthesized alternatives to the Reolink sounds; the original recordings live in a separate sound chip. Plays through the selected PWM or DAC output. Tap ▶ to preview.</p><div class="choices" id="melodies"></div></div>
<div class="card"><h2>Volume</h2><p class="sub">The audio level sets the maximum. The chime volume (also set in the Reolink app) scales it.</p>
<label class="field"><span>Audio level: <b id="levelOut"></b>%</span><input type="range" id="level" min="0" max="100" step="5"></label>
<div class="field"><span class="muted" style="display:block;font-size:.85rem;margin-bottom:6px">Chime volume</span><div class="row"><div class="seg" id="appVolume"></div><span class="spacer"></span><button id="previewVolume"><svg width="14" height="14"><use href="#i-play"/></svg>Preview at this volume</button></div></div></div>
<div class="card"><h2>Extra relay pulse</h2>
<label class="switch"><div>Also pulse the relay<small>Switches the relay once per ring while the melody plays, e.g. for a light.</small></div><input type="checkbox" id="relay"></label>
<label class="field" id="relayMsField"><span>Relay pulse: <b id="relayMsOut"></b> ms</span><input type="range" id="relayMs" min="50" max="2000" step="50"></label></div>
</div>
<div id="relayCards">
<div class="card"><h2>Strike pattern for local tests</h2><p class="sub">The relay energises the gong coil once per strike. On a classic two-tone gong, energising plays “ding” and releasing plays “dong”. Numbered patterns correspond to the doorbell ringtone slots. Tap ▶ to try it.</p><div class="choices" id="patterns"></div></div>
<div class="card"><h2>Timing</h2><p class="sub">Most 8–12 V AC gongs need 200–400 ms per strike. Too short and the plunger does not reach the bar.</p>
<label class="field"><span>Strike length: <b id="pulseOut"></b> ms</span><input type="range" id="pulse" min="50" max="2000" step="25"></label>
<label class="field"><span>Pause between strikes: <b id="gapOut"></b> ms</span><input type="range" id="gap" min="100" max="3000" step="50"></label>
<p class="sub" style="margin:8px 0 0">Setting the chime volume to “Mute” in the Reolink app silences the gong.</p>
<div class="notice hidden" id="noRelay">No relay GPIO is assigned. Set one under Settings → Advanced → Wiring first.</div></div>
<details class="card" style="margin-top:0"><summary>Wiring a gong</summary><p class="sub" style="margin-top:10px">Put the relay's normally-open contact where the old doorbell button was: in series with the gong and its bell transformer (e.g. 8 V AC). Keep mains wiring out of reach and use a relay module rated for the transformer voltage. Connect the relay GPIO to a relay module that accepts 3.3 V logic.</p></details>
</div>
<div id="noneCards" class="hidden"><div class="card"><h2>Silent</h2><p class="sub" style="margin:0">The chime makes no sound. Rings still appear in the ring log on Home and are sent to MQTT / Home Assistant, for example to announce them on your own speakers.</p></div></div>
<div class="card"><h2>Status LED</h2><label class="switch"><div>Blink while listening<small>Also set in the Reolink app.</small></div><input type="checkbox" id="led"></label></div>
<div class="btnbar" style="margin-top:0"><button class="ghost left" id="soundReset">Discard</button><button class="primary" id="soundSave">Save</button></div>
</section>
<!-- SETUP -->
<section class="tab" id="tab-setup"><div class="card">
<div class="steps" id="steps"><div></div><div></div><div></div><div></div><div></div></div>
<div id="wiz0"><div class="stepper-title">Step 1 of 5</div><h2>Connect to WiFi</h2><p class="sub">The chime works without WiFi, but on your network you can open this page at <b>reolink-chime.local</b>.</p>
<div id="wifiNow" class="notice hidden"></div>
<div class="row"><b>Networks nearby</b><span class="spacer"></span><button class="small" id="scan">Scan again</button></div>
<div class="list" id="networks" style="margin-top:10px"><button disabled>Scanning…</button></div>
<label class="field"><span>Network name</span><input id="wSsid" maxlength="32" autocomplete="off"></label>
<label class="field"><span>Password</span><input id="wPass" type="password" maxlength="63" autocomplete="new-password" placeholder="Empty for an open network"></label>
<div id="wifiProgress" class="hidden"><div class="progress"><div id="wifiBar"></div></div><p class="sub" id="wifiProgressText" style="margin-top:8px"></p></div>
<div class="stepact"><button id="wConnect"><svg width="16" height="16"><use href="#i-wifi"/></svg>Connect</button></div>
<div class="btnbar wiz"><button id="wNext">Continue</button></div></div>
<div id="wiz1" class="hidden"><div class="stepper-title">Step 2 of 5</div><h2>Pair with your doorbell</h2><p class="notice" style="margin-top:12px"><b>For first pairing, place the chime 1–2 metres from the doorbell.</b> Strong reception helps automatic frequency tuning calibrate the radio. Keep it nearby through pairing, renaming and a few doorbell tests.</p>
<label class="field"><span>Region</span><select id="pBand"><option value="868000000">Europe (tested)</option><option value="915000000">North America (untested)</option><option value="920000000">Asia-Pacific (untested)</option><option value="" disabled>Custom (Settings → Advanced)</option></select></label>
<p class="sub" id="pFreq" style="margin:-4px 0 12px"></p>
<div class="notice hidden" id="pUntested">Untested: only a European doorbell has been tested, and which frequency other regions use is assumed. This region needs a CC1101 module made for 915 MHz. Please report whether pairing works.</div>
<ol class="howto"><li>In the <b>Reolink app</b>, open the doorbell → <b>Settings</b> → <b>Chime</b> → add a chime.</li><li>Wait until the app shows <b>“Searching…”</b>.</li><li>Press <b>Start pairing</b> below. The chime appears in the app as <b>Reolink Chime</b>.</li><li>Tap <b>Pair</b> in the app, then give the chime a name.</li><li>Press the physical doorbell or use the main Reolink chime <b>Test</b> control a few times. Wait a few seconds, then move the chime to its intended location and test again.</li></ol>
<div class="phases" id="phases"></div><div class="progress"><div id="pairBar"></div></div>
<div class="stepact"><button class="hidden" id="pCancel">Cancel</button><button id="pStart"><svg width="16" height="16"><use href="#i-link"/></svg><span>Start pairing</span></button><p class="sub" id="pairText">Ready.</p></div>
<div class="btnbar wiz"><button class="ghost left" id="pBack">Back</button><button id="pNext">Continue</button></div></div>
<div id="wiz2" class="hidden"><div class="stepper-title">Step 3 of 5</div><h2>Make it yours</h2><p class="sub">The name appears in the Reolink app. Doorbell rings follow its selected ringtone slot. Pick an output and a default sound for local tests and MQTT plays.</p>
<label class="field"><span>Chime name</span><input id="sName" maxlength="32"></label>
<div class="field"><span>Sound mode</span><div class="choices" id="wizTypes"></div></div>
<div class="field hidden" id="wizAudioField"><span>Audio output</span><div class="choices" id="wizAudioOutputs"></div></div>
<div class="field" id="wizSoundField"><span id="wizSoundLabel">Sound</span><div class="choices" id="melodies2"></div></div>
<div id="wizWiring" style="border:1px solid var(--line);border-radius:14px;padding:16px;margin:16px 0;background:var(--panel2)"><h3>Wiring</h3><p class="sub" id="wizWireHint"></p>
<div class="pair2">
<label class="field hidden" id="wizAudioPinField"><span>Audio output GPIO</span><select id="wizAudioPin"></select></label>
<label class="field hidden" id="wizRelayPinField"><span>Relay GPIO</span><select id="wizRelayPin"></select></label>
<label class="field hidden" id="wizRelayLowField"><span>Relay input level</span><select id="wizRelayLow"><option value="0">Active high</option><option value="1">Active low</option></select></label>
</div>
<label class="switch hidden" id="wizExtraField"><div>Extra relay pulse<small>Also switch a relay while the melody plays.</small></div><input type="checkbox" id="wizExtraRelay"></label>
<p class="sub" id="wizWireNote" style="margin:8px 0 0"></p><div id="wizWireIssues" role="status" aria-live="polite"></div>
</div>
<div class="row" id="wizAdvancedRow"><button class="small ghost" id="wizAdvanced">Advanced wiring</button><span class="muted" style="font-size:.85rem">Radio, LED and button GPIOs</span></div>
<div class="btnbar wiz"><button class="ghost left" id="sBack">Back</button><button class="primary" id="sNext">Continue</button></div></div>
<div id="wiz3" class="hidden"><div class="stepper-title">Step 4 of 5 · Optional</div><h2>Protect your chime</h2><p class="sub">Choose whether this page requires sign-in. You can change this later under Settings → Sign-in.</p>
<label class="switch"><div>Enable sign-in<small>Use your own user name and password to open the chime interface.</small></div><input type="checkbox" id="wizAuthOn"></label>
<div id="wizAuthFields" class="hidden">
<label class="field"><span>User name</span><input id="wizAuthUser" maxlength="32" autocomplete="username" autocapitalize="none" spellcheck="false"></label>
<label class="field"><span id="wizAuthPassLabel">Password (8–63 characters)</span><input id="wizAuthPass" type="password" maxlength="63" autocomplete="new-password"></label>
<label class="field"><span>Confirm new password</span><input id="wizAuthConfirm" type="password" maxlength="63" autocomplete="new-password"></label>
<p class="sub" id="wizAuthPasswordHint"></p></div>
<p class="sub" id="wizAuthHint"></p><p id="wizAuthError" class="issue error" role="alert"></p>
<div class="btnbar wiz authbar"><button class="ghost left" id="wizAuthBack">Back</button><button class="ghost" id="wizAuthSkip">Skip for now</button><button class="primary" id="wizAuthNext">Continue</button></div></div>
<div id="wiz4" class="hidden"><div class="stepper-title">Step 5 of 5</div><div class="success"><div class="big"><svg width="40" height="40"><use href="#i-check"/></svg></div><h2 id="doneTitle">Ready to finish</h2><p class="sub" id="doneText">Review your sound, wiring and sign-in settings. Finish saves your choices and closes setup; reopen it under Settings → General.</p></div>
<dl class="kv" id="doneSummary"></dl>
<div class="notice hidden" id="doneWiring" style="margin-top:14px"></div>
<div class="notice hidden" id="doneAuth" style="margin-top:14px">Without sign-in, anyone on your WiFi (or near the setup hotspot) can change the chime. <a href="#" id="doneAuthLink">Configure sign-in</a>.</div>
<div class="stepact" id="dTestRow"><button id="dTest"><svg width="14" height="14"><use href="#i-play"/></svg>Test sound</button></div>
<div class="btnbar wiz"><button class="ghost left" id="dBack">Back</button><button class="primary" id="dHome">Finish</button></div></div>
</div></section>
<!-- SETTINGS -->
<section class="tab" id="tab-settings"><div class="settings" id="settingsView">
<div class="snav" id="snav" role="navigation" aria-label="Settings sections">
<button data-s="general"><svg><use href="#i-bell"/></svg><span><b>General</b><small id="sumGeneral">Name and appearance</small></span><svg class="chev"><use href="#i-chev"/></svg></button>
<button data-s="network"><svg><use href="#i-wifi"/></svg><span><b>Network &amp; time</b><small id="sumNetwork">WiFi and clock</small></span><svg class="chev"><use href="#i-chev"/></svg></button>
<button data-s="doorbell"><svg><use href="#i-link"/></svg><span><b>Doorbell</b><small id="sumDoorbell">Pairing</small></span><svg class="chev"><use href="#i-chev"/></svg></button>
<button data-s="ha"><svg><use href="#i-home"/></svg><span><b>MQTT</b><small id="sumHa">Home Assistant</small></span><svg class="chev"><use href="#i-chev"/></svg></button>
<button data-s="security"><svg><use href="#i-lock"/></svg><span><b>Sign-in</b><small id="sumSecurity">Password protection</small></span><svg class="chev"><use href="#i-chev"/></svg></button>
<button data-s="system"><svg><use href="#i-gear"/></svg><span><b>Firmware</b><small id="sumSystem">Update and restart</small></span><svg class="chev"><use href="#i-chev"/></svg></button>
<button data-s="advanced"><svg><use href="#i-chip"/></svg><span><b>Advanced</b><small>Radio and wiring</small></span><svg class="chev"><use href="#i-chev"/></svg></button>
<button data-s="debug"><svg><use href="#i-bug"/></svg><span><b>Debug</b><small id="sumDebug">Log, packets and tools</small></span><svg class="chev"><use href="#i-chev"/></svg></button>
</div>
<div class="spanel"><button class="ghost small sback" id="sBackList"><svg width="16" height="16" style="transform:scaleX(-1)"><use href="#i-chev"/></svg>Settings</button>
<!-- General -->
<div class="ssec" id="sec-general"><div class="card"><h3>Setup wizard</h3><p class="sub">Setup hides after Finish. All its choices remain available in Sound and Settings.</p><label class="switch"><div>Show Setup tab<small>Saved on this chime for every browser.</small></div><input type="checkbox" id="cfgSetupVisible"></label><div class="btnbar"><button id="cfgSetupOpen" class="hidden">Open Setup</button></div></div><div class="card"><h3>Chime name</h3><p class="sub">Shown in the Reolink app and in Home Assistant.</p><div class="inline"><input id="cfgName" maxlength="32" aria-label="Chime name"><button class="primary" id="cfgNameSave">Save</button></div></div>
<div class="card"><h3>Appearance</h3><p class="sub">Colour theme of these pages, saved in this browser.</p><div class="seg" id="themeSeg"><button type="button" data-t="system">System</button><button type="button" data-t="light">Light</button><button type="button" data-t="dark">Dark</button></div></div>
</div>
<!-- Network -->
<div class="ssec" id="sec-network"><div class="card"><h3>WiFi mode</h3><div class="seg" id="wmSeg" style="margin-top:10px"><button type="button" data-m="client">Client</button><button type="button" data-m="ap">Access point</button><button type="button" data-m="off">Off</button></div><p class="sub" id="wmHint" style="margin-top:10px"></p>
<div id="wmAp"><div class="pair2"><label class="field"><span>Hotspot name</span><input id="wmSsid" maxlength="32" autocapitalize="none" spellcheck="false"></label>
<label class="field"><span id="wmPassLabel">Hotspot password</span><input id="wmPass" type="password" maxlength="63" autocomplete="new-password"></label></div>
<label class="check hidden" id="wmForgetRow"><input type="checkbox" id="wmForget"> Remove the saved password</label></div>
<div class="btnbar"><button class="primary" id="wmSave">Save and restart</button></div></div>
<div class="card" id="cfgWifiCard"><h3>WiFi connection</h3><dl class="kv" id="cfgWifi" style="margin-top:10px"></dl><div class="btnbar"><button class="danger left" id="cfgWifiForget">Forget WiFi</button><button class="primary" id="cfgWifiChange">Change network</button></div></div>
<div class="card settings-flow hidden" id="cfgWifiTask"></div>
<div class="card"><h3>Clock</h3><p class="sub">Ring times come from an internet time server and are shown in this browser's time zone.</p><dl class="kv" id="clockKv"></dl>
<label class="field"><span>Time server (empty turns it off)</span><div class="inline"><input id="ntp" maxlength="64" autocapitalize="none" spellcheck="false"><button class="primary" id="ntpSave">Save</button></div></label></div>
</div>
<!-- Doorbell -->
<div class="ssec" id="sec-doorbell"><div class="card"><h3>Connection</h3><dl class="kv" id="cfgBell" style="margin-top:10px"></dl><div class="btnbar"><button id="cfgRejoin">Reconnect</button><button class="primary" id="cfgRepair">Pair again</button></div></div>
<div class="card settings-flow hidden" id="cfgPairTask"></div>
<div class="card dangerzone"><h3>Forget doorbell</h3><p class="sub">Removes the pairing from this chime. Delete the chime in the Reolink app too, then pair again.</p><div class="btnbar"><button class="danger" id="cfgUnpair">Forget doorbell</button></div></div>
</div>
<!-- Home Assistant -->
<div class="ssec" id="sec-ha"><div class="card"><div class="row"><h3>MQTT connection</h3><span class="spacer"></span><span class="pill" id="mqState">…</span></div><p class="sub">Reports rings and lets Home Assistant or any MQTT client read and change the sound settings and mute. WiFi, sign-in, radio and wiring can only be changed here.</p>
<label class="switch"><div>Connect to an MQTT broker<small>For example the Mosquitto add-on in Home Assistant.</small></div><input type="checkbox" id="mqOn"></label>
<div id="mqFields"><div class="pair2"><label class="field"><span>Broker address</span><input id="mqHost" maxlength="64" placeholder="e.g. homeassistant.local" autocapitalize="none" spellcheck="false"></label>
<label class="field"><span>Port</span><input id="mqPort" type="number" min="1" max="65535"></label></div>
<div class="pair2"><label class="field"><span>User name</span><input id="mqUser" maxlength="64" autocomplete="off" autocapitalize="none" spellcheck="false"></label>
<label class="field"><span id="mqPassLabel">Password</span><input id="mqPass" type="password" maxlength="64" autocomplete="new-password"></label></div>
<label class="check hidden" id="mqForgetRow"><input type="checkbox" id="mqForget"> Remove the saved password</label>
<label class="switch"><div>Home Assistant discovery<small>Adds the chime as a device with all its controls.</small></div><input type="checkbox" id="mqDisc"></label>
<details class="plain"><summary>Topics</summary><div class="pair2"><label class="field"><span>Base topic</span><input id="mqTopic" maxlength="64" autocapitalize="none" spellcheck="false"></label>
<label class="field" id="mqPrefixField"><span>Discovery prefix</span><input id="mqPrefix" maxlength="32" autocapitalize="none" spellcheck="false"></label></div></details></div>
<div class="btnbar"><button class="primary" id="mqSave">Save</button></div></div>
</div>
<!-- Sign-in -->
<div class="ssec" id="sec-security"><div class="card" id="authCard"><label class="switch"><div>Require sign-in<small id="authState"></small></div><input type="checkbox" id="authOn"></label>
<div id="authFields"><p class="sub" style="margin-top:4px">The password also protects the setup hotspot from the next restart.</p>
<label class="field"><span>User name</span><input id="authUser" maxlength="32" autocomplete="username" autocapitalize="none" spellcheck="false"></label>
<div class="pair2"><label class="field"><span id="authPassLabel">Password (8–63 characters)</span><input id="authPass" type="password" maxlength="63" autocomplete="new-password"></label>
<label class="field"><span>Repeat password</span><input id="authPass2" type="password" maxlength="63" autocomplete="new-password"></label></div></div>
<div class="btnbar"><button class="primary" id="authSave">Save</button></div></div>
<div class="card hidden" id="sessionCard"><h3>Sessions</h3><p class="sub">Sign out this browser, or every browser and device that is signed in.</p><div class="btnbar"><button id="authOutAll">Sign out everywhere</button><button id="authOut">Sign out</button></div></div>
</div>
<!-- Firmware -->
<div class="ssec" id="sec-system"><div class="card"><h3>Installed version</h3><dl class="kv" id="cfgFw" style="margin-top:10px"></dl></div>
<div class="card"><h3>Install an update</h3><p class="sub">Choose the app-only <b>firmware.bin</b>. Pairing and settings are kept; the chime restarts afterwards.</p><input type="file" id="fwFile" accept=".bin" aria-label="Firmware file">
<div class="progress hidden" id="fwProgress" style="margin-top:12px"><div id="fwBar"></div></div><div class="btnbar"><button class="primary" id="fwUpload">Install update</button></div></div>
<div class="card"><h3>Restart</h3><p class="sub">Restarts the chime; it is back after a few seconds.</p><div class="btnbar"><button id="cfgReboot">Restart chime</button></div></div>
</div>
<!-- Advanced -->
<div class="ssec" id="sec-advanced"><div class="notice">Only change these if the radio or your wiring needs it. Wrong values can stop the chime from hearing the doorbell.</div>
<div class="card"><h3>Radio</h3><p class="sub">Radio link to the doorbell. The defaults suit an EU (868 MHz) doorbell.</p>
<label class="field"><span>Band</span><select id="rfBand"><option value="868000000">868 MHz, Europe (tested)</option><option value="915000000">915 MHz, North America (untested)</option><option value="920000000">920 MHz, Asia-Pacific (untested)</option><option value="">Custom frequency</option></select></label>
<div class="notice hidden" id="rfUntested">Untested: only an EU 868 MHz doorbell has been tested. 915/920 MHz follow the doorbell firmware's other two radio setups, need a CC1101 module made for 915 MHz, and transmit at about 0 dBm.</div>
<div class="pair2"><label class="field"><span>Frequency (Hz)</span><input id="rfHz" type="number" min="863000000" max="928000000"></label>
<label class="field"><span>Trim (steps of ≈1.587 kHz)</span><input id="rfTrim" type="number" min="-128" max="127"></label></div>
<label class="switch"><div>Automatic trim<small>Measures the radio crystal's offset from the doorbell's frames and corrects the trim. Listens on a wider bandwidth until the first measurement.</small></div><input type="checkbox" id="rfAuto"></label>
<label class="field"><span>Receiver</span><select id="rfMode"><option value="0">CC1101 packet engine (recommended)</option><option value="1">ESP32 bit sampling (needs GDO0 and GDO2)</option></select></label>
<label class="field"><span>Receive bit polarity</span><select id="rfPol"><option value="0">Automatic</option><option value="1">Normal</option><option value="2">Inverted</option></select></label>
<div class="btnbar"><button class="primary" id="rfSave">Save</button></div></div>
<div class="card"><div class="row"><h3>Wiring (GPIO)</h3><span class="spacer"></span><span class="pill" id="pinState">…</span></div>
<p class="sub">Which ESP32 pin each part connects to. Changes apply immediately when saved. Use the GPIO labels on the board (G4 = GPIO4), not the header position.</p>
<p class="sub" id="pinAudioHint"></p>
<div class="scroll"><table class="pins"><thead><tr><th>Function</th><th>GPIO</th><th>Notes</th></tr></thead><tbody id="pinRows"></tbody></table></div>
<p class="sub" style="margin-top:12px">Not offered: GPIO6–11 (internal flash), 20, 24 and 28–31 (do not exist). GPIO34–39 are input-only.</p>
<div class="btnbar"><button class="ghost" id="pinDefaults">Use defaults</button><button class="ghost left" id="pinUndo">Undo</button><button class="primary" id="pinSave">Save</button></div></div>
</div>
<!-- Debug -->
<div class="ssec wide" id="sec-debug"><div class="card"><div class="row"><h3>Diagnostics</h3><span class="spacer"></span><span class="muted" id="dbgUp"></span></div><div class="grid" id="diag" style="margin-top:12px;grid-template-columns:repeat(auto-fit,minmax(140px,1fr))"></div></div>
<div class="card"><div class="row"><h3>Live log</h3><span class="spacer"></span><label class="check"><input type="checkbox" id="logFollow" checked> Follow</label><label class="check"><input type="checkbox" id="logDiag"> Status lines</label><button class="small" id="logDownload">Download</button><button class="small" id="logClear">Clear view</button></div>
<pre class="log" id="log" style="margin-top:12px"></pre></div>
<div class="card"><h3>Received packets</h3><p class="sub">Last 32 decoded frames with their receive time (this browser's time zone; uptime until the clock is set). Pairing frames are redacted.</p><div class="scroll"><table><thead><tr><th>#</th><th>Received</th><th>RSSI</th><th>Offset</th><th>CRC</th><th>Result</th><th>Bytes</th></tr></thead><tbody id="packets"></tbody></table></div></div>
<div class="card"><h3>Tools</h3><p class="sub">For first pairing, keep the chime 1–2 metres from the doorbell so automatic frequency tuning can calibrate. Keep it nearby through renaming and a few doorbell tests before moving it.</p><div class="row"><button class="small" id="dbgRejoin">Rejoin doorbell</button><button class="small" id="dbgPair">Start pairing</button><button class="small" id="dbgCancel">Cancel pairing</button><button class="small" id="dbgTest">Test gong</button></div>
<details><summary>Manual receive profile (passive, no pairing)</summary><p class="sub" style="margin-top:8px">For listening with an existing chime's key. Saving a key replaces the paired profile with a passive one.</p>
<label class="field"><span>Doorbell source ID (8 hex)</span><input id="mSource" maxlength="8" class="mono"></label><label class="field"><span>Chime destination ID (8 hex)</span><input id="mDest" maxlength="8" class="mono"></label>
<label class="field"><span>Pair/session ID (4 hex)</span><input id="mPair" maxlength="4" class="mono"></label><label class="field"><span>AES key (32 hex)</span><input id="mKey" type="password" maxlength="32" class="mono" autocomplete="new-password"></label>
<div class="btnbar"><button class="primary" id="mSave">Save</button></div></details></div>
</div>
</div></div></section>
</main><div class="toast" id="toast" role="status"></div>
<dialog class="modal" id="waitDlg" aria-labelledby="waitTitle"><form method="dialog"><div class="dlg-icon"><svg width="22" height="22"><use href="#i-restart"/></svg></div><h2 id="waitTitle"></h2>
<p id="waitText"></p><div class="spinner" role="progressbar" aria-label="Waiting for the chime"></div>
<div class="btnbar"><button type="button" class="ghost" id="waitReload">Reload now</button></div></form></dialog>
<dialog class="modal" id="dlg" aria-labelledby="dlgTitle" aria-describedby="dlgText"><form method="dialog"><div class="dlg-icon" id="dlgIcon"></div><h2 id="dlgTitle"></h2><p id="dlgText"></p>
<div class="btnbar"><button value="cancel" class="ghost left" id="dlgCancel">Cancel</button><button value="ok" class="primary" id="dlgOk">OK</button></div></form></dialog>
<script>
const $=id=>document.getElementById(id);let token='',S=null,C=null,G=null,dirty=false,logNext=0,logText='',pairStarted=0,scanTimer=0,wifiTimer=0,activeTab='home',settingsStep=-1,wizStep=0,soundPick=0,appVol=4;
const MELODIES=[['Ding dong','Classic two-tone'],['Ascending','Four rising notes'],['Double beep','Short and clear'],['Descending','Four falling notes'],['Triple beep','Three clear beeps'],['Four-note chime','A four-note bell phrase'],['Rising bell','Three spaced rising notes'],['Falling bell','Three spaced falling notes'],['Alternating','Five alternating notes'],['Attention','Two pairs of high and low notes']].map(([n,d],i)=>[`${i+1}. ${n}`,d]);
const PAIR_PHASES=[['discovering','Announcing the chime to the doorbell'],['pair acknowledgement','Exchanging the pairing key'],['joining','Joining the doorbell'],['join acknowledgement','Confirming the session'],['joined','Linked']];
// Modal dialogs: ask() resolves true for OK; notify() has a single button.
// Esc and a click on the backdrop cancel. Destructive actions focus Cancel.
function ask({title,text='',ok='OK',cancel='Cancel',danger=false,icon}){return new Promise(resolve=>{const d=$('dlg');
$('dlgTitle').textContent=title;$('dlgText').textContent=text;$('dlgOk').textContent=ok;$('dlgOk').className=danger?'dangerbtn':'primary';
$('dlgCancel').textContent=cancel||'';$('dlgCancel').classList.toggle('hidden',!cancel);const kind=icon||(danger?'warn':'info');
$('dlgIcon').className='dlg-icon'+(danger||kind==='warn'?' danger':kind==='check'?' ok':'');$('dlgIcon').innerHTML=`<svg width="22" height="22"><use href="#i-${kind}"/></svg>`;
d.returnValue='';d.onclose=()=>resolve(d.returnValue==='ok');d.onclick=e=>{if(e.target===d)d.close('')};d.showModal();$(danger&&cancel?'dlgCancel':'dlgOk').focus()})}
const notify=(title,text,opts={})=>ask({title,text,cancel:null,...opts});
function toast(t,bad){const e=$('toast');e.textContent=t;e.className='toast show'+(bad?' bad':'');clearTimeout(e.t);e.t=setTimeout(()=>e.className='toast',3200)}
async function api(path,body){const headers=body===undefined?{}:{'Content-Type':'application/json','X-CSRF-Token':token};if(!document.hidden)headers['X-WebUI-Active']='1';const r=await fetch(path,{method:body===undefined?'GET':'POST',headers,body:body===undefined?undefined:JSON.stringify(body)});let d={};try{d=await r.json()}catch(e){}if(r.status===401){location.href='/login';throw Error('Sign in required')}if(!r.ok)throw Error(d.error||r.statusText);return d}
async function act(path,body,ok){try{const r=await api(path,body===undefined?{}:body);toast(ok||r.message||'Done');refresh();return r}catch(e){toast(e.message,true)}}
const ago=ms=>{const s=Math.max(0,Math.round(ms/1000));return s<60?s+' s ago':s<3600?Math.round(s/60)+' min ago':s<86400?Math.round(s/3600)+' h ago':Math.round(s/86400)+' d ago'};
const dur=ms=>{const s=Math.floor(ms/1000),d=Math.floor(s/86400),h=Math.floor(s/3600)%24,m=Math.floor(s/60)%60;return d?`${d} d ${h} h`:h?`${h} h ${m} min`:`${m} min ${s%60} s`};
const bars=rssi=>{const n=rssi>=-55?4:rssi>=-67?3:rssi>=-75?2:rssi>=-85?1:0;return '<span class="bars">'+[6,9,12,14].map((h,i)=>`<i style="height:${h}px" class="${i<n?'on':''}"></i>`).join('')+'</span>'};
const esc=t=>String(t).replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]));
function kv(el,rows){$(el).innerHTML=rows.map(([k,v])=>`<dt>${esc(k)}</dt><dd>${esc(v)}</dd>`).join('')}
function setDot(id,cls,pulse){const e=$(id);e.setAttribute('class',(e.dataset.base||'dot')+' '+cls+(pulse?' pulse':''));const pill=e.closest('.pill.status');if(pill)pill.title=pill.textContent.trim()}
function linkState(){if(!S)return['…',''];const p=S.pairing;if(S.joined)return['Linked','ok'];if(['discovering','pair acknowledgement'].includes(p))return['Pairing…','warn'];if(['joining','join acknowledgement'].includes(p))return['Connecting…','warn'];if(S.paired)return['Not connected','bad'];return['Not paired','bad']}
// Tabs
const initialSetup=()=>!!(S&&S.setup_required);
function show(tab){if(!S)return;if(initialSetup()){if(tab!=='setup'&&activeTab==='setup')return;tab='setup'}else if(tab==='setup'&&S.setup_visible===false)tab='home';if(tab!=='settings')closeSettingsFlow();if(tab!=='setup'&&wizStep===4)wizStep=0;activeTab=tab;for(const b of $('nav').children)b.classList.toggle('on',b.dataset.tab===tab);for(const t of document.querySelectorAll('.tab'))t.classList.toggle('on',t.id==='tab-'+tab);try{localStorage.setItem('tab',tab)}catch(e){}if(tab==='setup')wizard(wizStep);if(tab==='settings')openSection(section,innerWidth>=860);scrollTo(0,0)}
for(const b of $('nav').children)b.onclick=()=>show(b.dataset.tab);
// Status
async function refresh(){try{S=await api('/api/status');token=S.csrf;C=await api('/api/chime')}catch(e){$('heroTitle').textContent='Chime unreachable';$('heroText').textContent=e.message;$('hdrLink').textContent='Offline';setDot('hdrLinkDot','bad');$('hdrLinkPill').title='Offline';return}render()}
function render(){if(!S||!C)return;const [lt,lc]=linkState(),busy=lc==='warn';$('hdrName').textContent=C.name||'Chime';document.title=C.name||'Chime';$('hdrLink').textContent=lt;setDot('hdrLinkDot',lc,busy);
const ownAp=S.wifi_mode==='ap';$('hdrWifi').textContent=S.wifi_connected?S.ssid:ownAp?S.ap_name:S.ap_active?'Setup hotspot':'Offline';setDot('hdrWifiDot',S.wifi_connected||ownAp&&S.ap_active?'ok':S.ap_active?'warn':'bad');
const incompatible=S.identity_compatible===false;$('identityNotice').classList.toggle('hidden',!incompatible);
for(const b of $('nav').children)b.classList.toggle('hidden',b.dataset.tab==='setup'?!initialSetup()&&S.setup_visible===false:initialSetup());$('cfgSetupVisible').checked=S.setup_visible!==false;$('cfgSetupOpen').classList.toggle('hidden',S.setup_visible===false);
$('wizAdvancedRow').classList.toggle('hidden',initialSetup());if(initialSetup()&&activeTab!=='setup'){wizStep=S.wifi_configured?1:0;show('setup')}document.body.classList.remove('ui-loading');
const needsSetup=!S.paired||incompatible;$('heroSetup').textContent=S.setup_visible===false?'Pair doorbell':'Start setup';$('heroSetup').classList.toggle('hidden',!needsSetup);
$('heroTitle').textContent=incompatible?'Pair again to enable ringing':S.joined?'Ready':needsSetup?'Let’s set up your chime':lt;
$('heroText').textContent=incompatible?'Remove the old chime in the Reolink app, then pair again under Settings → Doorbell.':S.joined?`“${C.name}” is listening. ${soundText()}.`:needsSetup?'Connect it to WiFi and pair it with your Reolink doorbell in a few steps.':busy?'Talking to the doorbell…':'The chime is paired but not connected. It retries after a restart, or reconnect it in Settings.';
$('userWrap').classList.toggle('hidden',!S.auth_enabled);$('userName').textContent=S.auth_user||'';
const M=S.mute||{},muted=!!M.active;$('hdrMute').classList.toggle('hidden',!muted||initialSetup());$('heroMuteText').textContent=muted?'Unmute':'Mute';
if(muted&&S.joined&&!incompatible){$('heroTitle').textContent='Muted';$('heroText').textContent=`Doorbell rings are logged but make no sound ${muteUntilText(M)}. Play chime still plays.`}
if(!S.radio_ok){$('heroTitle').textContent='Radio error';$('heroText').textContent=`${S.radio_error||'The CC1101 radio does not respond'}. Check its wiring and power; details in Settings → Debug.`}
$('heroIcon').style.color=!S.radio_ok?'var(--bad)':muted?'var(--warn)':S.joined?'var(--ok)':needsSetup?'var(--accent)':'var(--warn)';$('heroIcon').innerHTML=`<svg><use href="#i-${muted?'belloff':'bell'}"/></svg>`;
if(S.ring_seq!==ringSeq)loadRings();else drawRings();
kv('cfgWifi',[['Status',S.wifi_connected?'Connected':S.wifi_configured?'Not connected':'Not set up'],['Network',S.wifi_configured?S.ssid||'—':'—'],['Signal',S.wifi_connected?`${S.wifi_rssi} dBm (${S.wifi_rssi>=-55?'excellent':S.wifi_rssi>=-67?'good':S.wifi_rssi>=-75?'fair':'weak'})`:'—'],['Address',S.wifi_connected?S.ip:'—'],['Name',S.hostname],[ownAp?'Access point':'Setup hotspot',S.ap_active?`${S.ap_name} (192.168.4.1)`:'Off']]);
kv('cfgBell',[['Status',lt],['Doorbell ID',S.paired?S.doorbell_id:'—'],['Chime ID',S.node_id],['Session',S.joined?C.session:'—'],['Requests',C.requests]]);
kv('cfgFw',[['Version',S.version||'unknown'],['Build date',S.firmware],['Uptime',dur(S.uptime_ms)],['Free memory',Math.round(S.free_heap/1024)+' KB']]);
kv('clockKv',[['Status',S.time_synced?'Synchronised':'Not synchronised yet'],['Chime time',S.epoch?new Date(S.epoch*1000).toLocaleString():'—']]);$('mqState').innerHTML=mqPill(S.mqtt);
if(document.activeElement!==$('cfgName')&&!$('cfgName').dataset.edited)$('cfgName').value=C.name;
summaries();if(!dirty)loadSound();if(activeTab==='setup')wizard(wizStep,true);if(settingsStep===0)wifiView();if(settingsStep===1)pairView();if(debugOpen())renderDiag()}
// Mute
function muteUntilText(M){if(M.permanent||!M.remaining_s)return'until you unmute';const end=new Date(Date.now()+M.remaining_s*1000);return`until ${end.toLocaleTimeString([],{hour:'2-digit',minute:'2-digit'})} (${M.remaining_s<3600?Math.ceil(M.remaining_s/60)+' min':dur(M.remaining_s*1000)} left)`}
$('heroMute').onclick=()=>{if(S&&S.mute&&S.mute.active){$('muteMenu').classList.add('hidden');act('/api/mute',{mute:false},'Chime unmuted')}else $('muteMenu').classList.toggle('hidden')};
for(const b of $('muteMenu').querySelectorAll('[data-m]'))b.onclick=()=>{const m=+b.dataset.m;$('muteMenu').classList.add('hidden');act('/api/mute',m?{mute_minutes:m}:{mute:true},m?`Muted for ${b.textContent}`:'Muted until you unmute')};
$('hdrMute').onclick=()=>show('home');
// Sign out (header button, shown only while sign-in is on)
const signOut=async()=>{try{await api('/api/logout',{})}catch(e){}location.href='/login'};$('umOut').onclick=signOut;
// Restart spinner: polls until the chime answers again, then reloads.
// expect: firmware version that should be running afterwards (update only).
let restarting=false;
function waitForRestart(title,expect){restarting=true;const d=$('waitDlg'),total=30,t0=Date.now();$('waitTitle').textContent=title;d.oncancel=e=>e.preventDefault();
try{if(expect!==undefined)sessionStorage.setItem('expectVersion',expect||'?')}catch(e){}
$('waitReload').onclick=()=>location.reload();d.showModal();
const step=async()=>{const el=(Date.now()-t0)/1000,left=el<total;
$('waitText').textContent=left?'The chime is restarting. This page reloads automatically when it is back.':'The chime has not answered yet. It keeps trying; check its power or WiFi, or reload.';
if(el>=4){try{const r=await fetch('/api/status',{cache:'no-store'});if(r.ok||r.status===401){location.reload();return}}catch(e){}}setTimeout(step,1000)};step()}
async function restartChime(title){try{await api('/api/reboot',{});waitForRestart(title||'Restarting')}catch(e){toast(e.message,true)}}
// After an update: confirm which version came back.
function checkUpdatedVersion(){let want=null;try{want=sessionStorage.getItem('expectVersion');sessionStorage.removeItem('expectVersion')}catch(e){}if(!want||!S)return;
if(want==='?'||S.version===want)notify('Update complete',`The chime runs version ${S.version||'unknown'}.`,{icon:'check'});else notify('Update not applied',`The chime still runs version ${S.version||'unknown'}, not ${want}. Try installing again.`,{icon:'warn'})}
// Reads the version marker from a chosen firmware file (null if absent; false if not an ESP32 image).
async function fileVersion(f){const b=new Uint8Array(await f.arrayBuffer());if(b[0]!==0xE9)return false;const t=new TextDecoder('latin1').decode(b),key='REOLINK_CHIME'+'_VERSION=';// Split: this page is inside the image too.
for(let i=t.indexOf(key);i>=0;i=t.indexOf(key,i+1)){const m=/^[0-9][0-9A-Za-z.+-]{0,31}\0/.exec(t.slice(i+key.length,i+key.length+40));if(m)return m[0].slice(0,-1)}return null}
// Appearance (per browser)
function setTheme(t){try{if(t==='system')localStorage.removeItem('theme');else localStorage.setItem('theme',t)}catch(e){}if(t==='system')delete document.documentElement.dataset.theme;else document.documentElement.dataset.theme=t;
for(const b of $('themeSeg').children)b.classList.toggle('on',b.dataset.t===t);const dark=t==='dark'||(t==='system'&&!matchMedia('(prefers-color-scheme: light)').matches);document.querySelector('meta[name=theme-color]').content=dark?'#0f1720':'#f3f6f9'}
for(const b of $('themeSeg').children)b.onclick=()=>setTheme(b.dataset.t);
setTheme(document.documentElement.dataset.theme||'system');
// Sound
const TYPES=[['Audio','Plays melodies through an external amplifier'],['Gong','Rings a wired gong (e.g. 8 V AC) via the relay'],['None','Silent: rings are only logged and reported']];
const OUTPUTS=[['PWM','Square-wave tones on the Audio output GPIO'],['DAC','Sine-wave audio on GPIO25 or GPIO26']];
const TYPE_ORDER=[1,0,2],isAudio=t=>t===0;
function typeCards(el,selected,onPick){cards(el,TYPE_ORDER.map(i=>TYPES[i]),TYPE_ORDER.indexOf(selected),i=>onPick(TYPE_ORDER[i]))}
function typeAvailable(type){if(!X)return true;const ok=type===1?X.relay_available:type===0?X.pwm_available:true;if(!ok)toast(type===1?'Assign a relay GPIO under Settings → Advanced → Wiring first.':'Assign an Audio output GPIO under Settings → Advanced → Wiring first.',true);return ok}
function audioAvailable(output){if(!X)return true;const ok=output===1?X.dac_available:X.pwm_available;if(!ok)toast(output===1?'DAC requires Audio output GPIO25 or GPIO26. Set it under Settings → Advanced → Wiring first.':'Assign an Audio output GPIO under Settings → Advanced → Wiring first.',true);return ok}
function audioOutputCards(el,selected,onPick,allowUnwired=false){cards(el,OUTPUTS,selected,i=>{if(i===selected)return;if(!allowUnwired&&!audioAvailable(i))return false;onPick(i)})}
const soundText=()=>!X?'Rings follow the doorbell ringtone':X.type===2?'Rings are silent (mode None) and only logged':X.type===1?'Gong patterns follow the doorbell ringtone':'Audio melodies follow the doorbell ringtone';
const PATTERNS=[['Ding-dong','One strike'],['Double','Two strikes'],['Triple','Three strikes'],['Long ring','One long strike, for bells'],['Insistent','Five quick strikes'],['Slow double','Two strikes with a longer gap'],['Slow triple','Three strikes with longer gaps'],['Four strikes','Four evenly spaced strikes'],['Two pairs','Two pairs of strikes'],['Three pairs','Three pairs of strikes']].map(([n,d],i)=>[`${i+1}. ${n}`,d]);
let X=null,gongType=1,audioPick=0,patternPick=0;
// Lists with previews (melodies, patterns) become a dropdown with a play button on phones.
function cards(el,list,selected,onPick,onPlay){const box=$(el);box.classList.toggle('picklist',!!onPlay);
box.innerHTML=(onPlay?`<div class="pickm"><div class="row"><select aria-label="Choose">${list.map(([n],i)=>`<option value="${i}"${i===selected?' selected':''}>${n}</option>`).join('')}</select><button class="ghost" data-mplay aria-label="Preview"><svg width="14" height="14"><use href="#i-play"/></svg></button></div><small></small></div>`:'')+list.map(([n,d],i)=>`<div class="choice${i===selected?' on':''}" data-i="${i}"><div><b>${n}</b><small>${d}</small></div><span class="spacer"></span>${onPlay?`<button class="small ghost" data-play="${i}" aria-label="Preview ${n}"><svg width="14" height="14"><use href="#i-play"/></svg></button>`:''}</div>`).join('');
const choices=box.querySelectorAll('.choice'),pick=box.querySelector('.pickm select'),mark=i=>{for(const o of choices)o.classList.toggle('on',+o.dataset.i===i);if(pick){pick.value=i;box.querySelector('.pickm small').textContent=list[i]?list[i][1]:''}};
for(const c of choices)c.onclick=e=>{const p=e.target.closest('[data-play]');if(p){onPlay(+p.dataset.play);return}if(onPick(+c.dataset.i)===false)return;mark(+c.dataset.i)};
if(pick){mark(selected);let last=selected;pick.onchange=()=>{const i=+pick.value;if(onPick(i)===false){pick.value=last;return}last=i;mark(i)};box.querySelector('[data-mplay]').onclick=()=>onPlay(+pick.value)}}
const playMelody=(i,output=audioPick)=>act('/api/gong/test',{type:0,audio_output:output,melody:i,volume:Math.floor(+$('level').value*appVol/4),extra_relay:$('relay').checked,extra_relay_ms:+$('relayMs').value},'Playing preview');
const playPattern=i=>act('/api/gong/test',{type:1,pattern:i,pulse_ms:+$('pulse').value,gap_ms:+$('gap').value},'Ringing the gong');
function melodyCards(el,selected,onPick,output=audioPick){cards(el,MELODIES,selected,onPick,i=>playMelody(i,output))}
function showType(){$('audioCards').classList.toggle('hidden',!isAudio(gongType));$('relayCards').classList.toggle('hidden',gongType!==1);$('noneCards').classList.toggle('hidden',gongType!==2);$('noRelay').classList.toggle('hidden',!X||X.relay_available);drawAudioOutput()}
function loadSound(){if(!G||!C||!X)return;soundPick=G.melody;appVol=C.volume;gongType=X.type;audioPick=X.audio_output;patternPick=X.pattern;$('level').value=G.volume;$('levelOut').textContent=G.volume;$('led').checked=!!C.led;$('relay').checked=G.relay;$('relayMs').value=G.relay_ms;$('relayMsOut').textContent=G.relay_ms;$('relayMsField').classList.toggle('hidden',!G.relay);
$('pulse').value=X.pulse_ms;$('pulseOut').textContent=X.pulse_ms;$('gap').value=X.gap_ms;$('gapOut').textContent=X.gap_ms;
typeCards('gongTypes',gongType,i=>{if(i===gongType)return;if(!typeAvailable(i))return false;if(i===1&&isAudio(gongType))patternPick=soundPick;else if(isAudio(i)&&gongType===1)soundPick=patternPick;gongType=i;dirty=true;drawPicks();showType()});drawPicks();segVol();showType()}
function drawAudioOutput(){audioOutputCards('audioOutputs',audioPick,i=>{audioPick=i;dirty=true;drawPicks();drawAudioOutput()});$('audioOutputHint').textContent=audioPick===1?'Uses the shared Audio output GPIO (25 or 26 only). Move or disable the Relay pin if it uses the same GPIO.':'Uses the shared Audio output GPIO. Configure it under Settings → Advanced → Wiring.'}
function drawPicks(){melodyCards('melodies',soundPick,i=>{soundPick=i;dirty=true});cards('patterns',PATTERNS,patternPick,i=>{patternPick=i;dirty=true},playPattern)}
function segVol(){$('appVolume').innerHTML=[0,1,2,3,4].map(v=>`<button type="button" class="${v===appVol?'on':''}">${v===0?'Mute':v}</button>`).join('');[...$('appVolume').children].forEach((b,v)=>b.onclick=()=>{appVol=v;dirty=true;segVol()})}
for(const [id,out] of [['level','levelOut'],['relayMs','relayMsOut'],['pulse','pulseOut'],['gap','gapOut']])$(id).oninput=()=>{$(out).textContent=$(id).value;dirty=true};
$('relay').onchange=()=>{$('relayMsField').classList.toggle('hidden',!$('relay').checked);dirty=true};$('led').onchange=()=>dirty=true;
$('previewVolume').onclick=()=>playMelody(soundPick);
$('soundReset').onclick=()=>{dirty=false;loadSound()};
$('soundSave').onclick=async()=>{try{await api('/api/settings',{melody:soundPick,volume:+$('level').value,relay:$('relay').checked,relay_ms:+$('relayMs').value});await api('/api/gong',{type:gongType,audio_output:audioPick,pattern:patternPick,pulse_ms:+$('pulse').value,gap_ms:+$('gap').value});await api('/api/chime',{volume:appVol,led:$('led').checked});dirty=false;await loadSettings();await refresh();toast('Sound settings saved')}catch(e){toast(e.message,true)}};
$('heroTest').onclick=()=>act('/api/gong/test',{},'Playing chime');$('heroSetup').onclick=()=>{if(S&&S.setup_visible===false){show('settings');openSettingsFlow(1)}else{wizStep=S&&S.wifi_configured?1:0;show('setup')}};
// Settings
async function loadSettings(){try{G=await api('/api/settings');X=await api('/api/gong');A=await api('/api/auth');I=await api('/api/integrations');W=await api('/api/wifi');if(!authDirty)fillAuth();if(!intDirty)fillIntegrations();if(!wmDirty)fillWifiMode();if(!pinsDirty&&(!P||activeTab==='settings'))await loadPins();$('rfHz').value=G.frequency_hz;rfBandSync();pBandSync();$('rfTrim').value=G.trim;$('rfAuto').checked=G.auto_trim;$('rfTrim').disabled=G.auto_trim;$('rfPol').value=G.polarity;$('rfMode').value=G.rx_mode;for(const [id,k] of [['mSource','source'],['mDest','destination'],['mPair','pair']])$(id).value=G[k];if(!dirty)loadSound()}catch(e){toast(e.message,true)}}
$('cfgName').oninput=()=>$('cfgName').dataset.edited=1;
$('cfgNameSave').onclick=async()=>{const n=$('cfgName').value.trim();if(!n)return toast('Enter a name',true);await act('/api/chime',{name:n},'Name saved. The Reolink app shows it after its next refresh.');delete $('cfgName').dataset.edited};
$('cfgSetupVisible').onchange=async()=>{const input=$('cfgSetupVisible');input.disabled=true;try{await api('/api/setup',{visible:input.checked});await refresh();toast(input.checked?'Setup tab shown':'Setup tab hidden')}catch(e){input.checked=S.setup_visible!==false;toast(e.message,true)}input.disabled=false};
$('cfgSetupOpen').onclick=()=>{wizStep=0;show('setup')};
$('cfgWifiChange').onclick=()=>openSettingsFlow(0);
$('cfgWifiForget').onclick=async()=>{if(!await ask({title:'Forget WiFi?',text:`The chime forgets ${S&&S.ssid?'“'+S.ssid+'”':'the network'}, restarts and opens its setup hotspot. You then connect to the hotspot to set up WiFi again.`,ok:'Forget WiFi',danger:true}))return;try{await api('/api/settings',{ssid:'',open_wifi:true});await api('/api/reboot',{});toast('Restarting into the setup hotspot…')}catch(e){toast(e.message,true)}};
$('cfgRepair').onclick=()=>openSettingsFlow(1);$('cfgRejoin').onclick=()=>act('/api/join/start');$('dbgRejoin').onclick=()=>act('/api/join/start');
$('cfgUnpair').onclick=async()=>{if(!await ask({title:'Forget the doorbell?',text:'The chime stops ringing until it is paired again. Also delete the chime in the Reolink app, then pair again under Settings → Doorbell.',ok:'Forget doorbell',danger:true}))return;await act('/api/settings',{clear_profile:true},'Doorbell forgotten')};
// Settings sections: list + one section (phones show the list first)
let section='general';try{section=localStorage.getItem('section')||'general'}catch(e){}
function openSection(name,detail=true){if(!$('sec-'+name))name='general';if(settingsStep>=0&&name!==(settingsStep===0?'network':'doorbell'))closeSettingsFlow();section=name;try{localStorage.setItem('section',name)}catch(e){}
for(const b of $('snav').children)b.classList.toggle('on',b.dataset.s===name);for(const sct of document.querySelectorAll('.ssec'))sct.classList.toggle('on',sct.id==='sec-'+name);
$('settingsView').classList.toggle('detail',detail);if(detail&&innerWidth<860)scrollTo(0,0);if(debugOpen()){renderDiag();pollDebug(true)}}
// Debug is the last settings section; it polls the log and packets only while visible.
const debugOpen=()=>activeTab==='settings'&&section==='debug'&&(innerWidth>=860||$('settingsView').classList.contains('detail'));
for(const b of $('snav').children)b.onclick=()=>openSection(b.dataset.s);$('sBackList').onclick=()=>{closeSettingsFlow();$('settingsView').classList.remove('detail');scrollTo(0,0)};
function summaries(){if(!S||!C)return;const [lt]=linkState();$('sumGeneral').textContent=C.name;
$('sumNetwork').textContent=S.wifi_connected?`${S.ssid} · ${S.ip}`:S.wifi_mode==='ap'?'Access point · '+S.ap_name:S.ap_active?'Setup hotspot':'Not connected';
$('sumDoorbell').textContent=lt+(S.paired?' · '+S.doorbell_id:'');$('sumHa').textContent=S.mqtt==='off'?'Off':S.mqtt[0].toUpperCase()+S.mqtt.slice(1);
$('sumSecurity').textContent=S.auth_enabled?'On':'Off · anyone on your network';$('sumSystem').textContent='Version '+(S.version||'unknown');
$('sumDebug').textContent=`Radio ${S.radio_ok?'OK':'error'} · ${S.packets} packets`}
// Sign-in
let A=null,I=null,W=null,authDirty=false,intDirty=false,wmDirty=false,wmPick='client';
function fillAuth(){if(!A)return;$('authOn').checked=A.enabled;$('authUser').value=A.username;$('authPass').value='';$('authPass2').value='';$('authPassLabel').textContent=A.password_set?'New password (leave empty to keep the current one)':'Password (8–63 characters)';
$('authState').textContent=A.enabled?'On: this page asks for a user name and password':'Off: anyone on your network can open this page';$('sessionCard').classList.toggle('hidden',!A.enabled);authFieldsView()}
const authFieldsView=()=>$('authFields').classList.toggle('hidden',!$('authOn').checked);$('authOn').addEventListener('change',authFieldsView);
for(const id of ['authOn','authUser','authPass','authPass2'])$(id).addEventListener('input',()=>authDirty=true);
$('authSave').onclick=async()=>{const on=$('authOn').checked,user=$('authUser').value.trim(),p=$('authPass').value;
if(!user)return toast('Enter a user name',true);if(p!==$('authPass2').value)return toast('The passwords do not match',true);if(p&&p.length<8)return toast('Use at least 8 characters',true);if(on&&!p&&!A.password_set)return toast('Choose a password to turn sign-in on',true);
const body={enabled:on,username:user};if(p)body.password=p;try{const r=await api('/api/auth',body);authDirty=false;A=await api('/api/auth');fillAuth();toast(r.message);refresh()}catch(e){toast(e.message,true)}};
$('authOut').onclick=()=>signOut();
$('authOutAll').onclick=async()=>{if(await ask({title:'Sign out everywhere?',text:'All other browsers and devices must sign in again. This browser stays signed in.',ok:'Sign out everywhere'}))act('/api/logout/all')};
// WiFi mode
const WM_HINTS={client:'Joins your home WiFi. If it cannot connect for 20 seconds it also opens the setup hotspot.',ap:'The chime runs its own WiFi network; phones and tablets join it directly. No internet: the clock and MQTT stay off.',off:'The WiFi radio stays off and this page is unreachable. Doorbell rings still work.'};
function fillWifiMode(){if(!W)return;wmPick=W.mode;$('wmSsid').value=W.ap_ssid;$('wmSsid').placeholder=W.default_ap_ssid;$('wmPass').value='';$('wmForget').checked=false;$('wmForgetRow').classList.toggle('hidden',!W.ap_password_set);
$('wmPassLabel').textContent=W.ap_password_set?'Hotspot password (leave empty to keep it)':'Hotspot password (8–63 characters)';$('wmPass').placeholder=W.ap_password_set?'':A&&A.enabled?'Empty: the sign-in password':'Empty: open network';wifiModeView()}
function wifiModeView(){for(const b of $('wmSeg').children)b.classList.toggle('on',b.dataset.m===wmPick);$('wmAp').classList.toggle('hidden',wmPick!=='ap');
$('cfgWifiCard').classList.toggle('hidden',wmPick!=='client');if(wmPick!=='client'&&settingsStep===0)closeSettingsFlow();
$('wmHint').textContent=WM_HINTS[wmPick]+(wmPick==='off'?` To turn it back on, hold the chime's button for 8 seconds${W&&!W.button?' (no button GPIO is set)':''} or send w on the USB serial console.`:'')}
for(const b of $('wmSeg').children)b.onclick=()=>{wmPick=b.dataset.m;wmDirty=true;wifiModeView()};
for(const id of ['wmSsid','wmPass','wmForget'])$(id).addEventListener('input',()=>wmDirty=true);
$('wmSave').onclick=async()=>{const p=$('wmPass').value;if(p&&p.length<8)return toast('Use at least 8 characters',true);
const body={mode:wmPick,ap_ssid:$('wmSsid').value.trim()};if(p)body.ap_password=p;if($('wmForget').checked)body.clear_ap_password=true;
const name=body.ap_ssid||W.default_ap_ssid,open=!p&&(!W.ap_password_set||body.clear_ap_password)&&!(A&&A.enabled);
if(wmPick==='ap'&&open&&!await ask({title:'Open hotspot?',text:'Without a password (and with sign-in off) anyone nearby can join the hotspot and change the chime.',ok:'Continue',danger:true}))return;
const text=wmPick==='off'?`The chime restarts with WiFi off and this page stops working. Turn WiFi back on by holding the button for 8 seconds${W.button?'':' (no button GPIO is set!)'} or by sending w on the USB serial console.`:wmPick==='ap'?`The chime restarts as access point “${name}”. Join that network, then open http://192.168.4.1/.`:'The chime restarts and joins your home WiFi.';
if(!await ask({title:'Save and restart?',text,ok:'Restart',danger:wmPick==='off'}))return;
try{await api('/api/wifi',body);wmDirty=false;await api('/api/reboot',{});if(wmPick==='client'&&S.wifi_mode==='client')waitForRestart('Restarting');else notify('Restarting',text,{icon:'check'})}catch(e){toast(e.message,true)}};
// MQTT and clock
const mqPill=st=>`<span class="dot ${st==='connected'?'ok':st==='off'?'':st==='connecting'?'warn':'bad'}"></span>${esc(st==='off'?'Off':st[0].toUpperCase()+st.slice(1))}`;
function fillIntegrations(){if(!I)return;$('mqOn').checked=I.enabled;$('mqHost').value=I.host;$('mqPort').value=I.port;$('mqUser').value=I.username;$('mqPass').value='';$('mqForget').checked=false;$('mqForgetRow').classList.toggle('hidden',!I.password_set);
$('mqPassLabel').textContent=I.password_set?'Password (leave empty to keep the saved one)':'Password';$('mqTopic').value=I.topic;$('mqTopic').placeholder=I.default_topic;$('mqDisc').checked=I.discovery;$('mqPrefix').value=I.prefix;$('mqPrefixField').classList.toggle('hidden',!I.discovery);$('ntp').value=I.ntp;$('mqState').innerHTML=mqPill(I.state);mqFieldsView()}
const mqFieldsView=()=>$('mqFields').classList.toggle('hidden',!$('mqOn').checked);$('mqOn').addEventListener('change',mqFieldsView);
for(const id of ['mqOn','mqHost','mqPort','mqUser','mqPass','mqForget','mqTopic','mqDisc','mqPrefix','ntp'])$(id).addEventListener('input',()=>{intDirty=true;$('mqPrefixField').classList.toggle('hidden',!$('mqDisc').checked)});
async function saveIntegrations(body){try{const r=await api('/api/integrations',body);intDirty=false;I=await api('/api/integrations');fillIntegrations();toast(r.message)}catch(e){toast(e.message,true)}}
$('mqSave').onclick=()=>{const b={enabled:$('mqOn').checked,host:$('mqHost').value.trim(),port:+$('mqPort').value,username:$('mqUser').value,topic:$('mqTopic').value.trim(),discovery:$('mqDisc').checked,prefix:$('mqPrefix').value.trim()};
if(b.enabled&&!b.host)return toast('Enter the broker address',true);if($('mqPass').value)b.password=$('mqPass').value;if($('mqForget').checked)b.clear_password=true;saveIntegrations(b)};
$('ntpSave').onclick=()=>saveIntegrations({ntp:$('ntp').value.trim()});
// Ring log
let R=null,ringSeq=-1,ringFilter='all';try{ringFilter=localStorage.getItem('ringFilter')||'all'}catch(e){}
const SOURCES={doorbell:'Doorbell',web:'Played from this page',button:'Button test',mqtt:'Played via MQTT',serial:'Serial console test'};
const OUTCOMES={played:'Played',muted:'Volume 0',silent:'Silent',busy:'Busy',chime_muted:'Muted'};
async function loadRings(){try{const r=await api('/api/rings');R=r;ringSeq=r.last_id;drawRings()}catch(e){}}
function ringTime(r){if(r.epoch){const d=new Date(r.epoch*1000),now=S&&S.epoch?S.epoch*1000:Date.now(),today=d.toDateString()===new Date(now).toDateString();return[(today?'':d.toLocaleDateString()+' ')+d.toLocaleTimeString([],{hour:'2-digit',minute:'2-digit',second:'2-digit'}),ago(now-d.getTime())]}
if(r.previous_boot)return['Before restart','Time unknown (clock was not set)'];return[ago((S?S.uptime_ms:R.uptime_ms)-r.ms),'Clock not set; time since the ring']}
function ringSound(r){if(r.type===2)return'Silent (mode None)';const s=r.type===1?'Gong · '+(PATTERNS[r.sound]||PATTERNS[0])[0]:'Audio · '+(OUTPUTS[r.audio_output||0]||OUTPUTS[0])[0]+' · '+(MELODIES[r.sound]||MELODIES[0])[0];return s+(r.volume===0?' · muted':r.volume<4?` · volume ${r.volume}/4`:'')}
function drawRings(){for(const b of $('ringFilter').children)b.classList.toggle('on',b.dataset.f===ringFilter);if(!R)return;
const door=R.rings.filter(r=>r.source==='doorbell'),list=ringFilter==='all'?R.rings:door,last=door[0];
$('ringSummary').textContent=last?`Last doorbell ring ${ringTime(last)[last.epoch?1:0]} · ${R.rings_since_start} since start-up`:'No doorbell rings recorded yet';
$('ringClear').classList.toggle('hidden',!R.rings.length);
$('rings').innerHTML=list.length?list.map(r=>{const d=r.source==='doorbell',t=ringTime(r);return `<li><span class="ring-ic${d?' door':''}"><svg width="18" height="18"><use href="#i-${d?'bell':'play'}"/></svg></span><div class="ring-main"><b>${esc(SOURCES[r.source]||r.source)}${r.preview?' · preview':''}</b><small>${esc(ringSound(r))}${d?` · doorbell tone ${r.tone} · ${r.rssi_dbm} dBm`:''}</small></div><div class="ring-side" title="${esc(t[1])}">${esc(t[0])}<br><span class="badge ${r.outcome}">${OUTCOMES[r.outcome]||r.outcome}</span></div></li>`}).join(''):`<li class="empty">${ringFilter==='all'?'Nothing played yet. Rings and tests appear here.':'No doorbell rings yet.'}</li>`}
for(const b of $('ringFilter').children)b.onclick=()=>{ringFilter=b.dataset.f;try{localStorage.setItem('ringFilter',ringFilter)}catch(e){}drawRings()};
$('ringClear').onclick=async()=>{if(!await ask({title:'Clear the ring log?',text:'All entries are deleted. This cannot be undone.',ok:'Clear',danger:true}))return;await act('/api/rings/clear',{},'Ring log cleared');loadRings()};
$('cfgReboot').onclick=async()=>{if(await ask({title:'Restart the chime?',text:'It is back after a few seconds and reconnects to the doorbell by itself.',ok:'Restart'}))restartChime()};
$('rfAuto').onchange=()=>{$('rfTrim').disabled=$('rfAuto').checked};
function rfBandSync(){const hz=+$('rfHz').value,o=[...$('rfBand').options].find(x=>x.value&&+x.value===hz);$('rfBand').value=o?o.value:'';$('rfUntested').classList.toggle('hidden',!(hz>=902e6&&hz<=928e6))}
$('rfBand').onchange=()=>{if($('rfBand').value)$('rfHz').value=$('rfBand').value;rfBandSync()};$('rfHz').oninput=rfBandSync;
$('rfSave').onclick=()=>act('/api/settings',{frequency_hz:+$('rfHz').value,trim:+$('rfTrim').value,polarity:+$('rfPol').value,auto_trim:$('rfAuto').checked,rx_mode:+$('rfMode').value},'Radio settings saved');
$('fwUpload').onclick=async()=>{const f=$('fwFile').files[0];if(!f)return notify('No file chosen','Choose the firmware.bin file first.',{icon:'warn'});
let v;try{v=await fileVersion(f)}catch(e){return notify('File not readable',e.message,{icon:'warn'})}
if(v===false)return notify('Not a firmware file',`${f.name} is not an ESP32 firmware image. Choose firmware/dist/firmware.bin.`,{icon:'warn'});
const cur=S&&S.version?S.version:'unknown',ok=v?await ask({title:`Install version ${v}?`,text:`Installed now: ${cur}. ${v===cur?'This is the same version. ':''}The chime restarts afterwards; pairing and settings are kept.`,ok:'Install'}):
await ask({title:'Unknown firmware version',text:`${f.name} contains no chime version information (older build, or firmware for something else). Installed now: ${cur}. Only install firmware built for this chime.`,ok:'Install anyway',danger:true});
if(!ok)return;const x=new XMLHttpRequest(),d=new FormData();d.append('firmware',f);$('fwProgress').classList.remove('hidden');$('fwUpload').disabled=true;
x.upload.onprogress=e=>{if(e.lengthComputable)$('fwBar').style.width=(100*e.loaded/e.total)+'%'};x.onload=()=>{if(x.status===401){location.href='/login';return}let m='';try{const j=JSON.parse(x.responseText);m=j.message||j.error}catch(e){}$('fwUpload').disabled=false;$('fwProgress').classList.add('hidden');$('fwBar').style.width='0';
if(x.status==200)waitForRestart(v?`Restarting with version ${v}`:'Restarting with the update',v||'');else notify('Update failed',m||'The chime rejected the file. The previous firmware keeps running.',{icon:'warn'})};
x.onerror=()=>{$('fwUpload').disabled=false;$('fwProgress').classList.add('hidden');notify('Upload failed','The connection to the chime was lost during the upload. The previous firmware keeps running; try again.',{icon:'warn'})};x.open('POST','/api/update');x.setRequestHeader('X-CSRF-Token',token);x.send(d)};
// Wiring (GPIO)
const PIN_ROLES=[['sck','Radio SCK','CC1101 SPI clock (output).'],['miso','Radio MISO / SO','CC1101 SPI data to the ESP32; also signals chip-ready (input).'],['mosi','Radio MOSI / SI','CC1101 SPI data from the ESP32 (output).'],['cs','Radio CSN','CC1101 chip select (output). Optional 10 kΩ pull-up to 3.3 V.'],['clock','Radio GDO0','Recovered clock for ESP32 bit-sampling RX (input); unused by the default packet-engine receiver.'],['data','Radio GDO2','Recovered data for ESP32 bit-sampling RX (input); unused by the default packet-engine receiver.'],['audio','Audio output','Shared amplifier pin for Audio mode. Choose PWM or DAC under Sound. PWM accepts any output GPIO; DAC requires GPIO25 or GPIO26.',1],['relay','Relay','Relay module input for Gong mode or the optional pulse in Audio mode (output). Choose active low for optocoupler relay modules that switch on when IN is LOW.',1],['led','Status LED','Blinks while listening (output).',1],['button','Button','Short press: test · 2 s: pairing · 8 s: WiFi reset / WiFi on · 15 s: sign-in off (input, active low).',1]];
const GPIOS=[0,1,2,3,4,5,12,13,14,15,16,17,18,19,21,22,23,25,26,27,32,33,34,35,36,37,38,39];let P=null,pinsDirty=false,pinTimer=0,pinContext='';
const pinValue=()=>{const m={};for(const [k] of PIN_ROLES)m[k]=+$('pin-'+k).value;m.relay_active_low=$('pin-relayLow').value==='1';return m};
// Periodic refreshes only redraw when the wiring changed, so the table does not flash empty notes.
async function loadPins(){try{$('pinAudioHint').textContent=X&&X.type===0?'Audio mode uses '+OUTPUTS[X.audio_output][0]+'. '+(X.audio_output===1?'Assign Audio output to GPIO25 or GPIO26; it cannot share the Relay GPIO.':'Assign Audio output to an available output GPIO.'):'Gong uses the Relay GPIO. The Audio output GPIO is reserved for Audio mode, with PWM or DAC selected under Sound.';const n=await api('/api/pins');const context=X?X.type+':'+X.audio_output:'';if(P&&$('pin-sck')&&JSON.stringify(n)===JSON.stringify(P)&&context===pinContext)return;pinContext=context;P=n;drawPins(P.saved);checkPins()}catch(e){}}
const pinOptions=(role,map)=>role==='audio'&&X&&X.type===0&&X.audio_output===1?[...new Set([25,26,...(map.audio>=0?[map.audio]:[])])]:GPIOS;
function drawPins(map){const old={};for(const [k] of PIN_ROLES)old[k]=$('iss-'+k)?$('iss-'+k).innerHTML:'';$('pinRows').innerHTML=PIN_ROLES.map(([k,label,desc,opt])=>`<tr id="row-${k}"><td><b>${label}</b><div class="muted" style="font-size:.85rem">${desc}</div></td><td><select id="pin-${k}">${opt?'<option value="-1">Not connected</option>':''}${pinOptions(k,map).map(g=>`<option value="${g}">GPIO${g}${g>=34?' (input only)':''}</option>`).join('')}</select><div class="muted" style="font-size:.8rem;margin-top:4px">Default GPIO${P.defaults[k]}</div>${k==='relay'?'<select id="pin-relayLow" style="margin-top:8px" aria-label="Relay input level"><option value="0">Active high</option><option value="1">Active low</option></select>':''}</td><td id="iss-${k}">${old[k]}</td></tr>`).join('');
$('pin-relayLow').value=map.relay_active_low?'1':'0';
for(const k of [...PIN_ROLES.map(r=>r[0]),'relayLow']){if(k!=='relayLow')$('pin-'+k).value=map[k];$('pin-'+k).onchange=()=>{pinsDirty=true;clearTimeout(pinTimer);pinTimer=setTimeout(checkPins,250)}}}
async function checkPins(){if(!P)return;const m=pinValue();let r;try{r=await api('/api/pins/check',m)}catch(e){return toast(e.message,true)}
for(const [k] of PIN_ROLES){const list=r.issues.filter(i=>i.role===k);$('iss-'+k).innerHTML=list.map(i=>`<div class="issue ${i.level}"><svg><use href="#i-${i.level==='error'?'x':i.level==='warning'?'warn':'info'}"/></svg><span>${esc(i.text)}</span></div>`).join('')||'<div class="issue ok"><svg><use href="#i-check"/></svg><span>OK</span></div>';$('row-'+k).classList.toggle('changed',m[k]!==P.pins[k]||(k==='relay'&&m.relay_active_low!==P.pins.relay_active_low))}
const errs=r.issues.filter(i=>i.level==='error').length,warns=r.issues.filter(i=>i.level==='warning').length,pending=PIN_ROLES.some(([k])=>P.saved[k]!==P.pins[k])||P.saved.relay_active_low!==P.pins.relay_active_low;
$('pinState').innerHTML=`<span class="dot ${errs?'bad':warns?'warn':'ok'}"></span>${errs?errs+' error'+(errs>1?'s':''):warns?warns+' warning'+(warns>1?'s':''):'Valid'}${pending?' · unapplied changes':''}`;$('pinSave').disabled=!r.ok;return r}
$('pinUndo').onclick=()=>{pinsDirty=false;drawPins(P.saved);checkPins()};
$('pinDefaults').onclick=()=>{pinsDirty=true;drawPins(P.defaults);checkPins()};
$('pinSave').onclick=async()=>{const r=await checkPins();if(!r||!r.ok)return;if(r.issues.some(i=>i.level==='warning')&&!await ask({title:'Save with warnings?',text:'Some pins have warnings (see the Notes column). The chime may not start or flash correctly with them.',ok:'Save anyway',icon:'warn'}))return;
try{const s=await api('/api/pins',pinValue());pinsDirty=false;await loadPins();toast(s.message)}catch(e){toast(e.message,true)}};
// WiFi and pairing reuse the same controls within their Settings sections.
function closeSettingsFlow(){if(settingsStep<0)return;const step=settingsStep;settingsStep=-1;$('tab-setup').querySelector('.card').appendChild($('wiz'+step));$(step===0?'cfgWifiTask':'cfgPairTask').classList.add('hidden');$('wNext').textContent=$('pNext').textContent='Continue'}
function openSettingsFlow(step){if(initialSetup())return;closeSettingsFlow();openSection(step===0?'network':'doorbell');settingsStep=step;const host=$(step===0?'cfgWifiTask':'cfgPairTask');host.appendChild($('wiz'+step));host.classList.remove('hidden');$('wNext').textContent=$('pNext').textContent='Done';wizard(step);host.scrollIntoView({block:'start',behavior:'smooth'})}
// Setup wizard
function wizard(step,passive){wizStep=step;for(let i=0;i<5;i++){$('wiz'+i).classList.toggle('hidden',i!==step);$('steps').children[i].className=i<step?'done':i===step?'on':''}
if(step===0&&!passive){scan();const w=$('wifiNow');const ap=S&&S.wifi_mode==='ap';w.classList.toggle('hidden',!S||!S.wifi_connected&&!ap);if(ap)w.innerHTML=`The chime runs its own access point <b>${esc(S.ap_name)}</b>. Connecting to a network switches it to client mode.`;else if(S&&S.wifi_connected)w.innerHTML=`Connected to <b>${esc(S.ssid)}</b> (${S.ip}). Choose another network or press ${settingsStep===0?'Done':'Continue'}.`}
if(step===0)wifiView();if(step===1)pairView();if(step===2&&!passive){$('sName').value=C?C.name:'';wizMelody=G?G.melody:0;wizPattern=X?X.pattern:0;wizType=X?X.type:1;wizAudioPick=X?X.audio_output:0;wizAudioPin=P&&P.saved.audio>=0?P.saved.audio:25;wizRelayPin=P&&P.saved.relay>=0?P.saved.relay:26;wizRelayLow=!!(P&&P.saved.relay_active_low);wizExtraRelay=!!(G&&G.relay);wizReviewBody=null;wizReviewPins=null;wizSetupSaved=false;wizAuthInitialized=false;wizReviewAuth=null;wizSounds()}
if(step===3&&!passive)wizardAuth();if(step===4&&!passive)doneView()}
async function scan(again){clearTimeout(scanTimer);try{const r=await api('/api/wifi/scan'+(again?'?refresh=1':''));if(r.scanning){if(again||!$('networks').dataset.ok)$('networks').innerHTML='<button disabled>Scanning…</button>';scanTimer=setTimeout(scan,1500);return}
$('networks').dataset.ok=1;$('networks').innerHTML=r.networks.length?r.networks.map(n=>`<button data-ssid="${esc(n.ssid)}">${n.secure?'🔒':'🔓'} ${esc(n.ssid)} ${bars(n.rssi)}</button>`).join(''):'<button disabled>No networks found</button>';
for(const b of $('networks').children)if(b.dataset.ssid!==undefined)b.onclick=()=>{$('wSsid').value=b.dataset.ssid;for(const o of $('networks').children)o.classList.toggle('on',o===b);$('wPass').focus()}}catch(e){toast(e.message,true)}}
// One primary button per step: the step action until it is done, then Continue; the footer offers Skip for now meanwhile.
function wifiView(){const ok=!!(S&&S.wifi_connected);$('wConnect').classList.toggle('primary',!ok);$('wNext').classList.toggle('primary',ok);$('wNext').classList.toggle('ghost',!ok);$('wNext').textContent=settingsStep===0?'Done':ok?'Continue':'Skip for now'}
$('scan').onclick=()=>scan(true);$('wNext').onclick=()=>settingsStep===0?closeSettingsFlow():wizard(1);
$('wConnect').onclick=async()=>{const ssid=$('wSsid').value.trim();if(!ssid)return toast('Choose or type a network name',true);try{await api('/api/wifi/connect',{ssid,password:$('wPass').value})}catch(e){return toast(e.message,true)}
$('wifiProgress').classList.remove('hidden');$('wConnect').disabled=true;const t0=Date.now();clearInterval(wifiTimer);wifiTimer=setInterval(async()=>{const el=Date.now()-t0;$('wifiBar').style.width=Math.min(100,el/200)+'%';await refresh();
if(S&&S.wifi_connected&&S.ssid===ssid){clearInterval(wifiTimer);$('wConnect').disabled=false;$('wifiBar').style.width='100%';$('wifiProgressText').innerHTML=`Connected. Address <b>${S.ip}</b>, or <b>http://reolink-chime.local</b> on the same network.`;toast('WiFi connected');if(activeTab==='setup'&&settingsStep<0)setTimeout(()=>{if(activeTab==='setup'&&settingsStep<0&&wizStep===0)wizard(S.paired?2:1)},1800)}
else if(el>20000){clearInterval(wifiTimer);$('wConnect').disabled=false;$('wifiProgressText').textContent='Could not connect. Check the password and use a 2.4 GHz network.'}else $('wifiProgressText').textContent=`Connecting to ${ssid}…`},1000)};
function pairView(){if(!S)return;const p=S.pairing,idx=PAIR_PHASES.findIndex(x=>x[0]===p),active=idx>=0&&idx<4,joined=S.joined&&!active;
$('phases').innerHTML=PAIR_PHASES.map(([k,t],i)=>{const st=joined||i<idx?'done':i===idx?'on':'';return `<div class="phase ${st}"><span class="dot ${st==='done'?'ok':st==='on'?'warn pulse':''}"></span>${t}</div>`}).join('');
const left=pairStarted?Math.max(0,300-Math.round((Date.now()-pairStarted)/1000)):0;$('pairBar').style.width=active&&pairStarted?(100*(300-left)/300)+'%':joined?'100%':'0';
$('pStart').classList.toggle('hidden',active);$('pBand').disabled=active||pBandBusy;$('pCancel').classList.toggle('hidden',!active);$('pStart').classList.toggle('primary',!joined);$('pNext').classList.toggle('primary',joined);$('pNext').classList.toggle('ghost',!joined);$('pNext').textContent=settingsStep===1?'Done':joined?'Continue':'Skip for now';$('pStart').lastChild.textContent=joined||S.paired?'Pair again':'Start pairing';
$('pairText').textContent=active?(p==='discovering'?(pairStarted?`Waiting for the doorbell… ${Math.floor(left/60)}:${String(left%60).padStart(2,'0')} left. `:'')+'Tap Pair in the app when “Reolink Chime” appears.':'Pairing in progress…'):joined?`Linked to doorbell ${S.doorbell_id}.`:p!=='idle'&&p!=='joined'?`Last attempt: ${p}. Try again.`:'Ready.';
if(joined&&pairStarted){pairStarted=0;toast('Paired with your doorbell');if(activeTab==='setup'&&settingsStep<0)setTimeout(()=>{if(activeTab==='setup'&&settingsStep<0&&wizStep===1)wizard(2)},1500)}}
function pBandSync(){if(pBandBusy||!G)return;const hz=G.frequency_hz,o=[...$('pBand').options].find(x=>x.value&&+x.value===hz);$('pBand').value=o?o.value:'';$('pFreq').textContent=`Frequency: ${(hz/1e6).toFixed(3)} MHz`;$('pUntested').classList.toggle('hidden',!(hz>=902e6&&hz<=928e6))}
let pBandBusy=false;$('pBand').onchange=async()=>{const hz=+$('pBand').value;if(!hz)return;pBandBusy=true;$('pBand').disabled=$('pStart').disabled=true;try{await api('/api/settings',{frequency_hz:hz});G.frequency_hz=hz;$('rfHz').value=hz;rfBandSync();toast('Region saved')}catch(e){toast(e.message,true)}pBandBusy=false;$('pStart').disabled=false;pBandSync();pairView()};
$('pStart').onclick=async()=>{try{await api('/api/pair/start',{});pairStarted=Date.now();refresh()}catch(e){toast(e.message,true)}};
$('pCancel').onclick=async()=>{pairStarted=0;await act('/api/pair/cancel',{},'Pairing cancelled')};$('pBack').onclick=()=>wizard(0);$('pNext').onclick=()=>settingsStep===1?closeSettingsFlow():wizard(2);
$('dbgPair').onclick=async()=>{await act('/api/pair/start');pairStarted=Date.now()};$('dbgCancel').onclick=()=>act('/api/pair/cancel');$('dbgTest').onclick=()=>act('/api/gong/test',{});
// Switching mode preserves the numbered doorbell sound slot.
let wizType=1,wizMelody=0,wizPattern=0,wizAudioPick=0,wizAudioPin=25,wizRelayPin=26,wizRelayLow=false,wizExtraRelay=false;
let wizReviewBody=null,wizReviewPins=null,wizSetupSaved=false;
let wizReviewAuth=null,wizAuthInitialized=false;
let wizWireOk=false,wizWireBusy=false,wizWireVersion=0,wizWireTimer=0;
const sameWiring=(a,b)=>!!a&&!!b&&Object.keys(a).every(k=>a[k]===b[k]);
function wizardPinMap(){const map={...(P?P.saved:{})};if(wizType===1){map.relay=wizRelayPin;map.relay_active_low=wizRelayLow;if(map.audio===map.relay)map.audio=-1}
if(wizType===0){map.audio=wizAudioPin;if(wizExtraRelay){map.relay=wizRelayPin;map.relay_active_low=wizRelayLow}else if(map.relay===map.audio)map.relay=-1}return map}
function wizardSoundBody(){const map=wizardPinMap();return{chime:{name:$('sName').value.trim()||(C?C.name:'Reolink Chime'),type:wizType,audio_output:wizAudioPick,melody:wizMelody,pattern:wizPattern,extra_relay:wizType===0?wizExtraRelay:!!(G&&G.relay)},pins:{audio:map.audio,relay:map.relay,relay_active_low:map.relay_active_low}}}
function wizardPreviewReady(){return wizWireOk&&!wizWireBusy&&P&&sameWiring(wizardPinMap(),P.pins)}
function wizardPreview(sound){if(!wizardPreviewReady())return toast('Finish setup on the last step to apply this wiring before testing sound.',true);
const body=wizType===1?{type:1,pattern:sound,pulse_ms:X.pulse_ms,gap_ms:X.gap_ms}:{type:0,audio_output:wizAudioPick,melody:sound,volume:Math.floor(G.volume*C.volume/4),extra_relay:wizExtraRelay,extra_relay_ms:G.relay_ms};
return act('/api/gong/test',body,'Playing preview')}
function wizardWireState(){$('sNext').disabled=wizWireBusy||!wizWireOk;$('sNext').textContent='Continue';for(const b of $('melodies2').querySelectorAll('[data-play],[data-mplay]'))b.disabled=!wizardPreviewReady()}
function wizardWireCheck(){const version=++wizWireVersion;clearTimeout(wizWireTimer);wizWireBusy=true;wizWireOk=false;wizardWireState();wizWireTimer=setTimeout(async()=>{try{
const r=await api('/api/setup/sound/check',wizardSoundBody());if(version!==wizWireVersion)return;wizWireOk=r.ok;
$('wizWireIssues').innerHTML=r.issues.filter(i=>i.level!=='info').map(i=>`<div class="issue ${i.level}"><span>${esc(i.role)}: ${esc(i.text)}</span></div>`).join('');
}catch(e){if(version!==wizWireVersion)return;$('wizWireIssues').textContent=e.message}if(version===wizWireVersion){wizWireBusy=false;wizardWireState()}},200)}
function drawWizardWiring(){const audio=wizType===0,relay=wizType===1||audio&&wizExtraRelay;
if(audio&&wizAudioPick===1&&![25,26].includes(wizAudioPin))wizAudioPin=25;
$('wizWiring').classList.toggle('hidden',wizType===2);$('wizAudioPinField').classList.toggle('hidden',!audio);$('wizRelayPinField').classList.toggle('hidden',!relay);$('wizRelayLowField').classList.toggle('hidden',!relay);$('wizExtraField').classList.toggle('hidden',!audio);
const options=(list,selected)=>list.map(pin=>`<option value="${pin}"${pin===selected?' selected':''}>GPIO${pin}</option>`).join('');
const outputPins=GPIOS.filter(pin=>pin!==1&&pin!==3&&pin<34);$('wizAudioPin').innerHTML=options(wizAudioPick===1?[25,26]:outputPins,wizAudioPin);$('wizRelayPin').innerHTML=options(outputPins,wizRelayPin);$('wizRelayLow').value=wizRelayLow?'1':'0';$('wizExtraRelay').checked=wizExtraRelay;
$('wizWireHint').textContent=audio?'Connect this GPIO to your amplifier input. '+(wizAudioPick===1?'DAC uses GPIO25 or GPIO26.':'PWM uses an available output GPIO.'):'Connect this GPIO to a relay module that accepts 3.3 V logic. Its COM and NO contacts connect to the gong button terminals.';
const map=wizardPinMap(),freed=P&&['audio','relay'].find(role=>P.saved[role]>=0&&map[role]===-1);
$('wizWireNote').textContent=(freed?`The unused ${freed==='audio'?'Audio output':'Relay'} assignment will be disconnected to free this GPIO. `:'')+'GPIO changes apply when you finish the last setup step, without a restart. Disconnect power before moving wires.';
wizardWireCheck()}
function wizSounds(){typeCards('wizTypes',wizType,i=>{if(i===wizType)return;
if(i===1&&isAudio(wizType))wizPattern=wizMelody;else if(isAudio(i)&&wizType===1)wizMelody=wizPattern;wizType=i;setTimeout(wizSounds)});
$('wizAudioField').classList.toggle('hidden',!isAudio(wizType));audioOutputCards('wizAudioOutputs',wizAudioPick,i=>{wizAudioPick=i;if(i===1&&![25,26].includes(wizAudioPin))wizAudioPin=25;setTimeout(wizSounds)},true);
$('wizSoundLabel').textContent=wizType===1?'Strike pattern':'Melody';$('wizSoundField').classList.toggle('hidden',wizType===2);
if(wizType===1)cards('melodies2',PATTERNS,wizPattern,i=>{wizPattern=i;wizardWireCheck()},wizardPreview);else if(isAudio(wizType))cards('melodies2',MELODIES,wizMelody,i=>{wizMelody=i;wizardWireCheck()},wizardPreview);
drawWizardWiring()}
$('wizAudioPin').onchange=()=>{wizAudioPin=+$('wizAudioPin').value;drawWizardWiring()};$('wizRelayPin').onchange=()=>{wizRelayPin=+$('wizRelayPin').value;drawWizardWiring()};$('wizRelayLow').onchange=()=>{wizRelayLow=$('wizRelayLow').value==='1';drawWizardWiring()};$('wizExtraRelay').onchange=()=>{wizExtraRelay=$('wizExtraRelay').checked;drawWizardWiring()};$('sName').oninput=wizardWireCheck;
$('wizAdvanced').onclick=()=>{if(initialSetup())return;show('settings');openSection('advanced');$('pinRows').scrollIntoView({block:'start',behavior:'smooth'})};
$('sBack').onclick=()=>wizard(1);$('sNext').onclick=()=>{if(!wizWireOk||wizWireBusy)return;wizReviewBody=wizardSoundBody();wizReviewPins=wizardPinMap();wizSetupSaved=false;wizard(3)};
function wizardAuth(){if(!wizAuthInitialized){$('wizAuthOn').checked=!!(A&&A.enabled);$('wizAuthUser').value=A?A.username:'admin';$('wizAuthPass').value='';$('wizAuthConfirm').value='';wizAuthInitialized=true}wizardAuthView()}
function wizardAuthError(){if(!$('wizAuthOn').checked)return'';const user=$('wizAuthUser').value.trim(),password=$('wizAuthPass').value,confirm=$('wizAuthConfirm').value;
if(!user||user.length>32||/[^\x20-\x7e]|:/.test(user))return'Use a user name of 1–32 printable characters, without a colon.';
if(!password&&!(A&&A.password_set))return'Choose a password to enable sign-in.';
if(password&&(password.length<8||password.length>63||/[^\x20-\x7e]/.test(password)))return'Use a password of 8–63 printable characters.';
if(password!==confirm)return'The passwords do not match.';return''}
function wizardAuthView(){const enabled=$('wizAuthOn').checked,existing=!!(A&&A.password_set);$('wizAuthFields').classList.toggle('hidden',!enabled);
$('wizAuthPassLabel').textContent=existing?'New password (leave empty to keep the current one)':'Password (8–63 characters)';$('wizAuthPasswordHint').textContent=existing?'Leave both password fields empty to keep your saved password.':'Use 8–63 printable characters and enter the same password twice.';
$('wizAuthHint').textContent=enabled?'Finish on the last step saves sign-in settings and keeps this browser signed in.':'Sign-in will be off. You can enable it later in Settings → Sign-in.';
$('wizAuthSkip').textContent=A&&A.enabled?'Keep current settings':'Skip for now';const error=wizardAuthError();$('wizAuthError').textContent=error;$('wizAuthNext').disabled=!!error}
function wizardAuthBody(){const enabled=$('wizAuthOn').checked;if(!enabled)return{enabled:false};const body={enabled:true,username:$('wizAuthUser').value.trim()};if($('wizAuthPass').value)body.password=$('wizAuthPass').value;return body}
for(const id of ['wizAuthOn','wizAuthUser','wizAuthPass','wizAuthConfirm'])$(id).addEventListener('input',wizardAuthView);
$('wizAuthBack').onclick=()=>{wizard(2,true);wizSounds()};$('wizAuthSkip').onclick=()=>{wizReviewAuth=null;wizard(4)};
$('wizAuthNext').onclick=()=>{if(wizardAuthError())return;wizReviewAuth=wizardAuthBody();wizard(4)};
async function finishSetup(){if(wizSetupSaved||!wizReviewBody){show('home');return}$('dHome').disabled=true;$('dBack').disabled=true;let soundSaved=false;try{
await api('/api/setup/sound',wizReviewBody);soundSaved=true;
if(wizReviewAuth)await api('/api/auth',wizReviewAuth);
await api('/api/setup',{visible:false});
$('wizAuthPass').value='';$('wizAuthConfirm').value='';wizReviewAuth=null;
wizReviewBody=null;wizReviewPins=null;wizSetupSaved=true;await loadSettings();await refresh();$('dHome').disabled=false;$('dBack').disabled=false;toast('Setup complete');show('home');
// The setup hotspot stays on until a restart once home WiFi works.
if(S.ap_active&&S.wifi_connected&&await ask({title:'Switch off the setup hotspot?',text:'The chime still runs its setup hotspot. Restart it to use your home WiFi only.',ok:'Restart now',cancel:'Later'}))act('/api/reboot',{},'Restarting… open http://reolink-chime.local in a moment');return
}catch(e){toast((soundSaved?'Sound and wiring saved; setup could not finish. ':'')+e.message,true);if(soundSaved){await loadSettings();await refresh();doneView()}}$('dHome').disabled=false;$('dBack').disabled=false}
function doneView(){const pending=!!wizReviewBody,cfg=pending?wizReviewBody.chime:null,map=pending?wizReviewPins:P&&P.pins,type=cfg?cfg.type:X?X.type:1,output=cfg?cfg.audio_output:X?X.audio_output:0;
const signIn=pending&&wizReviewAuth?wizReviewAuth.enabled:!!(A&&A.enabled),user=pending&&wizReviewAuth&&wizReviewAuth.username?wizReviewAuth.username:A?A.username:'admin';
const rows=[['Name',cfg?cfg.name:C.name],['Sign-in',signIn?'On · '+user:'Off'],['Sound mode',TYPES[type][0]+(type===0?' · '+OUTPUTS[output][0]:'')],['WiFi',S.wifi_connected?`${S.ssid} (${S.ip})`:'Not connected'],['Doorbell',S.joined?`Linked (${S.doorbell_id})`:S.paired?'Paired, not connected':'Not paired']];
if(type===1)rows.push(['Gong pattern',PATTERNS[cfg?cfg.pattern:X.pattern][0]],['Relay GPIO',map?'GPIO'+map.relay:'—'],['Relay level',map&&map.relay_active_low?'Active low':'Active high']);
else if(type===0){rows.push(['Melody',MELODIES[cfg?cfg.melody:G.melody][0]],['Audio output GPIO',map?'GPIO'+map.audio:'—']);if(cfg?cfg.extra_relay:G.relay)rows.push(['Extra relay GPIO',map?'GPIO'+map.relay:'—'])}else rows.push(['Sound','None (silent)']);rows.push(['Address','http://'+S.hostname]);kv('doneSummary',rows);
const wiringChanged=pending&&P&&!sameWiring(wizReviewPins,P.pins);
$('doneWiring').classList.toggle('hidden',!wiringChanged);$('doneWiring').textContent='Finish saves and applies your GPIO changes without restarting. Sound testing becomes available immediately.';
$('dTestRow').classList.toggle('hidden',type===2);$('dTest').disabled=pending?!wizardPreviewReady():!!P&&!sameWiring(P.saved,P.pins);
$('doneAuth').classList.toggle('hidden',signIn)}
$('dTest').onclick=()=>wizReviewBody?wizardPreview(wizType===1?wizPattern:wizMelody):act('/api/gong/test',{},'Playing chime');
$('dBack').onclick=()=>{if(wizReviewBody)wizard(3);else wizard(2)};$('dHome').onclick=finishSetup;$('doneAuthLink').onclick=e=>{e.preventDefault();if(wizReviewBody){wizard(3);$('wizAuthOn').focus()}else{show('settings');openSection('security');$('authOn').focus()}}
// Debug
function renderDiag(){if(!S)return;const d=[['Radio',S.radio_ok?'OK':'Error'],['Chip',`CC1101 v${S.radio_version}`],['State',S.marcstate===13?'Receiving':'0x'+S.marcstate.toString(16)],['Noise',S.rssi_dbm.toFixed(1)+' dBm'],['Receiver',S.rx_mode===1?'ESP32 bit sampling':'CC1101 packet engine'],['Frequency',(S.rf_frequency_hz/1e6).toFixed(4)+' MHz'],['Trim',`${S.rf_trim>0?'+':''}${S.rf_trim} (${!S.auto_trim?'manual':S.rx_mode===1?'automatic, paused in ESP32 mode':S.trim_calibrated?'automatic':'automatic, measuring'}) · ${S.rx_bandwidth_khz} kHz bandwidth`],['TX polarity',S.tx_polarity],['Packets',S.packets],['Good CRC',S.crc_ti],['Bad CRC',S.crc_bad],['Dropped frames',S.dropped],['Invalid lengths',S.invalid_lengths],['False syncs',S.false_syncs],['Sent',`${S.transmitted} (${S.transmit_errors} errors)`],['Radio resets',S.radio_resets],['Link',S.pairing],['Free memory',Math.round(S.free_heap/1024)+' KB'],['Storage',S.storage_ok?'OK':'Write failed'],['MQTT',S.mqtt],['Clock',S.time_synced?'Synchronised':'Not set'],['Sign-in',S.auth_enabled?'On':'Off']];
$('diag').innerHTML=d.map(([k,v])=>`<div class="stat"><div class="label">${k}</div><div style="font-weight:600;overflow-wrap:anywhere">${esc(v)}</div></div>`).join('');$('dbgUp').textContent='Up '+dur(S.uptime_ms)}
// Receive time to the millisecond; the date only when not today.
function packetTime(x){if(!x.epoch)return`${(x.ms/1000).toFixed(3)} s <span class="muted">uptime</span>`;const d=new Date(x.epoch*1000+(x.epoch_ms||0)),p=n=>String(n).padStart(2,'0');
return(d.toDateString()===new Date().toDateString()?'':esc(d.toLocaleDateString())+' ')+`${p(d.getHours())}:${p(d.getMinutes())}:${p(d.getSeconds())}.${String(d.getMilliseconds()).padStart(3,'0')}`}
function colour(l){const c=l.includes('[rx]')?'rx':l.includes('[tx]')?'tx':l.includes('[app]')?'app':l.includes('[diag]')?'diag':/fail|error|lost/i.test(l)?'err':'';return `<span class="${c}">${esc(l)}</span>`}
function drawLog(){const lines=logText.split('\n').filter(l=>l&&($('logDiag').checked||!l.includes('[diag]')));$('log').innerHTML=lines.slice(-600).map(colour).join('\n');if($('logFollow').checked)$('log').scrollTop=$('log').scrollHeight}
async function pollDebug(force){if(!debugOpen())return;try{const r=await fetch('/api/log?from='+logNext);const n=+r.headers.get('X-Log-Next');const t=await r.text();if(n<logNext)logText='';logNext=n;if(t||force){logText=(logText+t).slice(-200000);drawLog()}
const p=await api('/api/packets');$('packets').innerHTML=p.packets.slice().reverse().map(x=>`<tr><td>${x.id}</td><td style="white-space:nowrap" title="Uptime ${(x.ms/1000).toFixed(3)} s">${packetTime(x)}</td><td>${x.rssi_dbm}</td><td style="white-space:nowrap">${x.offset_khz==null?'—':(x.offset_khz>0?'+':'')+x.offset_khz+' kHz'}</td><td>${x.crc.startsWith('TI')?'✓':'✗'}</td><td>${esc(x.result)}</td><td class="hex">${esc(x.hex)}</td></tr>`).join('')}catch(e){}}
$('logDiag').onchange=drawLog;$('logClear').onclick=()=>{logText='';drawLog()};
$('logDownload').onclick=()=>{const a=document.createElement('a');a.href=URL.createObjectURL(new Blob([logText],{type:'text/plain'}));a.download='chime-log.txt';a.click()};
$('mSave').onclick=async()=>{if(!await ask({title:'Replace the pairing?',text:'The paired profile is replaced by this passive receive profile. The chime then only listens and cannot rejoin until you pair it again.',ok:'Replace',danger:true}))return;act('/api/settings',{source:$('mSource').value,destination:$('mDest').value,pair:$('mPair').value,key:$('mKey').value},'Profile saved')};
// Start
// Auto refresh: status every 2 s, settings every 10 s unless the user is
// editing. One request chain at a time keeps the ESP32 web server responsive;
// hidden browser tabs pause polling entirely.
let ticks=0,lastUp=0,ticking=false;
const editing=()=>{const a=document.activeElement;return dirty||pinsDirty||authDirty||intDirty||wmDirty||(a&&['INPUT','SELECT','TEXTAREA'].includes(a.tagName))};
async function tick(){if(document.hidden||ticking||restarting)return;ticking=true;try{await refresh();if(S){const restarted=lastUp&&S.uptime_ms<lastUp;lastUp=S.uptime_ms;
if(restarted){toast('The chime restarted');logNext=0;logText='';await loadSettings();render()}else if(++ticks%5===0&&!editing()){await loadSettings();render()}}await pollDebug()}finally{ticking=false}}
document.addEventListener('visibilitychange',()=>{if(!document.hidden)tick()});
(async()=>{await refresh();await loadSettings();render();let t='home';try{t=localStorage.getItem('tab')||'home'}catch(e){}if(t==='debug'){t='settings';section='debug'}if(!$('tab-'+t))t='home';if(t==='setup'&&S&&S.setup_visible===false)t='home';if(initialSetup()||S&&!S.paired&&S.setup_visible!==false){wizStep=S.wifi_configured?1:0;t='setup'}show(t);checkUpdatedVersion();setInterval(tick,2000)})();
</script></body></html>)HTML";

// Sign-in page served at /login while sign-in is enabled. It posts to
// /api/login, which sets an HttpOnly session cookie.
static const char LoginUi[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#0f1720"><title>Sign in · Chime</title>
<link rel="icon" href="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24'%3E%3Cpath fill='%2338bdf8' d='M12 22a2.5 2.5 0 0 0 2.45-2h-4.9A2.5 2.5 0 0 0 12 22Zm7-6V11a7 7 0 0 0-5.5-6.84V3.5a1.5 1.5 0 0 0-3 0v.66A7 7 0 0 0 5 11v5l-2 2v1h18v-1Z'/%3E%3C/svg%3E">
<script>try{const t=localStorage.getItem('theme');if(t==='light'||t==='dark')document.documentElement.dataset.theme=t}catch(e){}</script>
<style>
:root{--bg:#0f1720;--panel:#17222d;--panel2:#1e2b38;--line:#2b3b4a;--text:#e6eef5;--muted:#93a6b6;--accent:#38bdf8;--accent2:#0ea5e9;--bad:#f87171;--shadow:0 8px 30px rgba(0,0,0,.35);color-scheme:dark}
@media (prefers-color-scheme:light){:root:not([data-theme=dark]){--bg:#f3f6f9;--panel:#fff;--panel2:#f1f5f9;--line:#dbe3ea;--text:#0f1d2a;--muted:#5b6f80;--accent:#0284c7;--accent2:#0369a1;--bad:#dc2626;--shadow:0 6px 24px rgba(15,29,42,.08);color-scheme:light}}
:root[data-theme=light]{--bg:#f3f6f9;--panel:#fff;--panel2:#f1f5f9;--line:#dbe3ea;--text:#0f1d2a;--muted:#5b6f80;--accent:#0284c7;--accent2:#0369a1;--bad:#dc2626;--shadow:0 6px 24px rgba(15,29,42,.08);color-scheme:light}
*{box-sizing:border-box}html,body{margin:0;height:100%}body{font:15px/1.5 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif;background:var(--bg);color:var(--text);display:grid;place-items:center;padding:16px}
.card{width:100%;max-width:380px;background:var(--panel);border:1px solid var(--line);border-radius:18px;padding:28px 22px;box-shadow:var(--shadow)}
.logo{width:52px;height:52px;border-radius:16px;display:grid;place-items:center;background:linear-gradient(135deg,var(--accent),#6366f1);color:#fff;margin:0 auto 14px}
h1{font-size:1.3rem;margin:0;text-align:center}p.sub{color:var(--muted);text-align:center;margin:6px 0 20px;font-size:.92rem}
label{display:block;margin:14px 0}label>span{display:block;font-size:.85rem;color:var(--muted);margin-bottom:6px}
input{font:inherit;width:100%;padding:11px 12px;border-radius:12px;border:1px solid var(--line);background:var(--panel2);color:var(--text)}
input:focus{outline:2px solid color-mix(in srgb,var(--accent) 60%,transparent);border-color:var(--accent)}
.check{display:flex;align-items:center;gap:8px;color:var(--muted);font-size:.9rem;margin:6px 0 18px}.check input{width:18px;height:18px;accent-color:var(--accent)}
button{font:inherit;width:100%;border-radius:12px;border:0;background:var(--accent2);color:#fff;font-weight:600;padding:12px;cursor:pointer}button:disabled{opacity:.6;cursor:default}
.error{color:var(--bad);font-size:.9rem;min-height:1.4em;margin:12px 0 0;text-align:center}
details{margin-top:18px;color:var(--muted);font-size:.85rem}summary{cursor:pointer}
</style></head><body>
<form class="card" id="form" autocomplete="on">
<div class="logo"><svg width="26" height="26" viewBox="0 0 24 24"><path fill="currentColor" d="M12 22a2.5 2.5 0 0 0 2.45-2h-4.9A2.5 2.5 0 0 0 12 22Zm7-6V11a7 7 0 0 0-5.5-6.84V3.5a1.5 1.5 0 0 0-3 0v.66A7 7 0 0 0 5 11v5l-2 2v1h18v-1Z"/></svg></div>
<h1>Sign in</h1><p class="sub">ESP32 chime for Reolink doorbells</p>
<label><span>User name</span><input id="user" name="username" autocomplete="username" autocapitalize="none" spellcheck="false" required maxlength="32"></label>
<label><span>Password</span><input id="pass" name="password" type="password" autocomplete="current-password" required maxlength="63"></label>
<label class="check"><input type="checkbox" id="remember"> Keep me signed in for 30 days</label>
<button id="go" type="submit">Sign in</button>
<p class="error" id="error" role="alert"></p>
<details><summary>Forgot the password?</summary><p>Hold the chime's button for 15 seconds (or send <b>a</b> on the USB serial console at 115200 baud). Sign-in turns off; then set a new password in Settings → Security.</p></details>
</form>
<script>
const $=id=>document.getElementById(id);let timer=0;
try{$('user').value=localStorage.getItem('user')||''}catch(e){}
(($('user').value?$('pass'):$('user'))).focus();
function wait(s){clearInterval(timer);$('go').disabled=true;const tick=()=>{if(s<=0){clearInterval(timer);$('go').disabled=false;$('error').textContent='';return}$('error').textContent=`Too many attempts. Try again in ${s--} s.`};tick();timer=setInterval(tick,1000)}
$('form').onsubmit=async e=>{e.preventDefault();$('go').disabled=true;$('error').textContent='';
try{const r=await fetch('/api/login',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({username:$('user').value,password:$('pass').value,remember:$('remember').checked})});
let d={};try{d=await r.json()}catch(x){}
if(r.ok){try{localStorage.setItem('user',$('user').value)}catch(x){}location.replace('/');return}
if(r.status===429){wait(+(r.headers.get('Retry-After')||5));return}
$('error').textContent=d.error||'Sign-in failed';$('pass').select()}catch(x){$('error').textContent='The chime is not reachable'}
$('go').disabled=false};
</script></body></html>)HTML";
