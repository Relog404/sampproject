/* Notifications: quiet pills sliding up from the bottom, iOS style. */
(function () {
  var box = document.getElementById('toasts');
  var LIMIT = 4;

  function kind(k) {
    if (k === 1) return 'ok';
    if (k === 2) return 'err';
    return '';
  }

  RW.on('ui:toast', function (text, k) {
    var node = document.createElement('div');
    node.className = 'toast ' + kind(k);
    node.innerHTML = '<span class="dot"></span><span></span>';
    node.lastChild.textContent = text || '';
    box.appendChild(node);

    while (box.children.length > LIMIT) box.removeChild(box.firstChild);

    window.setTimeout(function () {
      node.classList.add('out');
      window.setTimeout(function () {
        if (node.parentNode) node.parentNode.removeChild(node);
      }, 280);
    }, 3200);
  });

  if (window.location.search.indexOf('fake=1') >= 0) {
    RW.fake('ui:toast', ['Engine started', 1]);
    window.setTimeout(function () { RW.fake('ui:toast', ['No key in the ignition', 2]); }, 700);
  }
})();
