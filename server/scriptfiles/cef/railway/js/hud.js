/* HUD: подсказки клавиш, спидометр, деньги и время.
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

  var hud = document.getElementById('hud');
  var hints = document.getElementById('hints');
  /* Клавиши сервер шлёт короткими именами, страница показывает их по-людски. */
  var KEYS = {
    AltBracket: 'Alt + [',
    AltU: 'Alt + U',
    AltY: 'Alt + Y'
  };
  function keyName(key) {
    return KEYS[key] || key;
  }
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
      html += '<div class="hint"><b>' + keyName(p[0]) + '</b><span>' + translate(p[1]) + '</span></div>';
    }
    hints.innerHTML = html;
  });

  /* Показ и скрытие всего HUD. */
  RW.on('hud:show', function (on) {
    hud.classList.toggle('off', !on);
  });

  /* Никакого контекстного меню браузера поверх игры. */
  document.addEventListener('contextmenu', function (e) { e.preventDefault(); return false; });
  document.addEventListener('selectstart', function (e) { e.preventDefault(); });

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

  document.addEventListener('keydown', function (e) {
    if (e.key === 'Escape') { e.preventDefault(); RW.emit('hud:mousetoggle', 0); }
    if (e.altKey && (e.code === 'KeyY' || e.key === 'y' || e.key === 'Y' || e.key === 'ы')) {
      e.preventDefault();
      RW.emit('hud:mousetoggle', 1);
    }
  });
})();
