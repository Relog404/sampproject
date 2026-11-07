/* Авторизация в браузере: вход и регистрация на русском.
   Сервер решает, кто перед нами — новый игрок или старый, и присылает
   только режим, ник и текст ошибки. Пароль уходит на сервер и больше
   никуда: страница его не хранит. */
(function () {
  var el = {
    auth: document.getElementById('auth'),
    nick: document.getElementById('nick'),
    pass: document.getElementById('password'),
    repeat: document.getElementById('repeat'),
    repeatWrap: document.getElementById('repeatWrap'),
    note: document.getElementById('note'),
    go: document.getElementById('go'),
    hint: document.getElementById('hint'),
    sw: document.getElementById('switch')
  };
  var register = false;

  function paint() {
    el.repeatWrap.classList.toggle('off', !register);
    el.go.textContent = register ? 'Создать аккаунт' : 'Войти';
    el.hint.textContent = register ? 'Уже есть аккаунт?' : 'Первый раз здесь?';
    el.sw.textContent = register ? 'Войти' : 'Создать аккаунт';
  }

  RW.on('auth:open', function (mode, nick, message) {
    register = !!mode;
    el.nick.textContent = nick || '';
    el.note.textContent = message || '';
    el.note.classList.toggle('bad', !!(message && message.length));
    el.pass.value = '';
    el.repeat.value = '';
    el.auth.classList.remove('off');
    paint();
    window.setTimeout(function () { el.pass.focus(); }, 60);
  });

  RW.on('auth:close', function () {
    el.auth.classList.add('off');
    el.pass.value = '';
    el.repeat.value = '';
  });

  function submit() {
    var pass = el.pass.value || '';
    if (!pass.length) { el.note.textContent = 'Введите пароль.'; el.note.classList.add('bad'); return; }
    if (register && pass !== (el.repeat.value || '')) {
      el.note.textContent = 'Пароли не совпадают.';
      el.note.classList.add('bad');
      return;
    }
    el.note.textContent = '';
    RW.emit('auth:submit', pass, register ? (el.repeat.value || '') : '');
  }

  el.go.addEventListener('click', submit);
  el.password = null;
  el.pass.addEventListener('keydown', function (e) { if (e.key === 'Enter') submit(); });
  el.repeat.addEventListener('keydown', function (e) { if (e.key === 'Enter') submit(); });
  el.sw.addEventListener('click', function () { register = !register; paint(); });
})();
