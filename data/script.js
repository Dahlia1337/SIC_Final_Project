/* ==========================================================================
   SMART FAN AI - REAL-TIME JAVASCRIPT CONTROLLER WITH CHART.JS
   ========================================================================== */

const gateway = `ws://${window.location.host}/ws`;
let websocket;

let systemState = {
  mode: "MANUAL",
  fan_speed: 0,
  swing_enable: false,
  target_angle: 0,
  auto_cfg: { t1: 28, s1: 50, t2: 32, s2: 100 }
};

// Sensor statistics tracking
let stats = {
  tempMin: Infinity,
  tempMax: -Infinity,
  humiMin: Infinity,
  humiMax: -Infinity
};

// Chart.js instance
let realtimeChart = null;
const MAX_CHART_POINTS = 30;

window.addEventListener('DOMContentLoaded', () => {
  initChart();
  initWebSocket();
});

/* ==========================================================================
   1. WEBSOCKET MANAGEMENT
   ========================================================================== */
function initWebSocket() {
  websocket = new WebSocket(gateway);
  websocket.onopen = () => updateWsStatus(true);
  websocket.onclose = () => {
    updateWsStatus(false);
    setTimeout(initWebSocket, 2000);
  };
  websocket.onmessage = onMessage;
}

function updateWsStatus(connected) {
  const badge = document.getElementById('ws-badge');
  const status = document.getElementById('ws-status');
  if (badge && status) {
    if (connected) {
      badge.classList.add('connected');
      status.innerText = 'Trực tuyến';
    } else {
      badge.classList.remove('connected');
      status.innerText = 'Mất kết nối';
    }
  }
}

/* ==========================================================================
   2. DATA MESSAGE DISPATCHER
   ========================================================================== */
function onMessage(event) {
  try {
    const data = JSON.parse(event.data);

    // Cập nhật cảm biến & AI
    if (data.type === 'sensor' || data.type === 'status') {
      if (data.temp !== undefined && data.humi !== undefined) {
        const temp = parseFloat(data.temp);
        const humi = parseFloat(data.humi);

        updateSensorDisplay(temp, humi);
        updateStatistics(temp, humi);
        pushChartData(temp, humi);
      }

      if (data.comfort !== undefined || data.comfort_label !== undefined) {
        updateComfortUI(data.is_auto, data.comfort, data.comfort_label);
      }
    }

    // Cập nhật trạng thái quạt & thông số
    if (data.type === 'status') {
      if (data.mode) syncModeUI(data.mode);
      if (data.fan_speed !== undefined) updateFanSpeedUI(data.fan_speed);
      if (data.swing_state !== undefined) updateSwingUI(data.swing_state);
      if (data.current_angle !== undefined) {
        const dispAngle = document.getElementById('disp-angle');
        if (dispAngle) dispAngle.innerText = `${data.current_angle}°`;
      }
      if (data.auto_cfg) {
        const cfgT1 = document.getElementById('cfg-t1');
        const cfgS1 = document.getElementById('cfg-s1');
        const cfgT2 = document.getElementById('cfg-t2');
        const cfgS2 = document.getElementById('cfg-s2');
        if (cfgT1) cfgT1.value = data.auto_cfg.t1;
        if (cfgS1) cfgS1.value = data.auto_cfg.s1;
        if (cfgT2) cfgT2.value = data.auto_cfg.t2;
        if (cfgS2) cfgS2.value = data.auto_cfg.s2;
      }
    }
  } catch (err) {
    console.error('Lỗi phân tích WebSocket payload:', err);
  }
}

/* ==========================================================================
   3. SENSOR & STATS UI UPDATES
   ========================================================================== */
function updateSensorDisplay(temp, humi) {
  const tempEl = document.getElementById('val-temp');
  const humiEl = document.getElementById('val-humi');
  if (tempEl) tempEl.innerText = temp.toFixed(1);
  if (humiEl) humiEl.innerText = humi.toFixed(1);
}

function updateStatistics(temp, humi) {
  if (temp < stats.tempMin) stats.tempMin = temp;
  if (temp > stats.tempMax) stats.tempMax = temp;
  if (humi < stats.humiMin) stats.humiMin = humi;
  if (humi > stats.humiMax) stats.humiMax = humi;

  const minT = document.getElementById('stat-temp-min');
  const maxT = document.getElementById('stat-temp-max');
  const minH = document.getElementById('stat-humi-min');
  const maxH = document.getElementById('stat-humi-max');

  if (minT) minT.innerText = stats.tempMin.toFixed(1);
  if (maxT) maxT.innerText = stats.tempMax.toFixed(1);
  if (minH) minH.innerText = stats.humiMin.toFixed(0);
  if (maxH) maxH.innerText = stats.humiMax.toFixed(0);
}

