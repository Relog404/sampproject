/*  ============================================================================
 *  Инвентарь: сетка вещей и карточка по правой кнопке.
 *
 *  Сервер шлёт:   inv:open     (заголовок, подзаголовок)
 *                 inv:cell     (id, название, пометка, количество, выбран, стиль)
 *                 inv:context  (id, название, описание, характеристики)
 *                 inv:action   (id, подпись, стиль)
 *                 inv:end
 *  Страница шлёт: ui:click (число) —
 *                 id            левая кнопка: выбрать вещь
 *                 1000 + id     правая кнопка: карточка вещи
 *                 2000 + id     кнопка действия в карточке
 *                 3000          закрыть
 *
 *  Ячейки: 0..79 — карманы и рюкзак, 80..129 — то, что лежит рядом,
 *  131/132/133 — рюкзак, оружие и броня на персонаже.
 *  ========================================================================= */
(function () {
  var root = null, grid = null, sub = null, card = null;
  var cardTitle = null, cardDesc = null, cardStats = null, cardActions = null;
  var opened = false;

  function el(tag, cls, text) {
    var node = document.createElement(tag);
    if (cls) node.className = cls;
    if (text !== undefined && text !== null) node.textContent = text;
    return node;
  }

  function css() {
    if (document.getElementById('inv-css')) return;
    var style = document.createElement('style');
    style.id = 'inv-css';
    style.textContent = [
      '.inv{position:fixed;left:0;top:0;right:0;bottom:0;z-index:60;display:none;',
      'background:rgba(10,13,17,.62);backdrop-filter:blur(2px);',
      'font-family:"Segoe UI",Tahoma,sans-serif;color:#eeeae3}',
      '.inv.on{display:block}',
      '.inv-box{position:absolute;left:50%;top:50%;transform:translate(-50%,-50%);',
      'width:920px;max-width:96vw;max-height:88vh;display:flex;gap:14px;',
      'background:#141920;border:1px solid #2a323c;border-radius:12px;padding:16px 18px}',
      '.inv-main{flex:1 1 auto;min-width:0;display:flex;flex-direction:column}',
      '.inv-head{display:flex;align-items:baseline;gap:10px;margin-bottom:10px}',
      '.inv-title{font-size:19px;letter-spacing:.04em;color:#d8c39b;text-transform:uppercase}',
      '.inv-sub{font-size:13px;color:#939ba6}',
      '.inv-x{margin-left:auto;background:none;border:1px solid #2a323c;color:#939ba6;',
      'border-radius:6px;padding:3px 10px;cursor:pointer;font-size:12px}',
      '.inv-x:hover{color:#eeeae3;border-color:#d8c39b}',
      '.inv-grid{display:grid;grid-template-columns:repeat(10,1fr);gap:6px;',
      'overflow-y:auto;padding-right:4px}',
      '.inv-cell{position:relative;aspect-ratio:1/1;background:#1b212b;border:1px solid #2a323c;',
      'border-radius:6px;display:flex;align-items:center;justify-content:center;',
      'font-size:10px;line-height:1.15;text-align:center;padding:3px;cursor:pointer;',
      'user-select:none;overflow:hidden}',
      '.inv-cell:hover{border-color:#d8c39b;background:#212a36}',
      '.inv-cell.sel{border-color:#d8c39b;box-shadow:inset 0 0 0 1px #d8c39b}',
      '.inv-cell.empty{background:#171c24;border-style:dashed;cursor:default}',
      '.inv-cell.locked{opacity:.35;cursor:default}',
      '.inv-cell.equip{border-color:#3d4a5c;background:#1e2733}',
      '.inv-amt{position:absolute;right:3px;bottom:2px;font-size:10px;color:#939ba6}',
      '.inv-foot{margin-top:10px;font-size:12px;color:#6f7885}',
      '.inv-card{width:250px;flex:0 0 250px;background:#1b212b;border:1px solid #2a323c;',
      'border-radius:10px;padding:14px;display:none;flex-direction:column;gap:8px}',
      '.inv-card.on{display:flex}',
      '.inv-card h3{margin:0;font-size:16px;color:#d8c39b;font-weight:600}',
      '.inv-card p{margin:0;font-size:12px;color:#b9c1cc;line-height:1.45}',
      '.inv-card .inv-stats{font-size:12px;color:#939ba6;margin-top:auto}',
      '.inv-card button{background:#232c38;border:1px solid #2a323c;color:#eeeae3;',
      'border-radius:6px;padding:7px 10px;font-size:13px;cursor:pointer}',
      '.inv-card button:hover{border-color:#d8c39b}',
      '.inv-card button.danger:hover{border-color:#e0705f;color:#e0705f}',
      '.inv-card button.ok:hover{border-color:#7bd88f;color:#7bd88f}'
    ].join('\n');
    document.head.appendChild(style);
  }

  function build() {
    if (root) return;
    css();
    root = el('div', 'inv');
    var box = el('div', 'inv-box');

    var main = el('div', 'inv-main');
    var head = el('div', 'inv-head');
    head.appendChild(el('div', 'inv-title', 'Инвентарь'));
    sub = el('div', 'inv-sub', '');
    head.appendChild(sub);
    var close = el('button', 'inv-x', 'ESC');
    close.addEventListener('click', function () {
      show(false);
      RW.emit('ui:click', 3000);
      RW.emit('ui:closed');
    });
    head.appendChild(close);
    main.appendChild(head);

    grid = el('div', 'inv-grid');
    main.appendChild(grid);
    main.appendChild(el('div', 'inv-foot', 'ЛКМ — выбрать вещь, ПКМ — описание и действия.'));
    box.appendChild(main);

    card = el('div', 'inv-card');
    cardTitle = el('h3', null, '');
    cardDesc = el('p', null, '');
    cardStats = el('div', 'inv-stats', '');
    cardActions = el('div', 'inv-actions');
    cardActions.style.display = 'flex';
    cardActions.style.flexDirection = 'column';
    cardActions.style.gap = '6px';
    card.appendChild(cardTitle);
    card.appendChild(cardDesc);
    card.appendChild(cardStats);
    card.appendChild(cardActions);
    box.appendChild(card);

    root.appendChild(box);
    document.body.appendChild(root);

    document.addEventListener('keydown', function (event) {
      if (event.key !== 'Escape' || !opened) return;
      show(false);
      RW.emit('ui:click', 3000);
      RW.emit('ui:closed');
    });
  }

  function show(on) {
    build();
    opened = on;
    root.className = on ? 'inv on' : 'inv';
    if (!on && card) card.className = 'inv-card';
  }

  function cell(id, label, note, amount, selected, style) {
    var node = el('div', 'inv-cell' + (style ? ' ' + style : '') + (selected ? ' sel' : '') + (id >= 131 ? ' equip' : ''));
    if (label) node.appendChild(el('span', null, label));
    if (amount > 1) node.appendChild(el('span', 'inv-amt', String(amount)));
    if (style !== 'empty' && style !== 'locked') {
      node.addEventListener('click', function () { RW.emit('ui:click', id); });
      node.addEventListener('contextmenu', function (event) {
        event.preventDefault();
        RW.emit('ui:click', 1000 + id);
      });
    }
    return node;
  }

  RW.on('inv:open', function (title, subtitle) {
    build();
    if (title) root.querySelector('.inv-title').textContent = title;
    if (sub) sub.textContent = subtitle || '';
    grid.innerHTML = '';
    card.className = 'inv-card';
    cardActions.innerHTML = '';
    show(true);
  });

  RW.on('inv:cell', function (id, label, note, amount, selected, style) {
    if (!grid) return;
    grid.appendChild(cell(id, label, note, amount, !!selected, style || ''));
  });

  /* Обновление раз в секунду: чистим только сетку. Карточку не трогаем,
     иначе открытое описание вещи закрывалось бы само каждую секунду. */
  RW.on('inv:redraw', function () {
    if (grid) grid.innerHTML = '';
  });

  RW.on('inv:context', function (id, title, desc, stats) {
    if (id < 0) { if (card) card.className = 'inv-card'; return; }
    build();
    cardActions.innerHTML = '';
    cardTitle.textContent = title || '';
    cardDesc.textContent = desc || '';
    cardStats.textContent = stats || '';
    card.className = 'inv-card on';
  });

  RW.on('inv:action', function (id, label, style) {
    if (!cardActions) return;
    var button = el('button', style || '', label);
    button.addEventListener('click', function () { RW.emit('ui:click', 2000 + id); });
    cardActions.appendChild(button);
    card.className = 'inv-card on';
  });

  RW.on('inv:end', function () { if (grid) grid.style.opacity = '1'; });

  RW.on('ui:close', function () { show(false); });

  /* Local preview: index.html?fake=inv renders the grid without the game,
     so the layout can be checked in a plain browser. */
  function demo() {
    RW.fake('inv:open', ['Инвентарь', '12.4 / 50 кг']);
    var names = ['Аптечка', 'Отмычка', 'Ключи', 'Фотоаппарат', 'Сим-карта', 'Нож'];
    for (var i = 0; i < 80; i++) {
      var filled = i % 5 !== 3;
      RW.fake('inv:cell', [i, filled ? names[i % names.length] : '', '', filled ? 1 + (i % 4) : 0,
        i === 7, filled ? '' : (i >= 50 ? 'locked' : 'empty')]);
    }
    RW.fake('inv:cell', [131, 'Рюкзак', '', 1, 0, '']);
    RW.fake('inv:cell', [132, 'Пистолет', '', 34, 0, '']);
    RW.fake('inv:cell', [133, 'Броня', '', 1, 0, '']);
    RW.fake('inv:end', []);
    RW.fake('inv:context', [7, names[1], 'Открывает замок без ключа. Одноразовая.', 'Вес: 120 г']);
    RW.fake('inv:action', [0, 'Использовать', 'ok']);
    RW.fake('inv:action', [1, 'Выбросить', 'danger']);
  }
  if (window.location.search.indexOf('fake=inv') >= 0) demo();
})();
