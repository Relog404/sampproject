/* ============================================================================
   Railway — логика CEF-спидометра.
   Сервер присылает: speedo:show(name, model, isOwner), speedo:update(speed, gear, rpm, flags)
   flags (битовая маска): 1 двигатель, 2 ключ, 4 двери, 8 владелец, 16 заблокировано.
   ========================================================================== */

(function () {
    'use strict';

    var MAX_SPEED = 240;     /* верхняя граница шкалы, км/ч */
    var SWEEP = 270;         /* угол шкалы, градусы */
    var START = 135;         /* начало шкалы, градусы (0° = 3 часа) */
    var R_CENTER = 110;
    var R_ARC = 86;

    /* ------------------------------------------------------------ плагин */

    var cefApi = window.railwayCef || (typeof window.cef !== 'undefined' && window.cef) ? window.cef : null;
    var isPreview = !cefApi;

    if (!cefApi) {
        cefApi = (function () {
            var handlers = {};
            function fire(name, a, b, c, d) {
                (handlers[name] || []).forEach(function (fn) { fn(a, b, c, d); });
            }
            window.addEventListener('load', function () {
                setTimeout(function () { fire('speedo:show', 'Sultan', 560, true); }, 400);
                /* демонстрационная езда для предпросмотра в браузере */
                var t = 0;
                setInterval(function () {
                    t += 0.016;
                    var speed = Math.round((Math.sin(t * 0.55) * 0.5 + 0.5) * 190);
                    var gear = speed < 2 ? 0 : Math.min(6, Math.floor(speed / 32) + 1);
                    fire('speedo:update', speed, gear, 900 + (speed % 32) * 160, 1 | 2 | 8);
                }, 130);
            });
            return {
                on: function (name, fn) { (handlers[name] = handlers[name] || []).push(fn); },
                emit: function () {}
            };
        })();
    }

    /* ------------------------------------------------------------ элементы */

    var hud        = document.getElementById('hud');
    var carEl      = document.getElementById('car');
    var ownerEl    = document.getElementById('owner');
    var speedEl    = document.getElementById('speed');
    var gearEl     = document.getElementById('gear-value');
    var needleEl   = document.getElementById('needle');
    var arcEl      = document.getElementById('arc');
    var redlineEl  = document.getElementById('redline');
    var ticksEl    = document.getElementById('ticks');
    var chipKey    = document.getElementById('chip-key');
    var chipEngine = document.getElementById('chip-engine');
    var chipDoor   = document.getElementById('chip-door');
    var lockedEl   = document.getElementById('locked');

    /* ------------------------------------------------------------ геометрия */

    var circumference = 2 * Math.PI * R_ARC;
    var arcLength = circumference * (SWEEP / 360);

    function polar(radius, degrees) {
        var rad = degrees * Math.PI / 180;
        return {
            x: R_CENTER + radius * Math.cos(rad),
            y: R_CENTER + radius * Math.sin(rad)
        };
    }

    function buildDial() {
        /* дорожка */
        document.querySelector('.track').style.strokeDasharray = arcLength + ' ' + circumference;
        arcEl.style.strokeDasharray = arcLength + ' ' + circumference;
        arcEl.style.strokeDashoffset = 0;

        /* красная зона: последние 16% шкалы */
        var redStart = arcLength * 0.84;
        redlineEl.style.strokeDasharray = (arcLength * 0.16) + ' ' + circumference;
        redlineEl.style.strokeDashoffset = -(redStart);

        /* насечки */
        var total = 36;
        var html = '';
        for (var i = 0; i <= total; i++) {
            var angle = START + (SWEEP / total) * i;
            var major = (i % 6 === 0);
            var inner = polar(major ? 66 : 72, angle);
            var outer = polar(78, angle);
            html += '<line class="' + (major ? 'major' : '') + '" x1="' + inner.x.toFixed(2) +
                    '" y1="' + inner.y.toFixed(2) + '" x2="' + outer.x.toFixed(2) +
                    '" y2="' + outer.y.toFixed(2) + '"/>';
        }
        ticksEl.innerHTML = html;
    }

    /* ------------------------------------------------------------ состояние */

    var state = {
        shown: false,
        targetSpeed: 0,
        currentSpeed: 0,
        targetRpm: 0,
        currentRpm: 0,
        gear: 0,
        flags: 0,
        lastGear: null
    };

    function setChip(el, kind, text) {
        el.className = 'chip' + (kind ? ' is-' + kind : '');
        if (text !== undefined) {
            el.innerHTML = '<i></i>' + text;
        }
    }

    function paintFlags(flags) {
        var engine = (flags & 1) !== 0;
        var key    = (flags & 2) !== 0;
        var doors  = (flags & 4) !== 0;
        var owner  = (flags & 8) !== 0;
        var locked = (flags & 16) !== 0;

        hud.classList.toggle('is-locked', locked);
        hud.classList.toggle('is-foreign', !owner);

        setChip(chipKey, key ? 'on' : 'off', 'ключ');
        setChip(chipEngine, engine ? 'on' : 'off', 'двигатель');
        setChip(chipDoor, doors ? 'warn' : 'off', doors ? 'двери открыты' : 'двери');
    }

    function gearText(gear) {
        if (gear === -1) { return 'R'; }
        if (gear <= 0) { return 'N'; }
        return String(Math.min(gear, 6));
    }

    function paintGear(gear) {
        if (gear === state.lastGear) { return; }
        state.lastGear = gear;

        gearEl.textContent = gearText(gear);
        gearEl.classList.toggle('is-reverse', gear === -1);
        gearEl.classList.toggle('is-neutral', gear === 0);

        gearEl.classList.remove('is-shift');
        void gearEl.offsetWidth;
        gearEl.classList.add('is-shift');
    }

    /* ------------------------------------------------------------ отрисовка */

    function render() {
        var p = Math.min(1, Math.max(0, state.currentSpeed / MAX_SPEED));

        speedEl.textContent = String(Math.round(state.currentSpeed));

        arcEl.style.strokeDasharray = (arcLength * p) + ' ' + circumference;
        arcEl.style.strokeDashoffset = '0';
        arcEl.style.opacity = p > 0.001 ? '1' : '0';
        needleEl.setAttribute('transform',
            'rotate(' + (START + SWEEP * p + 90) + ' ' + R_CENTER + ' ' + R_CENTER + ')');

        /* мягкое «дыхание» подсветки на высоких оборотах */
        if (state.currentRpm > 5600) {
            hud.style.setProperty('--accent', '#FF8A5A');
        } else {
            hud.style.setProperty('--accent', '#F5B04C');
        }
    }

    function loop() {
        var speedDelta = state.targetSpeed - state.currentSpeed;
        var rpmDelta = state.targetRpm - state.currentRpm;

        state.currentSpeed += speedDelta * 0.16;
        state.currentRpm += rpmDelta * 0.22;

        if (Math.abs(speedDelta) < 0.4) { state.currentSpeed = state.targetSpeed; }
        if (Math.abs(rpmDelta) < 30) { state.currentRpm = state.targetRpm; }

        render();
        requestAnimationFrame(loop);
    }

    /* ------------------------------------------------------------ события сервера */

    cefApi.on('speedo:show', function (name, model, isOwner) {
        carEl.textContent = name || ('Модель ' + model);
        ownerEl.textContent = isOwner ? 'ваша машина' : 'чужая машина';

        state.shown = true;
        state.currentSpeed = 0;
        state.targetSpeed = 0;
        state.currentRpm = 0;
        state.targetRpm = 0;
        state.lastGear = null;
        paintGear(0);

        hud.classList.add('is-visible');
    });

    cefApi.on('speedo:update', function (speed, gear, rpm, flags) {
        state.targetSpeed = Math.max(0, speed || 0);
        state.targetRpm = Math.max(0, rpm || 0);
        state.flags = flags || 0;

        paintGear(gear === undefined ? 0 : gear);
        paintFlags(state.flags);

        if (!state.shown) {
            /* если событие обновления пришло раньше показа — показываем прибор */
            state.shown = true;
            hud.classList.add('is-visible');
        }
    });

    cefApi.on('speedo:hide', function () {
        state.shown = false;
        state.targetSpeed = 0;
        state.targetRpm = 0;
        hud.classList.remove('is-visible', 'is-locked', 'is-foreign');
    });

    /* ------------------------------------------------------------ старт */

    buildDial();
    paintFlags(0);
    paintGear(0);
    render();
    requestAnimationFrame(loop);

    if (isPreview) {
        document.documentElement.classList.add('is-preview');
    }

    cefApi.emit('speedo:ready');
})();