function updateComfortUI(isAuto, comfortClass, label) {
  const textEl = document.getElementById('comfort-text');
  const badgeEl = document.getElementById('comfort-badge');
  const descEl = document.getElementById('comfort-desc');
  if (!textEl || !badgeEl) return;

  if (isAuto) {
    textEl.innerText = label || "Đang phân tích...";
    badgeEl.className = 'badge';

    switch (comfortClass) {
      case 0:
        badgeEl.classList.add('badge-cold');
        badgeEl.innerText = 'COLD • Quạt tắt (0%)';
        if (descEl) descEl.innerText = 'Thời tiết mát/lạnh, hệ thống tự động tắt quạt để tiết kiệm năng lượng.';
        break;
      case 1:
        badgeEl.classList.add('badge-comfort');
        badgeEl.innerText = 'COMFORT • Quạt êm (35%)';
        if (descEl) descEl.innerText = 'Môi trường lý tưởng, duy trì luồng gió thoang thoảng dịu nhẹ.';
        break;
      case 2:
        badgeEl.classList.add('badge-warm');
        badgeEl.innerText = 'WARM • Gió mạnh (70%)';
        if (descEl) descEl.innerText = 'Độ ẩm cao hoặc phòng nóng hầm bí, tăng cường thông gió đối lưu không khí.';
        break;
      case 3:
        badgeEl.classList.add('badge-hot');
        badgeEl.innerText = 'HOT • Tối đa (100%)';
        if (descEl) descEl.innerText = 'Nhiệt độ hoặc chỉ số oi bức đạt ngưỡng cao, kích hoạt công suất làm mát tối đa.';
        break;
      default:
        badgeEl.innerText = 'Đang nhận diện';
    }
  } else {
    textEl.innerText = 'Chế độ Thủ Công';
    badgeEl.className = 'badge';
    badgeEl.innerText = 'MANUAL CONTROL';
    if (descEl) descEl.innerText = 'Quạt đang được điều khiển bằng tay, các thiết lập tự động tạm thời bỏ qua.';
  }
}

/* ==========================================================================
   4. FAN SPEED & ANIMATION CONTROLLER
   ========================================================================== */
function updateFanSpeedUI(speed) {
  systemState.fan_speed = speed;
  const slider = document.getElementById('slider-fan');
  const disp = document.getElementById('disp-fan-speed');
  if (slider) slider.value = speed;
  if (disp) disp.innerText = `${speed}%`;

  // Cập nhật tốc độ quay của Icon quạt qua CSS Animation
  const root = document.documentElement;
  if (speed <= 0) {
    root.style.setProperty('--fan-spin-duration', '0s');
  } else {
    // Tốc độ quay từ 2.0s (35%) xuống 0.3s (100%)
    const duration = (2.2 - (speed / 100) * 1.8).toFixed(2);
    root.style.setProperty('--fan-spin-duration', `${duration}s`);
  }
}

function onFanSlider(val) {
  updateFanSpeedUI(parseInt(val));
}

function setFanPreset(val) {
  updateFanSpeedUI(val);
  sendControl();
}

/* ==========================================================================
   5. MODE SWITCHING
   ========================================================================== */
function setMode(mode) {
  systemState.mode = mode;
  syncModeUI(mode);
  sendControl();
}

function syncModeUI(mode) {
  const btnManual = document.getElementById('btn-manual');
  const btnAuto = document.getElementById('btn-auto');
  const panelManual = document.getElementById('panel-manual');
  const panelAuto = document.getElementById('panel-auto');

  if (mode === 'AUTO') {
    if (btnAuto) btnAuto.classList.add('active');
    if (btnManual) btnManual.classList.remove('active');
    if (panelManual) panelManual.style.display = 'none';
    if (panelAuto) panelAuto.style.display = 'block';
  } else {
    if (btnManual) btnManual.classList.add('active');
    if (btnAuto) btnAuto.classList.remove('active');
    if (panelManual) panelManual.style.display = 'block';
    if (panelAuto) panelAuto.style.display = 'none';
  }
}

/* ==========================================================================
   6. SWING & STEPPER CONTROLLER
   ========================================================================== */
function updateSwingUI(isSwing) {
  systemState.swing_enable = isSwing;
  const toggle = document.getElementById('toggle-swing');
  const angleBox = document.getElementById('angle-ctrl-box');
  if (toggle) toggle.checked = isSwing;
  if (angleBox) {
    if (isSwing) angleBox.classList.add('disabled');
    else angleBox.classList.remove('disabled');
  }
}

function onSwingToggle() {
  const isSwing = document.getElementById('toggle-swing').checked;
  updateSwingUI(isSwing);
  sendControl();
}

function stepAngle(step) {
  let newAngle = systemState.target_angle + step;
  if (newAngle < -90) newAngle = -90;
  if (newAngle > 90) newAngle = 90;
  setAngleExact(newAngle);
}

function setAngleExact(angle) {
  systemState.target_angle = angle;
  const disp = document.getElementById('disp-angle');
  if (disp) disp.innerText = `${angle}°`;
  sendControl();
}

