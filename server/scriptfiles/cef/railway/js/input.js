/*  ============================================================================
 *  Поле ввода: цена лота, ставка на аукционе, сумма в кассе.
 *
 *  Сервер шлёт:   ui:ask (заголовок, подзаголовок, пример)
 *  Страница шлёт: ui:answer (что набрал игрок)
 *                 ui:closed — отмена, крестик или Esc
 *
 *  Страницей управляет сервер: он же решает, число это или нет. Здесь только
 *  поле, две кнопки и клавиши Enter и Escape.
 *  ========================================================================= */
(function () {
  var root = null, field = null, title = null, sub = null;
  var opened = false;

  function el(tag, cls, text) {
    var node = document.createElement(tag);
    if (cls) node.className = cls;
    if (text !== undefined && text !== null) node.textContent = text;
    return node;
  }

  function css() {
    if (document.getElementById('ask-css')) return;
    var style = document.createElement('style');
    style.id = 'ask-css';
    style.textContent = [
      '.ask{position:fixed;inset:0;z-index:90;display:none;align-items:center;',
      'justify-content:center;background:rgba(10,13,17,.62);backdrop-filter:blur(2px);',
      'font-family:"Segoe UI",Tahoma,sans-serif;color:#eeeae3}',
      '.ask.on{display:flex}',
      '.ask-box{width:380px;max-width:94vw;padding:18px 20px 16px;border-radius:14px;',
      'background:#141920;border:1px solid #2a323c;box-shadow:0 24px 60px rgba(0,0,0,.6)}',
      '.ask-title{font-size:17px;font-weight:600;color:#d8c39b}',
      '.ask-sub{margin-top:4px;font-size:12.5px;color:#939ba6;line-height:1.45}',
      '.ask-field{width:100%;margin-top:14px;padding:11px 13px;border-radius:10px;',
      'border:1px solid #2a323c;background:#1b212b;color:#eeeae3;font:inherit;font-size:15px}',
      '.ask-field:focus{outline:none;border-color:#d8c39b}',
      '.ask-hint{margin-top:6px;font-size:11.5px;color:#6f7885}',
      '.ask-actions{display:flex;gap:10px;margin-top:14px}',
      '.ask-btn{flex:1;padding:10px;border:1px solid #2a323c;border-radius:10px;cursor:pointer;',
      'background:#232c38;color:#eeeae3;font:inherit;font-size:13.5px}',
      '.ask-btn:hover{border-color:#d8c39b}',
      '.ask-btn.ok{background:#d8c39b;border-color:#d8c39b;color:#171a20;font-weight:600}',
      '.ask-btn.ok:hover{filter:brightness(1.06)}'
    ].join('\n');
    document.head.appendChild(style);
  }

  function build() {
    if (root) return;
    css();
    root = el('div', 'ask');
    var box = el('div', 'ask-box');
    title = el('div', 'ask-title', '');
    sub = el('div', 'ask-sub', '');
    field = el('input', 'ask-field');
    field.type = 'text';
    field.id = 'askField';
    field.maxLength = 96;
    field.spellcheck = false;
    field.autocomplete = 'off';
    var hint = el('div', 'ask-hint', 'Enter — подтвердить, Esc — отмена');
    var actions = el('div', 'ask-actions');
    var ok = el('button', 'ask-btn ok', 'Готово');
    var cancel = el('button', 'ask-btn', 'Отмена');

    function send() {
      var value = (field.value || '').trim();
      show(false);
      RW.emit('ui:answer', value);
    }
    function abort() {
      show(false);
      RW.emit('ui:closed');
    }

    ok.addEventListener('click', send);
    cancel.addEventListener('click', abort);
    field.addEventListener('keydown', function (event) {
      if (event.key === 'Enter') { event.preventDefault(); send(); }
      if (event.key === 'Escape') { event.preventDefault(); abort(); }
    });

    actions.appendChild(ok);
    actions.appendChild(cancel);
    box.appendChild(title);
    box.appendChild(sub);
    box.appendChild(field);
    box.appendChild(hint);
    box.appendChild(actions);
    root.appendChild(box);
    document.body.appendChild(root);
  }

  function show(on) {
    build();
    opened = on;
    root.className = on ? 'ask on' : 'ask';
    if (on) {
      field.value = '';
      window.setTimeout(function () { try { field.focus(); } catch (e) {} }, 30);
    }
  }

  RW.on('ui:ask', function (head, note, example) {
    build();
    title.textContent = head || '';
    sub.textContent = note || '';
    if (example) field.placeholder = example;
    show(true);
  });

  RW.on('ui:close', function () { if (opened) show(false); });

  /* Предпросмотр: index.html?fake=ask */
  if (window.location.search.indexOf('fake=ask') >= 0) {
    RW.fake('ui:ask', ['Продажа', 'Цена за час показа, от 1 до 100 000 000', 'Например: 5000']);
  }
})();
