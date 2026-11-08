/* Thin wrapper over the plugin bridge, so the UI also runs in a plain browser
   (useful for editing the pages without the game).

   The plugin injects window.cef, but not necessarily before these scripts run:
   a handler registered too early was kept locally and never reached the server.
   Registrations are therefore queued and bound as soon as the bridge appears.

   Several files listen to the same event (ui.js and inventory.js both want
   ui:close), so every event keeps a list of callbacks instead of one. */
(function () {
  var handlers = {};
  var bound = {};
  var queue = [];
  var polling = 0;

  function log() {
    if (window.console && window.console.log) console.log.apply(console, arguments);
  }

  window.RW = window.RW || {};

  function dispatch(name, args) {
    var list = handlers[name] || [];
    for (var i = 0; i < list.length; i++) list[i].apply(null, args);
  }

  function bind(name) {
    if (bound[name]) return;
    bound[name] = true;
    if (window.cef && typeof window.cef.on === 'function') {
      window.cef.on(name, function () { dispatch(name, Array.prototype.slice.call(arguments)); });
    }
  }

  RW.on = function (name, callback) {
    if (!handlers[name]) handlers[name] = [];
    handlers[name].push(callback);
    queue.push(name);
    bind(name);
    if (!polling) polling = window.setInterval(flush, 50);
  };

  /* Binds everything again if the bridge showed up after the scripts ran. */
  function flush() {
    if (!RW.ready()) return;
    while (queue.length) bind(queue.shift());
    window.clearInterval(polling);
    polling = 0;
  }
  flush();

  RW.emit = function (name) {
    var args = Array.prototype.slice.call(arguments, 1);
    if (window.cef && typeof window.cef.emit === 'function') {
      window.cef.emit.apply(window.cef, [name].concat(args));
    } else {
      log('[RW emit]', name, args);
    }
  };

  /* True once the plugin bridge is really there (inside the game). */
  RW.ready = function () {
    return !!(window.cef && typeof window.cef.emit === 'function');
  };

  /* The server asks before it trusts the browser: some plugin builds never
     deliver page events back to Pawn, and clicks would vanish silently.
     Every page answers the ping, so the first one to load is enough. */
  RW.on('cef:ping', function () {
    RW.emit('cef:pong');
  });

  /* Local preview support: ?fake=1 renders a demo card in a browser.
     In the game this never runs, so a first packet that arrives late cannot
     be overwritten by the demo. */
  RW.fake = function (name, args) {
    dispatch(name, args);
  };

  RW.demo = function () {
    return window.location.search.indexOf('fake=1') >= 0;
  };
})();
