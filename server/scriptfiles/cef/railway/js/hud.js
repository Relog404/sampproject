/* HUD: радар, подсказки клавиш и карточка бонуса.
   Все подписи на русском живут здесь, на клиенте: сервер шлёт короткие
   английские ключи, а страница переводит их. Так один и тот же сервер
   работает и с браузером, и с текстовым интерфейсом. */
(function () {
  var WORDS = {
    interaction: 'Взаимодействие',
    engine: 'Двигатель',
    doors: 'Двери',
    inventory: 'Инвентарь',
    phone: 'Телефон',
    map: 'Карта',
    job: 'Работа',
    enter: 'Войти',
    exit: 'Выйти',
    key: 'Клавиши',
    donate: 'Донат',
    mouse: 'Мышь',
    ganton: 'Гантон',
    jefferson: 'Джефферсон',
    willowfield: 'Уиллоуфилд',
    elcorona: 'Эль-Корона',
    idlewood: 'Айдлвуд',
    eastlosSantos: 'Восточный Лос-Сантос',
    lascolinas: 'Лас-Колинас',
    glenpark: 'Глен-Парк',
    downtown: 'Даунтаун',
    commerce: 'Коммерция',
    market: 'Маркет',
    pershingsquare: 'Площадь Першинга',
    oceandocks: 'Оушен-Докс',
    playadelseville: 'Плайя-дель-Севилья',
    veronabeach: 'Верона-Бич',
    vinewood: 'Вайнвуд',
    santamariabeach: 'Санта-Мария-Бич',
    rodeo: 'Родео'
  };

  /* Карточки бонусов: сервер шлёт только ключ и сумму. */
  function money(n) {
    return String(Math.max(0, n | 0)).replace(/(\d)(?=(\d{3})+$)/g, '$1 ');
  }
  var CARDS = {
    starter: function (amount, day) {
      return { title: 'Стартовый бонус', text: 'День ' + day + ' из 3. Награда: ' + money(amount) + ' $.' };
    },
    online: function (amount) {
      return { title: 'Бонус за игру', text: 'Каждые 30 минут в игре: ' + money(amount) + ' $.' };
    },
    next: function () {
      return { title: 'Следующий бонус', text: 'Всё получено.' };
    }
  };

  var hud = document.getElementById('hud');
  var hints = document.getElementById('hints');
  var bonus = document.getElementById('bonus');
  var bonusTitle = document.getElementById('bonusTitle');
  var bonusText = document.getElementById('bonusText');
  var bonusGo = document.getElementById('bonusGo');
  var bonusClose = document.getElementById('bonusClose');
  var head = document.getElementById('bonusHead');

  function translate(key) {
    return WORDS[key] || key;
  }

  /* Подсказки: что делает клавиша прямо сейчас. */
  RW.on('hud:hint', function (list) {
    var parts = (list || '').split(';');
    var html = '';
    for (var i = 0; i < parts.length; i++) {
      var p = parts[i].split(':');
      if (p.length < 2 || !p[0]) continue;
      html += '<div class="hint"><b>' + p[0] + '</b><span>' + translate(p[1]) + '</span></div>';
    }
    hints.innerHTML = html;
  });

  /* Карточка бонуса. Пустой заголовок прячет её целиком. */
  var nextLeft = 0, nextTimer = 0;

  function clock(sec) {
    var m = Math.floor(sec / 60), s = sec % 60;
    return m + ':' + (s < 10 ? '0' : '') + s;
  }
  function stopTimer() {
    if (nextTimer) { window.clearInterval(nextTimer); nextTimer = 0; }
  }
  function startTimer(sec, base) {
    nextLeft = Math.max(0, sec | 0);
    bonusText.textContent = base + ' Через ' + clock(nextLeft) + '.';
    stopTimer();
    nextTimer = window.setInterval(function () {
      nextLeft = Math.max(0, nextLeft - 1);
      bonusText.textContent = base + ' Через ' + clock(nextLeft) + '.';
      if (!nextLeft) stopTimer();
    }, 1000);
  }

  RW.on('hud:bonus', function (key, amount, day) {
    stopTimer();
    var card = CARDS[key];
    if (!card) { bonus.classList.add('off'); return; }
    card = card(amount, day);
    bonusTitle.textContent = card.title;
    bonusText.textContent = card.text;
    head.textContent = card.title;
    bonus.classList.remove('off');
    if (key === 'next') {
      bonusGo.textContent = 'Всё';
      bonusGo.disabled = true;
      bonusGo.classList.add('done');
      startTimer(day, card.text);
    } else {
      bonusGo.textContent = 'Забрать';
      bonusGo.disabled = false;
      bonusGo.classList.remove('done');
    }
  });

  /* Показ и скрытие всего HUD. */
  RW.on('hud:show', function (on) {
    hud.classList.toggle('off', !on);
  });

  /* Никакого контекстного меню браузера поверх игры. */
  document.addEventListener('contextmenu', function (e) { e.preventDefault(); return false; });
  document.addEventListener('selectstart', function (e) { e.preventDefault(); });

  /* Кнопка «Забрать»: своё имя события, чтобы не склеиваться с пакетом,
     который сервер шлёт этой же странице. */
  bonusGo.addEventListener('click', function () { RW.emit('hud:bonusclaim', 1); });

  /* Свернуть и развернуть: крестика больше нет. */
  function setMin(on) {
    bonus.classList.toggle('min', on);
    head.textContent = on ? 'Бонус (свёрнут)' : (bonusTitle.textContent || 'Бонус');
  }
  bonusClose.addEventListener('click', function (e) {
    e.stopPropagation();
    setMin(!bonus.classList.contains('min'));
  });
  bonus.addEventListener('click', function () {
    if (bonus.classList.contains('min')) setMin(false);
  });

  /* Режим мыши: включается командой /mouse, иначе курсор не мешает играть. */
  /* Спидометр: сервер шлёт имя машины, скорость, передачу, обороты и флаги
     (1 — мотор, 2 — ключ, 4 — двери). Пустое имя значит «имя не менять». */
  var speedo = document.getElementById('speedo');
  var speedoName = document.getElementById('speedoName');
  var speedoNum = document.getElementById('speedoNum');
  var speedoGear = document.getElementById('speedoGear');
  var speedoFlags = document.getElementById('speedoFlags');
  var speedoRpm = document.getElementById('speedoRpm');

  var ground = document.getElementById('ground');
  if (ground) {
    RW.on('hud:ground', function (on, text) {
      if (!on) {
        ground.className = 'ground';
        ground.textContent = '';
        return;
      }
      ground.innerHTML = text || '';
      ground.className = 'ground on';
    });
  }

  RW.on('hud:speedo', function (on, name, speed, gear, rpm, flags) {
    if (!speedo) return;
    if (!on) { speedo.className = 'speedo'; return; }
    if (name) speedoName.textContent = name;
    speedoNum.textContent = ('00' + (speed || 0)).slice(-3);
    speedoGear.textContent = gear < 0 ? 'R' : (gear === 0 ? 'N' : String(gear));
    speedoFlags.textContent = 'Ключ ' + (flags & 2 ? '+' : '-') +
      '  Мотор ' + (flags & 1 ? '+' : '-') +
      '  Двери ' + (flags & 4 ? 'открыты' : 'закрыты');
    var percent = (rpm || 0) / 6500 * 100;
    if (percent < 1) percent = 1;
    if (percent > 100) percent = 100;
    speedoRpm.style.width = percent + '%';
    speedo.className = 'speedo on';
  });

  RW.on('hud:mouse', function (on) {
    hud.classList.toggle('mouse', !!on);
  });

  /* F2 открывает донат-меню, когда окно в фокусе. */
  document.addEventListener('keydown', function (e) {
    if (e.key === 'F2') { e.preventDefault(); RW.emit('hud:donate', 1); }
    if (e.key === 'F3') { e.preventDefault(); RW.emit('hud:menu', 1); }
    if (e.key === 'Escape') { e.preventDefault(); RW.emit('hud:mousetoggle', 0); }
    if (e.altKey && (e.code === 'KeyY' || e.key === 'y' || e.key === 'Y' || e.key === 'ы')) {
      e.preventDefault();
      RW.emit('hud:mousetoggle', 1);
    }
  });
})();
