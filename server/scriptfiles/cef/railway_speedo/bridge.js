/* Wait for the real CEF bridge. Never mistake delayed injection for a demo client.
   Replay subscriptions before sending ready; retry ready until the server ACKs. */
(function () {
    'use strict';
    var nativeApi = null, handlers = [], queue = [], readyEvent = '', acknowledged = false;
    var attempts = 0;
    function connect() {
        var candidate = window.cef;
        if (nativeApi || !candidate || typeof candidate.on !== 'function' || typeof candidate.emit !== 'function') return;
        nativeApi = candidate;
        nativeApi.on('railway:ack', function () { acknowledged = true; });
        handlers.forEach(function (h) { nativeApi.on(h[0], h[1]); });
        queue.forEach(function (args) { nativeApi.emit.apply(nativeApi, args); });
        queue = [];
    }
    window.railwayCef = {
        on: function (name, fn) {
            handlers.push([name, fn]);
            if (nativeApi) nativeApi.on(name, fn);
        },
        emit: function (name) {
            var args = Array.prototype.slice.call(arguments);
            if (/:ready$/.test(name)) readyEvent = name;
            connect();
            if (nativeApi) nativeApi.emit.apply(nativeApi, args);
            else queue.push(args);
        }
    };
    var poll = setInterval(function () {
        connect();
        if (nativeApi) clearInterval(poll);
    }, 100);
    var retry = setInterval(function () {
        connect();
        if (acknowledged || (nativeApi && ++attempts > 90)) { clearInterval(retry); clearInterval(poll); return; }
        if (nativeApi && readyEvent) nativeApi.emit(readyEvent);
    }, 2000);
})();
