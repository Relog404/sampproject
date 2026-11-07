/*  ============================================================================
 *  Инвентарь: персонаж посередине, его вещи — вокруг.
 *
 *  Сервер шлёт:   inv:open     (заголовок, подзаголовок)
 *                 inv:cell     (id, название, пометка, количество, выбран, стиль)
 *                 inv:context  (id, название, описание, характеристики)
 *                 inv:action   (id, подпись, стиль)
 *                 inv:redraw   (сейчас всё перерисуется заново)
 *                 inv:end
 *  Страница шлёт: ui:click (число) —
 *                 id            левая кнопка: выбрать вещь
 *                 1000 + id     правая кнопка: карточка вещи
 *                 2000 + id     кнопка действия в карточке
 *                 3000          закрыть
 *
 *  Ячейки: 0..49 — карманы, 50..79 — рюкзак, 80..129 — что лежит рядом,
 *  131/132/133 — рюкзак, оружие и броня на персонаже, 141..148 — одежда
 *  вокруг него, 151..153 — быстрые слоты (клавиши 1, 2, 3).
 *
 *  Раскладка слева направо: округ и рюкзак, персонаж с одеждой, карманы.
 *  Блоков рюкзака нет, если рюкзака нет: сервер помечает такие ячейки
 *  стилем locked, и страница весь блок не показывает.
 *  ========================================================================= */
