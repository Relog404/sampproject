/* Phone OS: home grid, dock and the app screens.
   The server owns all the state - this page only renders phone:* events. */
(function () {
  var me = { number: 0, balance: 0, tariff: 0, dnd: false, wallpaper: 'dusk', ringtone: 'classic' };
  var apps = [];              // {id, name, icon, installed}
  var contacts = {};          // number -> name
  var threads = [];           // [{number, items:[{dir, text}]}]
  var screen = 'home';        // 'home' | app id | 'thread'
  var threadNumber = 0;
  var callState = 0, callNumber = 0, callName = '', callStart = 0;
  var typed = '';

  var el = {};
  ['device', 'wall', 'clock', 'dndDot', 'grid', 'dock', 'pageHome', 'pageApp',
   'homeBtn', 'call', 'callLabel', 'callWho', 'callTimer', 'callActions'].forEach(function (id) {
    el[id] = document.getElementById(id);
  });

  var ICONS = {
    phone: '<path d="M6 3h3l2 5-2.5 1.5a11 11 0 0 0 5 5L15 12l5 2v3a2 2 0 0 1-2 2A15 15 0 0 1 4 5a2 2 0 0 1 2-2z"/>',
    chat: '<path d="M4 4h16a1 1 0 0 1 1 1v10a1 1 0 0 1-1 1H9l-5 4V5a1 1 0 0 1 1-1z"/>',
    bank: '<path d="M3 9l9-6 9 6v2H3V9zm2 4h4v6H5v-6zm5 0h4v6h-4v-6zm5 0h4v6h-4v-6z"/>',
    sun: '<circle cx="12" cy="12" r="4"/><path d="M12 2v3M12 19v3M2 12h3M19 12h3M5 5l2 2M17 17l2 2M19 5l-2 2M7 17l-2 2"/>',
    group: '<circle cx="9" cy="8" r="3"/><circle cx="17" cy="10" r="2.4"/><path d="M3 20c0-3.3 2.7-5 6-5s6 1.7 6 5"/><path d="M16 20c0-2.5 1.2-4 3-4.6"/>',
    law: '<path d="M12 3v14M6 7h12M5 21h14M9 21l3-4 3 4"/>',
    car: '<path d="M4 13l2-6h12l2 6v5h-2a2 2 0 0 1-4 0H10a2 2 0 0 1-4 0H4v-5z"/>',
    news: '<path d="M4 5h11v14H4zM16 8h4v8h-4zM6 8h7v2H6zm0 4h7v2H6zm0 4h7v1H6z"/>',
    key: '<circle cx="8" cy="12" r="4"/><path d="M12 12h9v3h-3v3h-3v-3h-3z"/>',
    camera: '<path d="M4 8h3l2-2h6l2 2h3v11H4z"/><circle cx="12" cy="13" r="3.4"/>',
    gear: '<circle cx="12" cy="12" r="3.2"/><path d="M12 2v3M12 19v3M2 12h3M19 12h3M5 5l2 2M17 17l2 2M19 5l-2 2M7 17l-2 2"/>',
    signal: '<path d="M4 20V10M9 20V5M14 20v-8M19 20V3"/>',
    app: '<rect x="4" y="4" width="16" height="16" rx="4"/>'
  };
  function svg(name) {
    return '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round">' +
      (ICONS[name] || ICONS.app) + '</svg>';
  }
  function esc(text) {
    return String(text == null ? '' : text).replace(/[&<>"]/g, function (c) {
      return { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c];
    });
  }
  function who(number) {
    return contacts[number] || (number ? String(number) : 'Unknown');
  }

  /* ------------------------------------------------------------- screens */
  function showHome() {
    screen = 'home';
    el.pageApp.classList.add('off-right');
    el.pageApp.classList.add('hidden');
    el.pageHome.classList.remove('off-right');
    el.pageHome.classList.remove('hidden');
  }
  function showApp(id) {
    screen = id;
    el.pageApp.innerHTML = renderApp(id);
    el.pageApp.classList.remove('hidden');
    window.setTimeout(function () { el.pageApp.classList.remove('off-right'); }, 10);
    el.pageHome.classList.add('off-right');
    var first = el.pageApp.querySelector('input, textarea');
    if (first) first.focus();
  }
  function back() {
    if (screen === 'thread') { screen = 1; return showApp(1); }
    showHome();
  }

  /* ----------------------------------------------------------- home grid */
  function appTile(app, dock) {
    var cls = 'app' + (app.installed ? '' : ' off');
    var html = '<div class="' + cls + '" data-app="' + app.id + '" style="--i:' + app.id + '">' +
      '<div class="icon">' + svg(app.icon) + '</div><span>' + esc(app.name) + '</span></div>';
    return dock ? html : html;
  }
  function renderHome() {
    el.grid.innerHTML = apps.map(function (a) { return appTile(a, false); }).join('');
    var dockIds = [0, 1, 11, 10];
    el.dock.innerHTML = dockIds.map(function (id) {
      var app = apps.filter(function (a) { return a.id === id; })[0];
      return app ? appTile(app, true) : '';
    }).join('');
    el.wall.className = 'wall ' + me.wallpaper;
    el.dndDot.hidden = !me.dnd;
    Array.prototype.forEach.call(document.querySelectorAll('.app'), function (node) {
      node.onclick = function () { showApp(parseInt(node.getAttribute('data-app'), 10)); };
    });
  }

  /* --------------------------------------------------------- app screens */
  function head(title, extra) {
    return '<div class="page-head"><button class="back" data-back="1">&#8249; Home</button>' +
      '<h2>' + esc(title) + '</h2><span class="spacer"></span>' + (extra || '') + '</div>';
  }
  function renderApp(id) {
    if (id === 0) return screenPhone();
    if (id === 1) return screenThreads();
    if (id === 10) return screenSettings();
    if (id === 11) return screenOperator();
    return screenStub(id);
  }
  function screenPhone() {
    var digits = '';
    for (var i = 1; i <= 9; i++) digits += '<button class="key" data-digit="' + i + '">' + i + '</button>';
    return head('Phone') +
      '<div class="display" id="display">' + (typed || '&#160;') + '</div>' +
      '<div class="pad">' + digits +
      '<button class="key" data-digit="911">911</button>' +
      '<button class="key" data-digit="0">0</button>' +
      '<button class="key" data-backspace="1">&#9003;</button></div>' +
      '<div class="btn-row">' +
      '<button class="btn go wide" data-call="1">Call</button>' +
      '<button class="btn warn" data-hang="1">End</button></div>' +
      '<div class="btn-row"><button class="btn warn wide" data-sos="1">Emergency 911</button></div>';
  }
  function screenThreads() {
    if (!threads.length) {
      return head('Messages') + '<div class="empty">No messages yet.<br>Send one from the Messages app.</div>' +
        composeForm(0);
    }
    var rows = threads.map(function (t, i) {
      var last = t.items[t.items.length - 1];
      return '<div class="row" data-thread="' + t.number + '" style="--i:' + i + '">' +
        '<div class="grow"><div class="who">' + esc(who(t.number)) + '</div>' +
        '<div class="num">' + esc((last.dir ? 'You: ' : '') + last.text) + '</div></div>' +
        '<div class="side">' + t.number + '</div></div>';
    }).join('');
    return head('Messages') + '<div class="list">' + rows + '</div>' + composeForm(0);
  }
  function screenThread(number) {
    var t = threads.filter(function (x) { return x.number === number; })[0];
    var items = (t ? t.items : []).map(function (m) {
      return '<div class="bubble ' + (m.dir ? 'me' : 'them') + '">' + esc(m.text) + '</div>';
    }).join('');
    return head(who(number)) + '<div class="list" id="threadList">' + (items || '<div class="empty">No messages yet.</div>') +
      '</div>' + composeForm(number);
  }
  function composeForm(number) {
    return '<div class="field"><input id="smsNumber" inputmode="numeric" placeholder="Number" value="' +
      (number || '') + '"></div>' +
      '<div class="field"><textarea id="smsText" placeholder="Message"></textarea></div>' +
      '<button class="btn go wide" data-send="1">Send</button>';
  }
  function screenSettings() {
    var walls = ['dusk', 'ocean', 'sand', 'night'].map(function (w) {
      return '<button class="btn' + (me.wallpaper === w ? ' on' : '') + '" data-wall="' + w + '">' + w + '</button>';
    }).join('');
    var rings = ['classic', 'vibe', 'alarm'].map(function (r) {
      return '<button class="btn' + (me.ringtone === r ? ' on' : '') + '" data-ring="' + r + '">' + r + '</button>';
    }).join('');
    return head('Settings') +
      '<div class="card-soft"><div class="sub">Wallpaper</div><div class="btn-row" style="margin-top:9px">' + walls + '</div>' +
      '<div class="sub" style="margin-top:6px">Ringtone</div><div class="btn-row" style="margin-top:9px">' + rings + '</div></div>' +
      '<button class="btn ' + (me.dnd ? 'on' : '') + ' wide" data-dnd="1">Do not disturb: ' + (me.dnd ? 'ON' : 'OFF') + '</button>';
  }
  function screenOperator() {
    var tariffs = ['Start', 'Active', 'Maxi'].map(function (name, i) {
      return '<button class="btn' + (me.tariff === i ? 'on' : '') + '" data-tariff="' + i + '">' + name + '</button>';
    }).join('');
    return head('Operator') +
      '<div class="card-soft"><div class="sub">Number</div><div class="big">' + (me.number || '-') + '</div>' +
      '<div class="sub">Balance ' + me.balance + ' / tariff ' + ['Start', 'Active', 'Maxi'][me.tariff] + '</div></div>' +
      '<div class="btn-row">' + tariffs + '</div>' +
      '<div class="sub" style="margin:6px 2px">Top up from cash</div>' +
      '<div class="btn-row">' +
      '<button class="btn gold" data-topup="100">100</button>' +
      '<button class="btn gold" data-topup="500">500</button>' +
      '<button class="btn gold" data-topup="1000">1000</button></div>';
  }
  function screenStub(id) {
    var app = apps.filter(function (a) { return a.id === id; })[0];
    return head(app ? app.name : 'App') +
      '<div class="card-soft"><div class="big" style="font-size:20px">Not written yet</div>' +
      '<div class="sub">This app comes with its own system: jobs, factions, property or transport. ' +
      'Phone, messages, contacts, operator, settings and 911 already work.</div></div>' +
      (id === 3 ? '<div class="card-soft"><div class="sub">The weather affects job bonuses: ' +
        'the farmer grows faster in the sun, the mushroom picker in the rain.</div></div>' : '');
  }
  function screenSos() {
    var reasons = ['Robbery', 'Shooting', 'Hostages', 'Injured', 'Fire', 'Other'];
    return head('Emergency') + '<div class="list">' + reasons.map(function (r, i) {
      return '<div class="row" data-sos="' + i + '" style="--i:' + i + '"><div class="grow"><div class="who">' +
        esc(r) + '</div><div class="num">Sent to everyone on duty</div></div></div>';
    }).join('') + '</div>';
  }

  /* --------------------------------------------------------------- calls */
  function renderCall() {
    el.call.classList.toggle('on', callState !== 0);
    if (!callState) return;
    var label = ['', 'Calling', 'Incoming call', 'Connected'][callState];
    el.callLabel.textContent = label;
    el.callWho.textContent = callName || who(callNumber);
    el.callWho.className = 'who' + (callState === 2 ? ' ringing' : '');
    el.callActions.innerHTML = (callState === 2
      ? '<button class="round take" data-pick="1">' + svg('phone') + '</button>' +
        '<button class="round hang" data-hang="1">' + svg('phone') + '</button>'
      : '<button class="round hang" data-hang="1">' + svg('phone') + '</button>');
    el.callActions.querySelectorAll('[data-pick]').forEach(function (b) { b.onclick = function () { RW.emit('phone:pick'); }; });
    el.callActions.querySelectorAll('[data-hang]').forEach(function (b) { b.onclick = function () { RW.emit('phone:hang'); }; });
  }
  window.setInterval(function () {
    if (callState !== 3 || !callStart) { el.callTimer.textContent = ''; return; }
    var s = Math.floor((Date.now() - callStart) / 1000);
    el.callTimer.textContent = Math.floor(s / 60) + ':' + ('0' + (s % 60)).slice(-2);
  }, 500);
  window.setInterval(function () {
    var d = new Date();
    el.clock.textContent = ('0' + d.getHours()).slice(-2) + ':' + ('0' + d.getMinutes()).slice(-2);
  }, 15000);

  /* -------------------------------------------------------------- events */
  document.addEventListener('click', function (event) {
    var node = event.target.closest ? event.target.closest('[data-app],[data-back],[data-digit],[data-backspace],[data-call],[data-hang],[data-sos],[data-thread],[data-send],[data-wall],[data-ring],[data-dnd],[data-tariff],[data-topup]') : null;
    if (!node) return;
    var data = node.dataset;
    if (data.back) return back();
    if (data.app) { showApp(parseInt(data.app, 10)); return; }
    if (data.digit) {
      typed = (typed + data.digit).slice(0, 12);
      var d = document.getElementById('display');
      if (d) d.textContent = typed;
      return;
    }
    if (data.backspace) {
      typed = typed.slice(0, -1);
      var b = document.getElementById('display');
      if (b) b.textContent = typed || ' ';
      return;
    }
    if (data.hang) { RW.emit('phone:hang'); return; }
    if (data.call) {
      if (String(typed) === '911') {
        screen = 'sos';
        el.pageApp.innerHTML = screenSos();
        el.pageApp.classList.remove('hidden');
        el.pageApp.classList.remove('off-right');
        el.pageHome.classList.add('off-right');
        return;
      }
      if (!typed) return;
      RW.emit('phone:call', parseInt(typed, 10));
      return;
    }
    if (data.sos) { RW.emit('phone:911', parseInt(data.sos, 10)); return; }
    if (data.thread) {
      threadNumber = parseInt(data.thread, 10);
      screen = 'thread';
      el.pageApp.innerHTML = screenThread(threadNumber);
      return;
    }
    if (data.send) {
      var number = parseInt((document.getElementById('smsNumber') || {}).value || '0', 10);
      var text = ((document.getElementById('smsText') || {}).value || '').trim();
      if (!number || !text) return;
      RW.emit('phone:sendsms', String(number), text);
      document.getElementById('smsText').value = '';
      return;
    }
    if (data.wall || data.ring) {
      me.wallpaper = data.wall || me.wallpaper;
      me.ringtone = data.ring || me.ringtone;
      RW.emit('phone:set', me.wallpaper, me.ringtone, me.dnd ? 1 : 0);
      showApp(10);
      return;
    }
    if (data.dnd) {
      me.dnd = !me.dnd;
      RW.emit('phone:set', me.wallpaper, me.ringtone, me.dnd ? 1 : 0);
      showApp(10);
      return;
    }
    if (data.tariff) { RW.emit('phone:tariff', parseInt(data.tariff, 10)); return; }
    if (data.topup) { RW.emit('phone:topup', parseInt(data.topup, 10)); return; }
  });

  document.addEventListener('keydown', function (event) {
    if (event.key === 'Escape' || event.key === 'F4') RW.emit('phone:close');
    if (event.key === 'Backspace' && screen === 'thread') back();
    if (/^[0-9]$/.test(event.key) && screen === 0) {
      typed = (typed + event.key).slice(0, 12);
      var d = document.getElementById('display');
      if (d) d.textContent = typed;
    }
  });

  RW.on('phone:state', function (number, balance, tariff, dnd, wallpaper, ringtone) {
    me.number = number;
    me.balance = balance;
    me.tariff = tariff;
    me.dnd = !!dnd;
    if (wallpaper) me.wallpaper = wallpaper;
    if (ringtone) me.ringtone = ringtone;
    el.wall.className = 'wall ' + me.wallpaper;
    el.dndDot.hidden = !me.dnd;
    if (screen === 11) showApp(11);
    if (screen === 10) showApp(10);
  });
  RW.on('phone:apps', function (id, name, icon, installed) {
    apps = apps.filter(function (a) { return a.id !== id; });
    apps.push({ id: id, name: name, icon: icon, installed: !!installed });
  });
  RW.on('phone:appsend', function () {
    apps.sort(function (a, b) { return a.id - b.id; });
    renderHome();
    el.device.classList.add('on');
  });
  RW.on('phone:contact', function (number, name) { contacts[number] = name; });
  RW.on('phone:contactend', function () {
    if (screen === 1) showApp(1);
    if (screen === 'thread') el.pageApp.innerHTML = screenThread(threadNumber);
  });
  RW.on('phone:smsin', function (direction, number, text) {
    var t = threads.filter(function (x) { return x.number === number; })[0];
    if (!t) { t = { number: number, items: [] }; threads.push(t); }
    t.items.push({ dir: direction, text: text });
    if (t.items.length > 40) t.items.shift();
    if (screen === 1) showApp(1);
    if (screen === 'thread' && threadNumber === number) {
      el.pageApp.innerHTML = screenThread(number);
      var list = document.getElementById('threadList');
      if (list) list.scrollTop = list.scrollHeight;
    }
  });
  RW.on('phone:callstate', function (state, number, name) {
    callState = state;
    callNumber = number;
    callName = name || '';
    if (state === 3) callStart = Date.now();
    if (state === 0) { callStart = 0; typed = ''; }
    renderCall();
  });
  RW.on('phone:close', function () {
    el.device.classList.remove('on');
  });

  /* Local preview: ?fake=1, or just opening the file in a browser. */
  function demo() {
    RW.fake('phone:state', [555123, 240, 1, 0, 'dusk', 'classic']);
    RW.fake('phone:apps', [0, 'Phone', 'phone', 1]);
    RW.fake('phone:apps', [1, 'Messages', 'chat', 1]);
    RW.fake('phone:apps', [2, 'Bank', 'bank', 1]);
    RW.fake('phone:apps', [3, 'Weather', 'sun', 1]);
    RW.fake('phone:apps', [4, 'Group', 'group', 1]);
    RW.fake('phone:apps', [5, 'Lawyers', 'law', 1]);
    RW.fake('phone:apps', [6, 'Uber Drive', 'car', 1]);
    RW.fake('phone:apps', [7, 'Weazel News', 'news', 1]);
    RW.fake('phone:apps', [8, 'Auto Plus', 'key', 1]);
    RW.fake('phone:apps', [9, 'Camera', 'camera', 1]);
    RW.fake('phone:apps', [10, 'Settings', 'gear', 1]);
    RW.fake('phone:apps', [11, 'Operator', 'signal', 1]);
    RW.fake('phone:appsend', []);
    RW.fake('phone:contact', [555777, 'Alex']);
    RW.fake('phone:contact', [555901, 'Dispatch']);
    RW.fake('phone:contactend', []);
    RW.fake('phone:smsin', [0, 555777, 'Where are you?']);
    RW.fake('phone:smsin', [1, 555777, 'At the station, platform two']);
  }
  if (window.location.search.indexOf('fake=1') >= 0) demo();
  else window.setTimeout(function () { if (!RW.ready()) demo(); }, 200);
})();