function setZeroPoint() {
  if (confirm("Xác nhận đặt vị trí hiện tại làm mốc 0 độ (Chính diện)?")) {
    if (websocket && websocket.readyState === WebSocket.OPEN) {
      websocket.send(JSON.stringify({ cmd: "calib_zero" }));
      systemState.target_angle = 0;
      const disp = document.getElementById('disp-angle');
      if (disp) disp.innerText = "0°";
    }
  }
}

/* ==========================================================================
   7. WEBSOCKET COMMAND SENDERS
   ========================================================================== */
function sendControl() {
  if (websocket && websocket.readyState === WebSocket.OPEN) {
    const payload = {
      cmd: "control",
      mode: systemState.mode,
      fan_speed: systemState.fan_speed,
      swing_enable: systemState.swing_enable,
      target_angle: systemState.target_angle
    };
    websocket.send(JSON.stringify(payload));
  }
}

function sendAutoConfig() {
  const t1 = parseFloat(document.getElementById('cfg-t1').value);
  const s1 = parseInt(document.getElementById('cfg-s1').value);
  const t2 = parseFloat(document.getElementById('cfg-t2').value);
  const s2 = parseInt(document.getElementById('cfg-s2').value);

  if (t1 >= t2) {
    alert("Nhiệt độ Nấc 1 phải nhỏ hơn Nấc 2!");
    return;
  }

  if (websocket && websocket.readyState === WebSocket.OPEN) {
    const payload = {
      cmd: "set_auto_cfg",
      t1: t1,
      s1: s1,
      t2: t2,
      s2: s2
    };
    websocket.send(JSON.stringify(payload));
  }
}

/* ==========================================================================
   8. REAL-TIME CHART (CHART.JS)
   ========================================================================== */
function initChart() {
  const ctx = document.getElementById('realtimeChart');
  if (!ctx || typeof Chart === 'undefined') return;

  const tempGradient = ctx.getContext('2d').createLinearGradient(0, 0, 0, 250);
  tempGradient.addColorStop(0, 'rgba(249, 115, 22, 0.3)');
  tempGradient.addColorStop(1, 'rgba(249, 115, 22, 0.0)');

  const humiGradient = ctx.getContext('2d').createLinearGradient(0, 0, 0, 250);
  humiGradient.addColorStop(0, 'rgba(6, 182, 212, 0.25)');
  humiGradient.addColorStop(1, 'rgba(6, 182, 212, 0.0)');

  realtimeChart = new Chart(ctx, {
    type: 'line',
    data: {
      labels: [],
      datasets: [
        {
          label: 'Nhiệt độ (°C)',
          data: [],
          borderColor: '#f97316',
          backgroundColor: tempGradient,
          borderWidth: 2.5,
          tension: 0.35,
          fill: true,
          pointRadius: 2,
          pointHoverRadius: 6,
          yAxisID: 'yTemp'
        },
        {
          label: 'Độ ẩm (%)',
          data: [],
          borderColor: '#06b6d4',
          backgroundColor: humiGradient,
          borderWidth: 2,
          tension: 0.35,
          fill: true,
          pointRadius: 2,
          pointHoverRadius: 6,
          yAxisID: 'yHumi'
        }
      ]
    },
    options: {
      responsive: true,
      maintainAspectRatio: false,
      interaction: {
        mode: 'index',
        intersect: false
      },
      plugins: {
        legend: { display: false },
        tooltip: {
          backgroundColor: 'rgba(15, 23, 42, 0.9)',
          titleColor: '#fff',
          bodyColor: '#cbd5e1',
          borderColor: 'rgba(255, 255, 255, 0.1)',
          borderWidth: 1,
          padding: 10,
          boxPadding: 4,
          usePointStyle: true
        }
      },
      scales: {
        x: {
          grid: { color: 'rgba(255, 255, 255, 0.04)' },
          ticks: { color: '#64748b', font: { size: 10 }, maxTicksLimit: 8 }
        },
        yTemp: {
          type: 'linear',
          position: 'left',
          grid: { color: 'rgba(255, 255, 255, 0.06)' },
          ticks: { color: '#f97316', font: { size: 11, weight: '600' } },
          title: { display: true, text: '°C', color: '#f97316', font: { size: 11 } }
        },
        yHumi: {
          type: 'linear',
          position: 'right',
          grid: { drawOnChartArea: false },
          ticks: { color: '#06b6d4', font: { size: 11, weight: '600' } },
          title: { display: true, text: '%', color: '#06b6d4', font: { size: 11 } }
        }
      }
    }
  });
}

function pushChartData(temp, humi) {
  if (!realtimeChart) return;

  const now = new Date();
  const timeLabel = now.toTimeString().split(' ')[0];

  realtimeChart.data.labels.push(timeLabel);
  realtimeChart.data.datasets[0].data.push(temp);
  realtimeChart.data.datasets[1].data.push(humi);

  if (realtimeChart.data.labels.length > MAX_CHART_POINTS) {
    realtimeChart.data.labels.shift();
    realtimeChart.data.datasets[0].data.shift();
    realtimeChart.data.datasets[1].data.shift();
  }

  realtimeChart.update('none'); // Update without full layout animation for peak smoothness
}