(function () {
  var root = null, sub = null, card = null;
  var cardTitle = null, cardDesc = null, cardStats = null, cardActions = null;
  var opened = false;
  var grids = {};

  function el(tag, cls, text) {
    var node = document.createElement(tag);
    if (cls) node.className = cls;
    if (text !== undefined && text !== null) node.textContent = text;
    return node;
  }

  /* Простой человечек: голова, плечи, тело. Больше ничего и не нужно —
     смотрится как силуэт за стеклом, на модель не претендует. */
  function figure() {
    return '<svg viewBox="0 0 120 200" class="inv-figure" aria-hidden="true">' +
      '<circle cx="60" cy="26" r="17"/>' +
      '<path d="M60 48c-19 0-34 12-37 30l-6 40c-1 6 3 9 9 9h11l3 55c0 5 3 8 8 8h24c5 0 8-3 8-8l3-55h11c6 0 10-3 9-9l-6-40c-3-18-18-30-37-30z"/>' +
      '</svg>';
  }

  function css() {
    if (document.getElementById('inv-css')) return;
    var style = document.createElement('style');
    style.id = 'inv-css';
    style.textContent = [
      '.inv{position:fixed;inset:0;z-index:60;display:none;',
      'background:rgba(10,13,17,.62);backdrop-filter:blur(2px);',
      'font-family:"Segoe UI",Tahoma,sans-serif;color:#eeeae3}',
      '.inv.on{display:block}',
      '.inv-box{position:absolute;left:50%;top:50%;transform:translate(-50%,-50%);',
      'width:1120px;max-width:97vw;height:88vh;display:flex;gap:12px;',
      'background:#141920;border:1px solid #2a323c;border-radius:12px;padding:14px 16px;overflow:hidden}',
      '.inv-main{flex:1 1 auto;min-width:0;min-height:0;display:flex;flex-direction:column}',
      '.inv-head{display:flex;align-items:baseline;gap:10px;margin-bottom:10px;flex:0 0 auto}',
      '.inv-title{font-size:19px;letter-spacing:.04em;color:#d8c39b;text-transform:uppercase}',
      '.inv-sub{font-size:13px;color:#939ba6}',
      '.inv-x{margin-left:auto;background:none;border:1px solid #2a323c;color:#939ba6;',
      'border-radius:6px;padding:3px 10px;cursor:pointer;font-size:12px}',
      '.inv-x:hover{color:#eeeae3;border-color:#d8c39b}',
      '.inv-cols{flex:1 1 auto;min-height:0;display:flex;gap:12px}',
      '.inv-side{width:246px;flex:0 0 246px;min-height:0;display:flex;flex-direction:column;gap:10px}',
      '.inv-block{display:flex;flex-direction:column;min-height:0;background:#171c24;',
      'border:1px solid #232b36;border-radius:10px;padding:8px 9px}',
      '.inv-block.grow{flex:1 1 auto}',
      '.inv-block.off{display:none}',
      '.inv-head2{display:flex;align-items:baseline;gap:6px;margin-bottom:7px;flex:0 0 auto}',
      '.inv-h{font-size:11px;letter-spacing:.08em;text-transform:uppercase;color:#8b95a3}',
      '.inv-n{margin-left:auto;font-size:11px;color:#6f7885}',
      '.inv-grid{display:grid;grid-template-columns:repeat(5,1fr);gap:5px;',
      'min-height:0;flex:1 1 auto;overflow-y:auto}',
      '.inv-cell{position:relative;aspect-ratio:1/1;background:#1b212b;border:1px solid #2a323c;',
      'border-radius:6px;display:flex;align-items:center;justify-content:center;',
      'font-size:9.5px;line-height:1.15;text-align:center;padding:2px;cursor:pointer;',
      'user-select:none;overflow:hidden;min-width:0;word-break:break-word}',
      '.inv-cell:hover{border-color:#d8c39b;background:#212a36}',
      '.inv-cell.sel{border-color:#d8c39b;box-shadow:inset 0 0 0 1px #d8c39b}',
      '.inv-cell.empty{background:#171c24;border-style:dashed;cursor:default}',
      '.inv-cell.locked{opacity:.35;cursor:default}',
      '.inv-cell.equip{border-color:#3d4a5c;background:#1e2733}',
      '.inv-amt{position:absolute;right:3px;bottom:1px;font-size:9.5px;color:#939ba6}',
      '.inv-foot{margin-top:8px;flex:0 0 auto;font-size:11.5px;color:#6f7885}',
      /* ------------------------------ персонаж ------------------------------ */
      '.inv-hero{width:300px;flex:0 0 300px;min-height:0;display:flex;flex-direction:column;',
      'gap:8px;background:#171c24;border:1px solid #232b36;border-radius:10px;padding:10px}',
      '.inv-body{display:grid;grid-template-columns:repeat(3,58px);',
      'grid-template-rows:58px repeat(3,58px) 62px;gap:6px;justify-content:center;',
      'flex:1 1 auto;min-height:0;overflow-y:auto}',
      '.inv-fig{grid-column:2;grid-row:2 / span 3;display:flex;align-items:center;',
      'justify-content:center;background:radial-gradient(circle at 50% 40%,#232c38,#171c24 70%);',
      'border:1px solid #232b36;border-radius:10px;overflow:hidden}',
      '.inv-figure{width:74%;height:88%;fill:#2f3a48}',
      '.inv-slot{position:relative;background:#1b212b;border:1px solid #2a323c;border-radius:8px;',
      'display:flex;align-items:center;justify-content:center;text-align:center;',
      'font-size:9.5px;line-height:1.1;padding:2px;overflow:hidden;cursor:pointer;user-select:none}',
      '.inv-slot:hover{border-color:#d8c39b}',
      '.inv-slot.empty{background:#171c24;border-style:dashed;cursor:default}',
      '.inv-slot.empty:hover{border-color:#2a323c}',
      '.inv-slot.equip{border-color:#3d4a5c;background:#1e2733}',
      '.inv-slot.cloth{opacity:.75}',
      '.inv-slot.sel{border-color:#d8c39b;box-shadow:inset 0 0 0 1px #d8c39b}',
      '.inv-tag{position:absolute;left:3px;top:1px;font-size:9px;color:#6f7885}',
      '.inv-cap{position:absolute;left:0;right:0;bottom:-13px;font-size:9px;color:#6f7885;',
      'text-align:center;letter-spacing:.04em}',
      '.inv-quick{display:flex;gap:6px;justify-content:center;flex:0 0 auto}',
      '.inv-q{position:relative;width:58px;height:58px}',
      /* ------------------------------ карточка ----------------------------- */
      '.inv-card{width:250px;flex:0 0 250px;max-height:100%;overflow-y:auto;background:#1b212b;',
      'border:1px solid #2a323c;border-radius:10px;padding:14px;display:none;flex-direction:column;gap:8px}',
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

  /* Блок: заголовок, счётчик и сетка. Пустые ячейки считаем, чтобы не
     показывать «Рюкзак», когда его нет. */
  function block(cls, title, grow) {
    var wrap = el('div', 'inv-block' + (grow ? ' grow' : '') + (cls ? ' ' + cls : ''));
    var head = el('div', 'inv-head2');
    head.appendChild(el('div', 'inv-h', title));
    var count = el('div', 'inv-n', '');
    head.appendChild(count);
    var grid = el('div', 'inv-grid');
    wrap.appendChild(head);
    wrap.appendChild(grid);
    return { wrap: wrap, grid: grid, count: count };
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
    close.addEventListener('click', function () { hide(); });
    head.appendChild(close);
    main.appendChild(head);

    var cols = el('div', 'inv-cols');

    /* Слева: что лежит рядом и рюкзак. Каждому — половина высоты,
       рюкзака нет — его блок не показываем вовсе. */
    var left = el('div', 'inv-side');
    grids.ground = block('', 'Округ', true);
    grids.bag = block('bag', 'Рюкзак', true);
    left.appendChild(grids.ground.wrap);
    left.appendChild(grids.bag.wrap);
    cols.appendChild(left);

    /* Посередине: персонаж, вокруг него восемь слотов одежды,
       активное оружие и снаряжение, под ними три быстрых слота. */
    var hero = el('div', 'inv-hero');
    var body = el('div', 'inv-body');
    var fig = el('div', 'inv-fig');
    fig.innerHTML = figure();
    body.appendChild(fig);

    var around = [
      { id: 141, cls: 'cloth' }, { id: 142, cls: 'cloth' }, { id: 143, cls: 'cloth' },
      { id: 131, cls: 'equip', cap: 'рюкзак' },
      { id: 133, cls: 'equip', cap: 'снаряжение' },
      { id: 148, cls: 'cloth' }, { id: 144, cls: 'cloth' },
      { id: 147, cls: 'cloth' }, { id: 145, cls: 'cloth' },
      { id: 146, cls: 'cloth' },
      { id: 132, cls: 'equip', cap: 'в руках' }
    ];
    grids.slots = {};
    for (var i = 0; i < around.length; i++) {
      var node = el('div', 'inv-slot empty ' + around[i].cls);
      if (around[i].cap) node.appendChild(el('div', 'inv-cap', around[i].cap));
      node.setAttribute('data-slot', String(around[i].id));
      node.setAttribute('data-cap', around[i].cap || '');
      /* Одежда — задел на будущее: нажатие на эти слоты пока молчит. */
      if (around[i].id < 141) {
        node.addEventListener('click', function (event) {
          RW.emit('ui:click', parseInt(event.currentTarget.getAttribute('data-slot'), 10));
        });
        node.addEventListener('contextmenu', function (event) {
          event.preventDefault();
          RW.emit('ui:click', 1000 + parseInt(event.currentTarget.getAttribute('data-slot'), 10));
        });
      }
      body.appendChild(node);
      grids.slots[around[i].id] = node;
    }
    hero.appendChild(body);

    var quick = el('div', 'inv-quick');
    for (var q = 0; q < 3; q++) {
      var holder = el('div', 'inv-q');
      var slot = el('div', 'inv-slot empty');
      slot.appendChild(el('div', 'inv-cap', String(q + 1)));
      slot.setAttribute('data-slot', String(151 + q));
      slot.setAttribute('data-cap', String(q + 1));
      slot.addEventListener('click', function (event) {
        RW.emit('ui:click', parseInt(event.currentTarget.getAttribute('data-slot'), 10));
      });
      holder.appendChild(slot);
      quick.appendChild(holder);
      grids.slots[151 + q] = slot;
    }
    hero.appendChild(quick);
    cols.appendChild(hero);

    /* Справа: карманы. */
    var right = el('div', 'inv-side');
    grids.main = block('', 'Карманы', true);
    right.appendChild(grids.main.wrap);
    cols.appendChild(right);

    main.appendChild(cols);
    main.appendChild(el('div', 'inv-foot',
      'ЛКМ — выбрать вещь, ПКМ — описание и действия. 1, 2, 3 — быстрые слоты.'));
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
      hide();
    });
  }

  function hide() {
    show(false);
    RW.emit('ui:click', 3000);
    RW.emit('ui:closed');
  }

  function show(on) {
    build();
    opened = on;
    root.className = on ? 'inv on' : 'inv';
    if (!on && card) card.className = 'inv-card';
  }

  function fill(node, id, label, amount, selected, style, plain) {
    node.className = plain + (style ? ' ' + style : '') + (selected ? ' sel' : '');
    node.innerHTML = '';
    var cap = node.getAttribute('data-cap');
    if (cap) node.appendChild(el('div', 'inv-cap', cap));
    if (id === 131 || id === 132 || id === 133) node.classList.add('equip');
    if (id >= 141 && id <= 148) node.classList.add('cloth');
    if (label) node.appendChild(el('span', null, label));
    if (amount > 1) node.appendChild(el('span', 'inv-amt', String(amount)));
    if (node.classList.contains('cloth') && !label) node.appendChild(el('span', 'inv-tag', '·'));
    /* Пустой слот одежды — просто место; быстрый слот нажимаем даже пустым,
       иначе в него нельзя ничего положить. */
    if (style === 'empty' || style === 'locked') return;
    node.addEventListener('click', function () { RW.emit('ui:click', id); });
    node.addEventListener('contextmenu', function (event) {
      event.preventDefault();
      RW.emit('ui:click', 1000 + id);
    });
  }

  function cell(id, label, note, amount, selected, style) {
    var target = null, plain = 'inv-cell';
    if (id >= 80 && id < 130) target = grids.ground.grid;
    else if (id >= 50 && id < 80) target = grids.bag.grid;
    else if (id < 50) target = grids.main.grid;
    else if (grids.slots[id]) {
      target = null;
      var node = grids.slots[id];
      fill(node, id, label, amount, !!selected, style || '', 'inv-slot');
      if (label) node.classList.remove('empty');
      else node.classList.add('empty');
      if (node.classList.contains('cloth')) node.classList.remove('sel');
      return;
    }
    if (!target) return;
    var box = el('div', 'inv-cell');
    fill(box, id, label, amount, !!selected, style || '', 'inv-cell');
    target.appendChild(box);
  }

  RW.on('inv:open', function (title, subtitle) {
    build();
    if (title) root.querySelector('.inv-title').textContent = title;
    if (sub) sub.textContent = subtitle || '';
    clearGrids();
    card.className = 'inv-card';
    cardActions.innerHTML = '';
    show(true);
  });

  function clearGrids() {
    if (!grids.ground) return;
    grids.ground.grid.innerHTML = '';
    grids.bag.grid.innerHTML = '';
    grids.main.grid.innerHTML = '';
    for (var key in grids.slots) {
      if (!Object.prototype.hasOwnProperty.call(grids.slots, key)) continue;
      var node = grids.slots[key];
      node.innerHTML = '';
      node.className = 'inv-slot empty' + (node.classList.contains('equip') ? ' equip' : '')
        + (node.classList.contains('cloth') ? ' cloth' : '');
      if (node.getAttribute('data-cap')) node.appendChild(el('div', 'inv-cap', node.getAttribute('data-cap')));
    }
  }

  RW.on('inv:cell', function (id, label, note, amount, selected, style) {
    if (!grids.ground) return;
    cell(id, label, note, amount, !!selected, style || '');
  });

  /* Обновление раз в секунду: чистим только сетки. Карточку не трогаем,
     иначе открытое описание вещи закрывалось бы само каждую секунду. */
  RW.on('inv:redraw', clearGrids);

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

  RW.on('inv:end', function () {
    /* Рюкзака нет — его блока тоже нет: сервер помечает ячейки locked. */
    if (!grids.bag) return;
    var empty = grids.bag.grid.querySelectorAll('.locked').length;
    var all = grids.bag.grid.children.length;
    grids.bag.wrap.className = (all > 0 && empty === all) ? 'inv-block off bag' : 'inv-block grow bag';
    var count = function (node) {
      var filled = 0;
      var kids = node.grid.children;
      for (var i = 0; i < kids.length; i++) if (!kids[i].classList.contains('empty')) filled++;
      return filled + ' / ' + kids.length;
    };
    grids.main.count.textContent = count(grids.main);
    grids.ground.count.textContent = count(grids.ground);
    grids.bag.count.textContent = count(grids.bag);
  });

  RW.on('ui:close', function () { show(false); });

  /* Local preview: index.html?fake=inv renders the whole screen without the
     game, so the layout can be checked in a plain browser. */
  function demo() {
    RW.fake('inv:open', ['Инвентарь', '12.4 / 50 кг']);
    var names = ['Аптечка', 'Отмычка', 'Ключи', 'Фотоаппарат', 'Сим-карта', 'Нож'];
    for (var i = 0; i < 50; i++) {
      var filled = i % 5 !== 3;
      RW.fake('inv:cell', [i, filled ? names[i % names.length] : '', '', filled ? 1 + (i % 4) : 0,
        i === 7, filled ? '' : 'empty']);
    }
    for (var b = 50; b < 80; b++) {
      var has = b % 3 !== 1;
      RW.fake('inv:cell', [b, has ? names[b % names.length] : '', '', has ? 2 : 0, 0, has ? '' : 'locked']);
    }
    for (var g = 80; g < 130; g++) {
      var on = g % 7 === 0;
      RW.fake('inv:cell', [g, on ? names[g % names.length] : '', '', on ? 1 : 0, 0, on ? '' : 'empty']);
    }
    for (var c = 141; c <= 148; c++) RW.fake('inv:cell', [c, '', '', 0, 0, 'empty']);
    RW.fake('inv:cell', [131, 'Рюкзак', '', 1, 0, '']);
    RW.fake('inv:cell', [132, 'Пистолет', '', 34, 0, '']);
    RW.fake('inv:cell', [133, 'Броня', '', 1, 0, '']);
    RW.fake('inv:cell', [151, 'Бинт', '', 3, 0, '']);
    RW.fake('inv:cell', [152, '', '', 0, 0, 'empty']);
    RW.fake('inv:cell', [153, '', '', 0, 0, 'empty']);
    RW.fake('inv:end', []);
    RW.fake('inv:context', [7, names[1], 'Открывает замок без ключа. Одноразовая.', 'Вес: 120 г']);
    RW.fake('inv:action', [0, 'Использовать', 'ok']);
    RW.fake('inv:action', [1, 'Выбросить', 'danger']);
  }
  if (window.location.search.indexOf('fake=inv') >= 0) demo();
})();
