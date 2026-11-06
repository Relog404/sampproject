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
    donate: 'Донат',
    map: 'Карта',
    job: 'Работа',
    enter: 'Войти',
    exit: 'Выйти',
    key: 'Клавиши',
    donate: 'Донат',
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
    }
  };

  var RADIUS = 200;      // метров от края до края радара
  var hud = document.getElementById('hud');
  var blips = document.getElementById('blips');
  var hints = document.getElementById('hints');
  var zone = document.getElementById('zone');
  var radarN = document.getElementById('radarN');
  var bonus = document.getElementById('bonus');
  var bonusTitle = document.getElementById('bonusTitle');
  var bonusText = document.getElementById('bonusText');
  var bonusGo = document.getElementById('bonusGo');
  var bonusClose = document.getElementById('bonusClose');
  var COLORS = { 1: '#F5B04C', 2: '#7FD1AE', 3: '#8FC4FF', 4: '#FF8A8A', 5: '#C9A6FF' };

  function translate(key) {
    return WORDS[key] || key;
  }

  /* Радар: сервер шлёт угол и список "вид:dx:dy" через точку с запятой. */
  RW.on('hud:radar', function (angle, list, zoneName) {
    var parts = (list || '').split(';');
    var html = '';
    for (var i = 0; i < parts.length; i++) {
      var p = parts[i].split(':');
      if (p.length < 3) continue;
      var x = parseFloat(p[1]) / RADIUS * 100;
      var y = parseFloat(p[2]) / RADIUS * 100;
      if (x * x + y * y > 50 * 50) continue;
      html += '<i class="blip" style="left:' + (50 + x) + '%;top:' + (50 + y) + '%;background:' +
        (COLORS[p[0]] || '#FFFFFF') + '"></i>';
    }
    blips.innerHTML = html;
    zone.textContent = zoneName || '';
    if (radarN) radarN.style.transform = 'rotate(' + (-angle) + 'deg) translateY(-46px) rotate(' + angle + 'deg)';
  });

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
  RW.on('hud:bonus', function (key, amount, day) {
    var card = CARDS[key];
    if (!card) { bonus.classList.add('off'); return; }
    card = card(amount, day);
    bonusTitle.textContent = card.title;
    bonusText.textContent = card.text;
    bonusGo.textContent = 'Забрать';
    bonus.classList.remove('off');
  });

  /* Показ и скрытие всего HUD. */
  RW.on('hud:show', function (on) {
    hud.classList.toggle('off', !on);
  });

  bonusGo.addEventListener('click', function () { RW.emit('hud:bonus', 1); });
  bonusClose.addEventListener('click', function () {
    bonus.classList.add('off');
    RW.emit('hud:bonushide', 1);
  });

  /* F2 открывает донат-меню, когда окно в фокусе. */
  document.addEventListener('keydown', function (e) {
    if (e.key === 'F2') { e.preventDefault(); RW.emit('hud:donate', 1); }
  });
})();
