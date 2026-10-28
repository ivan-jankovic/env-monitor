// Draws the charts and keeps the page live without full reloads: every
// reading interval it fetches this same page and swaps in the fresh sections.
(function () {
  "use strict";

  const charts = {};
  const css = getComputedStyle(document.documentElement);
  const color = (name) => css.getPropertyValue(name).trim();
  const lineColors = ["--accent", "--ok", "--limit"];

  const fullTime = new Intl.DateTimeFormat(undefined, { dateStyle: "medium", timeStyle: "medium" });
  const axisTime = new Intl.DateTimeFormat(undefined, { hour: "2-digit", minute: "2-digit" });

  // Times are rendered in UTC by the server; show them in the viewer's local time.
  function localizeTimes(root) {
    root.querySelectorAll("time[datetime]").forEach((el) => {
      el.textContent = fullTime.format(new Date(el.dateTime));
    });
  }

  // Break the line where readings are missing (e.g. an outage) instead of
  // drawing a straight line across the gap.
  function withGaps(points, maxGapMs) {
    const out = [];
    points.forEach(([x, y], i) => {
      if (i > 0 && x - points[i - 1][0] > maxGapMs) out.push({ x: x - 1, y: null });
      out.push({ x, y });
    });
    return out;
  }

  function limitLine(value, data, label) {
    return {
      label,
      data: [{ x: data.since, y: value }, { x: data.until, y: value }],
      borderColor: color("--limit"),
      borderDash: [6, 4],
      borderWidth: 1,
      pointRadius: 0,
    };
  }

  function datasets(sensor, data) {
    const maxGapMs = 3 * data.bucket_ms;
    const sets = Object.entries(data.series[sensor] || {}).map(([station, points], i) => ({
      label: station,
      data: withGaps(points, maxGapMs),
      borderColor: color(lineColors[i % lineColors.length]),
      borderWidth: 2,
      pointRadius: 0,
      spanGaps: false,
    }));
    const limits = data.sensors[sensor].limits;
    if (limits && limits.max_val !== null) sets.push(limitLine(limits.max_val, data, "max"));
    if (limits && limits.min_val !== null) sets.push(limitLine(limits.min_val, data, "min"));
    return sets;
  }

  function drawCharts(data) {
    for (const sensor of Object.keys(data.sensors)) {
      const canvas = document.getElementById("chart-" + sensor);
      if (!canvas) continue;
      const unit = data.sensors[sensor].unit;
      const chart = charts[sensor];
      if (chart) {
        chart.data.datasets = datasets(sensor, data);
        chart.options.scales.x.min = data.since;
        chart.options.scales.x.max = data.until;
        chart.update("none");
        continue;
      }
      charts[sensor] = new Chart(canvas, {
        type: "line",
        data: { datasets: datasets(sensor, data) },
        options: {
          animation: false,
          maintainAspectRatio: false,
          parsing: false,
          interaction: { mode: "nearest", axis: "x", intersect: false },
          plugins: {
            legend: { display: false },
            tooltip: {
              callbacks: {
                title: (items) => fullTime.format(new Date(items[0].parsed.x)),
                label: (item) => `${item.dataset.label}: ${item.parsed.y} ${unit}`,
              },
            },
          },
          scales: {
            x: {
              type: "linear",
              min: data.since,
              max: data.until,
              ticks: { color: color("--muted"), maxTicksLimit: 7, callback: (v) => axisTime.format(new Date(v)) },
              grid: { color: color("--border") },
            },
            y: {
              ticks: { color: color("--muted") },
              grid: { color: color("--border") },
            },
          },
        },
      });
    }
  }

  function readData(doc) {
    return JSON.parse(doc.getElementById("chart-data").textContent);
  }

  async function refresh() {
    const banner = document.getElementById("connection-lost");
    try {
      const response = await fetch(location.href, { cache: "no-store" });
      if (!response.ok) throw new Error("HTTP " + response.status);
      const doc = new DOMParser().parseFromString(await response.text(), "text/html");
      const live = doc.getElementById("live");
      document.getElementById("live").replaceWith(live);
      localizeTimes(live);
      drawCharts(readData(doc));
      banner.hidden = true;
    } catch (err) {
      banner.hidden = false;
    }
  }

  localizeTimes(document);
  drawCharts(readData(document));
  const seconds = Number(document.body.dataset.refreshSeconds) || 10;
  setInterval(refresh, seconds * 1000);
})();
