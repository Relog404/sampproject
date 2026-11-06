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
    chevron: '<path d="M9 5l7 7-7 7"/>'
  };

  function svg(name) {
    var body = ICONS[name] || ICONS.chevron;
    return '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" ' +
      'stroke-linecap="round" stroke-linejoin="round">' + body + '</svg>';
  }

  var rows = [];
  var actions = [];
  var open = false;

  var elScrim = document.getElementById('scrim');
  var elSheet = document.getElementById('sheet');
  var elTitle = document.getElementById('title');
  var elSub = document.getElementById('subtitle');
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
        RW.emit('ui:click', parseInt(node.getAttribute('data-id'), 10));
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
    show(true);
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

  /* Local preview: index.html?fake=1 */
  if (window.location.search.indexOf('fake=1') >= 0) {
    RW.fake('ui:open', ['Infernus', 'Your car']);
    RW.fake('ui:row', [0, 'Doors', 'locked', 'door']);
    RW.fake('ui:row', [1, 'Engine', 'off', 'engine', '', 'state-off']);
    RW.fake('ui:row', [4, 'Passengers', '2', 'users']);
    RW.fake('ui:action', [7, 'Refresh', '']);
    RW.fake('ui:end', []);
  }
})();
