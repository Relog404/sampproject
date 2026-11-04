/* ============================================================================
   Railway — логика экрана входа/регистрации.
   Связь с сервером: cef.emit / cef.on (плагин aurora-mp/omp-cef).
   Если страница открыта в обычном браузере, включается демо-режим.
   ========================================================================== */

(function () {
    'use strict';

    /* ------------------------------------------------------------ плагин */

    var cefApi = (typeof window.cef !== 'undefined' && window.cef) ? window.cef : null;
    var isPreview = !cefApi;

    if (!cefApi) {
        cefApi = (function () {
            var handlers = {};
            window.addEventListener('load', function () {
                setTimeout(function () {
                    fire('auth:init', 'Ivan_Petrov', true, '1.0.0');
                }, 450);
            });
            function fire(name, a, b, c) {
                (handlers[name] || []).forEach(function (fn) { fn(a, b, c); });
            }
            return {
                on: function (name, fn) { (handlers[name] = handlers[name] || []).push(fn); },
                emit: function (name, a, b) {
                    if (name === 'auth:login' || name === 'auth:register') {
                        setTimeout(function () {
                            if (a === 'demo' || (a && a.length < 6)) {
                                fire('auth:result', false, 'err:badpass:2', 'Ivan_Petrov');
                            } else {
                                fire('auth:result', true, 'ok:login', 'Ivan_Petrov');
                            }
                        }, 900);
                    }
                }
            };
        })();
    }

    /* ------------------------------------------------------------ элементы */

    var card       = document.getElementById('card');
    var tabs       = document.getElementById('tabs');
    var form       = document.getElementById('form');
    var pass       = document.getElementById('pass');
    var pass2      = document.getElementById('pass2');
    var eye        = document.getElementById('eye');
    var fieldPass  = document.getElementById('field-pass');
    var fieldRepeat= document.getElementById('field-repeat');
    var msgPass    = document.getElementById('msg-pass');
    var strength   = document.getElementById('strength');
    var strengthTxt= document.getElementById('strength-text');
    var submit     = document.getElementById('submit');
    var submitLabel= document.getElementById('submit-label');
    var nickEl     = document.getElementById('nick');
    var avatarEl   = document.getElementById('avatar');
    var versionEl  = document.getElementById('version');
    var stage      = document.querySelector('.stage');
    var toast      = document.getElementById('toast');
    var toastText  = document.getElementById('toast-text');

    var mode = 'login';
    var busy = false;
    var ready = false;
    var toastTimer = 0;

    /* ------------------------------------------------------------ утилиты */

    var MESSAGES = {
        'err:short':     'Пароль слишком короткий — минимум 6 символов.',
        'err:long':      'Слишком длинный пароль (максимум 64 символа).',
        'err:mismatch':  'Пароли не совпадают.',
        'err:exists':    'Аккаунт с таким ником уже существует. Попробуйте войти.',
        'err:noaccount': 'Аккаунта ещё нет — переключитесь на регистрацию.',
        'err:inuse':     'Этот аккаунт уже находится в игре.',
        'err:chars':     'В пароле есть недопустимые символы.',
        'err:nick':      'Ник не подходит для аккаунта. Смените ник в SA-MP.',
        'err:db':        'Ошибка базы данных. Попробуйте позже.',
        'err:cooldown':  'Слишком часто. Подождите пару секунд.',
        'err:badpass':   'Неверный пароль.',
        'ok:register':   'Аккаунт создан. Добро пожаловать на вокзал!',
        'ok:login':      'Добро пожаловать на вокзал!'
    };

    function codeMessage(code) {
        if (!code) { return 'Неизвестная ошибка. Попробуйте ещё раз.'; }
        if (code.indexOf('err:badpass') === 0) {
            var parts = code.split(':');
            var left = parts.length > 2 ? parseInt(parts[2], 10) : 0;
            if (!isNaN(left) && left > 0) {
                return 'Неверный пароль. Осталось попыток: ' + left + '.';
            }
            return MESSAGES['err:badpass'];
        }
        return MESSAGES[code] || 'Что-то пошло не так. Попробуйте ещё раз.';
    }

    function initials(name) {
        var clean = (name || '').replace(/[^A-Za-z0-9_]/g, '');
        if (!clean) { return '?'; }
        return clean.charAt(0).toUpperCase();
    }

    function showToast(text, kind) {
        toastText.textContent = text;
        toast.classList.remove('is-ok', 'is-err');
        if (kind === 'ok') { toast.classList.add('is-ok'); }
        if (kind === 'err') { toast.classList.add('is-err'); }
        toast.classList.add('is-visible');

        clearTimeout(toastTimer);
        toastTimer = setTimeout(function () {
            toast.classList.remove('is-visible');
        }, 3200);
    }

    function fieldError(field, message) {
        if (!field) { return; }
        field.classList.add('is-error');
        var msg = field.querySelector('.field-msg');
        if (msg) { msg.textContent = message || ''; }
    }

    function clearErrors() {
        [fieldPass, fieldRepeat].forEach(function (field) {
            if (!field) { return; }
            field.classList.remove('is-error');
            var msg = field.querySelector('.field-msg');
            if (msg) { msg.textContent = ''; }
        });
    }

    function shake() {
        card.classList.remove('is-shaking');
        /* перезапуск анимации */
        void card.offsetWidth;
        card.classList.add('is-shaking');
        setTimeout(function () { card.classList.remove('is-shaking'); }, 560);
    }

    function setBusy(value) {
        busy = value;
        submit.disabled = value;
        submit.classList.toggle('is-loading', value);
        pass.disabled = value;
        pass2.disabled = value;
    }

    /* ------------------------------------------------------------ режимы */

    function setMode(next, focus) {
        mode = next;
        var register = (next === 'register');

        card.classList.toggle('mode-register', register);
        Array.prototype.forEach.call(tabs.querySelectorAll('.tab'), function (tab) {
            var active = tab.dataset.mode === next;
            tab.classList.toggle('is-active', active);
            tab.setAttribute('aria-selected', active ? 'true' : 'false');
        });

        submitLabel.textContent = register ? 'Создать аккаунт' : 'Войти на вокзал';
        clearErrors();

        if (register) {
            updateStrength(pass.value);
        }
        if (focus !== false) {
            setTimeout(function () {
                (register ? pass : pass).focus();
            }, 120);
        }
    }

    function updateStrength(value) {
        var score = 0;
        if (value.length >= 6) { score++; }
        if (value.length >= 10) { score++; }
        if (/[A-ZА-Я]/.test(value) && /[a-zа-я]/.test(value)) { score++; }
        if (/[0-9]/.test(value) && /[^A-Za-zА-Яа-я0-9]/.test(value)) { score++; }
        if (value.length === 0) { score = 0; }

        strength.dataset.level = String(score);
        var labels = ['надёжность пароля', 'слабый', 'средний', 'хороший', 'надёжный'];
        strengthTxt.textContent = labels[score];
        strengthTxt.style.color = score >= 4 ? '#6FE3A1' : (score >= 2 ? '#C9B48A' : '');
    }

    /* ------------------------------------------------------------ отправка */

    function submitForm() {
        if (busy || !ready) { return; }

        clearErrors();
        var value = pass.value || '';
        var repeat = pass2.value || '';

        if (value.length < 6) {
            fieldError(fieldPass, 'Минимум 6 символов.');
            shake();
            pass.focus();
            return;
        }

        if (mode === 'register') {
            if (repeat.length === 0) {
                fieldError(fieldRepeat, 'Повторите пароль для подтверждения.');
                shake();
                pass2.focus();
                return;
            }
            if (value !== repeat) {
                fieldError(fieldRepeat, 'Пароли не совпадают.');
                shake();
                pass2.focus();
                return;
            }
        }

        setBusy(true);

        if (mode === 'register') {
            cefApi.emit('auth:register', value, repeat);
        } else {
            cefApi.emit('auth:login', value);
        }
    }

    function handleResult(ok, code, nick) {
        setBusy(false);

        if (ok) {
            submit.classList.add('is-done');
            submitLabel.textContent = (code === 'ok:register') ? 'Аккаунт создан' : 'Готово';
            showToast(codeMessage(code), 'ok');

            if (nick) {
                nickEl.textContent = nick;
                avatarEl.textContent = initials(nick);
            }

            setTimeout(function () {
                stage.classList.add('is-leaving');
            }, 900);
            return;
        }

        showToast(codeMessage(code), 'err');
        shake();

        if (code === 'err:short' || code === 'err:long' || code === 'err:chars') {
            fieldError(fieldPass, codeMessage(code));
            pass.focus();
            pass.select();
        } else if (code === 'err:mismatch') {
            fieldError(fieldRepeat, codeMessage(code));
            pass2.focus();
        } else if (code === 'err:exists') {
            pass.focus();
            pass.select();
        } else if (code === 'err:badpass' || code.indexOf('err:badpass') === 0) {
            fieldError(fieldPass, 'Проверьте раскладку и Caps Lock.');
            pass.focus();
            pass.select();
        } else {
            pass.focus();
        }
    }

    /* ------------------------------------------------------------ события сервера */

    cefApi.on('auth:init', function (nick, hasAccount, version) {
        ready = true;
        card.classList.add('is-ready');

        nickEl.textContent = nick || 'игрок';
        avatarEl.textContent = initials(nick);

        if (version) {
            versionEl.textContent = 'v' + version;
        }

        setMode(hasAccount ? 'login' : 'register', false);

        setTimeout(function () {
            pass.focus();
        }, 380);
    });

    cefApi.on('auth:result', function (ok, code, nick) {
        handleResult(!!ok, code, nick);
    });

    cefApi.on('auth:busy', function (value) {
        setBusy(!!value);
    });

    cefApi.on('auth:hide', function () {
        stage.classList.add('is-leaving');
    });

    /* ------------------------------------------------------------ интерфейс */

    tabs.addEventListener('click', function (event) {
        var tab = event.target.closest('.tab');
        if (!tab || busy) { return; }
        if (tab.dataset.mode === mode) { return; }
        setMode(tab.dataset.mode);
    });

    eye.addEventListener('click', function () {
        var reveal = pass.type === 'password';
        pass.type = reveal ? 'text' : 'password';
        eye.classList.toggle('is-on', reveal);
        eye.setAttribute('aria-label', reveal ? 'Скрыть пароль' : 'Показать пароль');
        pass.focus();
    });

    pass.addEventListener('input', function () {
        fieldPass.classList.remove('is-error');
        if (mode === 'register') { updateStrength(pass.value); }
    });

    pass2.addEventListener('input', function () {
        fieldRepeat.classList.remove('is-error');
    });

    form.addEventListener('submit', function (event) {
        event.preventDefault();
        submitForm();
    });

    document.addEventListener('keydown', function (event) {
        if (event.key === 'Escape') {
            event.preventDefault();
            return;
        }
        if (event.key === 'Enter') {
            event.preventDefault();
            submitForm();
        }
    });

    document.addEventListener('contextmenu', function (event) { event.preventDefault(); });

    /* ------------------------------------------------------------ превью */

    if (isPreview) {
        document.documentElement.classList.add('is-preview');
        setTimeout(function () {
            card.classList.add('is-ready');
        }, 500);
    }

    cefApi.emit('auth:ready');
})();
