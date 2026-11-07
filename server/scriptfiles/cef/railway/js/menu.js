/* Полноэкранное меню (F3): главная, настройки и донат.
   Страница ничего не решает — сервер присылает строки, а на клик уходит
   только действие строки. */
(function () {
  var menu = document.getElementById('menu');
  var rows = document.getElementById('rows');
  var title = document.getElementById('title');
  var subtitle = document.getElementById('subtitle');

  function act(name) { RW.emit('menu:action', name); }

  Array.prototype.forEach.call(document.querySelectorAll('[data-act]'), function (b) {
    b.addEventListener('click', function () { act(b.getAttribute('data-act')); });
  });

  document.addEventListener('keydown', function (e) {
    if (e.key === 'Escape') { e.preventDefault(); act('resume'); }
    if (e.key === 'F3') { e.preventDefault(); act('resume'); }
  });

  RW.on('menu:open', function (head, sub) {
    title.textContent = head || 'Меню';
    subtitle.textContent = sub || '';
    rows.innerHTML = '';
    menu.classList.remove('off');
  });

  /* Сервер шлёт три аргумента: подпись, действие и стиль. */
  RW.on('menu:row', function (text, action, style) {
    var node = document.createElement('button');
    node.className = 'menu-row ' + (style || '');
    node.type = 'button';
    node.textContent = text || '';
    node.addEventListener('click', function () { act(action || ''); });
    rows.appendChild(node);
  });

  RW.on('menu:end', function () {
    if (!rows.children.length) {
      var empty = document.createElement('div');
      empty.className = 'menu-empty';
      empty.textContent = 'Пусто';
      rows.appendChild(empty);
    }
  });

  RW.on('menu:close', function () {
    menu.classList.add('off');
  });
})();
