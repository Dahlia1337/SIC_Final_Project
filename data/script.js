const gateway = `ws://${window.location.host}/ws`;
let websocket;

let systemState = {
  mode: "MANUAL",
  fan_speed: 0,
  swing_enable: false,
  target_angle: 0,
  auto_cfg: { t1: 28, s1: 50, t2: 32, s2: 100 }
};

window.addEventListener('load', initWebSocket);

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
  const dot = document.getElementById('ws-dot');
  const status = document.getElementById('ws-status');
  if (dot && status) {
    dot.className = connected ? 'dot connected' : 'dot';
    status.innerText = connected ? 'Trực tuyến' : 'Mất kết nối';
  }
}

function onMessage(event) {
  try {
    const data = JSON.parse(event.data);
    
    // Cập nhật cảm biến
    if (data.type === 'sensor' || data.type === 'status') {
      const tempEl = document.getElementById('val-temp');
      const humiEl = document.getElementById('val-humi');
      if (tempEl && data.temp !== undefined) tempEl.innerText = parseFloat(data.temp).toFixed(1);
      if (humiEl && data.humi !== undefined) humiEl.innerText = parseFloat(data.humi).toFixed(1);
    }

    // Cập nhật trạng thái quạt
    if (data.type === 'status') {
      if (data.mode) syncModeUI(data.mode);
      if (data.fan_speed !== undefined) {
        document.getElementById('slider-fan').value = data.fan_speed;
        document.getElementById('disp-fan-speed').innerText = `${data.fan_speed}%`;
      }
      if (data.swing_state !== undefined) {
        document.getElementById('toggle-swing').checked = data.swing_state;
        toggleAngleSliderState(data.swing_state);
      }
      if (data.current_angle !== undefined) {
        document.getElementById('disp-angle').innerText = `${data.current_angle}°`;
      }
      if (data.auto_cfg) {
        document.getElementById('cfg-t1').value = data.auto_cfg.t1;
        document.getElementById('cfg-s1').value = data.auto_cfg.s1;
        document.getElementById('cfg-t2').value = data.auto_cfg.t2;
        document.getElementById('cfg-s2').value = data.auto_cfg.s2;
      }
    }
  } catch (err) {
    console.error('Lỗi nhận dữ liệu WebSocket:', err);
  }
}

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
    btnAuto.classList.add('active');
    btnManual.classList.remove('active');
    panelManual.style.display = 'none';
    panelAuto.style.display = 'block';
  } else {
    btnManual.classList.add('active');
    btnAuto.classList.remove('active');
    panelManual.style.display = 'block';
    panelAuto.style.display = 'none';
  }
}

function onFanSlider(val) {
  document.getElementById('disp-fan-speed').innerText = `${val}%`;
  systemState.fan_speed = parseInt(val);
}

function onSwingToggle() {
  const isSwing = document.getElementById('toggle-swing').checked;
  systemState.swing_enable = isSwing;
  toggleAngleSliderState(isSwing);
  sendControl();
}

function toggleAngleSliderState(isSwing) {
  const angleBox = document.getElementById('angle-ctrl-box');
  if (isSwing) angleBox.classList.add('disabled-layer');
  else angleBox.classList.remove('disabled-layer');
}

function stepAngle(step) {
  let newAngle = systemState.target_angle + step;
  if (newAngle < -90) newAngle = -90;
  if (newAngle > 90) newAngle = 90;
  systemState.target_angle = newAngle;
  document.getElementById('disp-angle').innerText = `${newAngle}°`;
  sendControl();
}

function setZeroPoint() {
  if (confirm("Đặt vị trí hiện tại làm mốc 0 độ?")) {
    if (websocket && websocket.readyState === WebSocket.OPEN) {
      websocket.send(JSON.stringify({ cmd: "calib_zero" }));
      systemState.target_angle = 0;
      document.getElementById('disp-angle').innerText = `0°`;
    }
  }
}

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
    alert("Nhiệt độ nấc 1 phải nhỏ hơn nấc 2!");
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