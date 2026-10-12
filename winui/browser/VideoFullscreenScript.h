#pragma once
namespace miniwindow {
// Runs in every document/frame; only the top document reports to the host.
inline constexpr auto VideoFullscreenScript=LR"MWBVIDEO(
(() => {
  if (window.__miniWindowVideoWatch) return;
  window.__miniWindowVideoWatch = true;
  const frames = new Map();
  const tag = 'MINIWINDOW_VIDEO_VIEWPORT_V1';
  let last = '', queued = false;
  const visible = e => {
    const r = e.getBoundingClientRect(), s = getComputedStyle(e);
    return r.width > 1 && r.height > 1 && s.display !== 'none' && s.visibility !== 'hidden';
  };
  const covers = e => {
    const r = e.getBoundingClientRect();
    return r.left <= 3 && r.top <= 3 && r.right >= innerWidth - 3 && r.bottom >= innerHeight - 3;
  };
  const pageFullscreen = e => {
    for (let p = e; p && p !== document.documentElement; p = p.parentElement) {
      // CSS/web fullscreen is not a browser API. Require a viewport-covering
      // fixed container (or the player's explicit fullscreen class).
      if (covers(p) && (getComputedStyle(p).position === 'fixed' ||
          /(?:webfullscreen|web-fullscreen|web_fullscreen|mode-webscreen|mode-fullscreen)/i.test(p.className || ''))) return true;
    }
    return false;
  };
  const descendants = (root, list = []) => {
    for (const e of root.querySelectorAll('*')) {
      if (e.matches('video,iframe')) list.push(e);
      if (e.shadowRoot) descendants(e.shadowRoot, list);
    }
    return list;
  };
  function report() {
    queued = false;
    const media = descendants(document);
    const connectedFrames = media.filter(e => e.tagName === 'IFRAME');
    for (const source of frames.keys()) if (!connectedFrames.some(e => e.contentWindow === source)) frames.delete(source);
    const full = document.fullscreenElement;
    let candidate = null, area = 0;
    for (const e of media) {
      if (!visible(e)) continue;
      const frame = e.tagName === 'IFRAME' ? frames.get(e.contentWindow) : null;
      const ratio = frame ? frame.aspect : e.videoWidth / e.videoHeight;
      if (!Number.isFinite(ratio) || ratio <= .1 || ratio >= 10) continue;
      const r = e.getBoundingClientRect(), size = r.width * r.height;
      let inFull = false;
      for (let p = e; full && p; p = p.parentElement || p.getRootNode()?.host) {
        if (p === full) {inFull = true; break;}
      }
      if (full && !inFull) continue;
      const active = inFull || pageFullscreen(e) || (!!frame?.full && covers(e));
      if (!candidate || (active && !candidate.full) || (active === candidate.full && size > area)) {area = size; candidate = {full: active, aspect: ratio};}
    }
    const value = {full: !!full || !!candidate?.full, aspect: candidate?.aspect || 0};
    if (window !== window.top) {
      window.parent.postMessage({tag, ...value}, '*');
      return;
    }
    const key = value.full + ':' + value.aspect;
    if (key === last) return;
    last = key;
    window.chrome?.webview?.postMessage(JSON.stringify({kind: 'fullscreen', flag: value.full}));
    if (value.full && value.aspect > .1)
      window.chrome?.webview?.postMessage(JSON.stringify({kind: 'aspect', aspect: value.aspect}));
  }
  function schedule() { if (!queued) {queued = true; setTimeout(report, 80);} }
  addEventListener('message', event => {
    const v = event.data;
    if (!v || v.tag !== tag || typeof v.full !== 'boolean' || !Number.isFinite(v.aspect)) return;
    if (!descendants(document).filter(e => e.tagName === 'IFRAME').some(e => e.contentWindow === event.source)) return;
    frames.set(event.source, {full: v.full, aspect: v.aspect});schedule();
  });
  for (const name of ['fullscreenchange', 'loadedmetadata', 'resize', 'playing'])
    document.addEventListener(name, schedule, true);
  addEventListener('resize', schedule);
  addEventListener('pagehide', () => frames.clear());
  // A bounded polling cadence catches CSS-only player switches and metadata
  // becoming available without flooding native messages or mutating the page.
  setInterval(schedule, 500);
  schedule();
})();
)MWBVIDEO";
}
