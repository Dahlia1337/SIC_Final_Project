/* ==========================================================================
   SMART FAN AI - REAL-TIME JAVASCRIPT CONTROLLER WITH CHART & POWER MONITOR
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

// Chart & History State
let realtimeChart = null;
const MAX_LIVE_POINTS = 30;
let currentTimeframe = 'live'; // 'live', '1h', '24h', '7d', '30d'
let liveBuffer = {
  labels: [],
  temps: [],
  humis: []
};

window.addEventListener('DOMContentLoaded', () => {
  loadLiveCache();
  initChart();
  initWebSocket();
  initTabs();
});

/* ==========================================================================
   0. TAB NAVIGATION SYSTEM
   ========================================================================== */
function switchTab(tabId) {
  const tabs = ['control', 'health', 'ai', 'chart'];
  if (!tabs.includes(tabId)) tabId = 'control';

  tabs.forEach(t => {
    const btn = document.getElementById(`tab-btn-${t}`);
    const content = document.getElementById(`tab-content-${t}`);
    if (btn) {
      if (t === tabId) btn.classList.add('active');
      else btn.classList.remove('active');
    }
    if (content) {
      if (t === tabId) content.classList.add('active');
      else content.classList.remove('active');
    }
  });

  try {
    localStorage.setItem('active_dashboard_tab', tabId);
  } catch (e) {}

  // Khi chuyển sang tab chart, resize để Chart.js hiển thị đúng kích thước
  if (tabId === 'chart' && realtimeChart) {
    setTimeout(() => {
      realtimeChart.resize();
      realtimeChart.update('none');
    }, 60);
  }
}

