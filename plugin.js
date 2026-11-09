// Lampa plugin: send playback to VLC PS5 through vlc-bridge.
// Falls back to Lampa's own player if the bridge is not reachable.
(function () {
  var BRIDGE = 'http://127.0.0.1:8765';

  function start() {
    if (window.__vlc_bridge) return;
    window.__vlc_bridge = true;
    var orig = Lampa.Player.play;

    Lampa.Player.play = function (params) {
      var self = this, args = arguments;
      if (params && params.url && /^http/i.test(params.url)) {
        fetch(BRIDGE + '/play?url=' + encodeURIComponent(params.url) +
              '&title=' + encodeURIComponent(params.title || ''))
          .then(function (r) { if (!r.ok) throw new Error('bridge ' + r.status); })
          .catch(function () { orig.apply(self, args); });
        return;
      }
      return orig.apply(self, args);
    };
  }

  if (window.appready) start();
  else Lampa.Listener.follow('app', function (e) { if (e.type === 'ready') start(); });
})();
