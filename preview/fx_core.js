// ------------------------------------------------------ touchdown explosion
// A blast, not a firework: the panel flashes white, a shockwave ring tears
// outward in the team's colors, debris flies with trails, and secondary
// blasts keep going off around it. All of it drawn on the same 64x64 grid
// as the scoreboard, so the panel firmware can do exactly this.
const FX_MS = 5200;
let fx = null;

function rgbStr(c, b) {
  return "rgb(" + Math.round(c[0]*b) + "," + Math.round(c[1]*b) + "," + Math.round(c[2]*b) + ")";
}

// Dark team colors would vanish against black, so lift them the same way
// the scoreboard does before using them in the blast.
function liftFx(c) {
  if (!c) return [255,255,255];
  const m = Math.max(c[0], c[1], c[2]);
  if (m === 0) return [255,255,255];
  if (m < 150) { const f = 200/m;
    return [Math.min(255, c[0]*f|0), Math.min(255, c[1]*f|0), Math.min(255, c[2]*f|0)]; }
  return c;
}

function makeBlast(cx, cy, palette, power) {
  const parts = [];
  // three shells at different speeds - reads as one violent burst rather
  // than a tidy ring
  const rings = [{n: 30*power, spd: 1.9}, {n: 22*power, spd: 1.15}, {n: 14*power, spd: 0.55}];
  for (const ring of rings) {
    for (let i = 0; i < ring.n; i++) {
      const ang = Math.random() * Math.PI * 2;
      const spd = ring.spd * (0.6 + Math.random()*0.8);
      parts.push({
        x: cx, y: cy,
        vx: Math.cos(ang)*spd, vy: Math.sin(ang)*spd,
        life: 1, decay: 0.008 + Math.random()*0.007,
        color: palette[(Math.random()*palette.length)|0]
      });
    }
  }
  return parts;
}

function startFireworks(palette, label) {
  const pal = palette.map(liftFx);
  fx = {
    t0: performance.now(),
    pal: pal,
    label: label,
    parts: makeBlast(32, 30, pal, 1.6),      // the main hit, centre panel
    waves: [
      {cx:32, cy:30, t0:0,   color: pal[0], speed:0.055, max:56},
      {cx:32, cy:30, t0:110, color: pal[1] || pal[0], speed:0.042, max:50},
      {cx:32, cy:30, t0:230, color: [255,255,255], speed:0.032, max:44}
    ],
    nextBlast: 620
  };
  requestAnimationFrame(stepFireworks);
}

function drawRing(cx, cy, r, color, bright) {
  if (r < 1) return;
  const steps = Math.max(10, Math.round(r * 7));
  for (let i = 0; i < steps; i++) {
    const a = (Math.PI*2*i)/steps;
    put(Math.round(cx + Math.cos(a)*r), Math.round(cy + Math.sin(a)*r), rgbStr(color, bright));
  }
}

function stepFireworks(now) {
  if (!fx) return;
  const el = now - fx.t0;
  px = new Array(W*H).fill(null);

  // 1. the flash - whole panel white, gone in a blink
  if (el < 130) {
    const b = 1 - el/130;
    const c = rgbStr([255,255,255], b);
    for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) put(x, y, c);
  }

  // 2. shockwave rings tearing outward
  for (const w of fx.waves) {
    const t = el - w.t0;
    if (t <= 0) continue;
    const r = t * w.speed;
    if (r > w.max) continue;
    drawRing(w.cx, w.cy, r, w.color, Math.max(0, 1 - r/w.max));
    drawRing(w.cx, w.cy, r - 1, w.color, Math.max(0, 0.45 - r/w.max));
  }

  // 3. secondary blasts, so it keeps erupting instead of petering out
  if (el > fx.nextBlast && el < FX_MS - 800) {
    const cx = 6 + Math.random()*52, cy = 6 + Math.random()*46;
    fx.parts = fx.parts.concat(makeBlast(cx, cy, fx.pal, 1.05));
    fx.waves.push({cx:cx, cy:cy, t0:el, color: fx.pal[(Math.random()*fx.pal.length)|0],
                   speed:0.045, max:30});
    // drop spent debris so the list doesn't grow without bound
    fx.parts = fx.parts.filter(p => p.life > 0);
    fx.nextBlast = el + 240 + Math.random()*200;
  }

  // 4. debris
  for (const pt of fx.parts) {
    if (pt.life <= 0) continue;
    pt.x += pt.vx; pt.y += pt.vy;
    pt.vy += 0.022;      // gravity
    pt.vx *= 0.985;
    pt.life -= pt.decay;
    if (pt.life > 0) {
      const b = Math.max(0, Math.min(1, pt.life));
      put(Math.round(pt.x), Math.round(pt.y), rgbStr(pt.color, b));
      if (b > 0.4) put(Math.round(pt.x - pt.vx), Math.round(pt.y - pt.vy), rgbStr(pt.color, b*0.4));
      if (b > 0.7) put(Math.round(pt.x - pt.vx*2), Math.round(pt.y - pt.vy*2), rgbStr(pt.color, b*0.18));
    }
  }

  // 5. the team punches through once the first blast clears
  if (el > 620) {
    const cyc = fx.pal[Math.floor(el/130) % fx.pal.length];
    const lw = tw(fx.label, F5, 2);
    if (lw <= W - 2) {
      text((W-lw)>>1, 20, fx.label, rgbStr(cyc, 1), F5, 2);
    } else {
      const lw1 = tw(fx.label, F5, 1);
      text((W-lw1)>>1, 24, fx.label, rgbStr(cyc, 1), F5, 1);
    }
    const sw = tw("TOUCHDOWN", F3, 1);
    text((W-sw)>>1, 40, "TOUCHDOWN", (Math.floor(el/130) % 2) ? "rgb(255,255,255)" : rgbStr(cyc, 1), F3, 1);
  }

  paint();
  if (el < FX_MS) requestAnimationFrame(stepFireworks);
  else { fx = null; draw(); }
}