function initTabs() {
  let savedTab = 'control';
  try {
    const s = localStorage.getItem('active_dashboard_tab');
    if (s && ['control', 'health', 'ai', 'chart'].includes(s)) {
      savedTab = s;
    }
  } catch (e) {}
  switchTab(savedTab);
}

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

    // Cập nhật cảm biến & AI & Điện năng
    if (data.type === 'sensor' || data.type === 'status') {
      if (data.temp !== undefined && data.humi !== undefined) {
        const temp = parseFloat(data.temp);
        const humi = parseFloat(data.humi);

        updateSensorDisplay(temp, humi);
        updateStatistics(temp, humi);
        recordLivePoint(temp, humi);
      }

      if (data.comfort !== undefined || data.comfort_label !== undefined) {
        updateComfortUI(data.is_auto, data.comfort, data.comfort_label);
      }

      // Cập nhật phân tích tiện nghi nhiệt & sức khỏe
      if (data.health) {
        updateHealthUI(data.health);
      }

      // Cập nhật TinyML AI Intelligence Panel
      if (data.ai) {
        updateAIPanel(data.ai);
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
   2.1 THERMAL COMFORT & HEALTH ANALYTICS UI
   ========================================================================== */
function updateHealthUI(health) {
  if (!health) return;

  const heatIndex = parseFloat(health.hi) || 0.0;
  const dewPoint = parseFloat(health.dp) || 0.0;
  const score = parseFloat(health.score) || 100.0;
  const hiLvl = parseInt(health.hi_lvl) || 0;
  const moldLvl = parseInt(health.mold_lvl) || 0;

  const pctCold = isNaN(parseFloat(health.p_cld)) ? 0.0 : parseFloat(health.p_cld);
  const pctComfort = isNaN(parseFloat(health.p_cmf)) ? 100.0 : parseFloat(health.p_cmf);
  const pctWarm = isNaN(parseFloat(health.p_wrm)) ? 0.0 : parseFloat(health.p_wrm);
  const pctHot = isNaN(parseFloat(health.p_hot)) ? 0.0 : parseFloat(health.p_hot);

  const cntCold = health.c_cld || 0;
  const cntComfort = health.c_cmf || 0;
  const cntWarm = health.c_wrm || 0;
  const cntHot = health.c_hot || 0;
  const totalSamples = health.total || 0;

  // 1. Cập nhật Heat Index & Thẻ cảnh báo rủi ro NOAA
  setText('val-heat-index', heatIndex.toFixed(1));
  const elPillHi = document.getElementById('pill-hi-risk');
  const elNoteHi = document.getElementById('val-hi-note');
  if (elPillHi) {
    elPillHi.className = 'risk-pill';
    if (hiLvl === 0 || heatIndex < 27.0) {
      elPillHi.classList.add('safe');
      elPillHi.innerText = 'An Toàn';
      if (elNoteHi) elNoteHi.innerText = 'Môi trường an toàn, lý tưởng cho cơ thể (< 27°C)';
    } else if (hiLvl === 1 || heatIndex < 32.0) {
      elPillHi.classList.add('caution');
      elPillHi.innerText = 'Thận Trọng';
      if (elNoteHi) elNoteHi.innerText = 'Có thể mệt mỏi nếu tiếp xúc lâu hoặc vận động (27–32°C)';
    } else if (hiLvl === 2 || heatIndex < 41.0) {
      elPillHi.classList.add('extreme-caution');
      elPillHi.innerText = 'Đặc Biệt Thận Trọng';
      if (elNoteHi) elNoteHi.innerText = 'Nguy cơ say nắng, chuột rút & kiệt sức vì nhiệt (32–41°C)';
    } else if (hiLvl === 3 || heatIndex < 54.0) {
      elPillHi.classList.add('danger');
      elPillHi.innerText = 'Nguy Hiểm';
      if (elNoteHi) elNoteHi.innerText = 'Dễ bị kiệt sức, chuột rút; có thể sốc nhiệt nếu kéo dài (41–54°C)';
    } else {
      elPillHi.classList.add('extreme');
      elPillHi.innerText = 'Cực Kỳ Nguy Hiểm!';
      if (elNoteHi) elNoteHi.innerText = 'Nguy cơ sốc nhiệt & đột quỵ nhiệt rất cao (≥ 54°C)';
    }
  }

  // 2. Cập nhật Điểm Chất Lượng Vi Khí Hậu (Comfort Score)
  setText('val-score-num', score.toFixed(0));
  setText('val-score-pct', `${score.toFixed(0)}%`);
  const elPillScore = document.getElementById('pill-score-grade');
  const elNoteScore = document.getElementById('val-score-note');
  if (elPillScore) {
    if (score >= 80) {
      elPillScore.innerText = 'Xuất Sắc';
      if (elNoteScore) elNoteScore.innerText = 'Đạt chuẩn tiện nghi ASHRAE 55 cao';
    } else if (score >= 65) {
      elPillScore.innerText = 'Tốt';
      if (elNoteScore) elNoteScore.innerText = 'Vi khí hậu ổn định trong phòng';
    } else if (score >= 50) {
      elPillScore.innerText = 'Trung Bình';
      if (elNoteScore) elNoteScore.innerText = 'Cần luân chuyển thông gió thêm';
    } else {
      elPillScore.innerText = 'Kém';
      if (elNoteScore) elNoteScore.innerText = 'Cần tăng cường làm mát ngay';
    }
  }

  // 3. Cập nhật Nhiệt Độ Điểm Sương & Nguy cơ Nồm ẩm
  setText('val-dew-point', dewPoint.toFixed(1));
  const elPillMold = document.getElementById('pill-mold-risk');
  const elNoteMold = document.getElementById('val-mold-note');
  if (elPillMold) {
    elPillMold.className = 'risk-pill';
    if (moldLvl === 0) {
      elPillMold.classList.add('safe');
      elPillMold.innerText = 'Khô Thoáng';
      if (elNoteMold) elNoteMold.innerText = 'Ít nguy cơ ngưng tụ nấm mốc';
    } else if (moldLvl === 1) {
      elPillMold.classList.add('caution');
      elPillMold.innerText = 'Cảnh Giác';
      if (elNoteMold) elNoteMold.innerText = 'Độ ẩm cao, nên bật quạt thông gió';
    } else {
      elPillMold.classList.add('danger');
      elPillMold.innerText = 'Nồm Ẩm Cao!';
      if (elNoteMold) elNoteMold.innerText = 'Dễ đọng sương sàn nhà và sinh mốc';
    }
  }

  // 4. Cập nhật thanh Segmented Progress Bar (4 phân lớp TinyML)
  const barCold = document.getElementById('bar-cold');
  const barComfort = document.getElementById('bar-comfort');
  const barWarm = document.getElementById('bar-warm');
  const barHot = document.getElementById('bar-hot');

  if (barCold) barCold.style.width = `${pctCold.toFixed(1)}%`;
  if (barComfort) barComfort.style.width = `${pctComfort.toFixed(1)}%`;
  if (barWarm) barWarm.style.width = `${pctWarm.toFixed(1)}%`;
  if (barHot) barHot.style.width = `${pctHot.toFixed(1)}%`;

  // 5. Cập nhật nhãn đếm và tỷ lệ
  setText('health-sample-count', `${totalSamples.toLocaleString('vi-VN')} chu kỳ đo`);
  setText('cnt-cold-val', `${cntCold} chu kỳ`);
  setText('pct-cold-val', `(${pctCold.toFixed(0)}%)`);

  setText('cnt-comfort-val', `${cntComfort} chu kỳ`);
  setText('pct-comfort-val', `(${pctComfort.toFixed(0)}%)`);

  setText('cnt-warm-val', `${cntWarm} chu kỳ`);
  setText('pct-warm-val', `(${pctWarm.toFixed(0)}%)`);

  setText('cnt-hot-val', `${cntHot} chu kỳ`);
  setText('pct-hot-val', `(${pctHot.toFixed(0)}%)`);
}

/* ==========================================================================
   3. TINYML AI INTELLIGENCE PANEL
   ========================================================================== */
function updateAIPanel(ai) {
  if (!ai) return;

  // 1. Confidence scores (4 lớp phân loại)
  const probs = [
    parseFloat(ai.p0) || 0.0,  // COLD
    parseFloat(ai.p1) || 0.0,  // COMFORT
    parseFloat(ai.p2) || 0.0,  // WARM
    parseFloat(ai.p3) || 0.0   // HOT
  ];
  for (let i = 0; i < 4; i++) {
    const bar = document.getElementById(`conf-bar-${i}`);
    const pct = document.getElementById(`conf-pct-${i}`);
    if (bar) bar.style.width = `${probs[i].toFixed(1)}%`;
    if (pct) pct.innerText = `${probs[i].toFixed(1)}%`;
  }

  // 2. Overall Confidence (confidence cao nhất)
  const conf = parseFloat(ai.conf) || 0.0;
  const confBar = document.getElementById('conf-overall-bar');
  const confPct = document.getElementById('conf-overall-pct');
  if (confBar) {
    confBar.style.width = `${conf.toFixed(1)}%`;
    // Màu sắc theo độ tự tin
    if (conf >= 80) confBar.style.background = 'linear-gradient(90deg, #10b981, #34d399)';
    else if (conf >= 60) confBar.style.background = 'linear-gradient(90deg, #f59e0b, #fbbf24)';
    else confBar.style.background = 'linear-gradient(90deg, #ef4444, #f87171)';
  }
  if (confPct) confPct.innerText = `${conf.toFixed(1)}%`;

  // 3. Inference count
  const inferCount = ai.infer || 0;
  setText('ai-infer-count', inferCount.toLocaleString('vi-VN'));

  // 4. Trend Analysis (dT/dt, dH/dt)
  const dt = parseFloat(ai.dt) || 0.0;
  const dh = parseFloat(ai.dh) || 0.0;
  setText('trend-temp-val', Math.abs(dt).toFixed(2));
  setText('trend-humi-val', Math.abs(dh).toFixed(2));

  // Mũi tên & note cho nhiệt độ
  const arrowTemp = document.getElementById('trend-temp-arrow');
  const noteTemp = document.getElementById('trend-temp-note');
  if (arrowTemp && noteTemp) {
    if (dt > 0.1) {
      arrowTemp.innerText = '↑'; arrowTemp.className = 'trend-arrow up';
      noteTemp.innerText = 'Đang tăng nhanh';
    } else if (dt < -0.1) {
      arrowTemp.innerText = '↓'; arrowTemp.className = 'trend-arrow down';
      noteTemp.innerText = 'Đang giảm nhanh';
    } else {
      arrowTemp.innerText = '→'; arrowTemp.className = 'trend-arrow stable';
      noteTemp.innerText = 'Ổn định';
    }
  }

  // Mũi tên & note cho độ ẩm
  const arrowHumi = document.getElementById('trend-humi-arrow');
  const noteHumi = document.getElementById('trend-humi-note');
  if (arrowHumi && noteHumi) {
    if (dh > 0.5) {
      arrowHumi.innerText = '↑'; arrowHumi.className = 'trend-arrow up';
      noteHumi.innerText = 'Độ ẩm đang tăng';
    } else if (dh < -0.5) {
      arrowHumi.innerText = '↓'; arrowHumi.className = 'trend-arrow down';
      noteHumi.innerText = 'Độ ẩm đang giảm';
    } else {
      arrowHumi.innerText = '→'; arrowHumi.className = 'trend-arrow stable';
      noteHumi.innerText = 'Ổn định';
    }
  }

  // 5. Heap RAM (esp_get_free_heap_size)
  const heap = parseInt(ai.heap) || 0;
  const heapKB = (heap / 1024).toFixed(1);
  setText('sys-heap-val', `${heapKB} KB`);

  // 6. Phân bổ tích lũy
  const total = parseInt(ai.total) || 0;
  const cCold = parseInt(ai.c_cld) || 0;
  const cComf = parseInt(ai.c_cmf) || 0;
  const cWarm = parseInt(ai.c_wrm) || 0;
  const cHot  = parseInt(ai.c_hot) || 0;

  setText('ai-dist-total', `${total.toLocaleString('vi-VN')} suy luận`);

  if (total > 0) {
    const pCold = (cCold / total * 100);
    const pComf = (cComf / total * 100);
    const pWarm = (cWarm / total * 100);
    const pHot  = (cHot  / total * 100);

    const barCold = document.getElementById('ai-bar-cold');
    const barComf = document.getElementById('ai-bar-comfort');
    const barWarm = document.getElementById('ai-bar-warm');
    const barHot  = document.getElementById('ai-bar-hot');
    if (barCold) barCold.style.width = `${pCold.toFixed(1)}%`;
    if (barComf) barComf.style.width = `${pComf.toFixed(1)}%`;
    if (barWarm) barWarm.style.width = `${pWarm.toFixed(1)}%`;
    if (barHot)  barHot.style.width  = `${pHot.toFixed(1)}%`;

    setText('ai-cnt-cold',    cCold);
    setText('ai-pct-cold',    `${pCold.toFixed(0)}%`);
    setText('ai-cnt-comfort', cComf);
    setText('ai-pct-comfort', `${pComf.toFixed(0)}%`);
    setText('ai-cnt-warm',    cWarm);
    setText('ai-pct-warm',    `${pWarm.toFixed(0)}%`);
    setText('ai-cnt-hot',     cHot);
    setText('ai-pct-hot',     `${pHot.toFixed(0)}%`);
  }
}

function setText(id, text) {
  const el = document.getElementById(id);
  if (el) el.innerText = text;
}

/* ==========================================================================
   4. SENSOR & STATS UI UPDATES
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
   5. FAN SPEED & ANIMATION CONTROLLER
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
   6. MODE SWITCHING
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
   7. SWING & STEPPER CONTROLLER
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
   8. WEBSOCKET COMMAND SENDERS
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
   9. REAL-TIME & HISTORY CHART (CHART.JS)
   ========================================================================== */
function initChart() {
  const ctx = document.getElementById('realtimeChart');
  if (!ctx || typeof Chart === 'undefined') return;

  const tempGradient = ctx.getContext('2d').createLinearGradient(0, 0, 0, 260);
  tempGradient.addColorStop(0, 'rgba(249, 115, 22, 0.35)');
  tempGradient.addColorStop(1, 'rgba(249, 115, 22, 0.0)');

  const humiGradient = ctx.getContext('2d').createLinearGradient(0, 0, 0, 260);
  humiGradient.addColorStop(0, 'rgba(6, 182, 212, 0.3)');
  humiGradient.addColorStop(1, 'rgba(6, 182, 212, 0.0)');

  realtimeChart = new Chart(ctx, {
    type: 'line',
    data: {
      labels: liveBuffer.labels.length ? [...liveBuffer.labels] : [],
      datasets: [
        {
          label: 'Nhiệt độ (°C)',
          data: liveBuffer.temps.length ? [...liveBuffer.temps] : [],
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
          data: liveBuffer.humis.length ? [...liveBuffer.humis] : [],
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
          backgroundColor: 'rgba(15, 23, 42, 0.95)',
          titleColor: '#fff',
          bodyColor: '#cbd5e1',
          borderColor: 'rgba(255, 255, 255, 0.12)',
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
          min: 0,
          max: 50,
          grid: { color: 'rgba(255, 255, 255, 0.06)' },
          ticks: {
            color: '#f97316',
            font: { size: 11, weight: '600' },
            stepSize: 10
          },
          title: { display: true, text: 'Nhiệt độ (°C)', color: '#f97316', font: { size: 11, weight: '600' } }
        },
        yHumi: {
          type: 'linear',
          position: 'right',
          min: 0,
          max: 100,
          grid: { drawOnChartArea: false },
          ticks: {
            color: '#06b6d4',
            font: { size: 11, weight: '600' },
            stepSize: 20
          },
          title: { display: true, text: 'Độ ẩm (%)', color: '#06b6d4', font: { size: 11, weight: '600' } }
        }
      }
    }
  });
}

function recordLivePoint(temp, humi) {
  const now = new Date();
  const timeLabel = now.toTimeString().split(' ')[0];

  liveBuffer.labels.push(timeLabel);
  liveBuffer.temps.push(temp);
  liveBuffer.humis.push(humi);

  if (liveBuffer.labels.length > MAX_LIVE_POINTS) {
    liveBuffer.labels.shift();
    liveBuffer.temps.shift();
    liveBuffer.humis.shift();
  }

  saveLiveCache();

  // Chỉ cập nhật đồ thị nếu đang xem tab 'live'
  if (currentTimeframe === 'live' && realtimeChart) {
    realtimeChart.data.labels = [...liveBuffer.labels];
    realtimeChart.data.datasets[0].data = [...liveBuffer.temps];
    realtimeChart.data.datasets[1].data = [...liveBuffer.humis];
    realtimeChart.update('none');
  }
}

/* ==========================================================================
   10. TIMEFRAME SELECTOR (1h, 24h, 7d, 30d)
   ========================================================================== */
function setTimeframe(tf) {
  currentTimeframe = tf;

  // Cập nhật active button
  const buttons = ['live', '1h', '24h', '7d', '30d'];
  buttons.forEach(b => {
    const btn = document.getElementById(`tf-${b}`);
    if (btn) {
      if (b === tf) btn.classList.add('active');
      else btn.classList.remove('active');
    }
  });

  const noteEl = document.getElementById('chart-data-note');

  if (tf === 'live') {
    if (noteEl) noteEl.innerText = 'Đang xem dữ liệu trực tiếp từ cảm biến';
    if (realtimeChart) {
      realtimeChart.data.labels = [...liveBuffer.labels];
      realtimeChart.data.datasets[0].data = [...liveBuffer.temps];
      realtimeChart.data.datasets[1].data = [...liveBuffer.humis];
      realtimeChart.update();
    }
  } else {
    const rangeNames = { '1h': '1 giờ gần nhất', '24h': '24 giờ gần nhất', '7d': '7 ngày gần nhất', '30d': '30 ngày gần nhất' };
    if (noteEl) noteEl.innerText = `Đang tải dữ liệu ${rangeNames[tf]}...`;
    fetchHistoryData(tf);
  }
}

function fetchHistoryData(range) {
  fetch(`/api/history?range=${range}`)
    .then(res => res.json())
    .then(data => {
      const noteEl = document.getElementById('chart-data-note');
      if (!data || !data.data || data.data.length === 0) {
        if (noteEl) noteEl.innerText = `Chưa có đủ lịch sử cho khung ${range} (máy đang tích lũy dữ liệu)`;
        return;
      }

      const labels = [];
      const temps = [];
      const humis = [];

      data.data.forEach(item => {
        let label = '';
        if (item.ts > 1600000000) {
          const d = new Date(item.ts * 1000);
          if (range === '1h' || range === '24h') {
            label = d.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
          } else {
            label = `${d.getDate()}/${d.getMonth()+1} ${d.getHours()}h`;
          }
        } else {
          label = `T-${item.ts}s`;
        }

        labels.push(label);
        temps.push(parseFloat(item.t));
        humis.push(parseFloat(item.h));
      });

      if (noteEl) noteEl.innerText = `Hiển thị ${labels.length} điểm đo lưu trong LittleFS (${range})`;

      if (realtimeChart && currentTimeframe === range) {
        realtimeChart.data.labels = labels;
        realtimeChart.data.datasets[0].data = temps;
        realtimeChart.data.datasets[1].data = humis;
        realtimeChart.update();
      }
    })
    .catch(err => {
      console.error('Lỗi nạp dữ liệu lịch sử:', err);
      const noteEl = document.getElementById('chart-data-note');
      if (noteEl) noteEl.innerText = 'Không thể nạp dữ liệu lịch sử từ ESP32';
    });
}

/* ==========================================================================
   11. CLIENT-SIDE CACHE (LOCALSTORAGE)
   ========================================================================== */
function saveLiveCache() {
  try {
    localStorage.setItem('sf_live_labels', JSON.stringify(liveBuffer.labels));
    localStorage.setItem('sf_live_temps', JSON.stringify(liveBuffer.temps));
    localStorage.setItem('sf_live_humis', JSON.stringify(liveBuffer.humis));
  } catch (e) {}
}

function loadLiveCache() {
  try {
    const l = localStorage.getItem('sf_live_labels');
    const t = localStorage.getItem('sf_live_temps');
    const h = localStorage.getItem('sf_live_humis');
    if (l && t && h) {
      liveBuffer.labels = JSON.parse(l);
      liveBuffer.temps = JSON.parse(t);
      liveBuffer.humis = JSON.parse(h);
    }
  } catch (e) {}
}