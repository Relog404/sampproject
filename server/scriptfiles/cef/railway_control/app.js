(function () {
    'use strict';
    var api = window.railwayCef || window.cef || { on: function () {}, emit: function () {} };
    var panel = document.getElementById('panel'), content = document.getElementById('content');
    var keys = [76, 69, 220], waiting = -1;
    function on(name, fn) { api.on(name, fn); }
    function toast(text) {
        var host = document.getElementById('toasts');
        if (host.children.length >= 3) host.removeChild(host.firstChild);
        var el = document.createElement('div'); el.className = 'toast'; el.textContent = text; host.appendChild(el);
        setTimeout(function () { el.classList.add('out'); setTimeout(function () { el.remove(); }, 230); }, 3000);
    }
    function open(title, sub) {
        waiting = -1; panel.hidden = false; content.textContent = '';
        document.getElementById('title').textContent = title; document.getElementById('subtitle').textContent = sub;
    }
    function close() { waiting = -1; panel.hidden = true; api.emit('control:close'); }
    document.getElementById('close').onclick = close;
    function bindings() {
        content.textContent = '';
        ['Двери', 'Зажигание', 'Меню взаимодействия'].forEach(function (label, i) {
            var b = document.createElement('button'); b.className = 'binding' + (waiting === i ? ' wait' : '');
            var span = document.createElement('span'); span.textContent = label; b.appendChild(span);
            var key = document.createElement('b'); key.textContent = waiting === i ? '…' : (keys[i] === 220 ? String.fromCharCode(92) : String.fromCharCode(keys[i])); b.appendChild(key);
            b.onclick = function () { waiting = i; bindings(); }; content.appendChild(b);
        });
        var reset = document.createElement('button'); reset.className = 'action'; reset.textContent = 'По умолчанию · L / E / ' + String.fromCharCode(92);
        reset.onclick = function () { keys = [76, 69, 220]; waiting = -1; bindings(); api.emit('control:bind', keys[0], keys[1], keys[2]); }; content.appendChild(reset);
    }
    document.addEventListener('keydown', function (e) {
        if (panel.hidden) return;
        if (e.key === 'Escape') { e.preventDefault(); close(); return; }
        if (waiting < 0) return;
        e.preventDefault(); e.stopPropagation();
        var k = e.code === 'Backslash' || e.key === String.fromCharCode(92) ? 220 : e.keyCode;
        if ((k !== 220 && (k < 65 || k > 90)) || k === 84) return toast('Выберите A–Z. T занята чатом.');
        if (keys.some(function (v, i) { return i !== waiting && v === k; })) return toast('Эта клавиша уже используется.');
        keys[waiting] = k; waiting = -1; bindings(); api.emit('control:bind', keys[0], keys[1], keys[2]);
    });
    function action(label, id, target) {
        var b = document.createElement('button'); b.className = 'action';
        var text = document.createElement('div'); text.textContent = label; b.appendChild(text);
        var arrow = document.createElement('span'); arrow.textContent = '↗'; b.appendChild(arrow);
        b.onclick = function () { api.emit('control:action', id, target === undefined ? -1 : target); }; content.appendChild(b);
    }
    on('control:settings', function (door, engine, menu) { keys = [door, engine, menu]; open('Настройки клавиш', 'Нажмите на клавишу, чтобы изменить · на эту сессию'); bindings(); });
    on('control:vehicle', function (name, flags, lights) {
        open(name, flags & 8 ? 'Ваш транспорт · управление' : 'Чужой транспорт · ограниченный доступ');
        action(flags & 4 ? 'Закрыть двери' : 'Открыть двери', 0);
        action(flags & 1 ? 'Заглушить двигатель' : 'Завести двигатель', 1);
        action(flags & 2 ? 'Вынуть ключ' : 'Вставить ключ', 2);
        action(lights ? 'Выключить фары' : 'Включить фары', 3);
    });
    on('control:passenger', function (id, nick) { action('Высадить · ' + nick, 4, id); });
    on('control:toast', toast);
    on('control:hide', function () { panel.hidden = true; waiting = -1; });
    api.emit('control:ready');
})();
