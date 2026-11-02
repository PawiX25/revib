// The runtime's calls to the page (port/runtime/psx.h): each goes to the
// host object the page (port/web/vib.js) or the test runner gives the module
// as Module.vib.
addToLibrary({
  js_present: function (ptr, w, h, ops, n, fallback, vram) {
    Module.vib.present(HEAPU8.subarray(ptr, ptr + w * h * 4), w, h, {
      ops: HEAP32.subarray(ops >> 2, (ops >> 2) + n),
      fallback: !!fallback,
      vram: HEAPU16.subarray(vram >> 1, (vram >> 1) + 1024 * 512),
    });
  },
  js_audio_push: function (ptr, frames) {
    Module.vib.audio(HEAP16.subarray(ptr >> 1, (ptr >> 1) + frames * 2), frames);
  },
  js_audio_queued: function () {
    return Module.vib.audioQueued();
  },
  js_now: function () {
    return performance.now();
  },
  js_fast: function () {
    return Module.vib.fast ? 1 : 0;
  },
  js_paused: function () {
    return Module.vib.paused ? 1 : 0;
  },
  js_cd_read: function (lba, dest) {
    return Module.vib.cdRead(lba, HEAPU8.subarray(dest, dest + 2352));
  },
  // the game waits (Asyncify unwinds it) until the sector has been read
  js_cd_wait__async: true,
  js_cd_wait__deps: ['$Asyncify'],
  js_cd_wait: function (lba) {
    return Asyncify.handleAsync(() => Module.vib.cdWait(lba));
  },
  js_data_read: function (lba, dest) {
    return Module.vib.dataRead(lba, HEAPU8.subarray(dest, dest + 2048)) ? 1 : 0;
  },
  js_cd_state: function () {
    return Module.vib.cdState();
  },
  js_cd_changes: function () {
    return Module.vib.cdChanges();
  },
  js_cd_toc: function (ptr, max) {
    const toc = Module.vib.toc();
    const n = Math.min(toc.length - 1, max);
    for (let i = 0; i < n; i++) HEAP32[(ptr >> 2) + i] = toc[i];
    HEAP32[(ptr >> 2) + n] = toc[toc.length - 1];
    return n;
  },
  js_pad: function (port) {
    return Module.vib.pad(port);
  },
  js_menu: function (screen, rows, columns, x, y, active, wrap) {
    if (Module.vib.menu) Module.vib.menu({ screen, rows, columns, x, y, active: !!active, wrapY: !!(wrap & 1), wrapX: !!(wrap & 2) });
  },
  js_model: function (id, name) {
    if (Module.vib.model) Module.vib.model(id, UTF8ToString(name));
  },
  js_object: function (id, object, x0, y0, x1, y1) {
    if (Module.vib.object) Module.vib.object(id, object, x0, y0, x1, y1);
  },
  js_main_done: function (status) {
    Module.vib.mainDone(status);
  },
  js_log: function (ptr) {
    Module.vib.log(UTF8ToString(ptr));
  },
});
