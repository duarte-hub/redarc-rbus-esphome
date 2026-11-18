/* Development only: fakes the node's /events stream and REST API so redarc_ui.js can be
 * previewed in a browser (python3 -m http.server in this folder, then open dev.html). */
(function () {
  var P = 'TVMS 1 ';
  var S = [
    ['sensor', 'Battery SoC', 88], ['sensor', 'Battery Voltage', 13.3], ['sensor', 'Battery Temperature', 23],
    ['sensor', 'Battery Current', -0.2], ['sensor', 'Load Current', 0.2], ['sensor', 'Charger Output Current', 0],
    ['sensor', 'DC Input Current', 0], ['sensor', 'Solar Current', 0], ['sensor', 'AC Input Current', 0],
    ['sensor', 'Solar Power', 0], ['sensor', 'Solar Energy Today', 4],
    ['sensor', 'Tank 1 Level', 8], ['sensor', 'Tank 2 Level', 32],
    ['sensor', 'BLE Signal', -71], ['sensor', 'ESP WiFi Signal', -58], ['sensor', 'Zigbee Signal', -64],
    ['binary_sensor', 'DC Input Active', false], ['binary_sensor', 'Solar Input Active', false], ['binary_sensor', 'AC Input Active', false],
    ['binary_sensor', 'Pairing Required', false],
    ['text_sensor', 'Charging Stage', 'Not Charging'], ['text_sensor', 'BLE Link Status', 'Connected'],
    ['text_sensor', 'Battery Time Remaining (Text)', '28 days to flat'],
    ['text_sensor', 'SoC Hourly Log', '88,88,89,89,89,89,89,89,89,89,90,90,90,90,90,90,90,90,90,91,91,91,91,91,91,91,91,91'],
    ['text_sensor', 'Solar Hourly Log', '0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0'],
    ['sensor', 'ESP Uptime', 8123], ['sensor', 'Zigbee LQI', 198], ['sensor', 'BLE Disconnects', 2],
    ['text_sensor', 'BLE Last Disconnect', '41m ago'], ['text_sensor', 'BLE Last Pairing', 'never'],
    ['binary_sensor', 'VPN Connected', true], ['text_sensor', 'VPN IP', '100.101.102.103'],
    ['text_sensor', 'VPN Hostname', 'redarc-camper'], ['text_sensor', 'VPN Peer Status', '3 online, 2 direct'],
    ['text_sensor', 'VPN Setup Hint', 'Connected'], ['text_sensor', 'VPN Auth Key Source', 'Firmware'],
    ['switch', 'VPN Enabled', true], ['text', 'VPN Auth Key Override', ''],
    ['select', 'Charging Mode', 'Touring Mode'],
    ['number', 'Pairing Code', 123456],
    ['text', 'Display MAC', 'D4:CA:6E:00:00:01'], ['button', 'Check for Update', null]
  ];
  ['Main Light', 'Front Light', 'Left Light', 'Right Light', 'Tent Light', 'Accessories', 'Compressor', 'Stereo & 2-way', 'Fridge', 'Inverter', 'Water Pump']
    .forEach(function (n) { S.push(['switch', n, n === 'Accessories']); });
  S.push(['switch', 'Master Switch', true]);
  S.push(['update', 'Firmware', '2026.11.2']);   // value = latest version on GitHub Pages

  var running = '2026.11.1', latest = '2026.11.2', es = null;
  function emit(type, data) { ((es && es.h[type]) || []).forEach(function (fn) { fn({ data: data }); }); }
  function st(id, state, value) { emit('state', JSON.stringify({ id: id, state: state, value: value })); }
  // Fake an update: progress + log lines, a reboot (connection drops), then the new version.
  function fakeInstall() {
    var U = 'update/' + P + 'Firmware';
    st(U, 'INSTALLING', latest);
    st('text_sensor/' + P + 'Update Log', 'Downloading https://raw.githubusercontent.com/duarte-hub/redarc-rbus-esphome/ota/redarc-camper-' + latest + '.ota.bin (MD5 4e39…)', null);
    var pct = 0, t = setInterval(function () {
      pct += 12.5;
      st('sensor/' + P + 'Update Progress', String(pct), pct);
      st('text_sensor/' + P + 'Update Log', 'Downloaded and written ' + pct + '%', null);
      if (pct >= 100) {
        clearInterval(t);
        emit('log', '\x1b[0;33m[W][http_request.ota:120]: example warning line from the unit\x1b[0m');
        st('text_sensor/' + P + 'Update Log', 'Download complete, MD5 verified. Rebooting into the new firmware', null);
        setTimeout(function () { emit('error', ''); running = latest; }, 400);
        setTimeout(function () { emit('open', ''); st(U, 'NO UPDATE', latest); }, 2500);
      }
    }, 250);
  }
  function ES() {
    var self = this;
    es = this;
    this.h = {};
    setTimeout(function () {
      S.forEach(function (r) {
        var v = r[2], st = typeof v === 'boolean' ? (v ? 'ON' : 'OFF') : r[0] === 'update' ? 'UPDATE AVAILABLE' : String(v);
        var m = { data: JSON.stringify({ id: r[0] + '/' + P + r[1], state: st, value: v }) };
        (self.h.state || []).forEach(function (fn) { fn(m); });
      });
    }, 30);
  }
  ES.prototype.addEventListener = function (t, fn) { (this.h[t] = this.h[t] || []).push(fn); };
  window.EventSource = ES;

  window.fetch = function (url, o) {
    console.log('[mock]', (o && o.method) || 'GET', decodeURIComponent(url));
    if (/\/update\/.*\/install/.test(url)) setTimeout(fakeInstall, 100);
    if (/Check%20for%20Update\/press/.test(url)) setTimeout(function () { st('update/' + P + 'Firmware', 'UPDATE AVAILABLE', latest); }, 300);
    return Promise.resolve({
      ok: true,
      json: function () {
        return Promise.resolve(/\/update\//.test(url) ? { current_version: running, release_url: 'https://github.com/duarte-hub/redarc-rbus-esphome/releases' }
          : { option: ['Touring Mode', 'Storage Mode', 'Standby Mode'] });
      }
    });
  };
})();
