const TILE = 256;
const MAP_BASE = "https://basemaps.cartocdn.com/light_nolabels";
const MAP_LABELS = "https://basemaps.cartocdn.com/light_only_labels";

function tileXY(lat, lon, z) {
  const n = 2 ** z;
  const latRad = Math.max(-85.0511, Math.min(85.0511, lat)) * Math.PI / 180;
  return {
    x: (lon + 180) / 360 * n,
    y: (1 - Math.asinh(Math.tan(latRad)) / Math.PI) / 2 * n
  };
}

function loadImage(url) {
  return new Promise((resolve) => {
    const img = new Image();
    img.crossOrigin = "anonymous";
    img.onload = () => resolve(img);
    img.onerror = () => resolve(null);
    img.src = url;
  });
}

function tileUrl(base, z, x, y) {
  const n = 2 ** z;
  const wrappedX = ((x % n) + n) % n;
  if (y < 0 || y >= n) return null;
  return `${base}/${z}/${wrappedX}/${y}.png`;
}

function radarUrl(template, z, x, y) {
  const n = 2 ** z;
  const wrappedX = ((x % n) + n) % n;
  if (y < 0 || y >= n) return null;
  return template.replace("{z}", z).replace("{x}", wrappedX).replace("{y}", y);
}

async function composeMap(canvas, data) {
  const width = Math.max(180, canvas.clientWidth || 640);
  const height = Math.max(150, canvas.clientHeight || 400);
  canvas.width = width;
  canvas.height = height;
  const ctx = canvas.getContext("2d");
  ctx.fillStyle = "#f1f2ef";
  ctx.fillRect(0, 0, width, height);

  const z = Math.max(1, Math.min(7, Number(data.zoom) || 7));
  const center = tileXY(Number(data.latitude), Number(data.longitude), z);
  const centerPxX = center.x * TILE;
  const centerPxY = center.y * TILE;
  const left = centerPxX - width / 2;
  const top = centerPxY - height / 2;
  const minTileX = Math.floor(left / TILE);
  const maxTileX = Math.floor((left + width - 1) / TILE);
  const minTileY = Math.floor(top / TILE);
  const maxTileY = Math.floor((top + height - 1) / TILE);
  const jobs = [];

  for (let ty = minTileY; ty <= maxTileY; ty++) {
    for (let tx = minTileX; tx <= maxTileX; tx++) {
      const px = Math.round(tx * TILE - left);
      const py = Math.round(ty * TILE - top);
      const urls = [
        tileUrl(MAP_BASE, z, tx, ty),
        radarUrl(data.radar_tile_template, z, tx, ty),
        tileUrl(MAP_LABELS, z, tx, ty)
      ];
      jobs.push((async () => {
        const imgs = await Promise.all(urls.map(url => url ? loadImage(url) : Promise.resolve(null)));
        if (imgs[0]) ctx.drawImage(imgs[0], px, py, TILE, TILE);
        if (imgs[1]) {
          ctx.save();
          ctx.globalAlpha = 0.88;
          ctx.drawImage(imgs[1], px, py, TILE, TILE);
          ctx.restore();
        }
        if (imgs[2]) ctx.drawImage(imgs[2], px, py, TILE, TILE);
      })());
    }
  }

  await Promise.all(jobs);

  // Crosshair marks the configured center, not a measured precipitation value.
  ctx.save();
  ctx.strokeStyle = "#202020";
  ctx.lineWidth = 2;
  ctx.beginPath();
  ctx.arc(width / 2, height / 2, 6, 0, Math.PI * 2);
  ctx.moveTo(width / 2 - 11, height / 2);
  ctx.lineTo(width / 2 + 11, height / 2);
  ctx.moveTo(width / 2, height / 2 - 11);
  ctx.lineTo(width / 2, height / 2 + 11);
  ctx.stroke();
  ctx.restore();
}

function formatFrameTime(seconds) {
  if (!seconds) return "czas klatki niedostępny";
  return new Date(seconds * 1000).toLocaleString("pl-PL", {
    day: "2-digit", month: "2-digit", hour: "2-digit", minute: "2-digit",
    timeZone: "Europe/Warsaw"
  });
}

export default async function render(shadow, ctx) {
  const data = ctx.data || {};
  const title = String(data.title || "Radar opadów");
  shadow.innerHTML = `
    <link rel="stylesheet" href="/static/style/spectra-widgets.css">
    <style>
      .radar{height:100%;min-height:0;display:flex;flex-direction:column;gap:6px}
      .radar-head{display:flex;justify-content:space-between;align-items:baseline;gap:8px}
      .radar-title{font-size:var(--fs-lead,1.1em);font-weight:800;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
      .radar-meta{font-size:var(--fs-caption,.72em);color:var(--text-secondary,#555);display:flex;justify-content:space-between;gap:8px}
      .radar-map{width:100%;height:100%;min-height:0;flex:1;display:block;object-fit:fill;background:#f1f2ef}
      .radar-error{font-size:var(--fs-body,1em);padding:10px}
      .radar .w{height:100%;min-height:0}
      .radar-xs .radar-meta .coords{display:none}
    </style>
    <div class="w radar ${ctx.cell?.size === "xs" ? "radar-xs" : ""}">
      <div class="radar-head"><div class="radar-title"></div></div>
      <canvas class="radar-map" aria-label="Mapa radarowa opadów"></canvas>
      <div class="radar-meta"><span class="coords"></span><span class="time"></span></div>
      <div class="radar-meta"><span>Weather data by <a href="https://www.rainviewer.com/" target="_blank" rel="noreferrer">RainViewer</a></span><span>co 10 min</span></div>
    </div>`;
  shadow.querySelector(".radar-title").textContent = title;

  if (data.error) {
    shadow.querySelector(".radar").innerHTML = `<div class="radar-error"></div>`;
    shadow.querySelector(".radar-error").textContent = data.error;
    return;
  }

  shadow.querySelector(".coords").textContent =
    `${Number(data.latitude).toFixed(3)}, ${Number(data.longitude).toFixed(3)} · z${data.zoom}`;
  shadow.querySelector(".time").textContent = formatFrameTime(data.frame_time);

  try {
    await composeMap(shadow.querySelector("canvas"), data);
  } catch {
    shadow.querySelector(".radar").innerHTML =
      `<div class="radar-error">Nie udało się wyświetlić mapy. Sprawdź dostęp do kafelków mapy i RainViewer.</div>`;
  }
}
