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
    close: 'M19 6.41L17.59 5 12 10.59 6.41 5 5 6.41 10.59 12 5 17.59 6.41 19 12 13.41 17.59 19 19 17.59 13.41 12z'
  };
  function mi(name, size, cls) {
    return '<svg class="' + (cls || '') + '" width="' + size + '" height="' + size +
      '" viewBox="0 0 24 24" fill="currentColor"><path d="' + MI[name] + '"/></svg>';
  }
  function ic(name, size, cls) {
    var s = ICONS[name] || ICONS.output || '<svg viewBox="0 0 24 24"></svg>';
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
  var tab = ({ '#controls': 'controls', '#charts': 'charts' })[location.hash] || 'home';
  var drawer = false;
  var chart = { metric: 'soc', per: 'hour' };
  var toast = '', toastT = 0;

  function onState(d) {
    var id = d.name_id || d.id;
    if (!id || id.indexOf('/') < 0) return;
    var dom = id.slice(0, id.indexOf('/'));
    var name = id.slice(id.indexOf('/') + 1);
    var e = ents[id] || (ents[id] = { id: id, domain: dom, name: name });
    e.state = d.state;
    e.value = d.value;
    if (prefix === null && /battery so[ch]$/i.test(name)) prefix = name.replace(/battery so[ch]$/i, '');
    e.short = prefix ? name.replace(prefix, '') : name;
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
    return '<nav class="nav">' + b('home', 'home', 'Home') + b('controls', 'grid', 'Controls') + b('charts', 'chart', 'Charts') + '</nav>';
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
      battery(soc) + '<div class="srcs">' + ic('car', 40) + ic('sun', 40) + ic('plug', 40) + '</div></section>';

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
      if (e.domain !== 'switch' || /master|main power/i.test(e.short)) continue;
      var onn = e.value === true || e.state === 'ON', ico = iconFor(e.short);
      var letter = ico === 'light' ? '<i class="ltr">' + esc(e.short.charAt(0).toUpperCase()) + '</i>' : '';
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
  function sig(label, dbm, t) {
    return '<div class="srow"><span>' + label + '</span><span class="sv2">' + (dbm === null ? '--' :
      bars(dbm, t) + '<b>' + Math.round(dbm) + ' dBm</b><em>' + quality(dbm, t) + '</em>') + '</span></div>';
  }
  function side() {
    var wifi = num(/wifi (signal|rssi)/i), ble = num(/ble (signal|rssi)/i);
    var code = find(/pairing code/i, 'number');
    return '<div class="scrim" data-act="drawer"></div><aside class="drawer"><div class="dh"><b>REDARC Battery</b>' +
      '<button class="ib" data-act="drawer" aria-label="Close">' + mi('close', 28) + '</button></div>' +
      '<div class="ds">Connection</div>' +
      '<div class="srow"><span>Display link</span><b>' + esc(txt(/link status/i) || '--') + '</b></div>' +
      sig('Wi-Fi signal', wifi, [-50, -60, -70]) + sig('Bluetooth signal', ble, [-60, -75, -85]) +
      '<div class="ds">Pairing</div>' +
      (on(/pairing required/i) ? '<p class="warn">The Display has not bonded with this unit yet.</p>' : '') +
      '<p class="mut">1. Open <i>Settings → Bluetooth</i> on the Display.<br>2. Tap Pair Now.<br>3. Enter the 6-digit code the Display shows.</p>' +
      (code ? '<div class="srow"><span>Pairing code</span><input id="code" inputmode="numeric" maxlength="6" placeholder="000000" value="' +
        (code.value ? p6(code.value) : '') + '"><button class="sm" data-act="code" data-n="' + esc(code.name) + '">Set</button></div>' : '') +
      '<div class="pbtn"><button class="big-btn" data-act="pair">Pair Now</button>' +
      '<button class="big-btn zb' + (zbArm ? ' arm' : '') + '" data-act="zigbee">' + (zbArm ? 'Tap again to confirm' : 'Zigbee Pair') + '</button></div>' +
      '<p class="mut zbn">Zigbee Pair makes the ESP leave its current Zigbee network and search for a new one. Turn on <i>permit join</i> in Zigbee2MQTT or ZHA first.</p></aside>';
  }
  var zbArm = false, zbT = 0;
  function p6(n) { n = String(Math.round(n)); return ('000000' + n).slice(-6); }

  /* --------------------------------------------------------------- render */
  var last = '', pend = 0, root, fresh = false;
  function schedule() { if (!pend) pend = requestAnimationFrame(render); }
  function render() {
    pend = 0;
    var body = tab === 'home' ? home() : tab === 'controls' ? controls() : charts();
    var html = header() + '<main class="' + tab + '">' + body + '</main>' + nav() + (drawer ? side() : '') +
      (toast ? '<div class="toast">' + esc(toast) + '</div>' : '');
    if (html === last) return;
    var sc = root.querySelector('main') && !fresh ? root.querySelector('main').scrollTop : 0, ae = document.activeElement;
    fresh = false;
    if (ae && (ae.tagName === 'SELECT' || ae.id === 'code')) return;   // do not yank an open control
    last = html;
    root.innerHTML = html;
    var m = root.querySelector('main');
    if (m) m.scrollTop = sc;
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
    } else if (a === 'code') {
      var v = parseInt((document.getElementById('code') || {}).value, 10);
      if (isFinite(v)) post('number', n, 'set', '?value=' + v).then(function () { say('Pairing code sent.'); });
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

  window.addEventListener('hashchange', function () {
    var t = ({ '#controls': 'controls', '#charts': 'charts' })[location.hash] || 'home';
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
    '.batt{width:104px;height:150px;margin:0 22px}.srcs{display:flex;flex-direction:column;gap:22px;align-items:center}.srcs svg{display:block}' +
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
    '.ico{position:relative;display:grid;place-items:center;height:84px}.ltr{position:absolute;top:12px;left:0;right:0;text-align:center;font-style:normal;font-size:19px;color:#212121;line-height:1}.tile.on .ltr{color:#4CAF50}' +
    '.sel{display:flex;justify-content:space-between;gap:36px;padding:20px 8px 8px}.sel select{flex:1;background:none;border:0;border-bottom:1px solid var(--mut);font-size:23px;padding:8px 0;appearance:none;-webkit-appearance:none;' +
    'background:url("data:image/svg+xml,%3Csvg xmlns=\'http://www.w3.org/2000/svg\' width=\'20\' height=\'10\'%3E%3Cpath d=\'M0 0h20L10 10z\' fill=\'%23bdbdbd\'/%3E%3C/svg%3E") right 4px center no-repeat}.sel select:last-child{flex:.8}' +
    '.sel option{background:#212121;color:#fff}' +
    '.cap{display:flex;justify-content:space-between;padding:26px 8px 18px}.mut{color:var(--mut);font-size:16px}.ts{font-size:30px;font-weight:600;margin-top:4px}.ts small{font-size:18px;font-weight:400;color:var(--mut)}.r{text-align:right}' +
    '.chart{width:100%;display:block}.tk0{fill:rgba(255,255,255,.07)}.bar0{fill:var(--g)}.ax{fill:var(--mut);font-size:11px}.gl{stroke:rgba(255,255,255,.2);stroke-width:1}.nod{text-align:center;margin-top:30vh;font-size:20px}' +
    '.nav{display:flex;justify-content:space-around;align-items:center;height:96px;padding:0 10px 14px;background:var(--bar);border-top:1px solid rgba(255,255,255,.12);flex:none}' +
    '.nb{display:flex;align-items:center;gap:10px;height:52px;padding:0 20px;border-radius:26px;font-size:22px;font-weight:600}.nb.act{background:rgba(139,195,74,.2);color:var(--g)}' +
    '.scrim{position:absolute;inset:0;background:rgba(0,0,0,.55);z-index:5}.drawer{position:absolute;top:0;bottom:0;left:0;width:86%;max-width:380px;background:var(--bar);z-index:6;padding:16px 20px;overflow-y:auto;box-shadow:4px 0 16px rgba(0,0,0,.5)}' +
    '.dh{display:flex;align-items:center;justify-content:space-between;font-size:22px;margin-bottom:6px}.ds{margin:22px 0 8px;color:var(--g);font-weight:600;font-size:14px;text-transform:uppercase;letter-spacing:.8px}' +
    '.srow{display:flex;justify-content:space-between;align-items:center;gap:12px;padding:12px 0;border-bottom:1px solid var(--ln)}.sv2{display:inline-flex;align-items:center;gap:8px}.sv2 em{font-style:normal;color:var(--mut);font-size:14px}' +
    '.drawer input{width:110px;background:#303030;border:1px solid #616161;border-radius:8px;padding:8px 10px;font-size:18px;letter-spacing:2px;text-align:center}' +
    '.pbtn{display:flex;gap:10px;margin-top:18px}.pbtn .big-btn{margin-top:0;flex:1;width:auto;padding:15px 6px;font-size:18px}.pbtn .zb{background:transparent;color:var(--g);border:2px solid var(--g)}.pbtn .zb.arm{background:#EF5350;border-color:#EF5350;color:#fff}.zbn{font-size:13px;margin-top:12px}' +
    '.sm{background:var(--g);color:#000;border-radius:18px;padding:8px 16px;font-weight:600}.big-btn{width:100%;margin-top:18px;background:var(--g);color:#000;border-radius:28px;padding:15px;font-size:20px;font-weight:600}.warn{color:#EF5350}' +
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
    setInterval(schedule, 30000);   // keep the chart clock fresh
  }
  if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', boot); else boot();
})();
