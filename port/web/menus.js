// The game's menus by mouse and keyboard. The game tells, each frame, which
// menu it reads and where its cursor is (Menu.h's port_menu_report), and
// where each object of its models was drawn (TmdLineObj.h): pointing at an
// item's object moves the cursor to it, clicking chooses it. The cursor is
// moved a cell at a time, as by the d-pad, so the game plays its own
// movements and sounds.

const B = { up: 0x0010, right: 0x0020, down: 0x0040, left: 0x0080, triangle: 0x1000, cross: 0x4000 };

// each menu: the model its items are drawn with, and its items: the objects
// drawn for them, their cells ([x, y]: the first is gone to), and the button
// pressed there on a click (cross by default; an arrow's on its row)
const row = (objects, y, extra = {}) => ({ objects, cells: [[0, y]], ...extra });
const MENUS = {
  // the title executable: 100 + LanguageSelect's menu
  100: {
    model: 'menu/lang/lang.tmd',
    items: [
      { objects: [0], cells: [[0, 0]] }, // Deutsch
      { objects: [1], cells: [[1, 0]] }, // English
      { objects: [2], cells: [[2, 0]] }, // Español
      { objects: [3], cells: [[0, 1], [1, 1]] }, // Français
      { objects: [4], cells: [[2, 1]] }, // Italiano
    ],
  },
  101: { model: 'menu/mode/mode.tmd', items: [row([0], 0), row([1], 1), row([2], 2, { back: true })] }, // Game, How to play, Language
  // the game executable: Title::Screen
  1: { model: 'title/ma/ma.tmd', items: [row([1], 0), row([2], 1), row([3], 2), row([4], 3, { back: true })] },
  2: { model: 'title/mode/mdv.tmd', items: [row([1], 0), row([2], 1), row([3], 2), row([4], 3), row([5], 4, { back: true })] },
  3: { model: 'title/mode/mdu.tmd', items: [row([1], 0), row([2], 1), row([3], 2), row([4], 3), row([5], 4), row([6], 5, { back: true })] },
  4: { model: 'title/speed/md_spd.tmd', items: [row([1], 0), row([2], 1), row([3], 2), row([4], 3, { back: true })] },
  5: { model: 'title/cd/cd.tmd', items: [row([1], 0), row([2], 1), row([3], 2, { back: true })] },
  6: { model: 'title/trk/trk.tmd', items: [row([0], 0, { press: 'left' }), row([1], 0, { press: 'right' }), row([4], 0), row([2], 1, { back: true })] },
  10: { model: 'title/option/op.tmd', items: [row([5], 0), row([6], 1), row([7], 2, { back: true })] },
  11: { model: 'title/hiscore/hi.tmd', items: [row([0], 0, { press: 'left' }), row([1], 0, { press: 'right' }), row([7], 1, { back: true })] },
  12: { model: 'title/hiscore/hi.tmd', items: [row([0], 0, { press: 'left' }), row([1], 0, { press: 'right' }), row([7], 1, { back: true })] },
  15: { model: 'title/again/ag.tmd', items: [row([1], 0), row([2], 1, { back: true })] },
};
// from `at` to `to` of `n`: forward (down, right) or back; round the shorter way if the menu goes round
function forward(at, to, n, wraps) {
  if (!wraps) return to > at;
  return (to - at + n) % n <= (at - to + n) % n;
}

// screens with no menu (rows 0): a click confirms; the alerts take cancel only ("△ = Back")
const ALERTS = { 7: 1, 8: 1 };

export class MenuDriver {
  constructor() {
    this.menu = null;   // the last report
    this.seenAt = -99;  // its frame
    this.frame = 0;
    this.task = null;
    this.wait = 0;      // frames before the next step
    this.names = {};    // the models' files, by id
    this.drawing = [];  // the objects drawn this frame
    this.drawn = [];    // and in the frame before (on display)
  }

  report(menu) {
    this.menu = menu;
    this.seenAt = this.frame;
  }

  model(id, name) {
    this.names[id] = name;
  }

  object(id, object, x0, y0, x1, y1) {
    this.drawing.push({ model: this.names[id], object, x0: x0 + 256, y0: y0 + 144, x1: x1 + 256, y1: y1 + 144 });
  }

  // a menu is being read (its report from this frame or the last)
  get current() {
    return this.menu && this.frame - this.seenAt <= 1 ? this.menu : null;
  }

  // the item under (x, y), in the picture's 512 x 288; null if none
  itemAt(x, y) {
    const m = this.current;
    const menu = m && MENUS[m.screen];
    if (!menu) return null;
    let best = null;
    let bestArea = Infinity;
    for (const o of this.drawn) {
      if (o.model !== menu.model || x < o.x0 - 2 || x > o.x1 + 2 || y < o.y0 - 2 || y > o.y1 + 2) continue;
      const item = menu.items.find((i) => i.objects.includes(o.object));
      const area = (o.x1 - o.x0) * (o.y1 - o.y0);
      if (item && area < bestArea) {
        best = item;
        bestArea = area;
      }
    }
    return best;
  }

  // a click on the picture
  click(x, y) {
    const m = this.current;
    if (!m) return;
    if (m.rows === 0) {
      this.press(ALERTS[m.screen] ? 'triangle' : 'cross');
      return;
    }
    const item = this.itemAt(x, y);
    if (item) this.go(item, true);
  }

  // to the item, and confirm (or its button)
  go(item, confirm) {
    const m = this.current;
    if (!m) return;
    this.task = { type: 'goto', screen: m.screen, cells: item.cells, then: confirm ? item.press || 'cross' : null, age: 0 };
  }

  // back: the menu's Back item, or cancel
  back() {
    const m = this.current;
    if (!m) return;
    const back = MENUS[m.screen] && MENUS[m.screen].items.find((i) => i.back);
    if (back) this.go(back, true);
    else this.press('triangle');
  }

  press(button) {
    if (this.current) this.task = { type: 'press', button, age: 0 };
  }

  // each frame (the controller's poll): the buttons to hold
  step() {
    this.frame++;
    this.drawn = this.drawing;
    this.drawing = [];
    if (this.wait > 0) {
      /*
       * After a press: the game takes the controller in at the end of a
       * frame, moves its cursor in the next, and tells where it is (before
       * moving) in the one after; only then is the next step decided.
       */
      this.wait--;
      return 0;
    }
    const t = this.task;
    const m = this.current;
    if (!t || !m) return 0;
    if (++t.age > 400 || (t.screen !== undefined && m.screen !== t.screen)) {
      this.task = null;
      return 0;
    }
    if (t.type === 'press') {
      this.task = null;
      return this.tap(B[t.button]);
    }
    if (!m.active) return 0; // (a movement playing: no input is taken)
    if (!t.cells.some(([x, y]) => x === m.x && y === m.y)) {
      const [tx, ty] = t.cells[0];
      if (m.y !== ty) return this.tap(forward(m.y, ty, m.rows, m.wrapY) ? B.down : B.up);
      return this.tap(forward(m.x, tx, m.columns, m.wrapX) ? B.right : B.left);
    }
    this.task = null;
    return t.then ? this.tap(B[t.then]) : 0;
  }

  tap(bits) {
    this.wait = 3;
    return bits;
  }
}
