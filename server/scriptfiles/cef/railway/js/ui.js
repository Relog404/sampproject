/* Menu sheet: one renderer for every server menu (car card, passengers, ...).
   The server streams the content with ui:open / ui:row / ui:action / ui:end. */
(function () {
  var ICONS = {
    car: '<path d="M4 13l1.6-4.2A2 2 0 0 1 7.5 7.5h9a2 2 0 0 1 1.9 1.3L20 13v4a1 1 0 0 1-1 1h-1.5a1 1 0 0 1-1-1v-1h-9v1a1 1 0 0 1-1 1H5a1 1 0 0 1-1-1z"/><path d="M6.5 13h11"/>',
    door: '<path d="M14 3H6a1 1 0 0 0-1 1v16a1 1 0 0 0 1 1h8"/><path d="M14 3h4a1 1 0 0 1 1 1v16a1 1 0 0 1-1 1h-4"/><circle cx="11" cy="12" r="1.2"/>',
    engine: '<path d="M12 3v3"/><path d="M6.5 6.5l2 2"/><path d="M17.5 6.5l-2 2"/><path d="M4 12h3"/><path d="M17 12h3"/><circle cx="12" cy="13" r="4"/>',
    key: '<circle cx="8" cy="8" r="4"/><path d="M11 11l9 9"/><path d="M17 17l-2 2"/><path d="M20 14l-2 2"/>',
    light: '<path d="M12 3a6 6 0 0 0-3.5 10.9V17h7v-3.1A6 6 0 0 0 12 3z"/><path d="M9.5 20h5"/>',
    users: '<circle cx="9" cy="8" r="3.2"/><path d="M3.5 19c.6-3 2.8-4.6 5.5-4.6S13.9 16 14.5 19"/><path d="M16 5.5a3 3 0 0 1 0 5.6"/><path d="M17.5 14.6c1.6.6 2.7 2 3 4.4"/>',
    plane: '<path d="M10.5 20.5l1.2-4.2 5.3 2.6.6-.9-5.6-3.4 3.2-6.2a1 1 0 0 0-1.5-1.2L4 12.6v2.3l5.5-1.4-1.6 5.6a1 1 0 0 0 1.6 1z"/>',
    eject: '<path d="M12 4l7 8H5z"/><path d="M5 16h14"/>',
    refresh: '<path d="M20 11a8 8 0 1 0-2.3 6.3"/><path d="M20 5v6h-6"/>',
    close: '<path d="M6 6l12 12"/><path d="M18 6L6 18"/>',
    check: '<path d="M4 12.5l5 5L20 6.5"/>',
    alert: '<circle cx="12" cy="12" r="9"/><path d="M12 7.5v6"/><path d="M12 16.6v.4"/>',
    clock: '<circle cx="12" cy="12" r="9"/><path d="M12 7.5V12l3.2 2"/>',
    map: '<path d="M9 4.5L3.5 7v13L9 17.5l6 2.5 5.5-2.5V4.5L15 7z"/><path d="M9 4.5v13"/><path d="M15 7v13"/>',
    settings: '<circle cx="12" cy="12" r="3"/><path d="M12 2.5v3M12 18.5v3M2.5 12h3M18.5 12h3M5.2 5.2l2.1 2.1M16.7 16.7l2.1 2.1M18.8 5.2l-2.1 2.1M7.3 16.7l-2.1 2.1"/>',
    bag: '<path d="M5 8h14l-1 12H6z"/><path d="M9 8V6a3 3 0 0 1 6 0v2"/>',
    gun: '<path d="M3 10h13v4H8l-2 4H4l1-4H3z"/><path d="M16 11.5h5"/>',
    ammo: '<rect x="5" y="7" width="14" height="11" rx="2"/><path d="M9 7V5.5h6V7"/><path d="M9 12h6"/>',
    band: '<rect x="3" y="7" width="18" height="10" rx="3"/><circle cx="9" cy="12" r="1.4"/><circle cx="15" cy="12" r="1.4"/>',
    shot: '<path d="M7 4h10"/><path d="M9 4v6l-2 6a3 3 0 0 0 2.8 4h4.4A3 3 0 0 0 17 16l-2-6V4"/><path d="M8 13h8"/>',
    vest: '<path d="M6 5l4 2h4l4-2 2 5-3 2v7H7v-7L4 10z"/><path d="M10 7v12"/>',
    wallet: '<rect x="3" y="6" width="18" height="13" rx="2.5"/><path d="M3 9.8h18"/><circle cx="16.6" cy="13.6" r="1.2"/>',
    cash: '<rect x="2.5" y="6" width="19" height="12" rx="2"/><circle cx="12" cy="12" r="2.6"/><path d="M6 12h.01M18 12h.01"/>',
    info: '<circle cx="12" cy="12" r="9"/><path d="M12 11v5.6"/><path d="M12 7.6v.4"/>',
    phone: '<rect x="6.5" y="2.5" width="11" height="19" rx="2.5"/><path d="M10.5 18.6h3"/>',
    eye: '<path d="M2.5 12S6 6.2 12 6.2 21.5 12 21.5 12 18 17.8 12 17.8 2.5 12 2.5 12z"/><circle cx="12" cy="12" r="2.8"/>',
    cursor: '<path d="M5 3.2l13.6 6.3-5.9 1.6-1.6 5.9z"/>',
    app: '<rect x="4" y="4" width="6.5" height="6.5" rx="1.6"/><rect x="13.5" y="4" width="6.5" height="6.5" rx="1.6"/><rect x="4" y="13.5" width="6.5" height="6.5" rx="1.6"/><rect x="13.5" y="13.5" width="6.5" height="6.5" rx="1.6"/>',
    back: '<path d="M14.5 5.5L8 12l6.5 6.5"/>',
    next: '<path d="M9.5 5.5L16 12l-6.5 6.5"/>',
    bank: '<path d="M3.5 9.5L12 5l8.5 4.5"/><path d="M5 10.5v7M9.5 10.5v7M14.5 10.5v7M19 10.5v7"/><path d="M3 20h18"/>',
    sun: '<circle cx="12" cy="12" r="4.2"/><path d="M12 3v2.2M12 18.8V21M3 12h2.2M18.8 12H21M5.6 5.6l1.6 1.6M16.8 16.8l1.6 1.6M18.4 5.6l-1.6 1.6M7.2 16.8l-1.6 1.6"/>',
    home: '<path d="M4 11l8-6.5 8 6.5V19a1 1 0 0 1-1 1h-4.5v-6h-5v6H5a1 1 0 0 1-1-1z"/>',
    box: '<path d="M3.5 7.5L12 4l8.5 3.5v9L12 20l-8.5-3.5z"/><path d="M3.5 7.5L12 11l8.5-3.5"/><path d="M12 11v9"/>',
    camera: '<path d="M4 8h3l1.5-2h7L17 8h3a1 1 0 0 1 1 1v9a1 1 0 0 1-1 1H4a1 1 0 0 1-1-1V9a1 1 0 0 1 1-1z"/><circle cx="12" cy="13" r="3.2"/>',
    chat: '<path d="M20 12.5a7.5 7.5 0 0 1-10.9 6.7L4 20l.9-4.2A7.5 7.5 0 1 1 20 12.5z"/>',
    gear: '<circle cx="12" cy="12" r="3.2"/><path d="M12 2.8v2.4M12 18.8v2.4M4.9 7.1l2.1 1.2M17 15.7l2.1 1.2M4.9 16.9l2.1-1.2M17 8.3l2.1-1.2"/>',
    group: '<circle cx="8" cy="9" r="3"/><circle cx="16.5" cy="10.5" r="2.2"/><path d="M2.5 19c.6-2.9 2.7-4.4 5.5-4.4s4.9 1.5 5.5 4.4"/><path d="M15 14.4c2.2 0 4 .9 4.6 3.1"/>',
    law: '<path d="M12 4v16"/><path d="M7 20h10"/><path d="M5 7.5h14"/><path d="M5 7.5L2.5 13h5z"/><path d="M19 7.5l-2.5 5.5h5z"/>',
    news: '<rect x="3.5" y="5" width="17" height="14" rx="2"/><path d="M7 9h5M7 12h5M7 15h3M15 9h2.5M15 12h2.5M15 15h2.5"/>',
    quest: '<path d="M6 4h12v16H6z"/><path d="M9 9.4a3 3 0 1 1 4.3 2.7c-.8.4-1.3 1-1.3 1.9"/><path d="M12 17v.4"/>',
    radio: '<circle cx="12" cy="12" r="2.2"/><path d="M8.2 8.2a5.4 5.4 0 0 0 0 7.6M15.8 15.8a5.4 5.4 0 0 0 0-7.6"/><path d="M5.6 5.6a9 9 0 0 0 0 12.8M18.4 18.4a9 9 0 0 0 0-12.8"/>',
    signal: '<path d="M5 18v-3.5M10 18v-7M15 18v-10.5M20 18V5.5"/>',
    star: '<path d="M12 4.5l2.5 5.1 5.6.8-4 3.9.9 5.6-5-2.6-5 2.6.9-5.6-4-3.9 5.6-.8z"/>',
    target: '<circle cx="12" cy="12" r="8.5"/><circle cx="12" cy="12" r="4.5"/><circle cx="12" cy="12" r="1"/>',
    chevron: '<path d="M9 5l7 7-7 7"/>'
  };

  function svg(name) {
    var body = ICONS[name] || ICONS.chevron;
    return '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" ' +
      'stroke-linecap="round" stroke-linejoin="round">' + body + '</svg>';
  }

  var CLOSE_ID = 8;

  var rows = [];
  var actions = [];
  var open = false;

  var elScrim = document.getElementById('scrim');
  var elSheet = document.getElementById('sheet');
  var elTitle = document.getElementById('title');
  var elSub = document.getElementById('subtitle');
  var elPreview = document.getElementById('preview');
  var elRows = document.getElementById('rows');
  var elActions = document.getElementById('actions');

  function show(on) {
    open = on;
    elSheet.classList.toggle('on', on);
    elScrim.classList.toggle('on', on);
  }

  function render() {
    if (!rows.length && !actions.length) {
      elRows.innerHTML = '<div class="empty">Nothing to show</div>';
    } else {
      elRows.innerHTML = rows.map(function (r, i) {
        var value = r.value
          ? '<div class="row-value ' + (r.valueClass || '') + '">' + escapeHtml(r.value) + '</div>'
          : '';
        return '<div class="row" style="--i:' + i + '" data-id="' + r.id + '">' +
          '<div class="row-ico">' + svg(r.icon) + '</div>' +
          '<div class="row-body"><div class="row-label">' + escapeHtml(r.label) + '</div>' +
          (r.note ? '<div class="row-note">' + escapeHtml(r.note) + '</div>' : '') + '</div>' +
          value +
          '<div class="row-chev">' + svg('chevron') + '</div>' +
          '</div>';
      }).join('');
    }

    elActions.innerHTML = actions.length
      ? actions.map(function (a) {
          return '<button class="btn ' + (a.style || '') + '" data-id="' + a.id + '">' +
            escapeHtml(a.label) + '</button>';
        }).join('')
      : '';

    elRows.querySelectorAll('.row').forEach(function (node) {
      node.addEventListener('click', function () {
        RW.emit('ui:click', parseInt(node.getAttribute('data-id'), 10));
      });
    });
    elActions.querySelectorAll('.btn').forEach(function (node) {
      node.addEventListener('click', function () {
        var id = parseInt(node.getAttribute('data-id'), 10);
        RW.emit('ui:click', id);
        /* Закрыть (кнопка 8) одинакова во всех панелях: прячем лист сразу и
           говорим серверу, чтобы он снял фокус и курсор, даже если панель
           сама этот id не разобрала. */
        if (id === CLOSE_ID) {
          show(false);
          RW.emit('ui:closed');
        }
      });
    });
  }

  function escapeHtml(text) {
    return String(text)
      .replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;')
      .replace(/"/g, '&quot;');
  }

  RW.on('ui:open', function (title, subtitle) {
    rows = [];
    actions = [];
    elTitle.textContent = title || '';
    elSub.textContent = subtitle || '';
    elRows.innerHTML = '';
    elActions.innerHTML = '';
    if (elPreview) { elPreview.className = 'preview'; elPreview.innerHTML = ''; }
    show(true);
  });

  /* Машину в карточке рисует страница: сервер шлёт модель и название. */
  RW.on('ui:preview', function (model, name) {
    if (!elPreview) return;
    if (!model || model < 400 || model > 611) {
      elPreview.className = 'preview';
      elPreview.innerHTML = '';
      return;
    }
    elPreview.innerHTML =
      '<div class="preview-art">' + svg('car') + '</div>' +
      '<div class="preview-name">' + escapeHtml(name || '') + '</div>' +
      '<div class="preview-model">' + String(model) + '</div>';
    elPreview.className = 'preview on';
  });

  RW.on('ui:row', function (id, label, value, icon, note, valueClass) {
    rows.push({
      id: id,
      label: label || '',
      value: value || '',
      icon: icon || '',
      note: note || '',
      valueClass: valueClass || ''
    });
  });

  RW.on('ui:action', function (id, label, style) {
    actions.push({ id: id, label: label || '', style: style || '' });
  });

  RW.on('ui:end', function () { render(); });
  RW.on('ui:close', function () { show(false); });

  elScrim.addEventListener('click', function () {
    if (open) RW.emit('ui:closed');
  });

  document.addEventListener('keydown', function (event) {
    if (event.key === 'Escape' && open) RW.emit('ui:closed');
  });

  /* Local preview, one address per panel, no game needed:
       index.html?fake=1        index.html?fake=car
       index.html?fake=impound  index.html?fake=market
       index.html?fake=bank     index.html?fake=property
     The rows and actions below are the ones the server really sends. */
  var DEMOS = {
    car: function () {
      RW.fake('ui:open', ['Infernus', 'Your car']);
      RW.fake('ui:preview', [411, 'Infernus']);
      RW.fake('ui:row', [0, 'Doors', 'locked', 'door']);
      RW.fake('ui:row', [1, 'Engine', 'off', 'engine', '', 'state-off']);
      RW.fake('ui:row', [2, 'Key', 'inside', 'key', '', 'state-on']);
      RW.fake('ui:row', [3, 'Lights', 'off', 'light']);
      RW.fake('ui:row', [4, 'Passengers', '2', 'users']);
      RW.fake('ui:action', [7, 'Refresh', '']);
      RW.fake('ui:action', [8, 'Close', 'danger']);
    },
    impound: function () {
      RW.fake('ui:open', ['Impound lot', 'Fee 500 for a car, the lot at 1635 / -1700']);
      RW.fake('ui:row', [0, 'Infernus (token 501)', '', 'car', 'towed by Ivan, fee 500']);
      RW.fake('ui:row', [1, 'Sultan (token 502)', '', 'car', 'towed by Pavel, fee 500']);
      RW.fake('ui:row', [2, 'Nothing else on the lot', '', 'info', 'Cars appear here as they are towed']);
      RW.fake('ui:action', [8, 'Close', 'danger']);
    },
    market: function () {
      RW.fake('ui:open', ['\u041c\u0430\u0440\u043a\u0435\u0442\u043f\u043b\u0435\u0439\u0441',
        '\u041b\u043e\u0442\u043e\u0432 \u043d\u0430 \u0432\u0438\u0442\u0440\u0438\u043d\u0435: 2. \u0421\u0442\u0440\u0430\u043d\u0438\u0446\u0430 1. \u0412\u044b\u0431\u0435\u0440\u0438\u0442\u0435 \u043b\u043e\u0442 \u2014 \u0434\u0435\u0439\u0441\u0442\u0432\u0438\u0435 \u043f\u043e\u044f\u0432\u0438\u0442\u0441\u044f \u043d\u0438\u0436\u0435']);
      RW.fake('ui:row', [0, 'Medkit x3', '1 200', 'band', 'seller Ivan']);
      RW.fake('ui:row', [1, 'Desert Eagle', '18 000', 'gun', 'auction, 3 bids', 'state-on']);
      RW.fake('ui:row', [7, '\u0412\u044b\u0431\u0435\u0440\u0438\u0442\u0435 \u043b\u043e\u0442: \u043d\u0430\u0436\u043c\u0438\u0442\u0435 \u043d\u0430 \u0441\u0442\u0440\u043e\u043a\u0443 \u0432\u044b\u0448\u0435', '',
        'info', '\u0417\u0434\u0435\u0441\u044c \u043f\u043e\u044f\u0432\u0438\u0442\u0441\u044f \u043f\u043e\u043a\u0443\u043f\u043a\u0430, \u0441\u0442\u0430\u0432\u043a\u0430 \u0438\u043b\u0438 \u0441\u043d\u044f\u0442\u0438\u0435']);
      RW.fake('ui:action', [6, '\u0414\u0430\u043b\u044c\u0448\u0435', '']);
      RW.fake('ui:action', [9, '\u0417\u0430\u0431\u0440\u0430\u0442\u044c \u0441\u043e \u0441\u043a\u043b\u0430\u0434\u0430', 'wallet']);
      RW.fake('ui:action', [8, '\u0417\u0430\u043a\u0440\u044b\u0442\u044c', 'danger']);
    },
    bank: function () {
      RW.fake('ui:open', ['Bank', 'Account 12 400 / cash 3 150']);
      RW.fake('ui:row', [0, 'Put all cash on the account', 'all', 'wallet']);
      RW.fake('ui:row', [1, 'Put another sum on the account', 'sum', 'wallet']);
      RW.fake('ui:row', [2, 'Take cash from the account', 'sum', 'cash']);
      RW.fake('ui:row', [3, 'Transfer: pick a player below', 'nearby', 'users']);
      RW.fake('ui:row', [4, 'Send to Ivan (id 3)', 'transfer', 'users']);
      RW.fake('ui:row', [5, 'Send to Pavel (id 7)', 'transfer', 'users']);
      RW.fake('ui:action', [8, 'Close', 'danger']);
    },
    property: function () {
      RW.fake('ui:open', ['Property', 'Ganton, house 14']);
      RW.fake('ui:row', [0, 'House, 25 000', '', 'home']);
      RW.fake('ui:row', [1, 'Buy it for 25 000 (from the account)', 'buy', 'wallet']);
      RW.fake('ui:row', [2, 'The owner opens the doors', '', 'info']);
      RW.fake('ui:row', [3, 'Tax per payday: 500 (the owner pays)', '', 'info']);
      RW.fake('ui:row', [4, 'Go inside', 'enter', 'door']);
      RW.fake('ui:row', [5, 'Till: 0 (the owner only)', '', 'cash']);
      RW.fake('ui:row', [6, 'One owner, one property', '', 'info']);
      RW.fake('ui:row', [7, 'One owner, one property', '', 'info']);
      RW.fake('ui:action', [8, 'Close', 'danger']);
    }
  };

  function demo(name) {
    (DEMOS[name] || DEMOS.car)();
    RW.fake('ui:end', []);
  }

  var which = (window.location.search.match(/fake=([a-z0-9]+)/i) || [])[1];
  if (which === '1' || which === 'car') demo('car');
  else if (DEMOS[which]) demo(which);
})();
