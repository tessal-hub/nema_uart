#ifndef WEB_UI_H
#define WEB_UI_H

#include <Arduino.h>

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <title>ESP32-S3 6-Axis Motion Dashboard</title>
    <style>
        :root {
            --bg-root: #0a0e17;
            --surface-1: #131a26;
            --surface-2: #0b1019;
            --surface-3: #182232;
            --border: #1e2a3c;
            --border-hover: #334560;
            --primary: #238636;
            --primary-hover: #2ea043;
            --accent: #388bfd;
            --accent-hover: #58a6ff;
            --accent-soft: rgba(56, 139, 253, 0.12);
            --danger: #da3633;
            --danger-hover: #f85149;
            --danger-soft: rgba(218, 54, 51, 0.15);
            --warning: #d29922;
            --warning-soft: rgba(210, 153, 34, 0.15);
            --text-main: #f0f6fc;
            --text-dim: #8b949e;
            --text-muted: #64748b;
            --font-ui: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
            --font-mono: ui-monospace, SFMono-Regular, "SF Mono", Menlo, Consolas, monospace;
        }

        * { box-sizing: border-box; margin: 0; padding: 0; font-family: var(--font-ui); }
        body { background: var(--bg-root); color: var(--text-main); padding: 12px; min-height: 100vh; line-height: 1.4; }
        .container { max-width: 1240px; margin: 0 auto; display: flex; flex-direction: column; gap: 12px; }

        /* HEADER */
        header {
            display: flex; justify-content: space-between; align-items: center; flex-wrap: wrap; gap: 10px;
            background: var(--surface-1); padding: 12px 18px; border-radius: 10px; border: 1px solid var(--border);
        }
        .header-title { font-size: 1.08rem; font-weight: 700; display: flex; align-items: center; gap: 10px; color: #fff; }
        .status-dot { width: 8px; height: 8px; border-radius: 50%; background: #3fb950; box-shadow: 0 0 8px #3fb950; display: inline-block; }
        .header-controls { display: flex; align-items: center; gap: 8px; flex-wrap: wrap; }
        
        .badge { font-size: 0.72rem; padding: 4px 9px; border-radius: 6px; font-weight: 600; font-family: var(--font-mono); }
        .badge-online { background: rgba(46, 160, 67, 0.15); color: #3fb950; border: 1px solid rgba(46, 160, 67, 0.35); }
        .badge-homed { background: var(--accent-soft); color: #58a6ff; border: 1px solid rgba(56, 139, 253, 0.35); }
        .badge-unhomed { background: var(--warning-soft); color: #d29922; border: 1px solid rgba(210, 153, 34, 0.35); }
        .badge-danger { background: var(--danger-soft); color: #ff7b72; border: 1px solid rgba(218, 54, 51, 0.4); }

        /* ALERT BANNER */
        .alert-banner {
            background: var(--danger-soft); border: 1px solid var(--danger); border-radius: 8px;
            padding: 10px 14px; color: #ff7b72; font-size: 0.85rem; font-weight: 600; display: none; align-items: center; gap: 8px;
        }

        /* 6-AXIS SUMMARY GRID */
        .overview-grid {
            display: grid; grid-template-columns: repeat(auto-fit, minmax(175px, 1fr)); gap: 8px;
        }
        .axis-card {
            background: var(--surface-1); border: 1px solid var(--border); border-radius: 8px; padding: 10px 12px;
            cursor: pointer; transition: all 0.15s ease; position: relative; user-select: none;
        }
        .axis-card:hover { border-color: var(--border-hover); background: #151e2e; }
        .axis-card.active { border-color: var(--accent); background: #162235; }
        .axis-head { display: flex; justify-content: space-between; align-items: center; margin-bottom: 4px; }
        .axis-name { font-size: 0.78rem; font-weight: 700; color: var(--text-dim); }
        .axis-deg { font-size: 1.35rem; font-weight: 800; font-family: var(--font-mono); font-feature-settings: "tnum" 1; color: #fff; }
        .axis-sub { font-size: 0.7rem; font-family: var(--font-mono); color: var(--text-muted); margin-top: 2px; }

        /* TABS */
        .tab-bar { display: flex; gap: 6px; overflow-x: auto; padding-bottom: 2px; }
        .tab-btn {
            background: var(--surface-1); color: var(--text-dim); border: 1px solid var(--border);
            padding: 8px 14px; border-radius: 6px; font-weight: 600; font-size: 0.82rem;
            cursor: pointer; transition: all 0.15s; white-space: nowrap;
        }
        .tab-btn:hover { background: #1c2638; color: #fff; border-color: var(--border-hover); }
        .tab-btn.active { background: var(--accent); color: #fff; border-color: var(--accent); }

        /* MAIN GRID */
        .grid-layout { display: grid; grid-template-columns: 1fr; gap: 12px; }
        @media (min-width: 920px) { .grid-layout { grid-template-columns: 1.15fr 1fr; } }

        .card {
            background: var(--surface-1); border: 1px solid var(--border); border-radius: 10px; padding: 14px;
            display: flex; flex-direction: column; gap: 10px;
        }
        .card-title { font-size: 0.92rem; font-weight: 600; display: flex; align-items: center; justify-content: space-between; color: #fff; }

        /* GAUGE DIAL */
        .gauge-wrapper { display: flex; flex-direction: column; align-items: center; justify-content: center; position: relative; padding: 4px 0; }
        .gauge-svg { width: 100%; max-width: 240px; height: auto; }
        .gauge-readout {
            position: absolute; text-align: center; display: flex; flex-direction: column; align-items: center;
            top: 50%; transform: translateY(-50%); pointer-events: none;
        }
        .main-angle { font-size: 2.1rem; font-weight: 800; font-family: var(--font-mono); font-feature-settings: "tnum" 1; color: #fff; }
        .target-angle { font-size: 0.78rem; font-family: var(--font-mono); color: var(--text-dim); }

        /* BUTTONS & CONTROLS */
        .btn {
            background: #1e283a; color: var(--text-main); border: 1px solid var(--border);
            padding: 8px 12px; border-radius: 6px; font-weight: 600; font-size: 0.84rem;
            cursor: pointer; transition: all 0.15s ease; display: inline-flex; align-items: center; justify-content: center; gap: 6px;
            user-select: none; text-decoration: none;
        }
        .btn:hover { background: #2b3952; border-color: var(--border-hover); }
        .btn:active { transform: scale(0.98); }
        .btn-primary { background: var(--primary); color: #fff; border-color: transparent; }
        .btn-primary:hover { background: var(--primary-hover); }
        .btn-accent { background: var(--accent-soft); color: var(--accent); border-color: rgba(56, 139, 253, 0.35); }
        .btn-accent:hover { background: rgba(56, 139, 253, 0.22); border-color: var(--accent); }
        .btn-danger { background: var(--danger-soft); color: #ff7b72; border-color: rgba(218, 54, 51, 0.35); }
        .btn-danger:hover { background: rgba(218, 54, 51, 0.25); border-color: var(--danger); }
        .btn-stop { background: var(--danger); color: #fff; border-color: transparent; font-size: 0.92rem; padding: 11px; font-weight: 700; }
        .btn-stop:hover { background: var(--danger-hover); }

        .btn-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(54px, 1fr)); gap: 5px; }

        /* INPUTS & SLIDERS */
        .input-group { display: flex; gap: 6px; }
        input[type="text"], input[type="password"], input[type="number"], select {
            background: var(--surface-2); color: #fff; border: 1px solid var(--border); border-radius: 6px;
            padding: 8px 10px; font-size: 0.88rem; width: 100%; outline: none; font-family: var(--font-mono);
        }
        input:focus, select:focus { border-color: var(--accent); }
        
        input[type="range"] {
            width: 100%; height: 5px; background: #232f42; border-radius: 3px; outline: none; -webkit-appearance: none;
        }
        input[type="range"]::-webkit-slider-thumb {
            -webkit-appearance: none; width: 15px; height: 15px; border-radius: 50%; background: var(--accent); cursor: pointer;
        }

        .switch-row {
            display: flex; justify-content: space-between; align-items: center; padding: 5px 0; border-bottom: 1px solid rgba(30, 42, 60, 0.6);
        }
        .switch-row:last-child { border-bottom: none; }
        .switch-label { font-size: 0.84rem; color: var(--text-main); }
        .switch-sub { font-size: 0.7rem; color: var(--text-dim); }

        /* TOGGLE SWITCH */
        .toggle { position: relative; width: 38px; height: 20px; }
        .toggle input { opacity: 0; width: 0; height: 0; }
        .slider-round {
            position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0;
            background-color: #232f42; border-radius: 20px; transition: .2s;
        }
        .slider-round:before {
            position: absolute; content: ""; height: 14px; width: 14px; left: 3px; bottom: 3px;
            background-color: white; border-radius: 50%; transition: .2s;
        }
        input:checked + .slider-round { background-color: var(--primary); }
        input:checked + .slider-round:before { transform: translateX(18px); }

        /* STATS TABLE */
        .stat-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 6px; }
        .stat-item { background: var(--surface-2); padding: 8px 10px; border-radius: 6px; border: 1px solid rgba(30, 42, 60, 0.8); }
        .stat-name { font-size: 0.68rem; color: var(--text-muted); text-transform: uppercase; font-family: var(--font-mono); }
        .stat-val { font-size: 0.92rem; font-weight: 700; color: #fff; margin-top: 1px; font-family: var(--font-mono); }

        /* 6-AXIS SYNC MOVE GRID */
        .sync-grid { display: grid; grid-template-columns: repeat(6, 1fr); gap: 6px; }
        .sync-col { display: flex; flex-direction: column; align-items: center; gap: 3px; }
        .sync-col label { font-size: 0.72rem; font-weight: 700; color: var(--accent); font-family: var(--font-mono); }
        .sync-col input { text-align: center; padding: 6px 2px; font-size: 0.85rem; font-family: var(--font-mono); }

        /* KINEMATICS IK GRID */
        .ik-grid { display: grid; grid-template-columns: repeat(3, 1fr); gap: 8px; }
        .ik-box { background: var(--surface-2); padding: 10px; border-radius: 6px; border: 1px solid var(--border); display: flex; flex-direction: column; gap: 4px; }
        .ik-box label { font-size: 0.75rem; font-weight: 700; color: var(--text-dim); }

        /* CARTESIAN JOG GRID */
        .jog-pad-grid {
            display: grid; grid-template-columns: repeat(3, 1fr); gap: 6px; margin-top: 8px;
        }

        /* 3D CANVAS VIEW */
        .canvas-container {
            position: relative; width: 100%; height: 260px; background: #060910; border-radius: 8px; border: 1px solid var(--border); overflow: hidden;
        }
        #arm3d-canvas { width: 100%; height: 100%; cursor: grab; }
        #arm3d-canvas:active { cursor: grabbing; }

        /* TABLE */
        table { width: 100%; border-collapse: collapse; font-size: 0.8rem; font-family: var(--font-mono); }
        th, td { padding: 6px 8px; text-align: left; border-bottom: 1px solid var(--border); }
        th { color: var(--text-muted); font-weight: 600; }

        /* LOG CONSOLE */
        .log-box {
            background: #060910; border: 1px solid var(--border); border-radius: 6px; padding: 8px;
            font-family: var(--font-mono); font-size: 0.72rem; height: 95px; overflow-y: auto; color: #7ee787;
        }

        /* TOAST NOTIFICATION */
        .toast-container { position: fixed; bottom: 16px; right: 16px; z-index: 1000; display: flex; flex-direction: column; gap: 6px; }
        .toast {
            background: #1c2738; border: 1px solid var(--accent); color: #fff; padding: 8px 12px; border-radius: 6px;
            font-size: 0.8rem; box-shadow: 0 4px 12px rgba(0,0,0,0.5); opacity: 0; transform: translateY(10px);
            transition: all 0.2s ease;
        }
        .toast.show { opacity: 1; transform: translateY(0); }
    </style>
</head>
<body>

<div class="container">
    <!-- HEADER -->
    <header>
        <div class="header-title">
            <span class="status-dot"></span>
            <span>ESP32-S3 6-Axis Motion Dashboard</span>
            <span style="font-size: 0.7rem; color: var(--text-muted); font-family: var(--font-mono);" id="fw-ver-tag">v2.2.0-PRO</span>
        </div>
        <div class="header-controls">
            <span id="badge-heap" class="badge" style="background: rgba(255,255,255,0.06); color:#cbd5e1;">RAM: --</span>
            <span id="badge-wifi" class="badge badge-unhomed">Wi-Fi: ...</span>
            <span id="badge-global-home" class="badge badge-unhomed">Chưa Home Hết</span>
            <button class="btn btn-danger" onclick="sendStopAll()" style="padding: 5px 11px; font-size: 0.78rem;">⛔ STOP ALL</button>
        </div>
    </header>

    <!-- ALERT BANNER FOR DIRECTION RUNAWAY -->
    <div id="alert-runaway" class="alert-banner">
        <span>⚠️</span>
        <span id="alert-runaway-msg">CẢNH BÁO: Động cơ quay làm sai số tăng lên (ngược chiều cảm biến)! Đã tự động dừng an toàn.</span>
    </div>

    <!-- 6-AXIS SUMMARY OVERVIEW -->
    <div class="overview-grid" id="axis-overview-grid">
        <!-- Dynamic J1..J6 summary cards -->
    </div>

    <!-- NAVIGATION TABS -->
    <div class="tab-bar">
        <button class="tab-btn active" onclick="selectTab(0)">[1] Joint 1 (M0)</button>
        <button class="tab-btn" onclick="selectTab(1)">[2] Joint 2 (M1)</button>
        <button class="tab-btn" onclick="selectTab(2)">[3] Joint 3 (M2)</button>
        <button class="tab-btn" onclick="selectTab(3)">[4] Joint 4 (M3)</button>
        <button class="tab-btn" onclick="selectTab(4)">[5] Joint 5 (M4)</button>
        <button class="tab-btn" onclick="selectTab(5)">[6] Joint 6 (M5)</button>
        <button class="tab-btn" onclick="selectTab(6)">🌐 Đồng Bộ 6 Trục</button>
        <button class="tab-btn" onclick="selectTab(7)">🦾 Không Gian IK & 3D Arm</button>
        <button class="tab-btn" onclick="selectTab(8)">🎬 Quỹ Đạo Waypoints</button>
        <button class="tab-btn" onclick="selectTab(9)">🔍 Chẩn Đoán Phần Cứng</button>
    </div>

    <!-- TAB 0..5: SINGLE JOINT CONTROLLER VIEW -->
    <div class="grid-layout" id="panel-joint-detail">
        <!-- LEFT COLUMN: DIAL & POSITION CONTROL + MANUAL CONTROL -->
        <div style="display: flex; flex-direction: column; gap: 12px;">
            <div class="card">
                <div class="card-title">
                    <span id="lbl-active-joint">🧭 Joint 1 - Góc Vị Trí</span>
                    <div style="display:flex; gap:6px; align-items:center;">
                        <span id="state-uart" class="badge badge-online">UART OK</span>
                        <span id="state-motion" class="badge badge-online">ĐỨNG YÊN</span>
                    </div>
                </div>

                <!-- GAUGE SVG WITH TICK MARKS & LABELS -->
                <div class="gauge-wrapper">
                    <svg id="gauge" class="gauge-svg" viewBox="0 0 200 200">
                        <circle cx="100" cy="100" r="82" fill="none" stroke="#162030" stroke-width="12" />
                        <g stroke="#2d3b52" stroke-width="1.5">
                            <line x1="100" y1="18" x2="100" y2="28" stroke="#388bfd" stroke-width="2.5" />
                            <line x1="182" y1="100" x2="172" y2="100" />
                            <line x1="100" y1="182" x2="100" y2="172" />
                            <line x1="18" y1="100" x2="28" y2="100" />
                            <line x1="141" y1="29" x2="135" y2="39" />
                            <line x1="171" y1="59" x2="161" y2="65" />
                            <line x1="171" y1="141" x2="161" y2="135" />
                            <line x1="141" y1="171" x2="135" y2="161" />
                            <line x1="59" y1="171" x2="65" y2="161" />
                            <line x1="29" y1="141" x2="39" y2="135" />
                            <line x1="29" y1="59" x2="39" y2="65" />
                            <line x1="59" y1="29" x2="65" y2="39" />
                        </g>
                        <!-- Target pointer -->
                        <line id="pointer-target" x1="100" y1="100" x2="100" y2="28" stroke="rgba(56, 139, 253, 0.65)" stroke-width="2.5" stroke-dasharray="3,3" />
                        <!-- Measured pointer -->
                        <line id="pointer-current" x1="100" y1="100" x2="100" y2="24" stroke="#2ea043" stroke-width="3.5" stroke-linecap="round" />
                        <circle cx="100" cy="100" r="6" fill="#2ea043" />
                    </svg>

                    <div class="gauge-readout">
                        <div id="disp-angle" class="main-angle">0.00°</div>
                        <div id="disp-sub" class="target-angle">Tgt: 0.00° | Err: 0.00°</div>
                    </div>
                </div>

                <!-- GOTO ANGLE CONTROL -->
                <div class="input-group">
                    <input type="number" id="input-angle" placeholder="Góc đích (°)" step="0.5" value="0">
                    <button class="btn btn-primary" onclick="sendGoto()">Đến Góc</button>
                </div>
                <input type="range" id="slider-angle" min="-180" max="180" value="0" step="1" oninput="onSliderChange(this.value)">

                <!-- QUICK PRESETS -->
                <div style="font-size: 0.72rem; color: var(--text-muted);">Vị trí nhanh:</div>
                <div class="btn-grid">
                    <button class="btn btn-accent" onclick="setAngle(0)">0° (Home)</button>
                    <button class="btn" onclick="setAngle(45)">+45°</button>
                    <button class="btn" onclick="setAngle(-45)">-45°</button>
                    <button class="btn" onclick="setAngle(90)">+90°</button>
                    <button class="btn" onclick="setAngle(-90)">-90°</button>
                    <button class="btn" onclick="setAngle(180)">180°</button>
                </div>

                <!-- JOG BUTTONS -->
                <div style="font-size: 0.72rem; color: var(--text-muted);">Nhích góc (Jog):</div>
                <div class="btn-grid">
                    <button class="btn" onclick="jog(-10)">-10°</button>
                    <button class="btn" onclick="jog(-1)">-1°</button>
                    <button class="btn" onclick="jog(-0.1)">-0.1°</button>
                    <button class="btn" onclick="jog(0.1)">+0.1°</button>
                    <button class="btn" onclick="jog(1)">+1°</button>
                    <button class="btn" onclick="jog(10)">+10°</button>
                </div>

                <!-- EMERGENCY STOP CURRENT -->
                <button class="btn btn-stop" onclick="sendStopActive()">⛔ DỪNG TRỤC NÀY (ESC)</button>
            </div>

            <!-- MANUAL MOTOR CONTROL CARD -->
            <div class="card">
                <div class="card-title">
                    <span>🎛️ Điều Khiển Trực Tiếp (Manual Steps)</span>
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
                <div style="font-size: 0.78rem; font-weight: 600; color: var(--text-dim);">Quay theo số bước (Step Control):</div>
                <div class="input-group">
                    <input type="number" id="input-raw-steps" placeholder="Số bước" value="200" min="1" step="50">
                    <button class="btn btn-accent" onclick="sendMotorStep('ccw')">⟲ CCW</button>
                    <button class="btn btn-primary" onclick="sendMotorStep('cw')">CW ⟳</button>
                </div>

                <!-- QUICK STEP PRESETS -->
                <div style="font-size: 0.72rem; color: var(--text-muted);">Bước nhanh:</div>
                <div class="btn-grid">
                    <button class="btn" onclick="quickStep('ccw', 50)">⟲ 50</button>
                    <button class="btn" onclick="quickStep('ccw', 200)">⟲ 200</button>
                    <button class="btn" onclick="quickStep('ccw', 1600)">⟲ 1600</button>
                    <button class="btn" onclick="quickStep('cw', 50)">50 ⟳</button>
                    <button class="btn" onclick="quickStep('cw', 200)">200 ⟳</button>
                    <button class="btn" onclick="quickStep('cw', 1600)">1600 ⟳</button>
                </div>

                <!-- CONTINUOUS RUN -->
                <div style="font-size: 0.78rem; font-weight: 600; color: var(--text-dim); margin-top: 2px;">Quay liên tục (Continuous):</div>
                <div style="display: grid; grid-template-columns: 1fr 1fr 1fr; gap: 6px;">
                    <button class="btn btn-accent" onclick="sendMotorRun('ccw')">⟲ Chạy CCW</button>
                    <button class="btn btn-danger" onclick="sendStopActive()">⏹ Dừng</button>
                    <button class="btn btn-primary" onclick="sendMotorRun('cw')">Chạy CW ⟳</button>
                </div>
            </div>
        </div>

        <!-- RIGHT COLUMN: ACTIONS, STATS & SETTINGS -->
        <div style="display: flex; flex-direction: column; gap: 12px;">
            <!-- HOMING & ZEROING & CALIB -->
            <div class="card">
                <div class="card-title">🎯 Tác Vụ Chuẩn Hóa Khớp</div>
                <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 6px;">
                    <button class="btn btn-accent" onclick="sendHome()" style="padding: 9px;">
                        <span>🎯</span> Homing Cung Lớn
                    </button>
                    <button class="btn btn-accent" onclick="sendZeroHere()" style="padding: 9px;">
                        <span>📍</span> Đặt Gốc (Zero)
                    </button>
                </div>
                <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 6px;">
                    <button class="btn btn-accent" onclick="sendCalib()" style="padding: 8px; font-size: 0.78rem;">
                        <span>🔄</span> Auto Calib LUT
                    </button>
                    <button class="btn btn-danger" onclick="sendCalibClear()" style="padding: 8px; font-size: 0.78rem;">
                        Xóa LUT Calib
                    </button>
                </div>
            </div>

            <!-- LIVE STATS -->
            <div class="card">
                <div class="card-title">📊 Thông Số Hoạt Động</div>
                <div class="stat-grid">
                    <div class="stat-item">
                        <div class="stat-name">AS5600 Cảm Biến</div>
                        <div id="stat-sensor" class="stat-val" style="color:#3fb950;">OK</div>
                    </div>
                    <div class="stat-item">
                        <div class="stat-name">Giao Tiếp TMC2209</div>
                        <div id="stat-uart-val" class="stat-val">OK (v0x21)</div>
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
                        <div class="stat-name">Dòng RMS TMC2209</div>
                        <div id="stat-curr" class="stat-val">800 mA</div>
                    </div>
                    <div class="stat-item">
                        <div class="stat-name">Tốc Độ Đích / Thực</div>
                        <div id="stat-speed" class="stat-val">400 us</div>
                    </div>
                    <div class="stat-item">
                        <div class="stat-name">Tỉ Số Giảm Tốc</div>
                        <div id="stat-gear" class="stat-val">6.00 : 1</div>
                    </div>
                </div>
            </div>

            <!-- MOTOR SETTINGS -->
            <div class="card">
                <div class="card-title">⚙️ Cấu Hình Khớp & Đảo Chiều</div>
                
                <div class="switch-row">
                    <div>
                        <div class="switch-label">Đảo Chiều Motor (Invert)</div>
                        <div class="switch-sub">Bật nếu động cơ quay làm sai số tăng lên</div>
                    </div>
                    <label class="toggle">
                        <input type="checkbox" id="chk-invert" onchange="sendConfig()">
                        <span class="slider-round"></span>
                    </label>
                </div>

                <div class="switch-row">
                    <div>
                        <div class="switch-label">Giữ Vòng Kín (Closed-Loop Hold)</div>
                        <div class="switch-sub">Tự động chống vặn lệch với Schmitt deadband</div>
                    </div>
                    <label class="toggle">
                        <input type="checkbox" id="chk-hold" onchange="sendConfig()">
                        <span class="slider-round"></span>
                    </label>
                </div>

                <div style="display: flex; flex-direction: column; gap: 3px; margin-top: 3px;">
                    <div style="display:flex; justify-content:space-between; font-size:0.78rem;">
                        <span>Tốc độ xung đích: <b id="lbl-speed" style="font-family:var(--font-mono);">400 us (2500 Hz)</b></span>
                    </div>
                    <input type="range" id="rng-speed" min="150" max="1500" step="50" value="400" onchange="sendConfig()">
                </div>

                <div style="display: flex; flex-direction: column; gap: 3px; margin-top: 3px;">
                    <div style="display:flex; justify-content:space-between; font-size:0.78rem;">
                        <span>Dòng RMS: <b id="lbl-curr" style="font-family:var(--font-mono);">800 mA</b></span>
                    </div>
                    <input type="range" id="rng-curr" min="200" max="1400" step="50" value="800" onchange="sendConfig()">
                </div>
            </div>

            <!-- WI-FI CARD -->
            <div class="card">
                <div class="card-title">
                    <span>📶 Cài Đặt Wi-Fi</span>
                    <button class="btn" style="padding: 3px 8px; font-size: 0.72rem;" onclick="scanWifi()">🔍 Quét Mạng</button>
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
                <div style="font-size: 0.78rem; font-weight: 600; margin-bottom: 3px; color: var(--text-dim);">Nhật Ký Lệnh:</div>
                <div id="log-box" class="log-box">Hệ thống 6 trục đã khởi tạo...</div>
            </div>
        </div>
    </div>

    <!-- TAB 6: 6-AXIS SYNCHRONIZED MOVE PANEL -->
    <div class="card" id="panel-sync-move" style="display: none;">
        <div class="card-title">🌐 Điều Khiển Đồng Bộ 6 Trục (Synchronized Arrival)</div>
        <div style="font-size: 0.82rem; color: var(--text-dim);">
            Nhập góc đích cho 6 khớp (Độ Encoder). Bộ điều phối sẽ tính toán co giãn vận tốc để tất cả 6 trục hoàn thành cùng một thời điểm mà không bị méo quỹ đạo.
        </div>

        <div class="sync-grid" style="margin-top: 10px;">
            <div class="sync-col"><label>J1 (°)</label><input type="number" id="sync-j0" value="0" step="1"></div>
            <div class="sync-col"><label>J2 (°)</label><input type="number" id="sync-j1" value="0" step="1"></div>
            <div class="sync-col"><label>J3 (°)</label><input type="number" id="sync-j2" value="0" step="1"></div>
            <div class="sync-col"><label>J4 (°)</label><input type="number" id="sync-j3" value="0" step="1"></div>
            <div class="sync-col"><label>J5 (°)</label><input type="number" id="sync-j4" value="0" step="1"></div>
            <div class="sync-col"><label>J6 (°)</label><input type="number" id="sync-j5" value="0" step="1"></div>
        </div>

        <div style="display: flex; gap: 8px; align-items: center; margin-top: 8px; flex-wrap: wrap;">
            <button class="btn btn-accent" onclick="copyCurrentPose()" style="font-size:0.8rem;">📋 Lấy Góc Hiện Tại</button>
            <div style="display: flex; gap: 6px; align-items: center;">
                <span style="font-size: 0.78rem; color: var(--text-dim);">Thời gian (s):</span>
                <input type="number" id="sync-time" placeholder="Auto" step="0.1" min="0" value="0" style="width: 80px;">
            </div>
            <button class="btn btn-primary" onclick="sendSyncMove()" style="flex: 1; padding: 10px 14px; font-size: 0.95rem;">
                🚀 Bắt Đầu Quay Đồng Bộ 6 Trục
            </button>
            <button class="btn btn-danger" onclick="sendStopAll()" style="padding: 10px 16px;">
                ⛔ DỪNG TẤT CẢ
            </button>
        </div>

        <!-- PRESET POSES -->
        <div style="font-size: 0.78rem; color: var(--text-muted); margin-top: 10px;">Tư thế mẫu (Standard DH Presets):</div>
        <div class="btn-grid">
            <button class="btn" onclick="applyPosePreset([0,0,0,0,0,0])">Home (0°)</button>
            <button class="btn" onclick="applyPosePreset([30,45,-30,0,60,0])">Test 2 Pose</button>
            <button class="btn" onclick="applyPosePreset([0,30,30,0,-45,0])">Reach / Inspect</button>
            <button class="btn" onclick="applyPosePreset([45,45,20,0,-65,0])">Pick Left</button>
            <button class="btn" onclick="applyPosePreset([-45,45,20,0,-65,0])">Place Right</button>
        </div>
    </div>

    <!-- TAB 7: CARTESIAN INVERSE KINEMATICS (IK) & 3D ARM VISUALIZER PANEL -->
    <div class="card" id="panel-cartesian-ik" style="display: none;">
        <div class="card-title">🦾 Điều Khiển Không Gian Descartes (Standard DH 6-DOF IK)</div>
        <div style="font-size: 0.82rem; color: var(--text-dim);">
            Điều khiển vị trí đầu gắp TCP (Tool Center Point: D6_TOOL=20mm) trong không gian 3D theo bảng DH chuẩn (d1=139, a2=138, a3=88, d4=126 mm).
        </div>

        <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 12px; margin-top: 6px;">
            <!-- LEFT: CARTESIAN COORDINATES & DIRECT JOG PAD -->
            <div style="display:flex; flex-direction:column; gap:8px;">
                <div class="ik-grid">
                    <div class="ik-box">
                        <label>X (mm)</label>
                        <input type="number" id="ik-x" value="126" step="5">
                    </div>
                    <div class="ik-box">
                        <label>Y (mm)</label>
                        <input type="number" id="ik-y" value="0" step="5">
                    </div>
                    <div class="ik-box">
                        <label>Z (mm)</label>
                        <input type="number" id="ik-z" value="385" step="5">
                    </div>
                    <div class="ik-box">
                        <label>Roll (°)</label>
                        <input type="number" id="ik-roll" value="0" step="5">
                    </div>
                    <div class="ik-box">
                        <label>Pitch (°)</label>
                        <input type="number" id="ik-pitch" value="0" step="5">
                    </div>
                    <div class="ik-box">
                        <label>Yaw (°)</label>
                        <input type="number" id="ik-yaw" value="0" step="5">
                    </div>
                </div>

                <div style="display: flex; gap: 6px; align-items: center; flex-wrap: wrap;">
                    <div style="display: flex; gap: 4px; align-items: center;">
                        <span style="font-size: 0.75rem; color: var(--text-dim);">T (s):</span>
                        <input type="number" id="ik-time" value="2.0" step="0.5" min="0.5" style="width: 65px;">
                    </div>
                    <button class="btn btn-primary" onclick="sendIKGoto()" style="flex: 1; padding: 8px 12px;">
                        🎯 Đến Tọa Độ TCP
                    </button>
                    <button class="btn btn-danger" onclick="sendStopAll()" style="padding: 8px 12px;">
                        ⛔ DỪNG
                    </button>
                </div>

                <!-- CARTESIAN JOGGING CONTROLLER -->
                <div style="background: var(--surface-2); padding: 10px; border-radius: 8px; border: 1px solid var(--border); margin-top: 4px;">
                    <div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:6px;">
                        <span style="font-size:0.78rem; font-weight:700; color:var(--text-dim);">🕹️ Bàn Phím Nhích Tọa Độ (Cartesian Jog Pad):</span>
                        <div style="display:flex; gap:4px;">
                            <button class="btn" style="padding:2px 6px; font-size:0.7rem;" onclick="setJogStep(1)">1mm</button>
                            <button class="btn btn-accent" id="btn-jog-5" style="padding:2px 6px; font-size:0.7rem;" onclick="setJogStep(5)">5mm</button>
                            <button class="btn" style="padding:2px 6px; font-size:0.7rem;" onclick="setJogStep(20)">20mm</button>
                        </div>
                    </div>
                    <div class="jog-pad-grid">
                        <button class="btn" onclick="jogCartesian('x', -1)">⬅ -X</button>
                        <button class="btn" onclick="jogCartesian('z', 1)">⬆ +Z</button>
                        <button class="btn" onclick="jogCartesian('x', 1)">+X ➡</button>
                        <button class="btn" onclick="jogCartesian('y', 1)">⬈ +Y (Tới)</button>
                        <button class="btn" onclick="jogCartesian('z', -1)">⬇ -Z</button>
                        <button class="btn" onclick="jogCartesian('y', -1)">⬋ -Y (Lui)</button>
                    </div>
                </div>
            </div>

            <!-- RIGHT: INTERACTIVE 3D PERSPECTIVE CANVAS VISUALIZER -->
            <div style="display:flex; flex-direction:column; gap:6px;">
                <div style="display:flex; justify-content:space-between; align-items:center;">
                    <span style="font-size:0.78rem; font-weight:700; color:var(--text-dim);">🎨 Mô Hình 3D Isometric Cánh Tay (Kéo chuột xoay):</span>
                    <span id="tcp-coords-tag" style="font-size:0.72rem; font-family:var(--font-mono); color:var(--accent);">TCP: (126.0, 0.0, 385.0) mm</span>
                </div>
                <div class="canvas-container">
                    <canvas id="arm3d-canvas"></canvas>
                </div>
            </div>
        </div>
    </div>

    <!-- TAB 8: TRAJECTORY WAYPOINTS SEQUENCER -->
    <div class="card" id="panel-waypoints" style="display: none;">
        <div class="card-title">
            <span>🎬 Trình Phát Quỹ Đạo Waypoints (Trajectory Player)</span>
            <span id="seq-badge" class="badge badge-unhomed">DỪNG</span>
        </div>
        <div style="font-size: 0.82rem; color: var(--text-dim);">
            Lưu các điểm tư thế cánh tay và lập trình chuỗi chuyển động tuần tự tự động.
        </div>

        <div style="display: flex; gap: 8px; align-items: center; margin-top: 6px; flex-wrap: wrap;">
            <input type="text" id="wp-name" placeholder="Tên điểm (vd: Pick-1)" style="max-width: 160px;">
            <div style="display: flex; gap: 4px; align-items: center;">
                <span style="font-size:0.75rem; color:var(--text-muted);">T (s):</span>
                <input type="number" id="wp-time" value="2.0" step="0.5" style="width: 60px;">
            </div>
            <div style="display: flex; gap: 4px; align-items: center;">
                <span style="font-size:0.75rem; color:var(--text-muted);">Dwell (ms):</span>
                <input type="number" id="wp-dwell" value="500" step="100" style="width: 75px;">
            </div>
            <button class="btn btn-accent" onclick="saveCurrentPoseAsWaypoint()">➕ Lưu Vị Trí Hiện Tại</button>
        </div>

        <!-- WAYPOINTS TABLE -->
        <div style="max-height: 220px; overflow-y: auto; margin-top: 6px; border: 1px solid var(--border); border-radius: 6px;">
            <table>
                <thead>
                    <tr>
                        <th>#</th>
                        <th>Tên</th>
                        <th>Góc Khớp [J1..J6]</th>
                        <th>T (s)</th>
                        <th>Dwell</th>
                    </tr>
                </thead>
                <tbody id="wp-table-body">
                    <tr><td colspan="5" style="text-align:center; color:var(--text-muted);">Chưa có điểm nào được lưu</td></tr>
                </tbody>
            </table>
        </div>

        <div style="display: flex; gap: 8px; align-items: center; margin-top: 8px; flex-wrap: wrap;">
            <label style="display:flex; align-items:center; gap:6px; font-size:0.84rem; cursor:pointer;">
                <input type="checkbox" id="wp-loop">
                <span>Lặp vô tận (Loop)</span>
            </label>
            <button class="btn btn-primary" onclick="startWaypointSequence()">▶ Bắt Đầu Chạy</button>
            <button class="btn btn-accent" onclick="pauseWaypointSequence()">⏸ Tạm Dừng</button>
            <button class="btn btn-danger" onclick="stopWaypointSequence()">⏹ Dừng Hẳn</button>
            <button class="btn" onclick="clearAllWaypoints()" style="margin-left:auto;">🗑 Xóa Hết</button>
        </div>
    </div>

    <!-- TAB 9: HARDWARE TELEMETRY & DIAGNOSTICS -->
    <div class="card" id="panel-hardware-diag" style="display: none;">
        <div class="card-title">🔍 Chẩn Đoán Chi Tiết Phần Cứng (TMC2209 & AS5600)</div>
        
        <div style="display: flex; justify-content: space-between; align-items: center; flex-wrap: wrap; gap: 8px; margin-bottom: 6px;">
            <button class="btn btn-accent" onclick="fetchStatus(true)">🔄 Làm Mới Dữ Liệu</button>
            <button class="btn btn-danger" onclick="rebootSystem()">⚠️ Khởi Động Lại ESP32 (Reboot)</button>
        </div>

        <div id="diag-cards-container" style="display: grid; grid-template-columns: repeat(auto-fit, minmax(320px, 1fr)); gap: 10px;">
            <!-- Rendered dynamically -->
        </div>
    </div>
</div>

<!-- TOAST CONTAINER -->
<div class="toast-container" id="toasts"></div>

<script>
    let activeAxis = 0;
    let currentTab = 0;
    let latestData = null;
    let cartesianJogStep = 5.0;

    // 3D Visualizer Orbit State
    let camYaw = -45 * Math.PI / 180;
    let camPitch = 30 * Math.PI / 180;
    let isDragging = false;
    let prevMouseX = 0, prevMouseY = 0;

    function toast(msg) {
        const container = document.getElementById('toasts');
        const t = document.createElement('div');
        t.className = 'toast';
        t.innerText = msg;
        container.appendChild(t);
        setTimeout(() => t.classList.add('show'), 10);
        setTimeout(() => {
            t.classList.remove('show');
            setTimeout(() => t.remove(), 200);
        }, 2500);
    }

    function log(msg) {
        const box = document.getElementById('log-box');
        if (!box) return;
        const d = new Date().toLocaleTimeString();
        box.innerHTML = `[${d}] ${msg}<br>` + box.innerHTML;
    }

    function selectTab(idx) {
        currentTab = idx;
        const tabs = document.querySelectorAll('.tab-btn');
        tabs.forEach((t, i) => {
            if (i === idx) t.classList.add('active');
            else t.classList.remove('active');
        });

        document.getElementById('panel-joint-detail').style.display = (idx <= 5) ? 'grid' : 'none';
        document.getElementById('panel-sync-move').style.display   = (idx === 6) ? 'flex' : 'none';
        document.getElementById('panel-cartesian-ik').style.display = (idx === 7) ? 'flex' : 'none';
        document.getElementById('panel-waypoints').style.display    = (idx === 8) ? 'flex' : 'none';
        document.getElementById('panel-hardware-diag').style.display = (idx === 9) ? 'flex' : 'none';

        if (idx <= 5) {
            activeAxis = idx;
            document.getElementById('lbl-active-joint').innerText = `🧭 Joint ${activeAxis + 1} (M${activeAxis}) - Góc Vị Trí`;
            fetchStatus(true);
        } else if (idx === 7) {
            init3DCanvas();
            render3DArm();
        } else if (idx === 8) {
            refreshWaypointsList();
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
                    const isAct = (i === activeAxis && currentTab <= 5) ? 'active' : '';
                    const okTag = ax.as5600_ok ? '<span style="color:#3fb950;">●</span>' : '<span style="color:#f85149;">●</span>';
                    const uartTag = ax.uart_ok ? '' : '<span style="color:#f85149; font-size:0.62rem;">[NO UART]</span>';
                    const runTag = ax.isRunning ? '<span style="color:#58a6ff;">QUAY</span>' : '<span style="color:#8b949e;">TĨNH</span>';
                    html += `
                    <div class="axis-card ${isAct}" onclick="selectTab(${i})">
                        <div class="axis-head">
                            <span class="axis-name">JOINT ${i + 1} ${okTag} ${uartTag}</span>
                            <span style="font-size:0.7rem; font-weight:700;">${runTag}</span>
                        </div>
                        <div class="axis-deg">${ax.currentAngle.toFixed(1)}°</div>
                        <div class="axis-sub">Tgt: ${ax.targetAngle.toFixed(1)}° | Err: ${ax.error.toFixed(1)}°</div>
                    </div>`;
                });
                grid.innerHTML = html;
            }

            // 2. Badges & System Telemetry
            document.getElementById('badge-heap').innerText = `RAM: ${(latestData.free_heap / 1024).toFixed(0)}KB`;
            
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

            // 3. Update Active Joint View
            if (latestData.axes && latestData.axes[activeAxis]) {
                const ax = latestData.axes[activeAxis];
                document.getElementById('disp-angle').innerText = ax.currentAngle.toFixed(2) + '°';
                document.getElementById('disp-sub').innerText = `Tgt: ${ax.targetAngle.toFixed(2)}° | Err: ${ax.error.toFixed(2)}°`;
                
                updatePointer(ax.currentAngle, false);
                updatePointer(ax.targetAngle, true);

                // Runaway alert banner
                const alertRunaway = document.getElementById('alert-runaway');
                if (ax.runaway_error) {
                    alertRunaway.style.display = 'flex';
                    document.getElementById('alert-runaway-msg').innerText = 
                        `⚠️ CẢNH BÁO JOINT ${activeAxis + 1}: Động cơ quay làm sai số tăng lên (ngược chiều cảm biến)! Đã dừng an toàn. Hãy gạt công tắc [Đảo Chiều Motor (Invert)]!`;
                } else {
                    alertRunaway.style.display = 'none';
                }

                // Status Badges
                const uartBadge = document.getElementById('state-uart');
                if (uartBadge) {
                    uartBadge.className = ax.uart_ok ? "badge badge-online" : "badge badge-danger";
                    uartBadge.innerText = ax.uart_ok ? `UART OK (0x${ax.driver_version.toString(16).toUpperCase()})` : "MẤT UART";
                }

                const motionBadge = document.getElementById('state-motion');
                if (ax.isRunning) {
                    motionBadge.innerText = "ĐANG QUAY (S-CURVE)";
                    motionBadge.style.background = "rgba(56, 139, 253, 0.15)";
                    motionBadge.style.color = "#58a6ff";
                } else {
                    motionBadge.innerText = ax.inDeadband ? "GIỮ GÓC (DEADBAND)" : "ĐỨNG YÊN";
                    motionBadge.style.background = "rgba(46, 160, 67, 0.15)";
                    motionBadge.style.color = "#3fb950";
                }

                const driverBadge = document.getElementById('state-driver');
                if (driverBadge) {
                    driverBadge.className = ax.driver_enabled ? "badge badge-online" : "badge badge-unhomed";
                    driverBadge.innerText = ax.driver_enabled ? "DRIVER BẬT" : "TRỤC TỰ DO";
                }

                document.getElementById('stat-sensor').innerText = ax.as5600_ok ? "OK" : "LỖI I2C";
                document.getElementById('stat-sensor').style.color = ax.as5600_ok ? "#3fb950" : "#f85149";
                
                const statUart = document.getElementById('stat-uart-val');
                if (statUart) {
                    statUart.innerText = ax.uart_ok ? `OK (v0x${ax.driver_version.toString(16).toUpperCase()})` : `LỖI (v0x${ax.driver_version.toString(16).toUpperCase()})`;
                    statUart.style.color = ax.uart_ok ? "#3fb950" : "#f85149";
                }

                document.getElementById('stat-mag').innerText = `${ax.agc} / ${ax.magnitude} ${ax.magnet_optimal ? '✓' : '!'}`;
                document.getElementById('stat-raw').innerText = ax.rawAngle.toFixed(2) + '°';
                document.getElementById('stat-stroke').innerText = ax.totalStroke.toFixed(2) + '°';
                document.getElementById('stat-curr').innerText = ax.current + ' mA';
                document.getElementById('stat-speed').innerText = `${ax.speed} us (chạy ${ax.currentSpeed} us)`;
                document.getElementById('stat-gear').innerText = ax.gearRatio.toFixed(2) + ' : 1';

                if (isManual) {
                    document.getElementById('chk-hold').checked = ax.closedLoopHold;
                    document.getElementById('chk-invert').checked = ax.dirInvert;
                    const chkDriver = document.getElementById('chk-driver-enable');
                    if (chkDriver) chkDriver.checked = ax.driver_enabled;
                    document.getElementById('rng-speed').value = ax.speed;
                    const freq = Math.round(1000000 / ax.speed);
                    document.getElementById('lbl-speed').innerText = `${ax.speed} us (${freq} Hz)`;
                    document.getElementById('rng-curr').value = ax.current;
                    document.getElementById('lbl-curr').innerText = ax.current + ' mA';
                }
            }

            // 4. Update 3D Arm Visualizer & TCP Coords
            if (currentTab === 7 && latestData.tcp && latestData.axes) {
                const tag = document.getElementById('tcp-coords-tag');
                if (tag) tag.innerText = `TCP: (${latestData.tcp.x.toFixed(1)}, ${latestData.tcp.y.toFixed(1)}, ${latestData.tcp.z.toFixed(1)}) mm`;
                render3DArm();
            }

            // 5. Update Waypoint Sequencer Status Badge
            const seqBadge = document.getElementById('seq-badge');
            if (seqBadge) {
                if (latestData.seq_running) {
                    seqBadge.className = "badge badge-online";
                    seqBadge.innerText = `ĐANG CHẠY: Điểm ${latestData.seq_idx + 1}/${latestData.seq_count}`;
                } else {
                    seqBadge.className = "badge badge-unhomed";
                    seqBadge.innerText = "DỪNG";
                }
            }

            // 6. Update Hardware Diagnostics Panel
            if (currentTab === 9) {
                renderHardwareDiagCards(latestData);
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
        toast(`[J${activeAxis + 1}] Đến góc: ${val}°`);
        await fetch(`/api/motor/goto?axis=${activeAxis}&angle=${val}`, { method: 'POST' });
    }

    function setAngle(deg) {
        document.getElementById('input-angle').value = deg;
        document.getElementById('slider-angle').value = deg;
        sendGoto();
    }

    async function jog(delta) {
        log(`[J${activeAxis + 1}] Nhích góc ${delta > 0 ? '+' : ''}${delta}°`);
        toast(`[J${activeAxis + 1}] Jog ${delta > 0 ? '+' : ''}${delta}°`);
        await fetch(`/api/motor/jog?axis=${activeAxis}&delta=${delta}`, { method: 'POST' });
    }

    async function sendStopActive() {
        log(`⛔ [J${activeAxis + 1}] DỪNG TRỤC`);
        toast(`⛔ Dừng Joint ${activeAxis + 1}`);
        await fetch(`/api/motor/stop?axis=${activeAxis}`, { method: 'POST' });
    }

    async function sendStopAll() {
        log(`⛔ DỪNG KHẨN CẤP TẤT CẢ 6 TRỤC!`);
        toast(`⛔ DỪNG KHẨN CẤP TẤT CẢ 6 TRỤC!`);
        await fetch(`/api/all/stop`, { method: 'POST' });
    }

    async function sendMotorStep(dir) {
        const steps = parseInt(document.getElementById('input-raw-steps').value) || 200;
        log(`[J${activeAxis + 1}] Quay Motor ${dir.toUpperCase()}: ${steps} bước`);
        toast(`[J${activeAxis + 1}] Step ${dir.toUpperCase()}: ${steps}`);
        await fetch(`/api/motor/step?axis=${activeAxis}&dir=${dir}&steps=${steps}`, { method: 'POST' });
    }

    async function quickStep(dir, steps) {
        document.getElementById('input-raw-steps').value = steps;
        log(`[J${activeAxis + 1}] Quay nhanh ${dir.toUpperCase()}: ${steps} bước`);
        toast(`[J${activeAxis + 1}] Quick Step ${dir.toUpperCase()}: ${steps}`);
        await fetch(`/api/motor/step?axis=${activeAxis}&dir=${dir}&steps=${steps}`, { method: 'POST' });
    }

    async function sendMotorRun(dir) {
        log(`[J${activeAxis + 1}] Bắt đầu quay liên tục ${dir.toUpperCase()}...`);
        toast(`[J${activeAxis + 1}] Run ${dir.toUpperCase()}`);
        await fetch(`/api/motor/run?axis=${activeAxis}&dir=${dir}`, { method: 'POST' });
    }

    async function toggleDriverEnable(enabled) {
        const en = enabled ? 1 : 0;
        log(`[J${activeAxis + 1}] ${enabled ? 'Bật cấp nguồn Driver' : 'Thả tự do động cơ (Free shaft)'}`);
        toast(`[J${activeAxis + 1}] ${enabled ? 'Driver Bật' : 'Trục tự do'}`);
        await fetch(`/api/motor/enable?axis=${activeAxis}&en=${en}`, { method: 'POST' });
    }

    async function sendHome() {
        log(`🎯 [J${activeAxis + 1}] Bắt đầu Homing Cung Lớn...`);
        toast(`🎯 Joint ${activeAxis + 1} Bắt đầu Homing...`);
        await fetch(`/api/motor/home?axis=${activeAxis}`, { method: 'POST' });
    }

    async function sendZeroHere() {
        if (!confirm(`Đặt vị trí hiện tại làm mốc 0.00° cho Joint ${activeAxis + 1}?`)) return;
        log(`📍 [J${activeAxis + 1}] Đặt mốc 0.00° tại vị trí hiện tại.`);
        toast(`📍 Joint ${activeAxis + 1} Set Home 0.00°`);
        await fetch(`/api/motor/zero?axis=${activeAxis}`, { method: 'POST' });
    }

    async function sendCalib() {
        log(`🔄 [J${activeAxis + 1}] Bắt đầu Auto Calibration 16 điểm...`);
        toast(`🔄 Joint ${activeAxis + 1} Auto Calib 16 điểm...`);
        await fetch(`/api/motor/calib?axis=${activeAxis}`, { method: 'POST' });
    }

    async function sendCalibClear() {
        if (!confirm(`Xóa bảng hiệu chuẩn Calib cho Joint ${activeAxis + 1}?`)) return;
        log(`[J${activeAxis + 1}] Xóa bảng hiệu chuẩn Calib`);
        toast(`Xóa Calib Joint ${activeAxis + 1}`);
        await fetch(`/api/motor/calib_clear?axis=${activeAxis}`, { method: 'POST' });
    }

    async function sendConfig() {
        const hold = document.getElementById('chk-hold').checked ? 1 : 0;
        const invert = document.getElementById('chk-invert').checked ? 1 : 0;
        const speed = document.getElementById('rng-speed').value;
        const curr = document.getElementById('rng-curr').value;

        const freq = Math.round(1000000 / speed);
        document.getElementById('lbl-speed').innerText = `${speed} us (${freq} Hz)`;
        document.getElementById('lbl-curr').innerText = curr + ' mA';

        await fetch(`/api/motor/settings?axis=${activeAxis}&hold=${hold}&invert=${invert}&speed=${speed}&curr=${curr}`, { method: 'POST' });
    }

    // 6-AXIS SYNCHRONIZED MOVE
    function copyCurrentPose() {
        if (!latestData || !latestData.axes) return;
        latestData.axes.forEach((ax, i) => {
            const input = document.getElementById(`sync-j${i}`);
            if (input) input.value = ax.currentAngle.toFixed(1);
        });
        toast('Đã sao chép góc hiện tại vào bảng');
    }

    function applyPosePreset(angles) {
        angles.forEach((ang, i) => {
            const input = document.getElementById(`sync-j${i}`);
            if (input) input.value = ang;
        });
        toast('Đã nạp góc tư thế mẫu');
    }

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
        toast(`🚀 Bắt đầu quay đồng bộ 6 trục!`);
        await fetch(`/api/all/goto?angles=${encodeURIComponent(anglesStr)}&time=${t}`, { method: 'POST' });
    }

    // CARTESIAN INVERSE KINEMATICS & JOGGING
    function setJogStep(step) {
        cartesianJogStep = step;
        toast(`Đã chọn bước nhích: ${step}mm`);
    }

    async function jogCartesian(axis, direction) {
        if (!latestData || !latestData.tcp) return;
        let newX = latestData.tcp.x;
        let newY = latestData.tcp.y;
        let newZ = latestData.tcp.z;

        if (axis === 'x') newX += direction * cartesianJogStep;
        if (axis === 'y') newY += direction * cartesianJogStep;
        if (axis === 'z') newZ += direction * cartesianJogStep;

        document.getElementById('ik-x').value = newX.toFixed(1);
        document.getElementById('ik-y').value = newY.toFixed(1);
        document.getElementById('ik-z').value = newZ.toFixed(1);

        log(`🕹️ [JOG TCP] ${axis.toUpperCase()} ${direction > 0 ? '+' : ''}${direction * cartesianJogStep}mm -> [${newX.toFixed(1)}, ${newY.toFixed(1)}, ${newZ.toFixed(1)}]`);
        const res = await fetch(`/api/ik/goto?x=${newX}&y=${newY}&z=${newZ}&roll=${latestData.tcp.roll}&pitch=${latestData.tcp.pitch}&yaw=${latestData.tcp.yaw}&time=0.8`, { method: 'POST' });
        if (!res.ok) toast('⚠️ Điểm ngoài tầm với hoặc giới hạn khớp!');
    }

    async function sendIKGoto() {
        const x = parseFloat(document.getElementById('ik-x').value) || 0;
        const y = parseFloat(document.getElementById('ik-y').value) || 0;
        const z = parseFloat(document.getElementById('ik-z').value) || 0;
        const roll = parseFloat(document.getElementById('ik-roll').value) || 0;
        const pitch = parseFloat(document.getElementById('ik-pitch').value) || 0;
        const yaw = parseFloat(document.getElementById('ik-yaw').value) || 0;
        const t = parseFloat(document.getElementById('ik-time').value) || 2.0;

        log(`🎯 Gửi tọa độ TCP: [X:${x}, Y:${y}, Z:${z}, R:${roll}, P:${pitch}, Y:${yaw}]`);
        toast(`🎯 Di chuyển TCP đến [${x}, ${y}, ${z}]`);
        const res = await fetch(`/api/ik/goto?x=${x}&y=${y}&z=${z}&roll=${roll}&pitch=${pitch}&yaw=${yaw}&time=${t}`, { method: 'POST' });
        if (!res.ok) {
            toast(`⚠️ Điểm ngoài tầm với hoặc giới hạn khớp!`);
        }
    }

    // 3D ISOMETRIC CANVAS VISUALIZER
    function init3DCanvas() {
        const canvas = document.getElementById('arm3d-canvas');
        if (!canvas) return;

        canvas.width = canvas.parentElement.clientWidth;
        canvas.height = canvas.parentElement.clientHeight;

        canvas.onmousedown = (e) => {
            isDragging = true;
            prevMouseX = e.clientX;
            prevMouseY = e.clientY;
        };
        window.onmousemove = (e) => {
            if (!isDragging) return;
            const dx = e.clientX - prevMouseX;
            const dy = e.clientY - prevMouseY;
            camYaw += dx * 0.01;
            camPitch -= dy * 0.01;
            if (camPitch > Math.PI / 2.2) camPitch = Math.PI / 2.2;
            if (camPitch < -Math.PI / 2.2) camPitch = -Math.PI / 2.2;
            prevMouseX = e.clientX;
            prevMouseY = e.clientY;
            render3DArm();
        };
        window.onmouseup = () => { isDragging = false; };
    }

    function project3D(x, y, z, cx, cy, scale) {
        // Rotate by camYaw around Z axis
        const cosY = Math.cos(camYaw), sinY = Math.sin(camYaw);
        const x1 = x * cosY - y * sinY;
        const y1 = x * sinY + y * cosY;

        // Rotate by camPitch around X axis
        const cosP = Math.cos(camPitch), sinP = Math.sin(camPitch);
        const y2 = y1 * cosP - z * sinP;
        const z2 = y1 * sinP + z * cosP;

        // Perspective / Isometric projection
        const px = cx + x1 * scale;
        const py = cy - z2 * scale;
        return { x: px, y: py, depth: y2 };
    }

    function render3DArm() {
        const canvas = document.getElementById('arm3d-canvas');
        if (!canvas) return;
        const ctx = canvas.getContext('2d');
        const w = canvas.width, h = canvas.height;
        ctx.clearRect(0, 0, w, h);

        const cx = w / 2, cy = h / 2 + 50;
        const scale = 0.55;

        // Draw Ground Grid
        ctx.strokeStyle = 'rgba(30, 42, 60, 0.8)';
        ctx.lineWidth = 1;
        for (let g = -200; g <= 200; g += 50) {
            const pA = project3D(g, -200, 0, cx, cy, scale);
            const pB = project3D(g, 200, 0, cx, cy, scale);
            ctx.beginPath(); ctx.moveTo(pA.x, pA.y); ctx.lineTo(pB.x, pB.y); ctx.stroke();

            const pC = project3D(-200, g, 0, cx, cy, scale);
            const pD = project3D(200, g, 0, cx, cy, scale);
            ctx.beginPath(); ctx.moveTo(pC.x, pC.y); ctx.lineTo(pD.x, pD.y); ctx.stroke();
        }

        if (!latestData || !latestData.axes) return;

        // Calculate Forward Kinematics 3D joint points
        const q1 = latestData.axes[0].currentAngle * (Math.PI / 180);
        const q2 = (latestData.axes[1].currentAngle - 90.0) * (Math.PI / 180);
        const q3 = latestData.axes[2].currentAngle * (Math.PI / 180);
        const q4 = latestData.axes[3].currentAngle * (Math.PI / 180);
        const q5 = latestData.axes[4].currentAngle * (Math.PI / 180);

        const d1 = 139, a2 = 138, a3 = 88, d4 = 126, d6 = 20;

        // P0 (Base)
        const p0 = { x: 0, y: 0, z: 0 };
        // P1 (Shoulder)
        const p1 = { x: 0, y: 0, z: d1 };
        // P2 (Elbow)
        const r2 = a2 * Math.cos(q2);
        const p2 = {
            x: r2 * Math.cos(q1),
            y: r2 * Math.sin(q1),
            z: d1 - a2 * Math.sin(q2)
        };
        // P3 (Wrist Center)
        const q23 = q2 + q3;
        const rw = a2 * Math.cos(q2) + a3 * Math.cos(q23) - d4 * Math.sin(q23);
        const zw = d1 - a2 * Math.sin(q2) - a3 * Math.sin(q23) - d4 * Math.cos(q23);
        const p3 = {
            x: rw * Math.cos(q1),
            y: rw * Math.sin(q1),
            z: zw
        };
        // P4 (TCP)
        const p4 = {
            x: (rw + d6) * Math.cos(q1),
            y: (rw + d6) * Math.sin(q1),
            z: zw
        };

        const pts = [
            project3D(p0.x, p0.y, p0.z, cx, cy, scale),
            project3D(p1.x, p1.y, p1.z, cx, cy, scale),
            project3D(p2.x, p2.y, p2.z, cx, cy, scale),
            project3D(p3.x, p3.y, p3.z, cx, cy, scale),
            project3D(p4.x, p4.y, p4.z, cx, cy, scale)
        ];

        // Draw Links
        ctx.lineCap = 'round';
        ctx.lineJoin = 'round';

        // Base Link
        ctx.strokeStyle = '#64748b'; ctx.lineWidth = 10;
        ctx.beginPath(); ctx.moveTo(pts[0].x, pts[0].y); ctx.lineTo(pts[1].x, pts[1].y); ctx.stroke();

        // Upper Arm
        ctx.strokeStyle = '#388bfd'; ctx.lineWidth = 8;
        ctx.beginPath(); ctx.moveTo(pts[1].x, pts[1].y); ctx.lineTo(pts[2].x, pts[2].y); ctx.stroke();

        // Forearm
        ctx.strokeStyle = '#58a6ff'; ctx.lineWidth = 6;
        ctx.beginPath(); ctx.moveTo(pts[2].x, pts[2].y); ctx.lineTo(pts[3].x, pts[3].y); ctx.stroke();

        // Wrist & Tool
        ctx.strokeStyle = '#f85149'; ctx.lineWidth = 4;
        ctx.beginPath(); ctx.moveTo(pts[3].x, pts[3].y); ctx.lineTo(pts[4].x, pts[4].y); ctx.stroke();

        // Draw Joint Spheres
        pts.forEach((p, idx) => {
            ctx.beginPath();
            ctx.arc(p.x, p.y, idx === 4 ? 5 : 6, 0, Math.PI * 2);
            ctx.fillStyle = idx === 4 ? '#ff7b72' : '#2ea043';
            ctx.fill();
            ctx.strokeStyle = '#ffffff';
            ctx.lineWidth = 1.5;
            ctx.stroke();
        });
    }

    // TRAJECTORY WAYPOINTS
    async function refreshWaypointsList() {
        try {
            const res = await fetch('/api/waypoint/list');
            const list = await res.json();
            const body = document.getElementById('wp-table-body');
            if (!body) return;

            if (list.length === 0) {
                body.innerHTML = '<tr><td colspan="5" style="text-align:center; color:var(--text-muted);">Chưa có điểm nào được lưu</td></tr>';
                return;
            }

            let html = '';
            list.forEach(w => {
                const jStr = w.joints.map(j => j.toFixed(1) + '°').join(', ');
                html += `<tr>
                    <td><b>${w.index + 1}</b></td>
                    <td>${w.name}</td>
                    <td>[${jStr}]</td>
                    <td>${w.time}s</td>
                    <td>${w.dwell}ms</td>
                </tr>`;
            });
            body.innerHTML = html;
        } catch (e) {
            console.error(e);
        }
    }

    async function saveCurrentPoseAsWaypoint() {
        const name = document.getElementById('wp-name').value.trim() || `WP-${Date.now() % 1000}`;
        const time = parseFloat(document.getElementById('wp-time').value) || 2.0;
        const dwell = parseInt(document.getElementById('wp-dwell').value) || 500;

        await fetch(`/api/waypoint/add?name=${encodeURIComponent(name)}&time=${time}&dwell=${dwell}`, { method: 'POST' });
        toast(`Đã lưu Waypoint: ${name}`);
        refreshWaypointsList();
    }

    async function startWaypointSequence() {
        const loop = document.getElementById('wp-loop').checked ? 1 : 0;
        await fetch(`/api/waypoint/start?loop=${loop}`, { method: 'POST' });
        toast('Bắt đầu chạy chuỗi Waypoints!');
    }

    async function pauseWaypointSequence() {
        await fetch(`/api/waypoint/pause`, { method: 'POST' });
        toast('Đã tạm dừng chuỗi Waypoints.');
    }

    async function stopWaypointSequence() {
        await fetch(`/api/waypoint/stop`, { method: 'POST' });
        toast('Đã dừng hẳn chuỗi Waypoints.');
    }

    async function clearAllWaypoints() {
        if (!confirm('Xóa tất cả các điểm Waypoints đã lưu?')) return;
        await fetch(`/api/waypoint/clear`, { method: 'POST' });
        toast('Đã xóa hết Waypoints.');
        refreshWaypointsList();
    }

    // HARDWARE DIAGNOSTICS
    function renderHardwareDiagCards(data) {
        const container = document.getElementById('diag-cards-container');
        if (!container || !data.axes) return;

        let html = '';
        data.axes.forEach((ax, i) => {
            const tempWarn = ax.over_temp_warn ? '<span style="color:#ff7b72;">[QUÁ NHIỆT PRE-WARN]</span>' : '';
            const tempCut = ax.over_temp ? '<span style="color:#ff7b72;">[NGẮT QUÁ NHIỆT]</span>' : '';
            const shortA = ax.short_gnd_a ? '<span style="color:#ff7b72;">[NGẮN MẠCH PHA A]</span>' : '';
            const shortB = ax.short_gnd_b ? '<span style="color:#ff7b72;">[NGẮN MẠCH PHA B]</span>' : '';
            const openA = ax.open_load_a ? '<span style="color:#d29922;">[HỞ MẠCH PHA A]</span>' : '';
            const openB = ax.open_load_b ? '<span style="color:#d29922;">[HỞ MẠCH PHA B]</span>' : '';

            html += `
            <div class="card" style="padding:10px;">
                <div class="card-title" style="font-size:0.85rem;">
                    <span>JOINT ${i + 1} Telemetry (M${i})</span>
                    <span class="badge ${ax.uart_ok ? 'badge-online' : 'badge-danger'}">UART 0x${ax.driver_version.toString(16).toUpperCase()}</span>
                </div>
                <div class="stat-grid" style="margin-top:4px;">
                    <div class="stat-item"><div class="stat-name">AS5600 I2C</div><div class="stat-val">${ax.as5600_ok ? 'OK' : 'LỖI'}</div></div>
                    <div class="stat-item"><div class="stat-name">Từ Trường AGC</div><div class="stat-val">${ax.agc} (${ax.magnet_optimal ? 'Tối Ưu' : 'Cảnh Báo'})</div></div>
                    <div class="stat-item"><div class="stat-name">Magnitude CORDIC</div><div class="stat-val">${ax.magnitude}</div></div>
                    <div class="stat-item"><div class="stat-name">StallGuard SG_RESULT</div><div class="stat-val">${ax.sg_result}</div></div>
                    <div class="stat-item"><div class="stat-name">Góc Tích Lũy Đa Vòng</div><div class="stat-val">${ax.accumAngle.toFixed(1)}° (${ax.turns} vòng)</div></div>
                    <div class="stat-item"><div class="stat-name">Tốc Độ Xung Hiện Tại</div><div class="stat-val">${ax.currentSpeed} us</div></div>
                </div>
                <div style="font-size:0.7rem; color:#ff7b72; margin-top:4px;">
                    ${tempWarn} ${tempCut} ${shortA} ${shortB} ${openA} ${openB}
                </div>
            </div>`;
        });
        container.innerHTML = html;
    }

    async function rebootSystem() {
        if (!confirm('Khởi động lại toàn bộ điều khiển ESP32?')) return;
        await fetch('/api/system/reboot', { method: 'POST' });
        toast('Đang khởi động lại ESP32...');
    }

    // WI-FI SCAN & CONNECT
    async function scanWifi() {
        log('Đang quét các mạng Wi-Fi...');
        toast('Đang quét Wi-Fi...');
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
        toast(`Đang lưu Wi-Fi: ${ssid}`);
        await fetch(`/api/wifi/save?ssid=${encodeURIComponent(ssid)}&pass=${encodeURIComponent(pass)}`, { method: 'POST' });
    }

    async function clearWifi() {
        if (!confirm('Xóa thông tin Wi-Fi đã lưu?')) return;
        log('Đã xóa thông tin Wi-Fi.');
        toast('Đã xóa Wi-Fi.');
        await fetch('/api/wifi/clear', { method: 'POST' });
    }

    // KEYBOARD SHORTCUTS
    window.addEventListener('keydown', (e) => {
        if (e.target.tagName === 'INPUT' || e.target.tagName === 'SELECT') return;
        if (e.key === ' ' || e.key === 'Escape') {
            e.preventDefault();
            sendStopAll();
        } else if (e.key >= '1' && e.key <= '6') {
            selectTab(parseInt(e.key) - 1);
        } else if (e.key === '7') {
            selectTab(6);
        } else if (e.key === '8') {
            selectTab(7);
        } else if (e.key === '9') {
            selectTab(8);
        } else if (e.key === '0') {
            selectTab(9);
        } else if (e.key === 'ArrowLeft') {
            e.preventDefault();
            jog(-1);
        } else if (e.key === 'ArrowRight') {
            e.preventDefault();
            jog(1);
        }
    });

    // Dynamic polling throttle: 100ms when moving, 250ms when stationary
    async function pollingLoop() {
        await fetchStatus(false);
        const pollInterval = (latestData && latestData.any_running) ? 100 : 250;
        setTimeout(pollingLoop, pollInterval);
    }

    window.onload = () => {
        fetchStatus(true);
        pollingLoop();
    };
</script>

</body>
</html>
)rawliteral";

#endif // WEB_UI_H
