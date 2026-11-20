/* RedVision-style web UI for the redarc_rbus ESPHome component.
 * Replaces the stock ESPHome page (web_server js_include) and talks to the node through its
 * /events stream and REST API. Entities are matched by name, ignoring the "TVMS 1 " style prefix,
 * so cards for entities that are not in your config simply do not appear. */
(function () {
  'use strict';

  var ICONS = /*ICONS*/{};

  // Material icon paths (24x24).
  var MI = {
    menu: 'M3 18h18v-2H3v2zm0-5h18v-2H3v2zm0-7v2h18V6H3z',
    power: 'M13 3h-2v10h2V3zm4.83 2.17l-1.42 1.42C17.99 7.86 19 9.81 19 12c0 3.87-3.13 7-7 7s-7-3.13-7-7c0-2.19 1.01-4.14 2.58-5.42L6.17 5.17C4.23 6.82 3 9.26 3 12c0 4.97 4.03 9 9 9s9-4.03 9-9c0-2.74-1.23-5.18-3.17-6.83z',
    bt: 'M17.71 7.71L12 2h-1v7.59L6.41 5 5 6.41 10.59 12 5 17.59 6.41 19 11 14.41V22h1l5.71-5.71-4.3-4.29 4.3-4.29zM13 5.83l1.88 1.88L13 9.59V5.83zm1.88 10.46L13 18.17v-3.76l1.88 1.88z',
    home: 'M10 20v-6h4v6h5v-8h3L12 3 2 12h3v8z',
    grid: 'M3 3h8v8H3V3zm2 2v4h4V5H5zm8-2h8v8h-8V3zm2 2v4h4V5h-4zM3 13h8v8H3v-8zm2 2v4h4v-4H5zm8-2h8v8h-8v-8zm2 2v4h4v-4h-4z',
    chart: 'M19 3H5c-1.1 0-2 .9-2 2v14c0 1.1.9 2 2 2h14c1.1 0 2-.9 2-2V5c0-1.1-.9-2-2-2zm0 16H5V5h14v14zM7 10h2v7H7zm4-3h2v10h-2zm4 6h2v4h-2z',
    close: 'M19 6.41L17.59 5 12 10.59 6.41 5 5 6.41 10.59 12 5 17.59 6.41 19 12 13.41 17.59 19 19 17.59 13.41 12z',
    gear: 'M19.14 12.94c.04-.3.06-.61.06-.94 0-.32-.02-.64-.07-.94l2.03-1.58a.49.49 0 0 0 .12-.61l-1.92-3.32a.488.488 0 0 0-.59-.22l-2.39.96c-.5-.38-1.03-.7-1.62-.94l-.36-2.54a.484.484 0 0 0-.48-.41h-3.84c-.24 0-.43.17-.47.41l-.36 2.54c-.59.24-1.13.57-1.62.94l-2.39-.96c-.22-.08-.47 0-.59.22L2.74 8.87c-.12.21-.08.47.12.61l2.03 1.58c-.05.3-.09.63-.09.94s.02.64.07.94l-2.03 1.58a.49.49 0 0 0-.12.61l1.92 3.32c.12.22.37.29.59.22l2.39-.96c.5.38 1.03.7 1.62.94l.36 2.54c.05.24.24.41.48.41h3.84c.24 0 .44-.17.47-.41l.36-2.54c.59-.24 1.13-.56 1.62-.94l2.39.96c.22.08.47 0 .59-.22l1.92-3.32c.12-.22.07-.47-.12-.61l-2.01-1.58zM12 15.6c-1.98 0-3.6-1.62-3.6-3.6s1.62-3.6 3.6-3.6 3.6 1.62 3.6 3.6-1.62 3.6-3.6 3.6z'
  };
  function mi(name, size, cls) {
    return '<svg class="' + (cls || '') + '" width="' + size + '" height="' + size +
      '" viewBox="0 0 24 24" fill="currentColor"><path d="' + MI[name] + '"/></svg>';
  }
  // Generic icons (Google Material Icons, Apache-2.0; see licenses/). build.py can swap in your
  // own set; none are needed for the public firmware.
  var GEN = {
    light: 'M9 21c0 .55.45 1 1 1h4c.55 0 1-.45 1-1v-1H9v1zm3-19C8.14 2 5 5.14 5 9c0 2.38 1.19 4.47 3 5.74V17c0 .55.45 1 1 1h6c.55 0 1-.45 1-1v-2.26c1.81-1.27 3-3.36 3-5.74 0-3.86-3.14-7-7-7zm2.85 11.1l-.85.6V16h-4v-2.3l-.85-.6C7.8 12.16 7 10.63 7 9c0-2.76 2.24-5 5-5s5 2.24 5 5c0 1.63-.8 3.16-2.15 4.1z',
    power12: 'M11 21h-1l1-7H7.5c-.58 0-.57-.32-.38-.66.19-.34.05-.08.07-.12C8.48 10.94 10.42 7.54 13 3h1l-1 7h3.5c.49 0 .56.33.47.51l-.07.15C12.96 17.55 11 21 11 21z',
    power240: 'M16 7V3h-2v4h-4V3H8v4h-.01C6.9 6.99 6 7.89 6 8.98v5.52L9.5 18v3h5v-3l3.5-3.51v-5.5c0-1.1-.9-2-2-2z',
    plug: 'M16 7V3h-2v4h-4V3H8v4h-.01C6.9 6.99 6 7.89 6 8.98v5.52L9.5 18v3h5v-3l3.5-3.51v-5.5c0-1.1-.9-2-2-2z',
    compressor: 'M14.5 17c0 1.65-1.35 3-3 3s-3-1.35-3-3h2c0 .55.45 1 1 1s1-.45 1-1-.45-1-1-1H2v-2h9.5c1.65 0 3 1.35 3 3zM19 6.5C19 4.57 17.43 3 15.5 3S12 4.57 12 6.5h2c0-.83.67-1.5 1.5-1.5s1.5.67 1.5 1.5S16.33 8 15.5 8H2v2h13.5c1.93 0 3.5-1.57 3.5-3.5zm-.5 4.5H2v2h16.5c.83 0 1.5.67 1.5 1.5s-.67 1.5-1.5 1.5v2c1.93 0 3.5-1.57 3.5-3.5S20.43 11 18.5 11z',
    fridge: 'M18 2.01L6 2c-1.1 0-2 .89-2 2v16c0 1.1.9 2 2 2h12c1.1 0 2-.9 2-2V4c0-1.11-.9-1.99-2-1.99zM18 20H6v-9.02h12V20zm0-11H6V4h12v5zM8 5h2v3H8zm0 7h2v5H8z',
    stereo: 'M3.24 6.15C2.51 6.43 2 7.17 2 8v12c0 1.1.89 2 2 2h16c1.11 0 2-.9 2-2V8c0-1.11-.89-2-2-2H8.3l8.26-3.34L15.88 1 3.24 6.15zM7 20c-1.66 0-3-1.34-3-3s1.34-3 3-3 3 1.34 3 3-1.34 3-3 3zm13-8h-2v-2h-2v2H4V8h16v4z',
    speaker: 'M17 2H7c-1.1 0-2 .9-2 2v16c0 1.1.9 1.99 2 1.99L17 22c1.1 0 2-.9 2-2V4c0-1.1-.9-2-2-2zm-5 2c1.1 0 2 .9 2 2s-.9 2-2 2c-1.11 0-2-.9-2-2s.89-2 2-2zm0 16c-2.76 0-5-2.24-5-5s2.24-5 5-5 5 2.24 5 5-2.24 5-5 5zm0-8c-1.66 0-3 1.34-3 3s1.34 3 3 3 3-1.34 3-3-1.34-3-3-3z',
    tap: 'M12 2c-5.33 4.55-8 8.48-8 11.8 0 4.98 3.8 8.2 8 8.2s8-3.22 8-8.2c0-3.32-2.67-7.25-8-11.8z',
    heater: 'M13.5.67s.74 2.65.74 4.8c0 2.06-1.35 3.73-3.41 3.73-2.07 0-3.63-1.67-3.63-3.73l.03-.36C5.21 7.51 4 10.62 4 14c0 4.42 3.58 8 8 8s8-3.58 8-8C20 8.61 17.41 3.8 13.5.67z',
    fan: 'M12 11a1 1 0 1 0 0 2 1 1 0 0 0 0-2zm.5-9c4.5 0 4.61 3.57 2.25 4.75-.99.49-1.43 1.54-1.62 2.47.48.2.9.51 1.22.91 3.7-2 7.68-1.21 7.68 2.37 0 4.5-3.57 4.6-4.75 2.25-.5-.99-1.56-1.43-2.49-1.62-.2.48-.51.89-.91 1.22 2.01 3.7 1.21 7.67-2.37 7.67-4.49 0-4.59-3.57-2.24-4.75.98-.49 1.42-1.53 1.62-2.45-.49-.2-.92-.52-1.24-.92C5.96 15.92 2 15.13 2 11.56 2 7.06 5.56 6.96 6.74 9.31c.5.99 1.56 1.43 2.49 1.62.19-.48.51-.9.91-1.22C8.13 6.03 8.92 2 12.5 2z',
    usb: 'M15 7v4h1v2h-3V5h2l-3-4-3 4h2v8H8v-2.07c.7-.37 1.2-1.08 1.2-1.93 0-1.21-.99-2.2-2.2-2.2-1.21 0-2.2.99-2.2 2.2 0 .85.5 1.56 1.2 1.93V13c0 1.11.89 2 2 2h3v3.05c-.71.37-1.2 1.1-1.2 1.95 0 1.22.99 2.2 2.2 2.2 1.21 0 2.2-.98 2.2-2.2 0-.85-.49-1.58-1.2-1.95V15h3c1.11 0 2-.89 2-2v-2h1V7h-4z',
    output: 'M13 3h-2v10h2V3zm4.83 2.17l-1.42 1.42C17.99 7.86 19 9.81 19 12c0 3.87-3.13 7-7 7s-7-3.13-7-7c0-2.19 1.01-4.14 2.58-5.42L6.17 5.17C4.23 6.82 3 9.26 3 12c0 4.97 4.03 9 9 9s9-4.03 9-9c0-2.74-1.23-5.18-3.17-6.83z',
    temp: 'M15 13V5c0-1.66-1.34-3-3-3S9 3.34 9 5v8c-1.21.91-2 2.37-2 4 0 2.76 2.24 5 5 5s5-2.24 5-5c0-1.63-.79-3.09-2-4zm-4-8c0-.55.45-1 1-1s1 .45 1 1h-1v1h1v2h-1v1h1v2h-2V5z',
    car: 'M18.92 6.01C18.72 5.42 18.16 5 17.5 5h-11c-.66 0-1.21.42-1.42 1.01L3 12v8c0 .55.45 1 1 1h1c.55 0 1-.45 1-1v-1h12v1c0 .55.45 1 1 1h1c.55 0 1-.45 1-1v-8l-2.08-5.99zM6.5 16c-.83 0-1.5-.67-1.5-1.5S5.67 13 6.5 13s1.5.67 1.5 1.5S7.33 16 6.5 16zm11 0c-.83 0-1.5-.67-1.5-1.5s.67-1.5 1.5-1.5 1.5.67 1.5 1.5-.67 1.5-1.5 1.5zM5 11l1.5-4.5h11L19 11H5z',
    sun: 'M6.76 4.84l-1.8-1.79-1.41 1.41 1.79 1.79 1.42-1.41zM4 10.5H1v2h3v-2zm9-9.95h-2V3.5h2V.55zm7.45 3.91l-1.41-1.41-1.79 1.79 1.41 1.41 1.79-1.79zm-3.21 13.7l1.79 1.8 1.41-1.41-1.8-1.79-1.4 1.4zM20 10.5v2h3v-2h-3zm-8-5c-3.31 0-6 2.69-6 6s2.69 6 6 6 6-2.69 6-6-2.69-6-6-6zm-1 16.95h2V19.5h-2v2.95zm-7.45-3.91l1.41 1.41 1.79-1.8-1.41-1.41-1.79 1.8z'
  };
  function ic(name, size, cls) {
    var g = GEN[name] || GEN.output;
    var s = ICONS[name] || '<svg viewBox="0 0 24 24" fill="currentColor"><path d="' + g + '"/></svg>';
    return s.replace('<svg ', '<svg class="' + (cls || '') + '" width="' + size + '" height="' + size + '" ');
  }
  // REDARC display unit (green outline), as in the app's header.
  var DISPLAY = '<svg width="62" height="40" viewBox="0 0 62 40" fill="none" stroke="currentColor" stroke-width="2.2">' +
    '<rect x="1.5" y="1.5" width="59" height="35" rx="4"/><rect x="15" y="7" width="32" height="20" rx="1"/>' +
    '<circle cx="8" cy="9" r="2.4"/><circle cx="8" cy="18" r="2.4"/><circle cx="8" cy="27" r="2.4"/>' +
    '<circle cx="54" cy="9" r="2.4"/><circle cx="54" cy="18" r="2.4"/><circle cx="54" cy="27" r="2.4"/>' +
    '<path d="M26 36.5c0-3 2-4.5 5-4.5s5 1.5 5 4.5"/></svg>';

  /* ---------------------------------------------------------------- state */
  var ents = {};           // "domain/name" -> {domain, name, short, state, value}
  var prefix = null;       // "TVMS 1 " (everything before "Battery SoC")
  var opts = {};           // select name -> [options]
  function tabFromHash() {
    return ({ '#controls': 'controls', '#charts': 'charts', '#config': 'config' })[location.hash] || 'home';
  }
  var tab = tabFromHash();
  var drawer = false;
  var chart = { metric: 'soc', per: 'hour' };
  var toast = '', toastT = 0;
  // Firmware update: cur (running version), url (release notes), and while an update or check
  // runs: log (lines shown under the progress bar), pct, active (install), dropped (rebooting).
  var fw = { log: [], pct: null, active: false, checking: false, dropped: false };
  function fwLog(line) {
    var t = new Date(), p2 = function (n) { return (n < 10 ? '0' : '') + n; };
    line = String(line).replace(/\x1b\[[0-9;]*m/g, '').replace(/\s+$/, '');
    if (!line || (fw.log.length && fw.log[fw.log.length - 1].m === line)) return;
    fw.log.push({ t: p2(t.getHours()) + ':' + p2(t.getMinutes()) + ':' + p2(t.getSeconds()), m: line });
    if (fw.log.length > 300) fw.log.shift();
    schedule();
  }
  function fwRefresh(name, then) {
    fetch('/update/' + encodeURIComponent(name) + '?detail=all')
      .then(function (r) { return r.json(); })
      .then(function (j) { fw.cur = j.current_version || ''; fw.url = j.release_url || ''; if (then) then(j); schedule(); })
      .catch(function () {});
  }

  function onState(d) {
    var id = d.name_id || d.id;
    if (!id || id.indexOf('/') < 0) return;
    var dom = id.slice(0, id.indexOf('/'));
    var name = id.slice(id.indexOf('/') + 1);
    var e = ents[id] || (ents[id] = { id: id, domain: dom, name: name });
    e.state = d.state;
    var prevState = e.state, prevValue = e.value;
    e.value = d.value;
    if (prefix === null && /battery so[ch]$/i.test(name)) prefix = name.replace(/battery so[ch]$/i, '');
    e.short = prefix ? name.replace(prefix, '') : name;
    if (dom === 'update') {
      if (fw.cur === undefined) { fw.cur = ''; fwRefresh(name); }
      var ust = String(d.state || '').toUpperCase();
      if (/INSTALLING/.test(ust) && !fw.active) {   // started here or from Home Assistant
        fw.active = true; fw.pct = 0; fwLog('Installing ' + (d.value || 'the update'));
      }
      if (fw.active && /AVAILABLE/.test(ust) && /INSTALLING/.test(String(prevState || '').toUpperCase())) {
        fw.active = false; fwLog('Install did not complete; still on ' + (fw.cur || 'the old firmware'));
      }
      if (fw.checking && !fw.active && (ust !== String(prevState || '').toUpperCase() || d.value !== prevValue || fw.checking === 'sent')) {
        fw.checking = false;
        fwLog(/AVAILABLE/.test(ust) ? 'Update available: ' + d.value + ' (running ' + (fw.cur || '?') + ')'
                                    : 'Up to date: latest on GitHub is ' + (d.value || fw.cur || '?'));
      }
    }
    if (dom === 'sensor' && /update progress$/i.test(name) && fw.active && d.value !== null && isFinite(d.value)) fw.pct = d.value;
    if (dom === 'text_sensor' && /update log$/i.test(name) && fw.active && d.state) fwLog(d.state);
    if (dom === 'select' && !opts[name]) {
      opts[name] = [];
      fetch('/select/' + encodeURIComponent(name) + '?detail=all')
        .then(function (r) { return r.json(); })
        .then(function (j) { opts[name] = j.option || []; schedule(); })
        .catch(function () {});
    }
    schedule();
  }

  function find(rx, dom) {
    for (var k in ents) {
      var e = ents[k];
      if ((!dom || e.domain === dom) && rx.test(e.short || e.name)) return e;
    }
    return null;
  }
  function num(rx, dom) {
    var e = find(rx, dom || 'sensor');
    return e && typeof e.value === 'number' && isFinite(e.value) ? e.value : null;
  }
  function txt(rx, dom) { var e = find(rx, dom || 'text_sensor'); return e ? String(e.state || '') : ''; }
  function on(rx) { var e = find(rx, 'binary_sensor'); return !!e && (e.value === true || e.state === 'ON'); }
  function f(v, d, unit) { return v === null ? '--' : v.toFixed(d) + (unit || ''); }
  function esc(s) { return String(s).replace(/[&<>"]/g, function (c) { return ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' })[c]; }); }

  function post(dom, name, action, q) {
    return fetch('/' + dom + '/' + encodeURIComponent(name) + '/' + action + (q || ''), { method: 'POST' });
  }
  function say(m) {
    toast = m; clearTimeout(toastT);
    toastT = setTimeout(function () { toast = ''; schedule(); }, 4500);
    schedule();
  }

  /* --------------------------------------------------------------- pieces */
  function linkOk() {
    var s = txt(/link status/i);
    return s !== '' && !/not |disconnect|scan|connecting|wait|error|lost|fail/i.test(s);
  }

  function header() {
    var m = find(/master|main power/i, 'switch');
    var isOn = m ? (m.value === true || m.state === 'ON') : linkOk();
    var ok = linkOk();
    return '<header class="bar"><button class="ib" data-act="drawer" aria-label="Menu">' + mi('menu', 30) + '</button>' +
      '<span class="hi ' + (ok ? 'g' : 'x') + '">' + DISPLAY + '</span>' +
      '<span class="hi ' + (ok ? 'g' : 'x') + '">' + mi('bt', 32) + '</span><span class="sp"></span>' +
      '<button class="pwr ' + (isOn ? 'on' : 'off') + '" data-act="master"' + (m ? ' data-n="' + esc(m.name) + '"' : ' disabled') + '>' +
      '<span class="lbl">' + (isOn ? 'ON' : 'OFF') + '</span><span class="knob">' + mi('power', 28) + '</span></button></header>';
  }

  function nav() {
    function b(id, icon, label) {
      return '<button class="nb' + (tab === id ? ' act' : '') + '" data-act="tab" data-n="' + id + '">' +
        mi(icon, 28) + (tab === id ? '<span>' + label + '</span>' : '') + '</button>';
    }
    return '<nav class="nav">' + b('home', 'home', 'Home') + b('controls', 'grid', 'Controls') + b('charts', 'chart', 'Charts') + b('config', 'gear', 'ESP') + '</nav>';
  }

  function battery(soc) {
    var h = soc === null ? 0 : Math.max(0, Math.min(100, soc)) * 1.12;
    return '<svg class="batt" viewBox="0 0 100 150"><defs><linearGradient id="bg" x1="0" y1="0" x2="0" y2="1">' +
      '<stop offset="0" stop-color="#8BC34A"/><stop offset="1" stop-color="#2E9A3E"/></linearGradient></defs>' +
      '<rect x="36" y="2" width="28" height="10" rx="2" fill="#fff"/>' +
      '<rect x="4" y="10" width="92" height="136" rx="6" fill="none" stroke="#fff" stroke-width="6"/>' +
      '<rect x="14" y="' + (136 - h) + '" width="72" height="' + h + '" rx="4" fill="url(#bg)"/></svg>';
  }

  function tank(label, lvl, alarm) {
    var p = lvl === null ? 0 : Math.max(0, Math.min(100, lvl));
    var top = 20, bot = 118, h = (bot - top) * p / 100;
    var red = alarm || p < 40;
    return '<div class="tk"><svg viewBox="0 0 190 170"><defs><clipPath id="tc' + label + '"><path d="M30 20V112a24 24 0 0 0 24 24H136a24 24 0 0 0 24-24V20z"/></clipPath></defs>' +
      '<rect x="30" y="' + (bot + 18 - h) + '" width="130" height="' + h + '" fill="' + (red ? '#F44336' : '#29B6F6') + '" clip-path="url(#tc' + label + ')"/>' +
      '<path d="M6 8v20h24M30 8v104a24 24 0 0 0 24 24H136a24 24 0 0 0 24-24V8" fill="none" stroke="#fff" stroke-width="7" stroke-linejoin="round"/>' +
      '<circle cx="95" cy="135" r="28" fill="#fff"/>' +
      '<path d="M95 120c-5 7-10 11-10 17a10 10 0 0 0 20 0c0-6-5-10-10-17z" fill="#03A9F4"/></svg><div class="tl">' + esc(label) + '</div></div>';
  }

  // Short current arrow under/over a branch: line from x1 to x2 with the head at x2.
  function arrow(x1, x2, y, color) {
    var s = x2 > x1 ? 1 : -1;
    return '<path d="M' + x1 + ' ' + y + 'H' + x2 + '" stroke="' + color + '" stroke-width="3"/><path d="M' + (x2 + 4 * s) + ' ' + y + 'l' +
      (-14 * s) + ' -7v14z" fill="' + color + '" stroke="none"/>';
  }

  function flow(src, load, bat) {
    var ar = '';
    if (load !== null && load > 0.05) ar += arrow(160, 200, 52, '#8BC34A');
    if (bat !== null && Math.abs(bat) > 0.05) ar += bat < 0 ? arrow(210, 164, 138, '#F44336') : arrow(164, 210, 138, '#8BC34A');
    return '<svg class="flow" viewBox="0 0 340 184" fill="none" stroke="#fff" stroke-width="2">' +
      '<rect x="18" y="62" width="48" height="48" rx="4" fill="#fff" stroke="none"/>' +
      '<path d="M44 68l-14 24h10l-4 14 16-26H42z" fill="#212121" stroke="none"/>' +
      '<path d="M70 86H150V38H252M150 86v66H252"/>' + ar +
      '<g fill="#fff" stroke="none" font-size="22" text-anchor="middle">' +
      '<text x="108" y="78">' + f(src, 1, 'A') + '</text><text x="202" y="30">' + f(load, 1, 'A') + '</text>' +
      '<text x="202" y="176">' + f(bat === null ? null : Math.abs(bat), 1, 'A') + '</text></g>' +
      '<g color="#fff" stroke="none"><g transform="translate(262 0)">' + ic('light', 56) + '</g>' +
      '<g transform="translate(262 100)"><svg width="56" height="66" viewBox="0 0 28 33" fill="none" stroke="#fff" stroke-width="1.8">' +
      '<rect x="3" y="4" width="22" height="27" rx="1.5"/><rect x="10" y="1" width="8" height="3" fill="#fff"/>' +
      '<path d="M15 10l-6 9h5l-2 7 7-10h-5z" fill="#fff" stroke="none"/></svg></g></g></svg>';
  }

  function srcBar(icon, pct, active) {
    return '<div class="src">' + ic(icon, 34) + '<div class="trk"><i style="width:' + (active ? Math.max(4, pct) : 0) + '%"></i></div></div>';
  }

  /* ----------------------------------------------------------------- tabs */
  function timeText() {
    var s = txt(/time remaining.*text|time remaining text/i);
    if (!s) {
      var m = num(/time remaining$/i);
      if (m === null) return { t: '', dir: 0 };
      var h = Math.abs(m) / 60;
      s = (h >= 48 ? Math.round(h / 24) + ' days' : h >= 1 ? Math.round(h) + ' hrs' : Math.round(Math.abs(m)) + ' min') + (m < 0 ? ' to flat' : ' to full');
    }
    var dir = /flat|empty/i.test(s) ? -1 : /full/i.test(s) && !/^full/i.test(s) ? 1 : 0;
    var t = s.replace(/\s*to (flat|full)/i, '');
    if (/days?/i.test(t)) t = '> ' + t;
    return { t: t, dir: dir };
  }

  function home() {
    var soc = num(/^battery so[ch]$/i), tr = timeText();
    var dcI = num(/^dc input current$/i), acI = num(/^ac input current$/i), soI = num(/^solar current$/i);
    var maxI = num(/battery max charge current$/i) || 40;
    var h = '<section class="card top"><div class="soc"><div class="big">' + (soc === null ? '--' : Math.round(soc)) + '%</div>' +
      '<div class="tr">' + esc(tr.t) + (tr.dir ? '<span class="dir ' + (tr.dir < 0 ? 'dn' : 'up') + '"><svg viewBox="0 0 24 24" width="30" height="30"><circle cx="12" cy="12" r="12" fill="#fff"/><path d="' +
      (tr.dir < 0 ? 'M12 5v12m-5-5l5 5 5-5' : 'M12 19V7m-5 5l5-5 5 5') + '" stroke="' + (tr.dir < 0 ? '#F44336' : '#4CAF50') + '" stroke-width="2.6" fill="none"/></svg></span>' : '') + '</div></div>' +
      battery(soc) + '<div class="srcs">' +
      // Lit while that source is charging: DC (12 V vehicle), solar, AC (240 V)
      ic('car', 40, on(/dc input active/i) ? 'act' : 'off') +
      ic('sun', 40, on(/solar input active/i) ? 'act' : 'off') +
      ic('plug', 40, on(/ac input active/i) ? 'act' : 'off') + '</div></section>';

    var t1 = num(/tank 1 level/i), t2 = num(/tank 2 level/i);
    if (t1 !== null || t2 !== null) {
      h += '<section class="card tanks">' + tank('Tank-1', t1, on(/tank 1 alarm/i)) + tank('Tank-2', t2, on(/tank 2 alarm/i)) + '</section>';
    }

    var cOut = num(/^charger output current$/i), ld = num(/^load current$/i), bc = num(/^battery current$/i);
    var src = cOut !== null ? cOut : (dcI || 0) + (acI || 0) + (soI || 0);
    if (ld !== null || bc !== null) h += '<section class="card">' + flow(src, ld, bc) + '</section>';

    if (dcI !== null || acI !== null || soI !== null) {
      h += '<section class="card srcb">' +
        srcBar('car', (dcI || 0) / maxI * 100, on(/dc input active/i)) +
        srcBar('sun', (soI || 0) / maxI * 100, on(/solar input active/i)) +
        srcBar('plug', (acI || 0) / maxI * 100, on(/ac input active/i)) + '</section>';
    }

    var v = num(/^battery voltage$/i), tp = num(/^battery temperature$/i);
    var mode = find(/^charging mode$/i, 'select'), stage = txt(/^charging stage$/i);
    h += '<section class="card bs"><div class="ct">Battery Status</div><div class="two"><b>' + f(v, 1, 'V') + '</b>' +
      (tp !== null ? '<b>' + f(tp, 0, '°C') + '</b>' : '') + '</div><hr><div class="two lab"><span>' +
      (mode ? 'Charging Mode' : '') + '</span><span>' + (stage ? 'Charging Stage' : '') + '</span></div><div class="two">';
    if (mode) {
      h += '<select class="pill" data-act="mode" data-n="' + esc(mode.name) + '">' + (opts[mode.name] || [mode.state]).map(function (o) {
        return '<option' + (o === mode.state ? ' selected' : '') + '>' + esc(o) + '</option>';
      }).join('') + '</select>';
    }
    if (stage) h += '<span class="pill">' + esc(stage) + '</span>';
    h += '</div></section>';

    var sp = num(/^solar power$/i), se = num(/solar energy today/i);
    if (sp !== null) {
      h += '<section class="card sol">' + ic('sun', 96, 'sunc') + '<div class="sv"><div>' + f(sp, 0, ' W') + '</div>' +
        (se !== null ? '<div>' + f(se, 0, ' Wh') + '</div>' : '') + '</div></section>';
    }

    var rows = [], i;
    for (i = 1; i < 5; i++) { var a = num(new RegExp('analog input ' + i, 'i')); if (a !== null) rows.push(['output', 'Analog Input ' + i, f(a, 1, 'V')]); }
    if (rows.length) h += list(rows);
    rows = [];
    var ins = num(/^inside$/i), out = num(/^outside$/i);
    if (ins !== null) rows.push(['temp', 'Inside', f(ins, 0, '°C')]);
    if (out !== null) rows.push(['temp', 'Outside', f(out, 0, '°C')]);
    if (rows.length) h += list(rows);
    return h;
  }
  function list(rows) {
    return '<section class="card rows">' + rows.map(function (r) {
      return '<div class="row">' + ic(r[0], 30) + '<span>' + esc(r[1]) + '</span><b>' + esc(r[2]) + '</b></div>';
    }).join('<hr>') + '</section>';
  }

  var ICON_MAP = [
    [/light|lamp/i, 'light'], [/accessor|12 ?v/i, 'power12'], [/compress/i, 'compressor'], [/stereo|radio|2-?way|uhf/i, 'stereo'],
    [/fridge|freez/i, 'fridge'], [/inverter|240|230|110|mains/i, 'power240'], [/pump|water|tap|shower/i, 'tap'],
    [/heat/i, 'heater'], [/fan|vent/i, 'fan'], [/usb/i, 'usb']
  ];
  function iconFor(n) {
    for (var i = 0; i < ICON_MAP.length; i++) if (ICON_MAP[i][0].test(n)) return ICON_MAP[i][1];
    return 'output';
  }

  function controls() {
    var h = '<div class="grid">';
    for (var k in ents) {
      var e = ents[k];
      if (e.domain !== 'switch' || /master|main power|^vpn /i.test(e.short)) continue;
      var onn = e.value === true || e.state === 'ON', ico = iconFor(e.short);
      var letter = ico === 'light' ? '<i class="ltr' + (ICONS.light ? '' : ' gen') + '">' + esc(e.short.charAt(0).toUpperCase()) + '</i>' : '';
      h += '<button class="tile' + (onn ? ' on' : '') + '" data-act="sw" data-n="' + esc(e.name) + '"><span class="ico">' +
        ic(ico, 84) + letter + '</span><span class="nm">' + esc(e.short) + '</span></button>';
    }
    return h + '</div>';
  }

  /* --------------------------------------------------------------- charts */
  function series() {
    function parse(s) { return s ? s.split(',').map(Number).filter(function (x) { return isFinite(x); }) : []; }
    var c = chart;
    if (c.metric === 'soc' && c.per === 'hour') return { name: 'State of Charge', unit: '%', max: 100, data: parse(txt(/soc hourly log/i)).slice(0, 24), step: 1, avg: true, n: 24 };
    if (c.metric === 'soc') return { name: 'State of Charge', unit: '%', max: 100, data: parse(txt(/soc daily max log/i)).slice(0, 30), lo: parse(txt(/soc daily min log/i)).slice(0, 30), avg: true, n: 30, day: true };
    if (c.per === 'hour') return { name: 'Solar Power', unit: 'Wh', data: parse(txt(/solar hourly log/i)).slice(0, 24), n: 24 };
    return { name: 'Solar Power', unit: 'Wh', data: parse(txt(/solar daily log/i)).slice(0, 30), n: 30, day: true };
  }
  function p2(n) { return (n < 10 ? '0' : '') + n; }
  function charts() {
    var s = series(), d = new Date(), end = new Date(d); end.setMinutes(0, 0, 0);
    var hh = end.getHours(), stamp = s.day ? p2(end.getDate()) + '/' + p2(end.getMonth() + 1) + '/' + p2(end.getFullYear() % 100)
      : (hh % 12 || 12) + ':00 ' + (hh < 12 ? 'a.m.' : 'p.m.') + ' ' + p2(end.getDate()) + '/' + p2(end.getMonth() + 1) + '/' + p2(end.getFullYear() % 100);
    var data = s.data.slice().reverse(), lo = s.lo ? s.lo.slice().reverse() : null;
    var sum = data.reduce(function (a, b) { return a + b; }, 0);
    var big = s.avg ? (data.length ? (sum / data.length).toFixed(2) : '--') : (data.length ? Math.round(sum) : '--');
    var max = s.max || Math.max.apply(null, data.concat([1]));
    var h = '<div class="sel"><select data-act="metric"><option value="soc"' + (chart.metric === 'soc' ? ' selected' : '') + '>State of Charge</option>' +
      '<option value="solar"' + (chart.metric === 'solar' ? ' selected' : '') + '>Solar Power</option></select>' +
      '<select data-act="per"><option value="hour"' + (chart.per === 'hour' ? ' selected' : '') + '>Per Hour</option>' +
      '<option value="day"' + (chart.per === 'day' ? ' selected' : '') + '>Per Day</option></select></div>' +
      '<div class="cap"><div><div class="mut">' + s.n + (s.day ? ' day' : ' hour') + ' period ending</div><div class="ts">' + stamp + '</div></div>' +
      '<div class="r"><div class="mut">' + (s.avg ? 'Average' : 'Cumulative') + '</div><div class="ts">' + big + ' <small>' + s.unit + '</small></div></div></div>';
    if (!data.length) return h + '<div class="nod">No Data for this Chart</div>';
    var W = 340, H = 300, L = 36, B = 28, bw = (W - L - 4) / s.n, i, g = '';
    for (i = 0; i <= 4; i++) {
      var y = H - B - (H - B - 6) * i / 4, v = max * i / 4;
      g += '<text x="' + (L - 8) + '" y="' + (y + 4) + '" text-anchor="end" class="ax">' + (i === 0 ? '0' : s.unit === '%' ? Math.round(v) + '%' : Math.round(v)) + '</text>';
      if (i === 2) g += '<line x1="' + L + '" x2="' + W + '" y1="' + y + '" y2="' + y + '" class="gl"/>';
    }
    var off = s.n - data.length;
    data.forEach(function (val, j) {
      var x = L + (off + j) * bw + 1, ph = (H - B - 6), vh = ph * Math.min(val, max) / max, lh = lo ? ph * Math.min(lo[j] || 0, max) / max : 0;
      g += '<rect x="' + x + '" y="6" width="' + (bw - 2) + '" height="' + ph + '" class="tk0"/>' +
        '<rect x="' + x + '" y="' + (H - B - vh) + '" width="' + (bw - 2) + '" height="' + Math.max(0, vh - lh) + '" class="bar0"/>';
      var idx = off + j, lab = '';
      if (!s.day) { var hr = (hh - s.n + idx + 24) % 24; if (hr % 6 === 5 && j > 0) lab = (hr % 12 || 12) + (hr < 12 ? ' AM' : ' PM'); }
      else if (idx % 7 === 0) { var dd = new Date(end.getTime() - (s.n - 1 - idx) * 864e5); lab = dd.getDate() + '/' + (dd.getMonth() + 1); }
      if (lab) g += '<text x="' + (x + bw / 2) + '" y="' + (H - 6) + '" text-anchor="middle" class="ax">' + lab + '</text>';
    });
    return h + '<svg class="chart" viewBox="0 0 ' + W + ' ' + H + '">' + g + '</svg>';
  }

  /* --------------------------------------------------------------- drawer */
  function quality(dbm, t) {
    return dbm >= t[0] ? 'Excellent' : dbm >= t[1] ? 'Good' : dbm >= t[2] ? 'Fair' : 'Weak';
  }
  function bars(dbm, t) {
    var n = dbm >= t[0] ? 4 : dbm >= t[1] ? 3 : dbm >= t[2] ? 2 : 1, s = '<svg width="26" height="20" viewBox="0 0 26 20">';
    for (var i = 0; i < 4; i++) s += '<rect x="' + (i * 7) + '" y="' + (16 - i * 5) + '" width="5" height="' + (4 + i * 5) + '" rx="1" fill="' + (i < n ? '#8BC34A' : '#757575') + '"/>';
    return s + '</svg>';
  }
  function sig(label, dbm, t, unit) {
    return '<div class="srow"><span>' + label + '</span><span class="sv2">' + (dbm === null ? '--' :
      bars(dbm, t) + '<b>' + Math.round(dbm) + (unit || ' dBm') + '</b><em>' + quality(dbm, t) + '</em>') + '</span></div>';
  }
  function side() {
    var wifi = num(/wifi (signal|rssi)/i), ble = num(/ble (signal|rssi)/i);
    var code = find(/pairing code/i, 'number');
    return '<div class="scrim" data-act="drawer"></div><aside class="drawer"><div class="dh"><b>REDARC Battery</b>' +
      '<button class="ib" data-act="drawer" aria-label="Close">' + mi('close', 28) + '</button></div>' +
      '<div class="ds">Connection</div>' +
      '<div class="srow"><span>Display link</span><b>' + esc(txt(/link status/i) || '--') + '</b></div>' +
      sig('Wi-Fi signal', wifi, [-50, -60, -70]) + sig('Bluetooth signal', ble, [-60, -75, -85]) +
      (find(/zigbee lqi/i, 'sensor') ? sig('Zigbee link quality', num(/zigbee lqi/i), [200, 150, 100], ' LQI') :
        find(/zigbee (signal|rssi)/i, 'sensor') ? sig('Zigbee signal', num(/zigbee (signal|rssi)/i), [-60, -75, -85]) : '') +
      '<div class="ds">Pairing</div>' +
      (on(/pairing required/i) ? '<p class="warn">The Display has not bonded with this unit yet.</p>' : '') +
      '<p class="mut">1. Open <i>Settings → Bluetooth</i> on the Display.<br>2. Tap Pair Now.<br>3. Enter the 6-digit code the Display shows.</p>' +
      (code ? '<div class="srow"><span>Pairing code</span><input id="code" inputmode="numeric" maxlength="6" placeholder="000000" value="' +
        codeVal + '"><button class="sm" data-act="code" data-n="' + esc(code.name) + '">Set</button></div>' : '') +
      '<div class="pbtn"><button class="big-btn" data-act="pair">Pair Now</button>' +
      '<button class="big-btn zb' + (zbArm ? ' arm' : '') + '" data-act="zigbee">' + (zbArm ? 'Tap again to confirm' : 'Zigbee Pair') + '</button></div>' +
      '<p class="mut zbn">Zigbee Pair makes the ESP leave its current Zigbee network and search for a new one. Turn on <i>permit join</i> in Zigbee2MQTT or ZHA first.</p></aside>';
  }
  var zbArm = false, zbT = 0, rsArm = false, rsT = 0, fwArm = false, fwT = 0;

  /* ------------------------------------------------------------- ESP config */
  function fmtUp(s) {
    if (s === null) return '--';
    var d = Math.floor(s / 86400), h = Math.floor(s % 86400 / 3600), m = Math.floor(s % 3600 / 60);
    return (d ? d + 'd ' : '') + (d || h ? h + 'h ' : '') + m + 'm';
  }
  function crow(label, val) { return '<div class="srow"><span>' + esc(label) + '</span><b>' + esc(val) + '</b></div>'; }
  function cardc(title, inner) { return '<section class="card cfg"><div class="ds">' + title + '</div>' + inner + '</section>'; }
  // One row per entity that exists: label, name pattern, domain.
  function crows(list) {
    var h = '';
    list.forEach(function (r) {
      var e = find(r[1], r[2]);
      if (e) h += crow(r[0], e.state === undefined || e.state === '' ? '--' : e.state);
    });
    return h;
  }
  // Zigbee report settings: one "Zigbee Reporting" text entity holding "V:0.2,30,300;A:...".
  // [key, title, unit of the change, which values use it]
  var ZR = [
    ['V', 'Voltage', 'V', 'Battery, solar, DC and AC input voltage'],
    ['A', 'Current', 'A', 'Battery, load, solar, charger, DC and AC current'],
    ['W', 'Power', 'W', 'Battery, solar and inverter power'],
    ['P', 'Percent', '%', 'State of charge, tank levels'],
    ['T', 'Temperature', '°C', 'Battery temperature'],
    ['M', 'Time remaining', 'min', 'Battery time remaining'],
    ['S', 'Signal', 'dB', 'Bluetooth and Wi-Fi signal']
  ];
  var zrEdit = {};   // "V0" -> what was typed, kept across re-renders until saved
  function zrParse(s) {
    var o = {};
    String(s || '').split(';').forEach(function (p) {
      var m = /^\s*([A-Za-z])\s*:\s*([^,]+),\s*([^,]+),\s*(.+)$/.exec(p);
      if (m) o[m[1].toUpperCase()] = [m[2].trim(), m[3].trim(), m[4].trim()];
    });
    return o;
  }
  function zrForm(e) {
    var cur = zrParse(e.state);
    var rows = ZR.map(function (g) {
      var c = cur[g[0]] || ['', '', ''];
      function inp(i, unit) {
        var k = g[0] + i, v = zrEdit[k] !== undefined ? zrEdit[k] : c[i];
        return '<label><input id="zr-' + k + '" inputmode="decimal" autocomplete="off" value="' + esc(v) + '"><em>' + unit + '</em></label>';
      }
      return '<div class="zrr"><div class="zrh"><b>' + g[1] + '</b><span class="mut">' + g[3] + '</span></div>' +
        '<div class="zrc">' + inp(0, g[2]) + inp(1, 's') + inp(2, 's') + '</div></div>';
    }).join('');
    return '<p class="mut zbn">Each kind of value is sent when it has changed by at least the first number, but no more often ' +
      'than the minimum interval, and at least every maximum interval even if it has not changed. Binary inputs and ' +
      'switches always report at once.</p>' +
      '<div class="zrc zrcap"><span>Change</span><span>Min interval</span><span>Max interval</span></div>' + rows +
      '<div class="pbtn"><button class="big-btn zb" data-act="zrdef" data-n="' + esc(e.name) + '">Defaults</button>' +
      '<button class="big-btn" data-act="zrsave" data-n="' + esc(e.name) + '">Save</button></div>';
  }
  // The settings text from the form, or an error message.
  function zrText(e) {
    var cur = zrParse(e.state), out = [], bad = '';
    ZR.forEach(function (g) {
      var c = cur[g[0]] || ['', '', ''];
      var v = [0, 1, 2].map(function (i) { var k = g[0] + i; return String(zrEdit[k] !== undefined ? zrEdit[k] : c[i]).trim(); });
      var ch = Number(v[0]), mn = Number(v[1]), mx = Number(v[2]);
      if (!(v[0] !== '' && ch >= 0) || !(v[1] !== '' && mn >= 0 && mn <= 86400 && Math.floor(mn) === mn) ||
          !(v[2] !== '' && mx >= 1 && mx <= 86400 && Math.floor(mx) === mx)) { bad = bad || g[1]; return; }
      out.push(g[0] + ':' + ch + ',' + mn + ',' + Math.max(mn, mx));
    });
    return bad ? { err: 'Check the ' + bad.toLowerCase() + ' numbers: a change of 0 or more, and whole seconds (max at least 1).' }
               : { text: out.join(';') };
  }
  function cfg() {
    var h = cardc('Device',
      crow('Address', location.host) + crow('Uptime', fmtUp(num(/^(esp )?uptime$/i))) +
      crows([['Wi-Fi signal', /wifi (signal|rssi)/i, 'sensor']]) +
      '<button class="big-btn rs' + (rsArm ? ' arm' : '') + '" data-act="restart">' + (rsArm ? 'Tap again to restart' : 'Restart ESP') + '</button>');

    var upd = null;
    for (var k in ents) if (ents[k].domain === 'update') { upd = ents[k]; break; }
    if (upd) {
      var st = String(upd.state || '').toUpperCase(), latest = upd.value || '';
      var avail = /AVAILABLE/.test(st), busy = /INSTALLING/.test(st);
      var chk = find(/check for update/i, 'button');
      h += cardc('Firmware', crow('Running', fw.cur || '--') + crow('Latest', latest || '--') +
        crow('Status', busy ? 'Installing… the unit reboots when done' : avail ? 'Update available' : st ? 'Up to date' : '--') +
        (fw.url ? '<p class="mut zbn"><a href="' + esc(fw.url) + '" target="_blank" rel="noopener">Release notes</a></p>' : '') +
        (fw.active ? '<div class="prog"><i style="width:' + Math.max(2, Math.min(100, fw.pct || 0)) + '%"></i></div>' +
          '<p class="mut zbn">' + (fw.dropped ? 'Rebooting…' : fw.pct === null ? 'Starting…' : Math.round(fw.pct) + '%') + '</p>' : '') +
        (fw.log.length ? '<pre class="fwlog" id="fwlog">' + fw.log.map(function (l) { return esc(l.t + '  ' + l.m); }).join('\n') + '</pre>' +
          (fw.active ? '' : '<button class="sm fwclr" data-act="fwclear">Clear log</button>') : '') +
        '<div class="pbtn">' +
        (chk && !fw.active ? '<button class="big-btn zb" data-act="fwcheck" data-n="' + esc(chk.name) + '">Check for update</button>' : '') +
        (avail && !busy && !fw.active ? '<button class="big-btn' + (fwArm ? ' rs arm' : '') + '" data-act="fwinstall" data-n="' + esc(upd.name) + '">' +
          (fwArm ? 'Tap again to install' : 'Install &amp; reboot') + '</button>' : '') +
        '</div>');
    }

    var mac = find(/display mac/i, 'text');
    var disp = (mac ? crow('Display MAC', mac.state || 'not set') +
      '<div class="srow keyrow"><input id="macin" autocomplete="off" autocapitalize="characters" placeholder="AA:BB:CC:DD:EE:FF">' +
      '<button class="sm" data-act="mac" data-n="' + esc(mac.name) + '">Set</button></div>' : '') + crows([
      ['Link', /link status/i, 'text_sensor'], ['Bluetooth signal', /ble (signal|rssi)/i, 'sensor'],
      ['Disconnects', /ble disconnects/i, 'sensor'], ['Last disconnect', /last disconnect/i, 'text_sensor'],
      ['Last pairing', /last pairing/i, 'text_sensor'], ['Pairing requests', /pairing requests/i, 'sensor'],
      ['Pairing required', /pairing required/i, 'binary_sensor']]);
    h += cardc('Display (Bluetooth)', disp + '<button class="big-btn" data-act="pair">Pair Now</button>' +
      '<p class="mut zbn">Open <i>Settings → Bluetooth</i> on the Display first, then enter the code it shows in the menu.</p>');

    var zb = find(/zigbee lqi/i, 'sensor') ? crows([['Link quality (LQI)', /zigbee lqi/i, 'sensor']]) :
      crows([['Signal', /zigbee (signal|rssi)/i, 'sensor']]);
    if (zb) {
      h += cardc('Zigbee', zb + '<button class="big-btn zb' + (zbArm ? ' arm' : '') + '" data-act="zigbee">' + (zbArm ? 'Tap again to confirm' : 'Zigbee Pair') + '</button>' +
        '<p class="mut zbn">Leaves the current Zigbee network and searches for a new one. Turn on <i>permit join</i> in Zigbee2MQTT or ZHA first.</p>');
    }
    var zr = find(/zigbee reporting$/i, 'text');
    if (zr) h += cardc('Zigbee reporting', zrForm(zr));

    var vpn = find(/^vpn connected$/i, 'binary_sensor');
    if (vpn) {
      var base = vpn.name.replace(/connected$/i, '');
      var en = find(/^vpn enabled$/i, 'switch'), key = find(/^vpn auth key override$/i, 'text');
      h += cardc('Tailscale', crows([
        ['Connected', /^vpn connected$/i, 'binary_sensor'], ['Address', /^vpn ip$/i, 'text_sensor'],
        ['Hostname', /^vpn hostname$/i, 'text_sensor'], ['Peers', /^vpn peer status$/i, 'text_sensor'],
        ['Network', /^vpn network$/i, 'text_sensor'], ['Status', /^vpn setup hint$/i, 'text_sensor'],
        ['Key source', /^vpn auth key source$/i, 'text_sensor']]) +
        (en ? '<div class="srow"><span>Enabled</span><button class="sm" data-act="sw" data-n="' + esc(en.name) + '">' +
          (en.value === true || en.state === 'ON' ? 'On' : 'Off') + '</button></div>' : '') +
        '<button class="big-btn" data-act="btn" data-n="' + esc(base + 'Reconnect') + '">Reconnect</button>' +
        (key ? '<div class="srow keyrow"><input id="vpnkey" type="password" autocomplete="off" placeholder="New auth key (tskey-auth-…)">' +
          '<button class="sm" data-act="vpnkey" data-n="' + esc(key.name) + '">Set</button></div>' : ''));
    }

    var all = Object.keys(ents).sort().map(function (k) {
      var e = ents[k]; return crow(e.domain + ' · ' + e.name, e.state === undefined ? '--' : e.state);
    }).join('');
    return h + '<section class="card cfg"><details><summary>All entities (' + Object.keys(ents).length + ')</summary>' + all + '</details></section>';
  }
  function p6(n) { n = String(Math.round(n)); return ('000000' + n).slice(-6); }

  /* --------------------------------------------------------------- render */
  // last = '' forces a full re-render; lastBody is what <main> currently holds.
  var last = '', lastBody = '', pend = 0, root, fresh = false;
  var ptrDown = false, missed = false, vkeyVal = '', macVal = '';
  document.addEventListener('pointerdown', function () { ptrDown = true; });
  // A touch scroll ends in pointercancel. Only catch up if a render was held back while the
  // pointer was down: forcing one here replaced <main> mid-swipe and stopped the scroll.
  ['pointerup', 'pointercancel'].forEach(function (t) {
    document.addEventListener(t, function () {
      setTimeout(function () { ptrDown = false; if (missed) { missed = false; schedule(); } }, 0);
    });
  });
  // Digits typed into the pairing box live here, so a re-render (which replaces the input when
  // it loses focus on the Set tap) cannot wipe them before they are sent.
  var codeVal = '';
  function sendCode() {
    var c = find(/pairing code/i, 'number'), v = parseInt(codeVal || (document.getElementById('code') || {}).value, 10);
    if (!c || !isFinite(v)) return;
    codeVal = '';
    post('number', c.name, 'set', '?value=' + v).then(function () { say('Pairing code sent.'); });
  }
  document.addEventListener('input', function (ev) {
    if (ev.target.id === 'vpnkey') { vkeyVal = ev.target.value; return; }
    if (ev.target.id === 'macin') { macVal = ev.target.value; return; }
    if (ev.target.id && ev.target.id.indexOf('zr-') === 0) { zrEdit[ev.target.id.slice(3)] = ev.target.value; return; }
    if (ev.target.id !== 'code') return;
    codeVal = ev.target.value.replace(/\D/g, '').slice(0, 6);
    if (codeVal.length === 6) sendCode();   // the Display only waits a few seconds: send on the 6th digit
  });
  function schedule() { if (!pend) pend = requestAnimationFrame(render); }
  function render() {
    pend = 0;
    if (ptrDown) { missed = true; return; }   // a re-render between press and release would swallow the tap
    var body = tab === 'home' ? home() : tab === 'controls' ? controls() : tab === 'config' ? cfg() : charts();
    var html = header() + '<main class="' + tab + '"></main>' + nav() + (drawer ? side() : '') +
      (toast ? '<div class="toast">' + esc(toast) + '</div>' : '');
    var force = last === '';
    if (!force && html === last && body === lastBody) return;
    var ae = document.activeElement;
    if (ae && (ae.tagName === 'SELECT' || ae.tagName === 'INPUT')) return;   // do not yank an open control
    // Keep the <main> scroller itself alive and only swap its contents: replacing the element
    // under a finger ends the touch scroll (live sensor updates arrive several times a second).
    var m = root.querySelector('main');
    if (html !== last) {
      var keep = m && !fresh && m.className === tab ? m : null;
      if (keep) {
        // swap the header, nav and overlays around <main> without detaching it (a detached
        // element loses its scroll position)
        var tmp = document.createElement('div'), after = false;
        tmp.innerHTML = html;
        var nm = tmp.querySelector('main');
        Array.prototype.slice.call(root.childNodes).forEach(function (k) { if (k !== keep) root.removeChild(k); });
        Array.prototype.slice.call(tmp.childNodes).forEach(function (n) {
          if (n === nm) after = true;
          else if (after) root.appendChild(n);
          else root.insertBefore(n, keep);
        });
      } else {
        root.innerHTML = html;
        m = root.querySelector('main');
        lastBody = null;   // a new, empty <main>
      }
      last = html;
    }
    fresh = false;
    if (force || body !== lastBody) {
      m.innerHTML = body; lastBody = body;
      var lg = document.getElementById('fwlog');
      if (lg) lg.scrollTop = lg.scrollHeight;
    }
    var vk = document.getElementById('vpnkey');
    if (vk && vkeyVal) vk.value = vkeyVal;
    var mi2 = document.getElementById('macin');
    if (mi2 && macVal) mi2.value = macVal;
  }

  document.addEventListener('click', function (ev) {
    var b = ev.target.closest('[data-act]');
    if (!b || b.tagName === 'SELECT') return;
    var a = b.getAttribute('data-act'), n = b.getAttribute('data-n');
    if (a === 'tab') { tab = n; fresh = true; location.hash = n === 'home' ? '' : n; last = ''; }
    else if (a === 'drawer') drawer = !drawer;
    else if (a === 'sw') post('switch', n, 'toggle');
    else if (a === 'master' && n) post('switch', n, 'toggle');
    else if (a === 'pair') {
      var pn = (prefix || '') + 'Pair Now';
      post('button', pn, 'press').then(function () { say('Pairing window open for 60 s. Enter the code shown on the Display.'); })
        .catch(function () { say('Could not reach the unit.'); });
    } else if (a === 'restart') {
      if (!rsArm) {
        rsArm = true; clearTimeout(rsT);
        rsT = setTimeout(function () { rsArm = false; last = ''; schedule(); }, 5000);
      } else {
        rsArm = false; clearTimeout(rsT);
        post('button', (prefix || '') + 'Restart', 'press')
          .then(function (r) { say(r.ok ? 'Restarting. This page reconnects in about a minute.' : 'This firmware has no Restart button.'); })
          .catch(function () { say('Could not reach the unit.'); });
      }
    } else if (a === 'btn') {
      post('button', n, 'press').then(function (r) { say(r.ok ? 'Done.' : 'That button does not exist on this firmware.'); })
        .catch(function () { say('Could not reach the unit.'); });
    } else if (a === 'vpnkey') {
      var kel = document.getElementById('vpnkey'), kv = (vkeyVal || (kel && kel.value) || '').trim();
      if (kv) {
        post('text', n, 'set', '?value=' + encodeURIComponent(kv))
          .then(function (r) { vkeyVal = ''; if (kel) kel.value = ''; say(r.ok ? 'Auth key sent. The VPN reconnects now.' : 'Could not set the auth key.'); })
          .catch(function () { say('Could not reach the unit.'); });
      }
    } else if (a === 'zigbee') {
      if (!zbArm) {
        zbArm = true; clearTimeout(zbT);
        zbT = setTimeout(function () { zbArm = false; last = ''; schedule(); }, 5000);
      } else {
        zbArm = false; clearTimeout(zbT);
        post('button', (prefix || '') + 'Zigbee Pair', 'press')
          .then(function (r) { say(r.ok ? 'Zigbee pairing started. Now allow joining in Zigbee2MQTT or ZHA.' : 'This firmware has no Zigbee Pair button.'); })
          .catch(function () { say('Could not reach the unit.'); });
      }
    } else if (a === 'fwinstall') {
      if (!fwArm) {
        fwArm = true; clearTimeout(fwT);
        fwT = setTimeout(function () { fwArm = false; last = ''; schedule(); }, 5000);
      } else {
        fwArm = false; clearTimeout(fwT);
        var up = find(/./, 'update');
        fw.log = []; fw.active = true; fw.pct = null; fw.updName = n;
        fwLog('Install requested: ' + (fw.cur || '?') + ' -> ' + ((up && up.value) || '?'));
        post('update', n, 'install')
          .then(function (r) {
            if (r.ok) fwLog('The unit is fetching the update from GitHub');
            else { fw.active = false; fwLog('Could not start the update (HTTP ' + r.status + ')'); }
          })
          .catch(function () { fw.active = false; fwLog('Could not reach the unit'); });
      }
    } else if (a === 'fwcheck') {
      fw.checking = 'sent';
      fwLog('Checking GitHub for a new version');
      setTimeout(function () { if (fw.checking) { fw.checking = false; fwLog('No answer from the check yet; try again in a minute'); } }, 20000);
      post('button', n, 'press').catch(function () { fw.checking = false; fwLog('Could not reach the unit'); });
    } else if (a === 'fwclear') {
      fw.log = [];
    } else if (a === 'mac') {
      var mel = document.getElementById('macin'), mv = (macVal || (mel && mel.value) || '').trim().toUpperCase();
      if (!/^([0-9A-F]{2}:){5}[0-9A-F]{2}$/.test(mv)) say('Enter the MAC as AA:BB:CC:DD:EE:FF.');
      else post('text', n, 'set', '?value=' + encodeURIComponent(mv))
        .then(function (r) { macVal = ''; if (mel) mel.value = ''; say(r.ok ? 'Display MAC saved. Connecting to it now.' : 'Could not set the MAC.'); })
        .catch(function () { say('Could not reach the unit.'); });
    } else if (a === 'zrsave' || a === 'zrdef') {
      var zre = find(/zigbee reporting$/i, 'text'), zt = a === 'zrdef' ? { text: '' } : (zre ? zrText(zre) : null);
      if (!zre || !zt) say('This firmware has no Zigbee reporting settings.');
      else if (zt.err) say(zt.err);
      else post('text', n, 'set', '?value=' + encodeURIComponent(zt.text))
        .then(function (r) {
          if (r.ok) zrEdit = {};
          say(!r.ok ? 'Could not save the settings.' : a === 'zrdef' ? 'Back to the default report settings.' : 'Saved. Zigbee reports use these settings now.');
        })
        .catch(function () { say('Could not reach the unit.'); });
    } else if (a === 'code') {
      sendCode();
    }
    last = ''; schedule();
  });
  document.addEventListener('change', function (ev) {
    var t = ev.target, a = t.getAttribute && t.getAttribute('data-act');
    if (a === 'mode') post('select', t.getAttribute('data-n'), 'set', '?option=' + encodeURIComponent(t.value));
    else if (a === 'metric') chart.metric = t.value;
    else if (a === 'per') chart.per = t.value;
    t.blur(); last = ''; schedule();
  });

  // The code box is limited to 6 digits: select what is in it so typing replaces it.
  document.addEventListener('focusin', function (ev) {
    if (ev.target.id === 'code') ev.target.select();
  });
  window.addEventListener('hashchange', function () {
    var t = tabFromHash();
    if (t !== tab) { tab = t; fresh = true; last = ''; schedule(); }
  });

  /* ---------------------------------------------------------------- style */
  var CSS = ':root{--bg:#424242;--bar:#212121;--card:#212121;--tx:#fff;--mut:#bdbdbd;--g:#8BC34A;--g2:#4CAF50;--ln:#616161;--sh:0 2px 5px rgba(0,0,0,.45)}' +
    '@media(prefers-color-scheme:light){:root{--bg:#F2F2F5;--bar:#fff;--card:#fff;--tx:#000713;--mut:#60646C;--ln:#DDDDE3;--sh:0 1px 4px rgba(1,6,47,.17)}' +
    '.batt rect[fill="#fff"],.flow{filter:invert(1) hue-rotate(180deg)}.batt rect[stroke="#fff"]{stroke:#000713}}' +
    '*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}html,body{margin:0;height:100%;background:var(--bg);color:var(--tx);font:16px/1.3 Inter,Roboto,-apple-system,"Segoe UI",sans-serif}' +
    '#app{max-width:520px;height:100%;margin:0 auto;display:flex;flex-direction:column;position:relative;overflow:hidden;background:var(--bg)}' +
    'button,select,input{font:inherit;color:inherit}button{border:0;background:none;cursor:pointer;padding:0}' +
    '.bar{display:flex;align-items:center;gap:18px;height:72px;padding:0 20px;background:var(--bar);box-shadow:0 2px 6px rgba(0,0,0,.5);z-index:2;flex:none}' +
    '.ib{width:40px;height:40px;display:grid;place-items:center;margin-left:-6px}.hi{display:grid;place-items:center}.hi.g{color:var(--g)}.hi.x{color:#757575}.sp{flex:1}' +
    '.pwr{position:relative;width:108px;height:40px;border-radius:20px;display:flex;align-items:center;box-shadow:var(--sh);transition:.2s}' +
    '.pwr .lbl{flex:1;text-align:center;font-size:17px;letter-spacing:.3px}.pwr .knob{width:40px;height:40px;border-radius:20px;display:grid;place-items:center;color:#fff}' +
    '.pwr.on{background:#4F6B2A;flex-direction:row}.pwr.on .knob{background:var(--g)}.pwr.off{background:#E0E0E0;color:#212121;flex-direction:row-reverse}.pwr.off .knob{background:#757575}' +
    '.pwr:disabled{cursor:default}' +
    'main{flex:1;overflow-y:auto;padding:16px 16px 20px;-webkit-overflow-scrolling:touch}.card{background:var(--card);border-radius:12px;box-shadow:var(--sh);padding:22px 20px;margin-bottom:16px}' +
    '.top{display:flex;align-items:center;justify-content:space-between;padding:26px 24px}.soc{flex:1;text-align:center}.big{font-size:54px;font-weight:300;line-height:1}' +
    '.tr{margin-top:14px;font-size:24px;display:flex;align-items:center;justify-content:center;gap:10px;min-height:32px}.dir{display:inline-flex}' +
    '.batt{width:104px;height:150px;margin:0 22px}.srcs{display:flex;flex-direction:column;gap:22px;align-items:center}.srcs svg{display:block;transition:opacity .3s,color .3s}.srcs .off{opacity:.25}.srcs .act{color:var(--g)}' +
    '.tanks{display:flex;justify-content:space-around;padding:20px 8px}.tk{text-align:center;width:48%}.tk svg{width:100%;max-width:200px;display:block;margin:0 auto}.tl{font-size:20px;margin-top:6px}' +
    '.flow{width:100%;display:block}.flow text{font-family:inherit}' +
    '.srcb{padding:22px 18px}.src{display:flex;align-items:center;gap:20px;margin:14px 0}.src svg{flex:none}.trk{flex:1;max-width:215px;height:24px;border:2px solid #fff;border-radius:12px;overflow:hidden}.trk i{display:block;height:100%;background:var(--g)}' +
    '.bs{text-align:center}.ct{font-size:22px}.two{display:flex;justify-content:space-around;align-items:center;gap:12px;margin:12px 0}.two b{font-size:40px;font-weight:300}' +
    '.lab{font-size:18px;margin:22px 0 10px}.lab span{flex:1}.bs hr,.rows hr{border:0;border-top:1px solid var(--tx);opacity:.55;margin:14px 0}' +
    '.pill{flex:1;min-width:0;background:var(--g);color:#000;border:0;border-radius:24px;padding:11px 8px;font-size:22px;text-align:center;appearance:none;-webkit-appearance:none;text-align-last:center}' +
    '.sol{display:flex;align-items:center;justify-content:space-around}.sunc{color:#FFB300}.sv{font-size:40px;text-align:center;line-height:1.5;font-weight:300}' +
    '.rows{padding:10px 20px}.row{display:flex;align-items:center;gap:20px;padding:20px 0;font-size:22px}.row span{flex:1}.row b{font-weight:300;font-size:36px}.row svg{flex:none}' +
    '.grid{display:grid;grid-template-columns:1fr 1fr;gap:20px 16px;padding:4px 2px}.tile{background:var(--card);border-radius:20px;box-shadow:var(--sh);height:172px;display:flex;flex-direction:column;align-items:center;justify-content:center;gap:16px;font-size:23px;padding:8px;border:1px solid transparent}' +
    '.tile.on{background:linear-gradient(135deg,var(--g2),var(--g));color:#fff;font-weight:600;border-color:rgba(255,255,255,.35);box-shadow:0 4px 14px rgba(139,195,74,.35)}' +
    '.ico{position:relative;display:grid;place-items:center;height:84px}.ltr{position:absolute;top:12px;left:0;right:0;text-align:center;font-style:normal;font-size:19px;color:#212121;line-height:1}.tile.on .ltr{color:#4CAF50}.ltr.gen{top:27px;font-size:15px;color:var(--tx)}.tile.on .ltr.gen{color:#fff}' +
    '.sel{display:flex;justify-content:space-between;gap:36px;padding:20px 8px 8px}.sel select{flex:1;background:none;border:0;border-bottom:1px solid var(--mut);font-size:23px;padding:8px 0;appearance:none;-webkit-appearance:none;' +
    'background:url("data:image/svg+xml,%3Csvg xmlns=\'http://www.w3.org/2000/svg\' width=\'20\' height=\'10\'%3E%3Cpath d=\'M0 0h20L10 10z\' fill=\'%23bdbdbd\'/%3E%3C/svg%3E") right 4px center no-repeat}.sel select:last-child{flex:.8}' +
    '.sel option{background:#212121;color:#fff}' +
    '.cap{display:flex;justify-content:space-between;padding:26px 8px 18px}.mut{color:var(--mut);font-size:16px}.ts{font-size:30px;font-weight:600;margin-top:4px}.ts small{font-size:18px;font-weight:400;color:var(--mut)}.r{text-align:right}' +
    '.chart{width:100%;display:block}.tk0{fill:rgba(255,255,255,.07)}.bar0{fill:var(--g)}.ax{fill:var(--mut);font-size:11px}.gl{stroke:rgba(255,255,255,.2);stroke-width:1}.nod{text-align:center;margin-top:30vh;font-size:20px}' +
    '.nav{display:flex;justify-content:space-around;align-items:center;height:96px;padding:0 10px 14px;background:var(--bar);border-top:1px solid rgba(255,255,255,.12);flex:none}' +
    '.nb{display:flex;align-items:center;gap:8px;height:52px;padding:0 16px;border-radius:26px;font-size:20px;font-weight:600}.nb.act{background:rgba(139,195,74,.2);color:var(--g)}' +
    '.scrim{position:absolute;inset:0;background:rgba(0,0,0,.55);z-index:5}.drawer{position:absolute;top:0;bottom:0;left:0;width:86%;max-width:380px;background:var(--bar);z-index:6;padding:16px 20px;overflow-y:auto;box-shadow:4px 0 16px rgba(0,0,0,.5)}' +
    '.dh{display:flex;align-items:center;justify-content:space-between;font-size:22px;margin-bottom:6px}.ds{margin:22px 0 8px;color:var(--g);font-weight:600;font-size:14px;text-transform:uppercase;letter-spacing:.8px}' +
    '.srow{display:flex;justify-content:space-between;align-items:center;gap:12px;padding:12px 0;border-bottom:1px solid var(--ln)}.sv2{display:inline-flex;align-items:center;gap:8px}.sv2 em{font-style:normal;color:var(--mut);font-size:14px}' +
    '.drawer input{width:110px;background:#303030;border:1px solid #616161;border-radius:8px;padding:8px 10px;font-size:18px;letter-spacing:2px;text-align:center}' +
    '.cfg{padding:6px 18px 18px}.cfg .ds{margin:12px 0 4px}.cfg .srow b{max-width:62%;text-align:right;word-break:break-word;font-weight:500}.cfg .big-btn{margin-top:14px}' +
    '.cfg .zb{background:transparent;color:var(--g);border:2px solid var(--g)}.cfg .zb.arm,.big-btn.rs.arm{background:#EF5350;border-color:#EF5350;color:#fff}.big-btn.rs{background:transparent;color:#EF5350;border:2px solid #EF5350}' +
    '.cfg input{flex:1;min-width:0;width:auto;letter-spacing:0;text-align:left;font-size:16px}.cfg summary{cursor:pointer;color:var(--g);padding:10px 0}.cfg details .srow{font-size:14px}' +
    '.pbtn{display:flex;gap:10px;margin-top:18px}.pbtn .big-btn{margin-top:0;flex:1;width:auto;padding:15px 6px;font-size:18px}.pbtn .zb{background:transparent;color:var(--g);border:2px solid var(--g)}.pbtn .zb.arm{background:#EF5350;border-color:#EF5350;color:#fff}.zbn{font-size:13px;margin-top:12px}' +
    '.sm{background:var(--g);color:#000;border-radius:18px;padding:8px 16px;font-weight:600}.big-btn{width:100%;margin-top:18px;background:var(--g);color:#000;border-radius:28px;padding:15px;font-size:20px;font-weight:600}.warn{color:#EF5350}' +
    '.cfg a{color:var(--g)}.prog{height:14px;border-radius:7px;background:var(--ln);overflow:hidden;margin:16px 0 0}.prog i{display:block;height:100%;background:var(--g);transition:width .4s}' +
    '.fwlog{max-height:220px;overflow:auto;background:#111;color:#cfd8dc;font:12px/1.45 ui-monospace,Menlo,Consolas,monospace;padding:10px 12px;border-radius:8px;white-space:pre-wrap;word-break:break-word;margin:12px 0 6px}.fwclr{margin-top:4px}' +
    '.zrr{padding:10px 0;border-bottom:1px solid var(--ln)}.zrh{display:flex;flex-direction:column;gap:2px}.zrh span{font-size:13px}' +
    '.zrc{display:grid;grid-template-columns:repeat(3,1fr);gap:8px;margin-top:8px}.zrcap{margin-top:4px;font-size:12px;color:var(--mut);text-align:center}' +
    '.zrc label{display:flex;align-items:center;gap:4px;background:var(--bg);border:1px solid var(--ln);border-radius:8px;padding:6px 8px}' +
    '.cfg .zrc input{flex:1;min-width:0;width:100%;background:none;border:0;padding:0;font-size:16px;text-align:right}.zrc em{font-style:normal;color:var(--mut);font-size:13px}' +
    '.toast{position:absolute;left:16px;right:16px;bottom:110px;background:#E0E0E0;color:#212121;border-radius:10px;padding:14px 16px;z-index:9;box-shadow:var(--sh)}';

  function boot() {
    document.title = 'REDARC Battery';
    var vp = document.querySelector('meta[name=viewport]') || document.head.appendChild(document.createElement('meta'));
    vp.name = 'viewport'; vp.content = 'width=device-width,initial-scale=1,viewport-fit=cover';
    var st = document.createElement('style'); st.textContent = CSS; document.head.appendChild(st);
    document.body.innerHTML = '<div id="app"></div>';
    root = document.getElementById('app');
    render();
    var es = new EventSource('/events');
    es.addEventListener('state', function (m) { try { onState(JSON.parse(m.data)); } catch (e) {} });
    // While an update runs, show the unit's own log lines (warnings and errors) under the bar.
    es.addEventListener('log', function (m) { if (fw.active) fwLog(m.data); });
    es.addEventListener('error', function () {
      if (fw.active && !fw.dropped) { fw.dropped = true; fwLog('Connection to the unit closed: rebooting into the new firmware'); }
    });
    es.addEventListener('open', function () {
      if (!fw.dropped) return;
      fw.dropped = false;
      fwLog('Unit is back online');
      var up = find(/./, 'update'), was = fw.cur;
      fwRefresh((up && up.name) || fw.updName || '', function (j) {
        fw.active = false;
        fwLog(j.current_version && j.current_version !== was ? 'Update finished: now running ' + j.current_version
                                                             : 'Running ' + (j.current_version || '?') + ' (unchanged)');
      });
    });
    setInterval(schedule, 30000);   // keep the chart clock fresh
  }
  if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', boot); else boot();
})();
