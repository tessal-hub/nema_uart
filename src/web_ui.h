#ifndef WEB_UI_H
#define WEB_UI_H

#include <Arduino.h>

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="vi">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <title>ESP32 6-Axis Motion Dashboard</title>
    <style>
        :root {
            --bg: #0b0f19;
            --card-bg: #151b28;
            --card-sub: #0e1420;
            --border: #232d3f;
            --primary: #2ea043;
            --primary-hover: #3fb950;
            --accent: #58a6ff;
            --accent-hover: #79b8ff;
            --danger: #f85149;
            --warning: #d29922;
            --text: #f0f6fc;
            --text-dim: #8b949e;
            --font: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
        }

        * { box-sizing: border-box; margin: 0; padding: 0; font-family: var(--font); }
        body { background: var(--bg); color: var(--text); padding: 14px; min-height: 100vh; }
        .container { max-width: 1200px; margin: 0 auto; display: flex; flex-direction: column; gap: 14px; }

        /* HEADER */
        header {
            display: flex; justify-content: space-between; align-items: center; flex-wrap: wrap; gap: 10px;
            background: var(--card-bg); padding: 12px 18px; border-radius: 12px; border: 1px solid var(--border);
        }
        .header-title { font-size: 1.15rem; font-weight: 700; display: flex; align-items: center; gap: 8px; color: #fff; }
        .header-info { display: flex; align-items: center; gap: 8px; flex-wrap: wrap; }
        .badge { font-size: 0.75rem; padding: 4px 9px; border-radius: 12px; font-weight: 600; text-transform: uppercase; }
        .badge-online { background: rgba(46, 160, 67, 0.2); color: #3fb950; border: 1px solid #2ea043; }
        .badge-homed { background: rgba(88, 166, 255, 0.2); color: #58a6ff; border: 1px solid #58a6ff; }
        .badge-unhomed { background: rgba(210, 153, 34, 0.2); color: #d29922; border: 1px solid #d29922; }

        /* 6-AXIS SUMMARY GRID */
        .overview-grid {
            display: grid; grid-template-columns: repeat(auto-fit, minmax(170px, 1fr)); gap: 10px;
        }
        .axis-card {
            background: var(--card-bg); border: 1px solid var(--border); border-radius: 10px; padding: 12px;
            cursor: pointer; transition: all 0.15s ease; position: relative;
        }
        .axis-card:hover { border-color: var(--accent); }
        .axis-card.active { border-color: var(--accent); background: #1a2334; box-shadow: 0 0 10px rgba(88,166,255,0.2); }
        .axis-head { display: flex; justify-content: space-between; align-items: center; margin-bottom: 6px; }
        .axis-name { font-size: 0.85rem; font-weight: 700; color: var(--text-dim); }
        .axis-deg { font-size: 1.4rem; font-weight: 800; font-feature-settings: "tnum"; color: #fff; }
        .axis-sub { font-size: 0.72rem; color: var(--text-dim); margin-top: 2px; }

        /* TABS */
        .tab-bar {
            display: flex; gap: 6px; overflow-x: auto; padding-bottom: 2px;
        }
        .tab-btn {
            background: var(--card-bg); color: var(--text-dim); border: 1px solid var(--border);
            padding: 9px 16px; border-radius: 8px; font-weight: 600; font-size: 0.88rem;
            cursor: pointer; transition: all 0.15s; white-space: nowrap;
        }
        .tab-btn:hover { background: #212836; color: #fff; }
        .tab-btn.active { background: var(--accent); color: #fff; border-color: var(--accent); }

        /* MAIN GRID */
        .grid-layout { display: grid; grid-template-columns: 1fr; gap: 14px; }
        @media (min-width: 900px) { .grid-layout { grid-template-columns: 1.15fr 1fr; } }

        .card {
            background: var(--card-bg); border: 1px solid var(--border); border-radius: 12px; padding: 16px;
            display: flex; flex-direction: column; gap: 12px;
        }
        .card-title { font-size: 0.95rem; font-weight: 600; display: flex; align-items: center; justify-content: space-between; color: #fff; }

        /* GAUGE DIAL */
        .gauge-wrapper { display: flex; flex-direction: column; align-items: center; justify-content: center; position: relative; }
        .gauge-svg { width: 100%; max-width: 240px; height: auto; }
        .gauge-readout {
            position: absolute; text-align: center; display: flex; flex-direction: column; align-items: center;
            top: 50%; transform: translateY(-50%);
        }
        .main-angle { font-size: 2.1rem; font-weight: 800; font-feature-settings: "tnum"; color: #fff; }
        .target-angle { font-size: 0.8rem; color: var(--text-dim); }

        /* BUTTONS & CONTROLS */
        .btn {
            background: #212836; color: var(--text); border: 1px solid var(--border);
            padding: 9px 14px; border-radius: 8px; font-weight: 600; font-size: 0.88rem;
            cursor: pointer; transition: all 0.15s ease; display: inline-flex; align-items: center; justify-content: center; gap: 6px;
            user-select: none;
        }
        .btn:hover { background: #2d374b; border-color: #8b949e; }
        .btn:active { transform: scale(0.98); }
        .btn-primary { background: var(--primary); color: #fff; border-color: transparent; }
        .btn-primary:hover { background: var(--primary-hover); }
        .btn-accent { background: rgba(88, 166, 255, 0.15); color: var(--accent); border-color: rgba(88, 166, 255, 0.4); }
        .btn-accent:hover { background: rgba(88, 166, 255, 0.25); border-color: var(--accent); }
        .btn-danger { background: rgba(248, 81, 73, 0.15); color: var(--danger); border-color: rgba(248, 81, 73, 0.4); }
        .btn-danger:hover { background: rgba(248, 81, 73, 0.25); border-color: var(--danger); }
        .btn-stop { background: var(--danger); color: #fff; border-color: transparent; font-size: 1rem; padding: 13px; font-weight: 700; }
        .btn-stop:hover { background: #da3633; }

        .btn-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(60px, 1fr)); gap: 6px; }

        /* INPUTS & SLIDERS */
        .input-group { display: flex; gap: 8px; }
        input[type="text"], input[type="password"], input[type="number"], select {
            background: #0b0f19; color: #fff; border: 1px solid var(--border); border-radius: 8px;
            padding: 8px 10px; font-size: 0.9rem; width: 100%; outline: none;
        }
        input:focus, select:focus { border-color: var(--accent); }
        
        input[type="range"] {
            width: 100%; height: 5px; background: #2a3447; border-radius: 4px; outline: none; -webkit-appearance: none;
        }
        input[type="range"]::-webkit-slider-thumb {
            -webkit-appearance: none; width: 16px; height: 16px; border-radius: 50%; background: var(--accent); cursor: pointer;
        }

        .switch-row {
            display: flex; justify-content: space-between; align-items: center; padding: 6px 0; border-bottom: 1px solid rgba(48, 54, 61, 0.4);
        }
        .switch-row:last-child { border-bottom: none; }
        .switch-label { font-size: 0.86rem; color: var(--text); }
        .switch-sub { font-size: 0.72rem; color: var(--text-dim); }

        /* TOGGLE SWITCH */
        .toggle { position: relative; width: 40px; height: 22px; }
        .toggle input { opacity: 0; width: 0; height: 0; }
        .slider-round {
            position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0;
            background-color: #2a3447; border-radius: 22px; transition: .2s;
        }
        .slider-round:before {
            position: absolute; content: ""; height: 16px; width: 16px; left: 3px; bottom: 3px;
            background-color: white; border-radius: 50%; transition: .2s;
        }
        input:checked + .slider-round { background-color: var(--primary); }
        input:checked + .slider-round:before { transform: translateX(18px); }

        /* STATS TABLE */
        .stat-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 8px; }
        .stat-item { background: #0b0f19; padding: 8px 10px; border-radius: 8px; border: 1px solid rgba(48, 54, 61, 0.5); }
        .stat-name { font-size: 0.7rem; color: var(--text-dim); }
        .stat-val { font-size: 0.95rem; font-weight: 700; color: #fff; margin-top: 2px; }

        /* 6-AXIS SYNC MOVE GRID */
        .sync-grid { display: grid; grid-template-columns: repeat(6, 1fr); gap: 6px; }
        .sync-col { display: flex; flex-direction: column; align-items: center; gap: 4px; }
        .sync-col label { font-size: 0.75rem; font-weight: 700; color: var(--accent); }
        .sync-col input { text-align: center; padding: 6px 4px; font-size: 0.85rem; }

        /* LOG CONSOLE */
        .log-box {
            background: #080b12; border: 1px solid var(--border); border-radius: 8px; padding: 8px;
            font-family: monospace; font-size: 0.75rem; height: 100px; overflow-y: auto; color: #7ee787;
        }
    </style>
</head>
<body>

<div class="container">
    <!-- HEADER -->
    <header>
        <div class="header-title">
            <span>⚙️</span> ESP32-S3 6-Axis Motion Controller
        </div>
        <div class="header-info">
            <span id="badge-wifi" class="badge badge-unhomed">Wi-Fi: ...</span>
            <span id="badge-global-home" class="badge badge-unhomed">Chưa Home Hết</span>
            <button class="btn btn-danger" onclick="sendStopAll()" style="padding: 5px 10px; font-size: 0.8rem;">⛔ STOP ALL</button>
        </div>
    </header>

    <!-- 6-AXIS SUMMARY OVERVIEW -->
    <div class="overview-grid" id="axis-overview-grid">
        <!-- Generated dynamically via JS for J1..J6 -->
    </div>

    <!-- JOINT SELECTOR TABS -->
    <div class="tab-bar">
        <button class="tab-btn active" onclick="selectTab(0)">Joint 1 (M0)</button>
        <button class="tab-btn" onclick="selectTab(1)">Joint 2 (M1)</button>
        <button class="tab-btn" onclick="selectTab(2)">Joint 3 (M2)</button>
        <button class="tab-btn" onclick="selectTab(3)">Joint 4 (M3)</button>
        <button class="tab-btn" onclick="selectTab(4)">Joint 5 (M4)</button>
        <button class="tab-btn" onclick="selectTab(5)">Joint 6 (M5)</button>
        <button class="tab-btn" onclick="selectTab(6)">🌐 Đồng Bộ 6 Trục</button>
    </div>

    <!-- MAIN TWO COLUMN GRID -->
    <div class="grid-layout" id="panel-joint-detail">
        <!-- LEFT COLUMN: DIAL & POSITION CONTROL + MANUAL CONTROL -->
        <div style="display: flex; flex-direction: column; gap: 14px;">
            <div class="card">
                <div class="card-title">
                    <span id="lbl-active-joint">🧭 Joint 1 - Góc Hiện Tại</span>
                    <span id="state-motion" class="badge badge-online">ĐỨNG YÊN</span>
                </div>

                <!-- GAUGE SVG -->
                <div class="gauge-wrapper">
                    <svg id="gauge" class="gauge-svg" viewBox="0 0 200 200">
                        <circle cx="100" cy="100" r="80" fill="none" stroke="#1d2535" stroke-width="12" />
                        <line x1="100" y1="18" x2="100" y2="28" stroke="#58a6ff" stroke-width="3" stroke-linecap="round" />
                        <line id="pointer-target" x1="100" y1="100" x2="100" y2="30" stroke="rgba(88, 166, 255, 0.5)" stroke-width="3" stroke-dasharray="4,4" />
                        <line id="pointer-current" x1="100" y1="100" x2="100" y2="25" stroke="#2ea043" stroke-width="4" stroke-linecap="round" />
                        <circle cx="100" cy="100" r="7" fill="#2ea043" />
                    </svg>

                    <div class="gauge-readout">
                        <div id="disp-angle" class="main-angle">0.00°</div>
                        <div id="disp-sub" class="target-angle">Target: 0.00° | Err: 0.00°</div>
                    </div>
                </div>

                <!-- GOTO ANGLE CONTROL -->
                <div class="input-group">
                    <input type="number" id="input-angle" placeholder="Góc đích (vd: 45, -45)" step="0.5" value="0">
                    <button class="btn btn-primary" onclick="sendGoto()">Đến Góc</button>
                </div>
                <input type="range" id="slider-angle" min="-180" max="180" value="0" step="1" oninput="onSliderChange(this.value)">

                <!-- QUICK PRESETS -->
                <div style="font-size: 0.75rem; color: var(--text-dim); margin-top: 2px;">Vị trí nhanh:</div>
                <div class="btn-grid">
                    <button class="btn btn-accent" onclick="setAngle(0)">0° (Home)</button>
                    <button class="btn" onclick="setAngle(45)">+45°</button>
                    <button class="btn" onclick="setAngle(-45)">-45°</button>
                    <button class="btn" onclick="setAngle(90)">+90°</button>
                    <button class="btn" onclick="setAngle(-90)">-90°</button>
                </div>

                <!-- JOG BUTTONS -->
                <div style="font-size: 0.75rem; color: var(--text-dim); margin-top: 2px;">Nhích góc (Jog):</div>
                <div class="btn-grid">
                    <button class="btn" onclick="jog(-10)">⏪ -10°</button>
                    <button class="btn" onclick="jog(-1)">◀ -1°</button>
                    <button class="btn" onclick="jog(1)">+1° ▶</button>
                    <button class="btn" onclick="jog(10)">+10° ⏩</button>
                </div>

                <!-- EMERGENCY STOP CURRENT -->
                <button class="btn btn-stop" onclick="sendStopActive()">⛔ DỪNG TRỤC NÀY</button>
            </div>

            <!-- MANUAL MOTOR CONTROL CARD -->
            <div class="card">
                <div class="card-title">
                    <span>🎛️ Điều Khiển Motor Trực Tiếp (Manual)</span>
                    <span id="state-driver" class="badge badge-online">DRIVER BẬT</span>
                </div>

                <!-- DRIVER ENABLE / FREE SHAFT TOGGLE -->
                <div class="switch-row" style="padding-bottom: 4px;">
                    <div>
                        <div class="switch-label">Cấp Nguồn Động Cơ (Driver Enable)</div>
                        <div class="switch-sub">Tắt để thả tự do trục quay tay không cản</div>
                    </div>
                    <label class="toggle">
                        <input type="checkbox" id="chk-driver-enable" checked onchange="toggleDriverEnable(this.checked)">
                        <span class="slider-round"></span>
                    </label>
                </div>

                <!-- STEPPING CONTROL SECTION -->
                <div style="font-size: 0.8rem; font-weight: 600; color: var(--text);">Quay theo số bước (Step Control):</div>
                <div class="input-group">
                    <input type="number" id="input-raw-steps" placeholder="Số bước" value="200" min="1" step="50">
                    <button class="btn btn-accent" onclick="sendMotorStep('ccw')">⟲ CCW</button>
                    <button class="btn btn-primary" onclick="sendMotorStep('cw')">CW ⟳</button>
                </div>

                <!-- QUICK STEP PRESETS -->
                <div style="font-size: 0.75rem; color: var(--text-dim);">Bước nhanh:</div>
                <div class="btn-grid">
                    <button class="btn" onclick="quickStep('ccw', 50)">⟲ 50</button>
                    <button class="btn" onclick="quickStep('ccw', 200)">⟲ 200</button>
                    <button class="btn" onclick="quickStep('ccw', 1600)">⟲ 1600</button>
                    <button class="btn" onclick="quickStep('cw', 50)">50 ⟳</button>
                    <button class="btn" onclick="quickStep('cw', 200)">200 ⟳</button>
                    <button class="btn" onclick="quickStep('cw', 1600)">1600 ⟳</button>
                </div>

                <!-- CONTINUOUS RUN -->
                <div style="font-size: 0.8rem; font-weight: 600; color: var(--text); margin-top: 2px;">Quay liên tục (Continuous):</div>
                <div style="display: grid; grid-template-columns: 1fr 1fr 1fr; gap: 6px;">
                    <button class="btn btn-accent" onclick="sendMotorRun('ccw')">⟲ Chạy CCW</button>
                    <button class="btn btn-danger" onclick="sendStopActive()">⏹ Dừng</button>
                    <button class="btn btn-primary" onclick="sendMotorRun('cw')">Chạy CW ⟳</button>
                </div>
            </div>
        </div>

        <!-- RIGHT COLUMN: ACTIONS, STATS & SETTINGS -->
        <div style="display: flex; flex-direction: column; gap: 14px;">
            <!-- HOMING & CALIB -->
            <div class="card">
                <div class="card-title">🎯 Tác Vụ Chuẩn Hóa Trục</div>
                <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 8px;">
                    <button class="btn btn-accent" onclick="sendHome()" style="padding: 10px;">
                        <span>🎯</span> Homing Cung Lớn
                    </button>
                    <button class="btn btn-accent" onclick="sendCalib()" style="padding: 10px;">
                        <span>🔄</span> Auto Calib LUT
                    </button>
                </div>
                <div style="display: flex; gap: 8px;">
                    <button class="btn btn-danger" onclick="sendCalibClear()" style="flex: 1; font-size: 0.75rem;">
                        Xóa LUT Calib
                    </button>
                    <button class="btn" onclick="fetchStatus(true)" style="flex: 1; font-size: 0.75rem;">
                        Làm Mới
                    </button>
                </div>
            </div>

            <!-- LIVE STATS -->
            <div class="card">
                <div class="card-title">📊 Thông Số Hoạt Động Trục</div>
                <div class="stat-grid">
                    <div class="stat-item">
                        <div class="stat-name">AS5600 Cảm Biến</div>
                        <div id="stat-sensor" class="stat-val" style="color:#3fb950;">OK</div>
                    </div>
                    <div class="stat-item">
                        <div class="stat-name">Từ Trường (AGC / Mag)</div>
                        <div id="stat-mag" class="stat-val">128 / 141</div>
                    </div>
                    <div class="stat-item">
                        <div class="stat-name">Góc Thô AS5600</div>
                        <div id="stat-raw" class="stat-val">0.00°</div>
                    </div>
                    <div class="stat-item">
                        <div class="stat-name">Tổng Hành Trình</div>
                        <div id="stat-stroke" class="stat-val">0.00°</div>
                    </div>
                    <div class="stat-item">
                        <div class="stat-name">Dòng TMC2209</div>
                        <div id="stat-curr" class="stat-val">800 mA</div>
                    </div>
                    <div class="stat-item">
                        <div class="stat-name">Tốc Độ Xung (Interval)</div>
                        <div id="stat-speed" class="stat-val">400 us</div>
                    </div>
                </div>
            </div>

            <!-- MOTOR SETTINGS -->
            <div class="card">
                <div class="card-title">⚙️ Cấu Hình Trục & Deadband</div>
                
                <div class="switch-row">
                    <div>
                        <div class="switch-label">Giữ Góc Vòng Kín (Hold)</div>
                        <div class="switch-sub">Tự động chống vặn lệch với Schmitt deadband</div>
                    </div>
                    <label class="toggle">
                        <input type="checkbox" id="chk-hold" onchange="sendConfig()">
                        <span class="slider-round"></span>
                    </label>
                </div>

                <div class="switch-row">
                    <div>
                        <div class="switch-label">Đảo Chiều Motor (Invert)</div>
                        <div class="switch-sub">Đảo chiều quay so với cảm biến</div>
                    </div>
                    <label class="toggle">
                        <input type="checkbox" id="chk-invert" onchange="sendConfig()">
                        <span class="slider-round"></span>
                    </label>
                </div>

                <div style="display: flex; flex-direction: column; gap: 4px; margin-top: 4px;">
                    <div style="display:flex; justify-content:space-between; font-size:0.8rem;">
                        <span>Tốc độ xung: <b id="lbl-speed">400 us</b></span>
                        <span style="color:var(--text-dim);">Càng nhỏ càng nhanh</span>
                    </div>
                    <input type="range" id="rng-speed" min="150" max="1500" step="50" value="400" onchange="sendConfig()">
                </div>

                <div style="display: flex; flex-direction: column; gap: 4px; margin-top: 4px;">
                    <div style="display:flex; justify-content:space-between; font-size:0.8rem;">
                        <span>Dòng RMS: <b id="lbl-curr">800 mA</b></span>
                        <span style="color:var(--text-dim);">TMC2209</span>
                    </div>
                    <input type="range" id="rng-curr" min="200" max="1400" step="50" value="800" onchange="sendConfig()">
                </div>
            </div>

            <!-- WI-FI CARD -->
            <div class="card">
                <div class="card-title">
                    <span>📶 Cài Đặt Wi-Fi</span>
                    <button class="btn" style="padding: 3px 8px; font-size: 0.72rem;" onclick="scanWifi()">🔍 Quét Wi-Fi</button>
                </div>
                <div style="display: flex; flex-direction: column; gap: 6px;">
                    <select id="wifi-ssid-select" onchange="onSelectSSID(this.value)" style="display:none;">
                        <option value="">-- Chọn Wi-Fi đã quét --</option>
                    </select>
                    <input type="text" id="wifi-ssid" placeholder="Tên Wi-Fi (SSID)">
                    <input type="password" id="wifi-pass" placeholder="Mật khẩu Wi-Fi">
                    <div style="display: flex; gap: 6px;">
                        <button class="btn btn-primary" onclick="saveWifi()" style="flex:1;">Lưu & Kết Nối</button>
                        <button class="btn btn-danger" onclick="clearWifi()" style="font-size: 0.75rem;">Xóa Lưu</button>
                    </div>
                </div>
            </div>

            <!-- LOG CONSOLE -->
            <div class="card" style="padding: 10px;">
                <div style="font-size: 0.8rem; font-weight: 600; margin-bottom: 4px;">Nhật Ký Hoạt Động:</div>
                <div id="log-box" class="log-box">Hệ thống 6 trục sẵn sàng...</div>
            </div>
        </div>
    </div>

    <!-- PANEL FOR 6-AXIS SYNCHRONIZED MOVE -->
    <div class="card" id="panel-sync-move" style="display: none;">
        <div class="card-title">🌐 Điều Khiển Đồng Bộ 6 Trục (Synchronized Arrival)</div>
        <div style="font-size: 0.82rem; color: var(--text-dim);">
            Nhập góc đích cho 6 khớp. Bộ điều phối sẽ tính toán co giãn vận tốc để tất cả 6 trục hoàn thành cùng một thời điểm mà không bị méo quỹ đạo.
        </div>

        <div class="sync-grid" style="margin-top: 10px;">
            <div class="sync-col"><label>J1 (deg)</label><input type="number" id="sync-j0" value="0" step="1"></div>
            <div class="sync-col"><label>J2 (deg)</label><input type="number" id="sync-j1" value="0" step="1"></div>
            <div class="sync-col"><label>J3 (deg)</label><input type="number" id="sync-j2" value="0" step="1"></div>
            <div class="sync-col"><label>J4 (deg)</label><input type="number" id="sync-j3" value="0" step="1"></div>
            <div class="sync-col"><label>J5 (deg)</label><input type="number" id="sync-j4" value="0" step="1"></div>
            <div class="sync-col"><label>J6 (deg)</label><input type="number" id="sync-j5" value="0" step="1"></div>
        </div>

        <div style="display: flex; gap: 10px; align-items: center; margin-top: 10px;">
            <div style="display: flex; gap: 6px; align-items: center; width: 220px;">
                <span style="font-size: 0.8rem; color: var(--text-dim);">Thời gian (s):</span>
                <input type="number" id="sync-time" placeholder="Tự động" step="0.1" min="0" value="0" style="width: 100px;">
            </div>
            <button class="btn btn-primary" onclick="sendSyncMove()" style="flex: 1; padding: 12px; font-size: 1rem;">
                🚀 Bắt Đầu Quay Đồng Bộ 6 Trục
            </button>
            <button class="btn btn-danger" onclick="sendStopAll()" style="padding: 12px 18px;">
                ⛔ DỪNG TẤT CẢ
            </button>
        </div>
    </div>
</div>

<script>
    let activeAxis = 0;
    let latestData = null;

    function log(msg) {
        const box = document.getElementById('log-box');
        if (!box) return;
        const d = new Date().toLocaleTimeString();
        box.innerHTML = `[${d}] ${msg}<br>` + box.innerHTML;
    }

    function selectTab(idx) {
        const tabs = document.querySelectorAll('.tab-btn');
        tabs.forEach((t, i) => {
            if (i === idx) t.classList.add('active');
            else t.classList.remove('active');
        });

        if (idx === 6) {
            document.getElementById('panel-joint-detail').style.display = 'none';
            document.getElementById('panel-sync-move').style.display = 'flex';
        } else {
            activeAxis = idx;
            document.getElementById('panel-joint-detail').style.display = 'grid';
            document.getElementById('panel-sync-move').style.display = 'none';
            document.getElementById('lbl-active-joint').innerText = `🧭 Joint ${activeAxis + 1} (M${activeAxis}) - Góc Hiện Tại`;
            fetchStatus(true);
        }
    }

    function updatePointer(angle, isTarget = false) {
        const elem = document.getElementById(isTarget ? 'pointer-target' : 'pointer-current');
        if (elem) elem.setAttribute('transform', `rotate(${angle}, 100, 100)`);
    }

    async function fetchStatus(isManual = false) {
        try {
            const res = await fetch('/api/status');
            if (!res.ok) return;
            latestData = await res.json();

            // 1. Render 6-Axis Overview Grid
            const grid = document.getElementById('axis-overview-grid');
            if (grid && latestData.axes) {
                let html = '';
                latestData.axes.forEach((ax, i) => {
                    const isAct = (i === activeAxis) ? 'active' : '';
                    const okTag = ax.as5600_ok ? '<span style="color:#3fb950;">●</span>' : '<span style="color:#f85149;">●</span>';
                    const runTag = ax.isRunning ? '<span style="color:#58a6ff;">QUAY</span>' : '<span style="color:#8b949e;">TĨNH</span>';
                    html += `
                    <div class="axis-card ${isAct}" onclick="selectTab(${i})">
                        <div class="axis-head">
                            <span class="axis-name">JOINT ${i + 1} ${okTag}</span>
                            <span style="font-size:0.72rem; font-weight:700;">${runTag}</span>
                        </div>
                        <div class="axis-deg">${ax.currentAngle.toFixed(1)}°</div>
                        <div class="axis-sub">Tgt: ${ax.targetAngle.toFixed(1)}° | Err: ${ax.error.toFixed(1)}°</div>
                    </div>`;
                });
                grid.innerHTML = html;
            }

            // 2. Wi-Fi & Global Home Badge
            const badgeWifi = document.getElementById('badge-wifi');
            if (latestData.wifi_connected) {
                badgeWifi.className = "badge badge-online";
                badgeWifi.innerText = `STA: ${latestData.wifi_ip}`;
            } else {
                badgeWifi.className = "badge badge-unhomed";
                badgeWifi.innerText = `AP: 192.168.4.1`;
            }

            const badgeGlobalHome = document.getElementById('badge-global-home');
            if (latestData.all_homed) {
                badgeGlobalHome.className = "badge badge-homed";
                badgeGlobalHome.innerText = "Đã Home Hết";
            } else {
                badgeGlobalHome.className = "badge badge-unhomed";
                badgeGlobalHome.innerText = "Chưa Home Hết";
            }

            // 3. Active Axis Data
            if (latestData.axes && latestData.axes[activeAxis]) {
                const ax = latestData.axes[activeAxis];
                document.getElementById('disp-angle').innerText = ax.currentAngle.toFixed(2) + '°';
                document.getElementById('disp-sub').innerText = `Target: ${ax.targetAngle.toFixed(2)}° | Err: ${ax.error.toFixed(2)}°`;
                
                updatePointer(ax.currentAngle, false);
                updatePointer(ax.targetAngle, true);

                const motionBadge = document.getElementById('state-motion');
                if (ax.isRunning) {
                    motionBadge.innerText = "ĐANG QUAY";
                    motionBadge.style.background = "rgba(88, 166, 255, 0.2)";
                    motionBadge.style.color = "#58a6ff";
                } else {
                    motionBadge.innerText = ax.inDeadband ? "GIỮ GÓC (DEADBAND)" : "ĐỨNG YÊN";
                    motionBadge.style.background = "rgba(46, 160, 67, 0.2)";
                    motionBadge.style.color = "#3fb950";
                }

                const driverBadge = document.getElementById('state-driver');
                if (driverBadge) {
                    if (ax.driver_enabled) {
                        driverBadge.className = "badge badge-online";
                        driverBadge.innerText = "DRIVER BẬT";
                    } else {
                        driverBadge.className = "badge badge-unhomed";
                        driverBadge.innerText = "TRỤC TỰ DO";
                    }
                }

                document.getElementById('stat-sensor').innerText = ax.as5600_ok ? "OK" : "LỖI I2C";
                document.getElementById('stat-sensor').style.color = ax.as5600_ok ? "#3fb950" : "#f85149";
                document.getElementById('stat-mag').innerText = `${ax.agc} / ${ax.magnitude}`;
                document.getElementById('stat-raw').innerText = ax.rawAngle.toFixed(2) + '°';
                document.getElementById('stat-stroke').innerText = ax.totalStroke.toFixed(2) + '°';
                document.getElementById('stat-curr').innerText = ax.current + ' mA';
                document.getElementById('stat-speed').innerText = ax.speed + ' us';

                if (isManual) {
                    document.getElementById('chk-hold').checked = ax.closedLoopHold;
                    document.getElementById('chk-invert').checked = ax.dirInvert;
                    const chkDriver = document.getElementById('chk-driver-enable');
                    if (chkDriver) chkDriver.checked = ax.driver_enabled;
                    document.getElementById('rng-speed').value = ax.speed;
                    document.getElementById('lbl-speed').innerText = ax.speed + ' us';
                    document.getElementById('rng-curr').value = ax.current;
                    document.getElementById('lbl-curr').innerText = ax.current + ' mA';
                }
            }

        } catch (e) {
            console.error(e);
        }
    }

    function onSliderChange(val) {
        document.getElementById('input-angle').value = val;
    }

    async function sendGoto() {
        const val = parseFloat(document.getElementById('input-angle').value);
        if (isNaN(val)) return;
        log(`[J${activeAxis + 1}] Quay đến góc: ${val}°`);
        await fetch(`/api/motor/goto?axis=${activeAxis}&angle=${val}`, { method: 'POST' });
    }

    function setAngle(deg) {
        document.getElementById('input-angle').value = deg;
        document.getElementById('slider-angle').value = deg;
        sendGoto();
    }

    async function jog(delta) {
        log(`[J${activeAxis + 1}] Nhích góc ${delta > 0 ? '+' : ''}${delta}°`);
        await fetch(`/api/motor/jog?axis=${activeAxis}&delta=${delta}`, { method: 'POST' });
    }

    async function sendStopActive() {
        log(`⛔ [J${activeAxis + 1}] Lệnh DỪNG TRỤC`);
        await fetch(`/api/motor/stop?axis=${activeAxis}`, { method: 'POST' });
    }

    async function sendStopAll() {
        log(`⛔ LỆNH DỪNG KHẨN CẤP TẤT CẢ 6 TRỤC!`);
        await fetch(`/api/all/stop`, { method: 'POST' });
    }

    async function sendMotorStep(dir) {
        const steps = parseInt(document.getElementById('input-raw-steps').value) || 200;
        log(`[J${activeAxis + 1}] Quay Motor ${dir.toUpperCase()}: ${steps} bước`);
        await fetch(`/api/motor/step?axis=${activeAxis}&dir=${dir}&steps=${steps}`, { method: 'POST' });
    }

    async function quickStep(dir, steps) {
        document.getElementById('input-raw-steps').value = steps;
        log(`[J${activeAxis + 1}] Quay nhanh ${dir.toUpperCase()}: ${steps} bước`);
        await fetch(`/api/motor/step?axis=${activeAxis}&dir=${dir}&steps=${steps}`, { method: 'POST' });
    }

    async function sendMotorRun(dir) {
        log(`[J${activeAxis + 1}] Bắt đầu quay liên tục ${dir.toUpperCase()}...`);
        await fetch(`/api/motor/run?axis=${activeAxis}&dir=${dir}`, { method: 'POST' });
    }

    async function toggleDriverEnable(enabled) {
        const en = enabled ? 1 : 0;
        log(`[J${activeAxis + 1}] ${enabled ? 'Bật cấp nguồn Driver' : 'Thả tự do động cơ (Free shaft)'}`);
        await fetch(`/api/motor/enable?axis=${activeAxis}&en=${en}`, { method: 'POST' });
    }

    async function sendHome() {
        log(`🎯 [J${activeAxis + 1}] Bắt đầu Homing Cung Lớn...`);
        await fetch(`/api/motor/home?axis=${activeAxis}`, { method: 'POST' });
    }

    async function sendCalib() {
        log(`🔄 [J${activeAxis + 1}] Bắt đầu Auto Calibration 16 điểm...`);
        await fetch(`/api/motor/calib?axis=${activeAxis}`, { method: 'POST' });
    }

    async function sendCalibClear() {
        if (!confirm(`Xóa bảng hiệu chuẩn Calib cho Joint ${activeAxis + 1}?`)) return;
        log(`[J${activeAxis + 1}] Xóa bảng hiệu chuẩn Calib`);
        await fetch(`/api/motor/calib_clear?axis=${activeAxis}`, { method: 'POST' });
    }

    async function sendConfig() {
        const hold = document.getElementById('chk-hold').checked ? 1 : 0;
        const invert = document.getElementById('chk-invert').checked ? 1 : 0;
        const speed = document.getElementById('rng-speed').value;
        const curr = document.getElementById('rng-curr').value;

        document.getElementById('lbl-speed').innerText = speed + ' us';
        document.getElementById('lbl-curr').innerText = curr + ' mA';

        await fetch(`/api/motor/settings?axis=${activeAxis}&hold=${hold}&invert=${invert}&speed=${speed}&curr=${curr}`, { method: 'POST' });
    }

    // 6-AXIS SYNCHRONIZED MOVE
    async function sendSyncMove() {
        const a0 = parseFloat(document.getElementById('sync-j0').value) || 0;
        const a1 = parseFloat(document.getElementById('sync-j1').value) || 0;
        const a2 = parseFloat(document.getElementById('sync-j2').value) || 0;
        const a3 = parseFloat(document.getElementById('sync-j3').value) || 0;
        const a4 = parseFloat(document.getElementById('sync-j4').value) || 0;
        const a5 = parseFloat(document.getElementById('sync-j5').value) || 0;
        const t = parseFloat(document.getElementById('sync-time').value) || 0;

        const anglesStr = `${a0},${a1},${a2},${a3},${a4},${a5}`;
        log(`🚀 Gửi lệnh quay đồng bộ 6 trục: [${anglesStr}] | T=${t}s`);
        await fetch(`/api/all/goto?angles=${encodeURIComponent(anglesStr)}&time=${t}`, { method: 'POST' });
    }

    // WI-FI SCAN & CONNECT
    async function scanWifi() {
        log('Đang quét các mạng Wi-Fi...');
        try {
            const res = await fetch('/api/wifi/scan');
            const list = await res.json();
            const sel = document.getElementById('wifi-ssid-select');
            sel.innerHTML = '<option value="">-- Chọn Wi-Fi (' + list.length + ' mạng) --</option>';
            list.forEach(w => {
                sel.innerHTML += `<option value="${w.ssid}">${w.ssid} (${w.rssi} dBm)</option>`;
            });
            sel.style.display = 'block';
            log(`Tìm thấy ${list.length} mạng Wi-Fi.`);
        } catch (e) {
            log('Lỗi khi quét Wi-Fi.');
        }
    }

    function onSelectSSID(val) {
        if (val) document.getElementById('wifi-ssid').value = val;
    }

    async function saveWifi() {
        const ssid = document.getElementById('wifi-ssid').value.trim();
        const pass = document.getElementById('wifi-pass').value;
        if (!ssid) {
            alert('Vui lòng nhập tên Wi-Fi!');
            return;
        }
        log(`Lưu & kết nối Wi-Fi: ${ssid}...`);
        await fetch(`/api/wifi/save?ssid=${encodeURIComponent(ssid)}&pass=${encodeURIComponent(pass)}`, { method: 'POST' });
        alert('Đã gửi thông tin Wi-Fi! ESP32 đang kết nối...');
    }

    async function clearWifi() {
        if (!confirm('Xóa thông tin Wi-Fi đã lưu?')) return;
        log('Đã xóa thông tin Wi-Fi.');
        await fetch('/api/wifi/clear', { method: 'POST' });
    }

    setInterval(fetchStatus, 200);
    window.onload = () => fetchStatus(true);
</script>

</body>
</html>
)rawliteral";

#endif // WEB_UI_H
