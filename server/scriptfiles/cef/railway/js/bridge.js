/* Thin wrapper over the plugin bridge, so the UI also runs in a plain browser
   (useful for editing the pages without the game). */
(function () {
  var handlers = {};

  function log() {
    if (window.console && window.console.log) console.log.apply(console, arguments);
  }

  window.RW = window.RW || {};

  RW.on = function (name, callback) {
    handlers[name] = callback;
    if (window.cef && typeof window.cef.on === 'function') window.cef.on(name, callback);
  };

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

  /* Local preview support: ?fake=1 renders a demo card in a browser. */
  RW.fake = function (name, args) {
    if (handlers[name]) handlers[name].apply(null, args);
  };
})();
