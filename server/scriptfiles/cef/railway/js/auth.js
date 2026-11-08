/* Авторизация в браузере: вход и регистрация на русском.
   Сервер решает, кто перед нами — новый игрок или старый, и присылает
   только режим, ник и текст ошибки. Пароль уходит на сервер и больше
   никуда: страница его не хранит.

   Нажатие всегда видно: пока сервер не ответил, кнопка показывает
   «Отправляю…», а если ответа нет — страница говорит об этом прямо.
   Молчаливая кнопка выглядит сломанной и не объясняет, где искать беду. */
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
    sw: document.getElementById('switch'),
    native: document.getElementById('native')
  };
  var register = false;
  var pending = 0;
  var ANSWER_MS = 6000;

  function paint() {
    el.repeatWrap.classList.toggle('off', !register);
    el.go.textContent = register ? 'Создать аккаунт' : 'Войти';
    el.hint.textContent = register ? 'Уже есть аккаунт?' : 'Первый раз здесь?';
    el.sw.textContent = register ? 'Войти' : 'Создать аккаунт';
  }

  function say(text, bad) {
    el.note.textContent = text || '';
    el.note.classList.toggle('bad', !!(text && bad));
  }

  function stopWaiting() {
    if (pending) window.clearTimeout(pending);
    pending = 0;
    el.go.disabled = false;
  }

  function noAnswer() {
    pending = 0;
    el.go.disabled = false;
    say('Сервер не ответил. Нажмите «Обычные диалоги» и введите пароль там.', true);
  }

  /* Кнопка молчала бы и без этого: моста нет — сервер ничего не получит,
     и единственный выход — нативный диалог по команде. */
  function noBridge() {
    say('Браузер не связан с сервером: кнопка не сработает. Закройте окно ' +
        'клавишей Esc и наберите /uiru — откроются обычные диалоги.', true);
  }

  RW.on('auth:open', function (mode, nick, message) {
    register = !!mode;
    el.nick.textContent = nick || '';
    stopWaiting();
    say(message || '', true);
    el.pass.value = '';
    el.repeat.value = '';
    el.auth.classList.remove('off');
    paint();
    window.setTimeout(function () { el.pass.focus(); }, 60);
  });

  RW.on('auth:close', function () {
    stopWaiting();
    el.auth.classList.add('off');
    el.pass.value = '';
    el.repeat.value = '';
  });

  function submit() {
    var pass = el.pass.value || '';
    if (!pass.length) { say('Введите пароль.', true); return; }
    if (register && pass !== (el.repeat.value || '')) {
      say('Пароли не совпадают.', true);
      return;
    }
    if (!RW.ready()) { noBridge(); return; }
    stopWaiting();
    say('Отправляю…', false);
    el.go.disabled = true;
    pending = window.setTimeout(noAnswer, ANSWER_MS);
    RW.emit('auth:submit', pass, register ? (el.repeat.value || '') : '');
  }

  el.go.addEventListener('click', submit);
  el.pass.addEventListener('keydown', function (e) { if (e.key === 'Enter') submit(); });
  el.repeat.addEventListener('keydown', function (e) { if (e.key === 'Enter') submit(); });
  el.sw.addEventListener('click', function () { register = !register; paint(); });

  if (el.native) {
    el.native.addEventListener('click', function () {
      if (!RW.ready()) { noBridge(); return; }
      stopWaiting();
      say('Открываю обычные диалоги…', false);
      RW.emit('auth:native');
    });
  }

  paint();

  /* Мост подключается не мгновенно: если его нет и через три секунды,
     говорить «сервер не ответил» поздно — говорим сразу. */
  window.setTimeout(function () {
    if (!RW.ready() && !RW.demo()) noBridge();
  }, 3000);

  /* Локальный просмотр без игры: auth.html?fake=1 — регистрация,
     ?fake=2 — вход, ?fake=3 — карточка с текстом ошибки. */
  if (RW.demo()) {
    var view = window.location.search.indexOf('fake=3') >= 0 ? 3
      : (window.location.search.indexOf('fake=2') >= 0 ? 2 : 1);
    if (view === 3) RW.fake('auth:open', [1, 'Ivan_Ivanov', 'Пароли не совпадают.']);
    else if (view === 2) RW.fake('auth:open', [0, 'Ivan_Ivanov', '']);
    else RW.fake('auth:open', [1, 'Ivan_Ivanov', '']);
  }
})();
