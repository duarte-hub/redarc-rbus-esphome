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
    ['select', 'Charging Mode', 'Touring Mode'],
    ['number', 'Pairing Code', 123456]
  ];
  ['Main Light', 'Front Light', 'Left Light', 'Right Light', 'Tent Light', 'Accessories', 'Compressor', 'Stereo & 2-way', 'Fridge', 'Inverter', 'Water Pump']
    .forEach(function (n) { S.push(['switch', n, n === 'Accessories']); });
  S.push(['switch', 'Master Switch', true]);

  function ES() {
    var self = this;
    this.h = {};
    setTimeout(function () {
      S.forEach(function (r) {
        var v = r[2], st = typeof v === 'boolean' ? (v ? 'ON' : 'OFF') : String(v);
        var m = { data: JSON.stringify({ id: r[0] + '/' + P + r[1], state: st, value: v }) };
        (self.h.state || []).forEach(function (fn) { fn(m); });
      });
    }, 30);
  }
  ES.prototype.addEventListener = function (t, fn) { (this.h[t] = this.h[t] || []).push(fn); };
  window.EventSource = ES;

  window.fetch = function (url, o) {
    console.log('[mock]', (o && o.method) || 'GET', decodeURIComponent(url));
    return Promise.resolve({
      ok: true,
      json: function () { return Promise.resolve({ option: ['Touring Mode', 'Storage Mode', 'Standby Mode'] }); }
    });
  };
})();
