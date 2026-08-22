#ifndef WEB_UI_H
#define WEB_UI_H

#include <Arduino.h>

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <title>ESP32-S3 6-DOF Robot Motion Studio</title>
    <style>
        :root {
            --bg-base: #0b0f17;
            --bg-card: #131924;
            --bg-card-header: #17202e;
            --bg-input: #080c14;
            --border: #1e293b;
            --border-highlight: #334155;
            --primary: #10b981;
            --primary-hover: #059669;
            --primary-soft: rgba(16, 185, 129, 0.12);
            --accent: #38bdf8;
            --accent-hover: #0284c7;
            --accent-soft: rgba(56, 189, 248, 0.12);
            --danger: #f43f5e;
            --danger-hover: #e11d48;
            --danger-soft: rgba(244, 63, 94, 0.15);
            --warning: #f59e0b;
            --warning-soft: rgba(245, 158, 11, 0.15);
            --text-main: #f8fafc;
            --text-dim: #94a3b8;
            --text-muted: #475569;
            --font-sans: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
            --font-mono: "SF Mono", "Roboto Mono", Consolas, monospace;
        }

        * { box-sizing: border-box; margin: 0; padding: 0; font-family: var(--font-sans); }
        body { background: var(--bg-base); color: var(--text-main); padding: 10px; min-height: 100vh; font-size: 13px; line-height: 1.4; }
        .app-container { max-width: 1560px; margin: 0 auto; display: flex; flex-direction: column; gap: 10px; }

        /* HEADER / SYSTEM APP BAR */
        .app-header {
            background: var(--bg-card); border: 1px solid var(--border); border-radius: 8px;
            padding: 8px 16px; display: flex; justify-content: space-between; align-items: center; flex-wrap: wrap; gap: 10px;
        }
        .brand-section { display: flex; align-items: center; gap: 12px; }
        .brand-title { font-size: 1.05rem; font-weight: 800; letter-spacing: 0.5px; color: #fff; display: flex; align-items: center; gap: 8px; }
        .pulse-led { width: 9px; height: 9px; border-radius: 50%; background: var(--primary); box-shadow: 0 0 10px var(--primary); }
        .pulse-led.offline { background: var(--danger); box-shadow: 0 0 10px var(--danger); }

        .global-actions { display: flex; align-items: center; gap: 8px; flex-wrap: wrap; }
        .btn-estop {
            background: var(--danger); color: #fff; border: 1px solid #fda4af; padding: 8px 18px; border-radius: 6px;
            font-weight: 800; font-size: 0.88rem; cursor: pointer; display: flex; align-items: center; gap: 6px;
            box-shadow: 0 0 14px rgba(244, 63, 94, 0.4); transition: all 0.15s;
        }
        .btn-estop:hover { background: var(--danger-hover); transform: translateY(-1px); }
        .btn-estop:active { transform: scale(0.97); }

        /* BADGES */
        .badge { font-family: var(--font-mono); font-size: 0.72rem; padding: 3px 8px; border-radius: 4px; font-weight: 600; }
        .badge-ok { background: var(--primary-soft); color: #34d399; border: 1px solid rgba(16, 185, 129, 0.3); }
        .badge-warn { background: var(--warning-soft); color: #fbbf24; border: 1px solid rgba(245, 158, 11, 0.3); }
        .badge-err { background: var(--danger-soft); color: #fb7185; border: 1px solid rgba(244, 63, 94, 0.3); }
        .badge-info { background: var(--accent-soft); color: #38bdf8; border: 1px solid rgba(56, 189, 248, 0.3); }

        /* WORKSPACE MODE TABS */
        .workspace-tabs { display: flex; gap: 6px; border-bottom: 1px solid var(--border); padding-bottom: 6px; overflow-x: auto; }
        .tab-btn {
            background: var(--bg-card); color: var(--text-dim); border: 1px solid var(--border);
            padding: 8px 16px; border-radius: 6px; font-weight: 700; font-size: 0.82rem; cursor: pointer;
            transition: all 0.15s; display: flex; align-items: center; gap: 6px; white-space: nowrap;
        }
        .tab-btn:hover { background: #1e293b; color: #fff; }
        .tab-btn.active { background: var(--accent); color: #000; border-color: var(--accent); font-weight: 800; }

        /* 2-COLUMN MAIN WORKSPACE */
        .workspace-grid {
            display: grid; grid-template-columns: 1fr; gap: 10px;
        }
        @media (min-width: 1080px) {
            .workspace-grid { grid-template-columns: 1.35fr 1fr; }
        }

        /* CARD CONTAINERS */
        .panel-card {
            background: var(--bg-card); border: 1px solid var(--border); border-radius: 8px; overflow: hidden;
            display: flex; flex-direction: column;
        }
        .panel-header {
            background: var(--bg-card-header); padding: 8px 12px; border-bottom: 1px solid var(--border);
            display: flex; justify-content: space-between; align-items: center; font-weight: 700; font-size: 0.85rem; color: #f1f5f9;
        }
        .panel-body { padding: 10px; display: flex; flex-direction: column; gap: 8px; }

        /* 6-AXIS UNIFIED TEACH STRIP */
        .joint-strip-list { display: flex; flex-direction: column; gap: 8px; }
        .joint-row {
            background: #0d131f; border: 1px solid var(--border); border-radius: 6px; padding: 10px 12px;
            display: flex; flex-direction: column; gap: 8px; transition: border-color 0.15s;
        }
        .joint-row:hover { border-color: var(--border-highlight); }
        .joint-row.running { border-color: var(--primary); background: #0c1822; }

        .joint-top-bar { display: flex; justify-content: space-between; align-items: center; flex-wrap: wrap; gap: 6px; }
        .joint-id-badge { font-weight: 800; font-size: 0.9rem; color: #fff; display: flex; align-items: center; gap: 8px; }
        
        .joint-telemetry-readout { display: flex; align-items: baseline; gap: 8px; font-family: var(--font-mono); }
        .angle-actual { font-size: 1.4rem; font-weight: 800; color: #34d399; font-feature-settings: "tnum" 1; }
        .angle-target { font-size: 0.78rem; color: var(--text-dim); }
        .angle-error { font-size: 0.78rem; color: #94a3b8; }

        .joint-controls-bar {
            display: grid; grid-template-columns: 1fr; gap: 8px; align-items: center;
        }
        @media (min-width: 768px) {
            .joint-controls-bar { grid-template-columns: 1fr 140px auto; }
        }

        .jog-button-group {
            display: flex; gap: 4px; flex-wrap: wrap;
        }
        .btn-jog {
            background: #1e293b; color: #f8fafc; border: 1px solid var(--border); padding: 5px 8px;
            border-radius: 4px; font-family: var(--font-mono); font-weight: 700; font-size: 0.75rem; cursor: pointer;
            transition: all 0.1s; user-select: none;
        }
        .btn-jog:hover { background: #334155; border-color: var(--accent); }
        .btn-jog:active { background: var(--accent); color: #000; }

        .slider-joint {
            width: 100%; height: 6px; background: #1e293b; border-radius: 3px; outline: none; -webkit-appearance: none;
        }
        .slider-joint::-webkit-slider-thumb {
            -webkit-appearance: none; width: 14px; height: 14px; border-radius: 50%; background: var(--accent); cursor: pointer;
        }

        .joint-actions-group { display: flex; gap: 5px; align-items: center; position: relative; }
        .btn-action {
            background: #1e293b; color: var(--text-main); border: 1px solid var(--border); padding: 5px 10px;
            border-radius: 4px; font-weight: 600; font-size: 0.75rem; cursor: pointer; transition: all 0.15s;
        }
        .btn-action:hover { background: #334155; border-color: var(--border-highlight); }
        .btn-action.active { background: var(--primary); color: #000; font-weight: 700; }

        /* STREAMLINED DROPDOWN */
        .dropdown { position: relative; display: inline-block; }
        .dropdown-menu {
            display: none; position: absolute; right: 0; top: calc(100% + 4px);
            background: #131924; border: 1px solid var(--border-highlight); border-radius: 6px;
            padding: 4px; z-index: 1000; min-width: 170px; box-shadow: 0 8px 24px rgba(0,0,0,0.7);
        }
        .dropdown-menu.show { display: flex; flex-direction: column; gap: 2px; }
        .dropdown-item {
            background: transparent; border: none; color: #f8fafc; text-align: left;
            padding: 7px 10px; font-size: 0.74rem; cursor: pointer; border-radius: 4px; transition: background 0.12s;
            display: flex; align-items: center; gap: 6px; white-space: nowrap; width: 100%; font-family: var(--font-sans);
        }
        .dropdown-item:hover { background: #1e293b; color: var(--accent); }

        /* CARTESIAN IK PAD */
        .ik-axis-grid { display: grid; grid-template-columns: repeat(3, 1fr); gap: 8px; }
        .ik-axis-card {
            background: #0d131f; border: 1px solid var(--border); border-radius: 6px; padding: 10px;
            display: flex; flex-direction: column; gap: 6px;
        }
        .ik-axis-title { font-size: 0.8rem; font-weight: 700; color: var(--accent); display: flex; justify-content: space-between; }
        .ik-jog-pair { display: flex; gap: 4px; }
        .btn-ik-jog {
            flex: 1; background: #1e293b; color: #fff; border: 1px solid var(--border); padding: 7px;
            border-radius: 4px; font-weight: 700; font-size: 0.82rem; cursor: pointer;
        }
        .btn-ik-jog:hover { background: var(--accent); color: #000; }

        /* 3D WIREFRAME SIMULATOR CANVAS */
        .canvas-wrapper {
            position: relative; width: 100%; height: 320px; background: #060911; border-radius: 6px;
            border: 1px solid var(--border); overflow: hidden;
        }
        #robot3d-canvas { width: 100%; height: 100%; cursor: grab; }
        #robot3d-canvas:active { cursor: grabbing; }

        .canvas-overlay-hud {
            position: absolute; top: 10px; left: 10px; pointer-events: none;
            display: flex; flex-direction: column; gap: 3px; font-family: var(--font-mono); font-size: 0.75rem;
            background: rgba(11, 15, 23, 0.8); padding: 8px 10px; border-radius: 4px; border: 1px solid var(--border);
        }

        /* TABLES */
        .table-custom { width: 100%; border-collapse: collapse; font-size: 0.78rem; font-family: var(--font-mono); }
        .table-custom th, .table-custom td { padding: 6px 8px; text-align: left; border-bottom: 1px solid var(--border); }
        .table-custom th { color: var(--text-muted); font-weight: 600; background: rgba(0,0,0,0.2); }

        /* G-CODE CONSOLE */
        .console-output {
            background: #050811; border: 1px solid var(--border); border-radius: 6px; padding: 8px 10px;
            font-family: var(--font-mono); font-size: 0.74rem; height: 110px; overflow-y: auto; color: #38bdf8;
        }
        .console-input-row { display: flex; gap: 6px; margin-top: 6px; }
        .input-cli {
            flex: 1; background: var(--bg-input); border: 1px solid var(--border); color: #fff;
            padding: 6px 10px; border-radius: 4px; font-family: var(--font-mono); font-size: 0.82rem; outline: none;
        }
        .input-cli:focus { border-color: var(--accent); }

        /* INPUTS & GENERAL BUTTONS */
        .btn-ui {
            background: #1e293b; color: #fff; border: 1px solid var(--border); padding: 6px 12px;
            border-radius: 4px; font-weight: 600; font-size: 0.78rem; cursor: pointer; transition: all 0.12s;
        }
        .btn-ui:hover { background: #334155; border-color: var(--border-highlight); }
        .btn-ui-primary { background: var(--primary); color: #000; border-color: transparent; font-weight: 700; }
        .btn-ui-primary:hover { background: var(--primary-hover); }
        .btn-ui-accent { background: var(--accent); color: #000; border-color: transparent; font-weight: 700; }
        .btn-ui-accent:hover { background: var(--accent-hover); }

        /* TOAST */
        .toast-box {
            position: fixed; bottom: 16px; right: 16px; z-index: 9999;
            background: #1e293b; border: 1px solid var(--accent); color: #fff; padding: 8px 14px;
            border-radius: 6px; font-size: 0.8rem; box-shadow: 0 4px 16px rgba(0,0,0,0.6);
            opacity: 0; transform: translateY(10px); transition: all 0.2s; pointer-events: none;
        }
        .toast-box.show { opacity: 1; transform: translateY(0); }
    </style>
</head>
<body>

<div class="app-container">
    <!-- TOP INDUSTRIAL HEADER -->
    <header class="app-header">
        <div class="brand-section">
            <span class="pulse-led" id="led-heartbeat"></span>
            <div class="brand-title">
                <span>ROBOTICS 6-DOF TEACH STUDIO</span>
                <span style="font-size: 0.72rem; color: var(--text-muted); font-family: var(--font-mono);" id="lbl-fw-ver">v2.2.0</span>
            </div>
            <span class="badge badge-ok" id="badge-sys-state">SYSTEM READY</span>
        </div>

        <div class="global-actions">
            <button class="btn-ui btn-ui-accent" onclick="sendGlobalHome()">🎯 Home 6 Trục (G28)</button>
            <button class="btn-ui" onclick="sendGlobalZero()">📍 Set 0.00° Tất Cả</button>
            <button class="btn-ui" onclick="toggleAllPower()">⚡ Động Cơ (Bật/Tắt)</button>
            <button class="btn-estop" onclick="sendEmergencyStop()">⛔ E-STOP</button>
        </div>
    </header>

    <!-- WORKSPACE MODE SWITCHER -->
    <div class="workspace-tabs">
        <button class="tab-btn active" onclick="switchTab('joints')">🕹️ 6-Axis Teach Pendant</button>
        <button class="tab-btn" onclick="switchTab('cartesian')">🦾 Cartesian IK & TCP</button>
        <button class="tab-btn" onclick="switchTab('sync')">🌐 Synchronized 6-Axis Move</button>
        <button class="tab-btn" onclick="switchTab('waypoints')">🎬 Teach-in & Trajectories</button>
        <button class="tab-btn" onclick="switchTab('diagnostics')">🔍 Hardware Bus Diagnostics</button>
    </div>

    <!-- MAIN 2-COLUMN WORKSPACE -->
    <div class="workspace-grid">
        
        <!-- LEFT COLUMN: PRIMARY WORK AREA -->
        <div style="display: flex; flex-direction: column; gap: 10px;">
            
            <!-- VIEW 1: 6-AXIS UNIFIED TEACH PENDANT (ALL 6 JOINTS VISIBLE SIMULTANEOUSLY) -->
            <div class="panel-card" id="view-joints">
                <div class="panel-header">
                    <span>🕹️ 6-Axis Real-Time Motion Control</span>
                    <span style="font-size: 0.72rem; color: var(--text-dim);">Live Sensor: 500Hz | Control: 100Hz</span>
                </div>
                <div class="panel-body">
                    <div class="joint-strip-list" id="joint-strip-container">
                        <!-- Dynamic J1..J6 Rows populated by JS -->
                    </div>
                </div>
            </div>

            <!-- VIEW 2: CARTESIAN IK & TCP CONTROL -->
            <div class="panel-card" id="view-cartesian" style="display: none;">
                <div class="panel-header">
                    <span>🦾 Cartesian Space (Inverse Kinematics)</span>
                    <span id="ik-hud-dist" style="font-family: var(--font-mono); color: var(--accent);">Target TCP Mode</span>
                </div>
                <div class="panel-body">
                    <div class="ik-axis-grid">
                        <div class="ik-axis-card">
                            <div class="ik-axis-title"><span>Trục X (Trước / Sau)</span><span id="ik-disp-x">0.0 mm</span></div>
                            <div class="ik-jog-pair">
                                <button class="btn-ik-jog" onclick="jogIK('x', -10)">-10 mm</button>
                                <button class="btn-ik-jog" onclick="jogIK('x', 10)">+10 mm</button>
                            </div>
                        </div>
                        <div class="ik-axis-card">
                            <div class="ik-axis-title"><span>Trục Y (Trái / Phải)</span><span id="ik-disp-y">0.0 mm</span></div>
                            <div class="ik-jog-pair">
                                <button class="btn-ik-jog" onclick="jogIK('y', -10)">-10 mm</button>
                                <button class="btn-ik-jog" onclick="jogIK('y', 10)">+10 mm</button>
                            </div>
                        </div>
                        <div class="ik-axis-card">
                            <div class="ik-axis-title"><span>Trục Z (Cao / Thấp)</span><span id="ik-disp-z">0.0 mm</span></div>
                            <div class="ik-jog-pair">
                                <button class="btn-ik-jog" onclick="jogIK('z', -10)">-10 mm</button>
                                <button class="btn-ik-jog" onclick="jogIK('z', 10)">+10 mm</button>
                            </div>
                        </div>
                        <div class="ik-axis-card">
                            <div class="ik-axis-title"><span>Roll (Xoay X)</span><span id="ik-disp-roll">0.0°</span></div>
                            <div class="ik-jog-pair">
                                <button class="btn-ik-jog" onclick="jogIK('roll', -5)">-5°</button>
                                <button class="btn-ik-jog" onclick="jogIK('roll', 5)">+5°</button>
                            </div>
                        </div>
                        <div class="ik-axis-card">
                            <div class="ik-axis-title"><span>Pitch (Gật Y)</span><span id="ik-disp-pitch">0.0°</span></div>
                            <div class="ik-jog-pair">
                                <button class="btn-ik-jog" onclick="jogIK('pitch', -5)">-5°</button>
                                <button class="btn-ik-jog" onclick="jogIK('pitch', 5)">+5°</button>
                            </div>
                        </div>
                        <div class="ik-axis-card">
                            <div class="ik-axis-title"><span>Yaw (Quay Z)</span><span id="ik-disp-yaw">0.0°</span></div>
                            <div class="ik-jog-pair">
                                <button class="btn-ik-jog" onclick="jogIK('yaw', -5)">-5°</button>
                                <button class="btn-ik-jog" onclick="jogIK('yaw', 5)">+5°</button>
                            </div>
                        </div>
                    </div>

                    <div style="display: flex; gap: 8px; align-items: center; margin-top: 6px;">
                        <input type="number" id="ik-target-time" value="2.0" step="0.5" min="0.5" style="width: 100px; padding: 6px;" placeholder="Thời gian (s)">
                        <button class="btn-ui btn-ui-accent" onclick="executeCurrentIK()" style="flex: 1; padding: 8px;">🚀 Di Chuyển Đến Tọa Độ IK</button>
                    </div>
                </div>
            </div>

            <!-- VIEW 3: SYNCHRONIZED 6-AXIS ARRIVAL MOVE -->
            <div class="panel-card" id="view-sync" style="display: none;">
                <div class="panel-header">
                    <span>🌐 Synchronized Multi-Axis Move (Góc Đích Đồng Thời)</span>
                    <button class="btn-ui" onclick="copyCurrentToSync()" style="font-size: 0.72rem; padding: 3px 8px;">📋 Nạp Góc Hiện Tại</button>
                </div>
                <div class="panel-body">
                    <div style="display: grid; grid-template-columns: repeat(6, 1fr); gap: 6px;">
                        <div style="display:flex; flex-direction:column; align-items:center; gap:2px;">
                            <label style="font-size:0.7rem; font-weight:700; color:var(--accent);">J1 (Base)</label>
                            <input type="number" id="sync-input-0" value="0.0" step="1.0" class="input-cli" style="text-align:center;">
                        </div>
                        <div style="display:flex; flex-direction:column; align-items:center; gap:2px;">
                            <label style="font-size:0.7rem; font-weight:700; color:var(--accent);">J2 (Shoulder)</label>
                            <input type="number" id="sync-input-1" value="0.0" step="1.0" class="input-cli" style="text-align:center;">
                        </div>
                        <div style="display:flex; flex-direction:column; align-items:center; gap:2px;">
                            <label style="font-size:0.7rem; font-weight:700; color:var(--accent);">J3 (Elbow)</label>
                            <input type="number" id="sync-input-2" value="0.0" step="1.0" class="input-cli" style="text-align:center;">
                        </div>
                        <div style="display:flex; flex-direction:column; align-items:center; gap:2px;">
                            <label style="font-size:0.7rem; font-weight:700; color:var(--accent);">J4 (W-Roll)</label>
                            <input type="number" id="sync-input-3" value="0.0" step="1.0" class="input-cli" style="text-align:center;">
                        </div>
                        <div style="display:flex; flex-direction:column; align-items:center; gap:2px;">
                            <label style="font-size:0.7rem; font-weight:700; color:var(--accent);">J5 (W-Pitch)</label>
                            <input type="number" id="sync-input-4" value="0.0" step="1.0" class="input-cli" style="text-align:center;">
                        </div>
                        <div style="display:flex; flex-direction:column; align-items:center; gap:2px;">
                            <label style="font-size:0.7rem; font-weight:700; color:var(--accent);">J6 (Flange)</label>
                            <input type="number" id="sync-input-5" value="0.0" step="1.0" class="input-cli" style="text-align:center;">
                        </div>
                    </div>

                    <div style="display:flex; gap:10px; align-items:center; margin-top:8px;">
                        <label style="color:var(--text-dim);">Thời gian $T_{\text{sync}}$ (s):</label>
                        <input type="number" id="sync-time" value="2.5" step="0.5" min="0.5" class="input-cli" style="width:80px; text-align:center;">
                        <button class="btn-ui btn-ui-primary" onclick="sendSyncMove()" style="flex:1; padding:8px;">🚀 Bắt Đầu Quay Đồng Bộ 6 Trục</button>
                    </div>
                </div>
            </div>

            <!-- VIEW 4: WAYPOINT TEACH & PLAYBACK -->
            <div class="panel-card" id="view-waypoints" style="display: none;">
                <div class="panel-header">
                    <span>🎬 Teach & Playback Sequencer</span>
                    <div style="display:flex; gap:6px;">
                        <button class="btn-ui btn-ui-accent" onclick="teachCurrentPose()">➕ Dạy Điểm Này</button>
                        <button class="btn-ui btn-ui-primary" onclick="startSequence(false)">▶ Phát 1 Lượt</button>
                        <button class="btn-ui btn-ui-primary" onclick="startSequence(true)">🔁 Lặp Lại</button>
                        <button class="btn-ui" onclick="stopSequence()">⏹ Dừng</button>
                    </div>
                </div>
                <div class="panel-body">
                    <div style="overflow-x: auto; max-height: 250px;">
                        <table class="table-custom">
                            <thead>
                                <tr>
                                    <th>STT</th>
                                    <th>Tên Điểm Dạy</th>
                                    <th>Góc [J1..J6]</th>
                                    <th>Thời Gian</th>
                                    <th>Dwell</th>
                                    <th>Xóa</th>
                                </tr>
                            </thead>
                            <tbody id="wp-table-body">
                                <!-- Dynamic Waypoints -->
                            </tbody>
                        </table>
                    </div>
                </div>
            </div>

            <!-- VIEW 5: HARDWARE DIAGNOSTICS -->
            <div class="panel-card" id="view-diagnostics" style="display: none;">
                <div class="panel-header">
                    <span>🔍 Hardware Register Monitor (I2C PCA9548A & Dual UART TMC2209)</span>
                    <button class="btn-ui" onclick="fetchDiagnostics()">🔄 Refresh</button>
                </div>
                <div class="panel-body">
                    <div style="overflow-x: auto;">
                        <table class="table-custom">
                            <thead>
                                <tr>
                                    <th>Khớp</th>
                                    <th>UART Ver</th>
                                    <th>StallGuard</th>
                                    <th>Nhiệt Độ</th>
                                    <th>I2C AS5600</th>
                                    <th>Từ Trường AGC (25..230)</th>
                                </tr>
                            </thead>
                            <tbody id="diag-table-body">
                                <!-- Dynamic Diagnostics -->
                            </tbody>
                        </table>
                    </div>

                    <div class="panel-header" style="margin-top: 10px;">
                        <span>📡 Cấu Hình Wi-Fi STA</span>
                    </div>
                    <div style="display:flex; gap:6px; margin-top:6px;">
                        <input type="text" id="wifi-ssid" placeholder="Tên Wi-Fi (SSID)" class="input-cli">
                        <input type="password" id="wifi-pass" placeholder="Mật khẩu Wi-Fi" class="input-cli">
                        <button class="btn-ui btn-ui-primary" onclick="saveWifi()">Lưu Wi-Fi</button>
                    </div>

                    <div class="panel-header" style="margin-top: 10px;">
                        <span>🔄 OTA Firmware Update</span>
                    </div>
                    <div style="margin-top:6px;">
                        <div style="display:flex; gap:6px; align-items:center; flex-wrap:wrap;">
                            <input type="file" id="ota-file" accept=".bin" style="flex:1; color:var(--text-dim); background:var(--bg-input); border:1px solid var(--border); border-radius:4px; padding:4px 6px; font-size:0.75rem;">
                            <button class="btn-ui btn-ui-primary" onclick="startOtaUpload()">⬆ Flash Firmware</button>
                        </div>
                        <div id="ota-progress-wrap" style="display:none; margin-top:6px;">
                            <div style="background:var(--bg-input); border-radius:4px; overflow:hidden; height:8px;">
                                <div id="ota-progress-bar" style="width:0%; height:100%; background:var(--primary); transition:width 0.3s;"></div>
                            </div>
                            <div id="ota-status" style="margin-top:4px; font-size:0.72rem; color:var(--text-dim);">Uploading...</div>
                        </div>
                    </div>
                </div>
            </div>

        </div>

        <!-- RIGHT COLUMN: 3D DIGITAL TWIN & REAL-TIME HUD -->
        <div style="display: flex; flex-direction: column; gap: 10px;">
            
            <!-- 3D VISUALIZER DIGITAL TWIN -->
            <div class="panel-card">
                <div class="panel-header">
                    <span>🌐 3D Kinematics Digital Twin</span>
                    <span style="font-size: 0.7rem; color: var(--text-muted);">Xoay: Chuột trái | Thu phóng: Lăn chuột</span>
                </div>
                <div class="panel-body" style="padding: 0;">
                    <div class="canvas-wrapper">
                        <canvas id="robot3d-canvas"></canvas>
                        <div class="canvas-overlay-hud">
                            <div>TCP X: <span id="hud-x" style="color:#38bdf8;">--</span> mm</div>
                            <div>TCP Y: <span id="hud-y" style="color:#38bdf8;">--</span> mm</div>
                            <div>TCP Z: <span id="hud-z" style="color:#38bdf8;">--</span> mm</div>
                            <div>TCP RPY: <span id="hud-rpy" style="color:#34d399;">--</span></div>
                        </div>
                    </div>
                </div>
            </div>

            <!-- G-CODE / SERIAL CLI TERMINAL -->
            <div class="panel-card">
                <div class="panel-header">
                    <span>🖥️ Live Terminal & G-Code CLI</span>
                    <button class="btn-ui" onclick="clearConsole()" style="font-size:0.68rem; padding:2px 6px;">Xóa</button>
                </div>
                <div class="panel-body">
                    <div id="console-log" class="console-output"></div>
                    <div class="console-input-row">
                        <input type="text" id="cli-input" class="input-cli" placeholder="Gõ lệnh (vd: G0 X200 Y0 Z150, M1 45, M114, HELP)..." onkeydown="onCliKey(event)">
                        <button class="btn-ui btn-ui-accent" onclick="sendCliCommand()">Gửi Lệnh</button>
                    </div>
                </div>
            </div>

        </div>

    </div>
</div>

<div id="toast" class="toast-box"></div>

<script>
    const NUM_AXES = 6;
    const AXIS_NAMES = [
        "J1 (Base Yaw)",
        "J2 (Shoulder Pitch)",
        "J3 (Elbow Pitch)",
        "J4 (Wrist Roll)",
        "J5 (Wrist Pitch)",
        "J6 (Flange Roll)"
    ];

    let currentTabName = 'joints';
    let latestTelemetry = null;
    let isFetching = false;
    let lastActionTimestamp = 0;

    // TOAST & LOG
    function toast(msg) {
        const t = document.getElementById('toast');
        t.innerText = msg;
        t.classList.add('show');
        setTimeout(() => t.classList.remove('show'), 1600);
    }

    function logConsole(msg) {
        const c = document.getElementById('console-log');
        const time = new Date().toLocaleTimeString();
        c.innerHTML += `<div><span style="color:#64748b;">[${time}]</span> ${msg}</div>`;
        c.scrollTop = c.scrollHeight;
    }

    function clearConsole() {
        document.getElementById('console-log').innerHTML = '';
    }

    // WORKSPACE TAB SWITCHING
    function switchTab(tab) {
        currentTabName = tab;
        document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
        event.target.classList.add('active');

        document.getElementById('view-joints').style.display = (tab === 'joints') ? 'flex' : 'none';
        document.getElementById('view-cartesian').style.display = (tab === 'cartesian') ? 'flex' : 'none';
        document.getElementById('view-sync').style.display = (tab === 'sync') ? 'flex' : 'none';
        document.getElementById('view-waypoints').style.display = (tab === 'waypoints') ? 'flex' : 'none';
        document.getElementById('view-diagnostics').style.display = (tab === 'diagnostics') ? 'flex' : 'none';

        if (tab === 'waypoints') fetchWaypoints();
        if (tab === 'diagnostics') fetchDiagnostics();
    }

    // BUILD UNIFIED 6-AXIS STRIP
    function initJointStrip() {
        const container = document.getElementById('joint-strip-container');
        let html = '';
        for (let i = 0; i < NUM_AXES; i++) {
            html += `
            <div class="joint-row" id="joint-row-${i}">
                <div class="joint-top-bar">
                    <div class="joint-id-badge">
                        <span>${AXIS_NAMES[i]}</span>
                        <span id="badge-homed-${i}" class="badge badge-warn">UNHOMED</span>
                        <span id="badge-dir-${i}" class="badge badge-info">FWD</span>
                        <span id="badge-calib-${i}" class="badge badge-ok">CALIB</span>
                        <span id="badge-uart-${i}" class="badge badge-ok">UART</span>
                    </div>
                    <div class="joint-telemetry-readout">
                        <span class="angle-actual" id="deg-actual-${i}">0.00°</span>
                        <span class="angle-target" id="deg-target-${i}">Tgt: 0.00°</span>
                        <span class="angle-error" id="deg-err-${i}">Err: 0.00°</span>
                    </div>
                </div>

                <div class="joint-controls-bar">
                    <!-- COMPACT JOG CLUSTER (5 BUTTONS) -->
                    <div class="jog-button-group">
                        <button class="btn-jog" onclick="jogAxis(${i}, -10)">-10°</button>
                        <button class="btn-jog" onclick="jogAxis(${i}, -1)">-1°</button>
                        <button class="btn-jog" style="background:#0284c7; color:#fff;" onclick="gotoAngle(${i}, 0)">0°</button>
                        <button class="btn-jog" onclick="jogAxis(${i}, 1)">+1°</button>
                        <button class="btn-jog" onclick="jogAxis(${i}, 10)">+10°</button>
                    </div>

                    <!-- DIRECT GOTO INPUT -->
                    <div style="display:flex; gap:4px;">
                        <input type="number" id="input-goto-${i}" value="0.0" step="0.5" class="input-cli" style="width:70px; text-align:center;">
                        <button class="btn-ui btn-ui-accent" onclick="gotoAngleFromInput(${i})">Đi</button>
                    </div>

                    <!-- CONSOLIDATED ACTION CLUSTER (3 BUTTONS + 1 DROPDOWN) -->
                    <div class="joint-actions-group">
                        <button class="btn-action" id="btn-home-${i}" onclick="homeJoint(${i})">🎯 Home</button>
                        <button class="btn-action" id="btn-zero-${i}" onclick="zeroJoint(${i})">📍 Set 0°</button>
                        
                        <!-- DROPDOWN TÙY CHỌN NÂNG CAO -->
                        <div class="dropdown">
                            <button class="btn-action" onclick="toggleJointMenu(event, ${i})">⚙️ Thêm ▾</button>
                            <div class="dropdown-menu" id="menu-dropdown-${i}">
                                <button class="dropdown-item" id="menu-gear-${i}" onclick="setJointGearRatio(${i}); closeAllMenus();">⚙️ Tỉ Số Truyền: 6.00 : 1</button>
                                <button class="dropdown-item" id="menu-hcurr-${i}" onclick="setJointHomingCurrent(${i}); closeAllMenus();">🎯 Dòng Homing: 750 mA</button>
                                <button class="dropdown-item" id="menu-curr-${i}" onclick="setJointCurrent(${i}); closeAllMenus();">⚡ Dòng Chạy RMS: 800 mA</button>
                                <button class="dropdown-item" onclick="autoDirJoint(${i}); closeAllMenus();">🔄 Tự Nhận Chiều (Auto-Dir)</button>
                                <button class="dropdown-item" onclick="calibJoint(${i}); closeAllMenus();">📊 Calib 16 Điểm Phi Tuyến</button>
                                <button class="dropdown-item" id="menu-hold-${i}" onclick="toggleHold(${i}); closeAllMenus();">🔒 Khóa Vị Trí: TẮT</button>
                                <button class="dropdown-item" id="menu-inv-${i}" onclick="toggleInvert(${i}); closeAllMenus();">🔀 Đảo Chiều: THUẬN</button>
                            </div>
                        </div>

                        <button class="btn-action" id="btn-stop-${i}" style="color:#fda4af; border-color:#f43f5e;" onclick="stopJoint(${i})">⏹</button>
                    </div>
                </div>

                <!-- SLIDER FOR FAST POSITIONING -->
                <input type="range" class="slider-joint" id="slider-joint-${i}" min="-180" max="180" value="0" step="1"
                       oninput="onJointSliderInput(${i}, this.value)" onchange="onJointSliderChange(${i}, this.value)">
            </div>`;
        }
        container.innerHTML = html;
    }

    function onJointSliderInput(axis, val) {
        document.getElementById(`input-goto-${axis}`).value = val;
    }

    function onJointSliderChange(axis, val) {
        gotoAngle(axis, parseFloat(val));
    }

    // HIGH-PERFORMANCE ZERO-LATENCY JOG
    function jogAxis(axis, delta) {
        lastActionTimestamp = Date.now();
        const curTgt = (latestTelemetry && latestTelemetry.axes && latestTelemetry.axes[axis]) ? latestTelemetry.axes[axis].targetAngle : 0;
        const newTgt = curTgt + delta;
        document.getElementById(`deg-target-${axis}`).innerText = `Tgt: ${newTgt.toFixed(2)}°`;
        document.getElementById(`input-goto-${axis}`).value = newTgt.toFixed(2);
        document.getElementById(`slider-joint-${axis}`).value = Math.round(newTgt);
        toast(`[J${axis + 1}] Jog ${delta > 0 ? '+' : ''}${delta}°`);
        fetch(`/api/motor/jog?axis=${axis}&delta=${delta}`, { method: 'POST', cache: 'no-store' });
    }

    function gotoAngle(axis, angle) {
        lastActionTimestamp = Date.now();
        document.getElementById(`deg-target-${axis}`).innerText = `Tgt: ${angle.toFixed(2)}°`;
        document.getElementById(`input-goto-${axis}`).value = angle.toFixed(2);
        document.getElementById(`slider-joint-${axis}`).value = Math.round(angle);
        toast(`[J${axis + 1}] Đến: ${angle}°`);
        fetch(`/api/motor/goto?axis=${axis}&angle=${angle}`, { method: 'POST', cache: 'no-store' });
    }

    function gotoAngleFromInput(axis) {
        const val = parseFloat(document.getElementById(`input-goto-${axis}`).value);
        if (!isNaN(val)) gotoAngle(axis, val);
    }

    function homeJoint(axis) {
        lastActionTimestamp = Date.now();
        toast(`[J${axis + 1}] Bắt đầu Homing...`);
        fetch(`/api/motor/home?axis=${axis}`, { method: 'POST', cache: 'no-store' });
    }

    function autoDirJoint(axis) {
        lastActionTimestamp = Date.now();
        toast(`[J${axis + 1}] Tự động kiểm tra chiều quay...`);
        fetch(`/api/motor/autodir?axis=${axis}`, { method: 'POST', cache: 'no-store' });
    }

    function calibJoint(axis) {
        if (!confirm(`Chạy Auto Calibration 16 điểm cho Joint ${axis + 1}?\nĐộng cơ sẽ quay 1 vòng 360° để lập bảng bù sai số phi tuyến.`)) return;
        lastActionTimestamp = Date.now();
        toast(`[J${axis + 1}] Bắt đầu Auto Calib 16 điểm...`);
        fetch(`/api/motor/calib?axis=${axis}`, { method: 'POST', cache: 'no-store' });
    }

    function zeroJoint(axis) {
        if (!confirm(`Đặt vị trí hiện tại làm mốc 0.00° cho Joint ${axis + 1}?`)) return;
        lastActionTimestamp = Date.now();
        toast(`[J${axis + 1}] Đặt mốc 0.00°`);
        fetch(`/api/motor/zero?axis=${axis}`, { method: 'POST', cache: 'no-store' });
    }

    function stopJoint(axis) {
        lastActionTimestamp = Date.now();
        toast(`[J${axis + 1}] DỪNG TRỤC`);
        fetch(`/api/motor/stop?axis=${axis}`, { method: 'POST', cache: 'no-store' });
    }

    function toggleHold(axis) {
        if (!latestTelemetry || !latestTelemetry.axes) return;
        const cur = latestTelemetry.axes[axis].closedLoopHold;
        const newHold = !cur;
        const inv = latestTelemetry.axes[axis].dirInvert ? 1 : 0;
        const speed = latestTelemetry.axes[axis].speed || 400;
        const curr = latestTelemetry.axes[axis].current || 800;
        fetch(`/api/motor/settings?axis=${axis}&hold=${newHold ? 1 : 0}&invert=${inv}&speed=${speed}&curr=${curr}`, { method: 'POST', cache: 'no-store' });
        toast(`[J${axis + 1}] Closed-Loop Hold: ${newHold ? 'ON' : 'OFF'}`);
    }

    function toggleInvert(axis) {
        if (!latestTelemetry || !latestTelemetry.axes) return;
        const curInv = latestTelemetry.axes[axis].dirInvert;
        const newInv = !curInv;
        const hold = latestTelemetry.axes[axis].closedLoopHold ? 1 : 0;
        const speed = latestTelemetry.axes[axis].speed || 400;
        const curr = latestTelemetry.axes[axis].current || 800;
        fetch(`/api/motor/settings?axis=${axis}&hold=${hold}&invert=${newInv ? 1 : 0}&speed=${speed}&curr=${curr}`, { method: 'POST', cache: 'no-store' });
        toast(`[J${axis + 1}] Đảo chiều Invert: ${newInv ? 'BẬT (ĐẢO)' : 'TẮT (THUẬN)'}`);
    }

    function setJointHomingCurrent(axis) {
        if (!latestTelemetry || !latestTelemetry.axes) return;
        const cur = latestTelemetry.axes[axis].homing_current || 750;
        const val = prompt(`[Joint ${axis + 1}] Nhập dòng Homing an toàn (mA) [200 - 1200]:`, cur);
        if (val !== null && !isNaN(parseInt(val))) {
            const hcurr = parseInt(val);
            fetch(`/api/motor/settings?axis=${axis}&home_curr=${hcurr}`, { method: 'POST', cache: 'no-store' });
            toast(`[J${axis + 1}] Đã lưu Dòng Homing: ${hcurr} mA`);
        }
    }

    function setJointCurrent(axis) {
        if (!latestTelemetry || !latestTelemetry.axes) return;
        const cur = latestTelemetry.axes[axis].current || 800;
        const val = prompt(`[Joint ${axis + 1}] Nhập dòng chạy RMS bình thường (mA) [200 - 1400]:`, cur);
        if (val !== null && !isNaN(parseInt(val))) {
            const curr = parseInt(val);
            fetch(`/api/motor/settings?axis=${axis}&curr=${curr}`, { method: 'POST', cache: 'no-store' });
            toast(`[J${axis + 1}] Đã lưu Dòng RMS: ${curr} mA`);
        }
    }

    function setJointGearRatio(axis) {
        if (!latestTelemetry || !latestTelemetry.axes) return;
        const cur = latestTelemetry.axes[axis].gearRatio || 6.0;
        const val = prompt(`[Joint ${axis + 1}] Nhập tỉ số truyền Gear Ratio (nhập số âm nếu hộp số đảo chiều, ví dụ: 30 hoặc -30):`, cur);
        if (val !== null && !isNaN(parseFloat(val))) {
            const gr = parseFloat(val);
            fetch(`/api/motor/settings?axis=${axis}&gear=${gr}`, { method: 'POST', cache: 'no-store' });
            toast(`[J${axis + 1}] Đã lưu Tỉ số truyền: ${gr} : 1`);
        }
    }

    // GLOBAL ACTIONS
    function sendEmergencyStop() {
        lastActionTimestamp = Date.now();
        toast('⛔ DỪNG KHẨN CẤP TOÀN BỘ 6 TRỤC!');
        logConsole('⛔ DỪNG KHẨN CẤP TOÀN BỘ 6 TRỤC!');
        fetch('/api/all/stop', { method: 'POST', cache: 'no-store' });
    }

    function sendGlobalHome() {
        lastActionTimestamp = Date.now();
        toast('🎯 Bắt đầu Homing toàn bộ 6 trục...');
        fetch('/api/all/home', { method: 'POST', cache: 'no-store' });
    }

    function sendGlobalAutoDir() {
        lastActionTimestamp = Date.now();
        toast('🔄 Tự động kiểm tra & lưu chiều quay 6 trục...');
        fetch('/api/all/autodir', { method: 'POST', cache: 'no-store' });
    }

    function sendGlobalZero() {
        if (!confirm('Đặt vị trí hiện tại làm mốc 0.00° cho TẤT CẢ 6 TRỤC?')) return;
        lastActionTimestamp = Date.now();
        toast('📍 Đặt mốc 0.00° cho cả 6 trục');
        fetch('/api/all/zero', { method: 'POST', cache: 'no-store' });
    }

    function toggleAllPower() {
        const en = (latestTelemetry && latestTelemetry.axes && latestTelemetry.axes[0].driver_enabled) ? 0 : 1;
        fetch(`/api/all/enable?en=${en}`, { method: 'POST', cache: 'no-store' });
        toast(`Tất cả Driver: ${en ? 'BẬT NGUỒN' : 'TẮT NGUỒN (Trục tự do)'}`);
    }

    // CARTESIAN IK
    let ikPose = { x: 200, y: 0, z: 150, roll: 0, pitch: 0, yaw: 0 };
    function jogIK(axis, delta) {
        ikPose[axis] += delta;
        document.getElementById(`ik-disp-${axis}`).innerText = `${ikPose[axis].toFixed(1)} ${axis.length === 1 ? 'mm' : '°'}`;
        executeCurrentIK();
    }

    function executeCurrentIK() {
        lastActionTimestamp = Date.now();
        const t = parseFloat(document.getElementById('ik-target-time').value) || 2.0;
        toast(`🦾 IK Move: X=${ikPose.x}, Y=${ikPose.y}, Z=${ikPose.z}`);
        fetch(`/api/ik/goto?x=${ikPose.x}&y=${ikPose.y}&z=${ikPose.z}&roll=${ikPose.roll}&pitch=${ikPose.pitch}&yaw=${ikPose.yaw}&time=${t}`, { method: 'POST', cache: 'no-store' });
    }

    // SYNCHRONIZED MOVE
    function copyCurrentToSync() {
        if (!latestTelemetry || !latestTelemetry.axes) return;
        latestTelemetry.axes.forEach((ax, i) => {
            document.getElementById(`sync-input-${i}`).value = ax.currentAngle.toFixed(2);
        });
        toast('Đã sao chép góc hiện tại vào bảng quay đồng bộ');
    }

    function sendSyncMove() {
        const j = [];
        for (let i = 0; i < 6; i++) {
            j.push(parseFloat(document.getElementById(`sync-input-${i}`).value) || 0.0);
        }
        const time = parseFloat(document.getElementById('sync-time').value) || 2.5;
        toast(`🚀 Quay đồng bộ 6 trục trong ${time}s`);
        fetch(`/api/all/goto?j1=${j[0]}&j2=${j[1]}&j3=${j[2]}&j4=${j[3]}&j5=${j[4]}&j6=${j[5]}&time=${time}`, { method: 'POST', cache: 'no-store' });
    }

    // WAYPOINTS
    async function fetchWaypoints() {
        try {
            const res = await fetch('/api/waypoint/list', { cache: 'no-store' });
            if (res.ok) {
                const d = await res.json();
                const tbody = document.getElementById('wp-table-body');
                let html = '';
                (d.waypoints || []).forEach((wp, i) => {
                    html += `<tr>
                        <td><b>${i + 1}</b></td>
                        <td>${wp.name}</td>
                        <td>[${wp.angles.map(a => a.toFixed(1)).join(', ')}]</td>
                        <td>${wp.duration.toFixed(1)}s</td>
                        <td>${wp.dwellMs}ms</td>
                        <td><button class="btn-ui" style="color:#fda4af;" onclick="deleteWaypoint(${i})">Xóa</button></td>
                    </tr>`;
                });
                tbody.innerHTML = html || '<tr><td colspan="6" style="text-align:center; color:#64748b;">Chưa có điểm lưu</td></tr>';
            }
        } catch(e) {}
    }

    async function teachCurrentPose() {
        const name = prompt('Đặt tên cho điểm dạy:', `P${Date.now().toString().slice(-4)}`);
        if (!name) return;
        const time = prompt('Thời gian di chuyển (giây):', '2.0');
        const dwell = prompt('Thời gian dừng chờ (dwell ms):', '500');
        await fetch(`/api/waypoint/add?name=${encodeURIComponent(name)}&time=${time || 2}&dwell=${dwell || 500}`, { method: 'POST' });
        toast(`Đã lưu điểm dạy: ${name}`);
        fetchWaypoints();
    }

    function startSequence(loop) {
        fetch(`/api/waypoint/start?loop=${loop ? 1 : 0}`, { method: 'POST' });
        toast(`▶ Phát chuỗi quỹ đạo ${loop ? '(Lặp)' : ''}`);
    }

    function stopSequence() {
        fetch('/api/waypoint/stop', { method: 'POST' });
        toast('⏹ Dừng chuỗi quỹ đạo');
    }

    // HARDWARE DIAGNOSTICS
    async function fetchDiagnostics() {
        try {
            const res = await fetch('/api/diagnostics', { cache: 'no-store' });
            if (res.ok) {
                const d = await res.json();
                const tbody = document.getElementById('diag-table-body');
                let html = '';
                (d.diagnostics || []).forEach((m, i) => {
                    html += `<tr>
                        <td><b>J${i + 1} (${AXIS_NAMES[i].split(' ')[1]})</b></td>
                        <td>0x${m.driver_version ? m.driver_version.toString(16).toUpperCase() : '00'} (${m.uart_ok ? '<span style="color:#34d399;">OK</span>' : '<span style="color:#f43f5e;">ERR</span>'})</td>
                        <td>${m.sg_result}</td>
                        <td>${m.over_temp ? '<span style="color:#f43f5e;">QUÁ NHIỆT</span>' : 'Bình thường'}</td>
                        <td>${m.as5600_ok ? '<span style="color:#34d399;">OK</span>' : '<span style="color:#f43f5e;">LỖI I2C</span>'}</td>
                        <td><b>${m.agc}</b> (${m.magnet_optimal ? 'Tối Ưu' : 'Cảnh Báo'})</td>
                    </tr>`;
                });
                tbody.innerHTML = html;
            }
        } catch(e) {}
    }

    function saveWifi() {
        const ssid = document.getElementById('wifi-ssid').value;
        const pass = document.getElementById('wifi-pass').value;
        if (!ssid) return alert('Vui lòng nhập SSID');
        fetch(`/api/wifi/save?ssid=${encodeURIComponent(ssid)}&pass=${encodeURIComponent(pass)}`, { method: 'POST' });
        toast(`Đã gửi thông tin Wi-Fi: ${ssid}`);
    }

    function startOtaUpload() {
        const fileInput = document.getElementById('ota-file');
        const file = fileInput.files[0];
        if (!file) { alert('Please select a .bin firmware file first.'); return; }
        if (!file.name.endsWith('.bin')) { alert('File must be a .bin firmware image.'); return; }
        if (!confirm(`Flash firmware "${file.name}" (${(file.size/1024).toFixed(1)} KB)?\nAll motion will stop during the update.`)) return;

        const progressWrap = document.getElementById('ota-progress-wrap');
        const progressBar  = document.getElementById('ota-progress-bar');
        const statusEl     = document.getElementById('ota-status');
        progressWrap.style.display = 'block';
        progressBar.style.width = '0%';
        statusEl.textContent = 'Uploading firmware...';
        statusEl.style.color = 'var(--text-dim)';

        const formData = new FormData();
        formData.append('firmware', file, file.name);

        const xhr = new XMLHttpRequest();
        xhr.open('POST', '/api/ota/update', true);

        xhr.upload.onprogress = (e) => {
            if (e.lengthComputable) {
                const pct = Math.round(e.loaded * 100 / e.total);
                progressBar.style.width = pct + '%';
                statusEl.textContent = `Uploading... ${pct}%`;
            }
        };
        xhr.onload = () => {
            if (xhr.status === 200) {
                progressBar.style.width = '100%';
                progressBar.style.background = 'var(--primary)';
                statusEl.textContent = '✅ OTA success! Device rebooting...';
                toast('Firmware uploaded! Device rebooting in ~5s');
            } else {
                progressBar.style.background = 'var(--danger)';
                statusEl.style.color = 'var(--danger)';
                statusEl.textContent = '❌ OTA failed: ' + xhr.responseText;
                toast('OTA update failed!');
            }
        };
        xhr.onerror = () => {
            // Connection drop is expected if device reboots before sending response
            progressBar.style.width = '100%';
            statusEl.textContent = '✅ Upload complete. Device rebooting...';
            toast('OTA complete — reconnect in a few seconds');
        };
        xhr.send(formData);
    }

    // CLI COMMAND
    function onCliKey(e) {
        if (e.key === 'Enter') sendCliCommand();
    }

    async function sendCliCommand() {
        const input = document.getElementById('cli-input');
        const cmd = input.value.trim();
        if (!cmd) return;
        logConsole(`> ${cmd}`);
        input.value = '';

        try {
            const res = await fetch(`/api/cli?cmd=${encodeURIComponent(cmd)}`, { method: 'POST', cache: 'no-store' });
            if (res.ok) {
                const data = await res.json();
                if (data.response) {
                    logConsole(`< ${data.response}`);
                }
                toast(`Lệnh: ${cmd}`);
            } else {
                logConsole(`❌ Lỗi gửi lệnh: HTTP ${res.status}`);
            }
        } catch (e) {
            logConsole(`⚠️ Không nhận được phản hồi: ${e.message}`);
        }
    }

    // TELEMETRY TELEGRAPH LOOP (0.5ms Zero-blocking In-Memory Polling)
    async function fetchTelemetry() {
        if (isFetching) return;
        isFetching = true;
        const ctrl = new AbortController();
        const tid = setTimeout(() => ctrl.abort(), 750);

        try {
            const res = await fetch('/api/status', { signal: ctrl.signal, cache: 'no-store' });
            clearTimeout(tid);
            if (res.ok) {
                const data = await res.json();
                latestTelemetry = data;
                renderTelemetry(data);
            }
        } catch(e) {}
        finally {
            isFetching = false;
        }
    }

    function renderTelemetry(d) {
        // Stream live Serial logs to Web Console
        if (d.logs && d.log_seq !== undefined) {
            if (window._lastLogSeq === undefined) {
                window._lastLogSeq = d.log_seq;
                d.logs.forEach(l => logConsole(l));
            } else if (d.log_seq > window._lastLogSeq) {
                const newCount = Math.min(d.log_seq - window._lastLogSeq, d.logs.length);
                const newItems = d.logs.slice(d.logs.length - newCount);
                newItems.forEach(l => logConsole(l));
                window._lastLogSeq = d.log_seq;
            }
        }

        // System state badge
        const stateBadge = document.getElementById('badge-sys-state');
        if (d.any_running) {
            stateBadge.className = 'badge badge-ok';
            stateBadge.innerText = 'MOVING (ĐANG CHẠY)';
        } else {
            stateBadge.className = 'badge badge-info';
            stateBadge.innerText = 'IDLE (ĐỨNG YÊN)';
        }

        // HUD TCP
        if (d.tcp) {
            document.getElementById('hud-x').innerText = d.tcp.x.toFixed(1);
            document.getElementById('hud-y').innerText = d.tcp.y.toFixed(1);
            document.getElementById('hud-z').innerText = d.tcp.z.toFixed(1);
            document.getElementById('hud-rpy').innerText = `${d.tcp.roll.toFixed(0)}°, ${d.tcp.pitch.toFixed(0)}°, ${d.tcp.yaw.toFixed(0)}°`;
        }

        // Render each joint row in the unified 6-axis strip
        (d.axes || []).forEach((ax, i) => {
            const row = document.getElementById(`joint-row-${i}`);
            if (!row) return;

            row.classList.toggle('running', ax.isRunning);

            const actEl = document.getElementById(`deg-actual-${i}`);
            const tgtEl = document.getElementById(`deg-target-${i}`);
            const errEl = document.getElementById(`deg-err-${i}`);
            const hBadge = document.getElementById(`badge-homed-${i}`);
            const uBadge = document.getElementById(`badge-uart-${i}`);

            if (actEl) actEl.innerText = `${ax.currentAngle.toFixed(2)}°`;
            if (tgtEl) tgtEl.innerText = `Tgt: ${ax.targetAngle.toFixed(2)}°`;
            if (errEl) errEl.innerText = `Err: ${ax.error.toFixed(2)}°`;

            if (hBadge) {
                hBadge.className = ax.isHomed ? 'badge badge-ok' : 'badge badge-warn';
                hBadge.innerText = ax.isHomed ? 'HOMED' : 'UNHOMED';
            }
            const dBadge = document.getElementById(`badge-dir-${i}`);
            if (dBadge) {
                dBadge.className = ax.dirInvert ? 'badge badge-warn' : 'badge badge-info';
                dBadge.innerText = ax.dirInvert ? 'REV' : 'FWD';
            }
            const cBadge = document.getElementById(`badge-calib-${i}`);
            if (cBadge) {
                cBadge.className = ax.isCalibrated ? 'badge badge-ok' : 'badge badge-warn';
                cBadge.innerText = ax.isCalibrated ? 'CALIB' : 'RAW';
            }
            if (uBadge) {
                uBadge.className = ax.uart_ok ? 'badge badge-ok' : 'badge badge-err';
                uBadge.innerText = ax.uart_ok ? 'UART' : 'UART ERR';
            }

            const mGear = document.getElementById(`menu-gear-${i}`);
            if (mGear) mGear.innerText = `⚙️ Tỉ Số Truyền: ${(ax.gearRatio !== undefined ? ax.gearRatio : 6.0).toFixed(2)} : 1`;

            const mHcurr = document.getElementById(`menu-hcurr-${i}`);
            if (mHcurr) mHcurr.innerText = `🎯 Dòng Homing: ${ax.homing_current || 750} mA`;

            const mCurr = document.getElementById(`menu-curr-${i}`);
            if (mCurr) mCurr.innerText = `⚡ Dòng Chạy RMS: ${ax.current || 800} mA`;

            const mHold = document.getElementById(`menu-hold-${i}`);
            if (mHold) mHold.innerText = `🔒 Khóa Vị Trí: ${ax.closedLoopHold ? 'BẬT (ON)' : 'TẮT (OFF)'}`;

            const mInv = document.getElementById(`menu-inv-${i}`);
            if (mInv) mInv.innerText = `🔀 Đảo Chiều: ${ax.dirInvert ? 'ĐẢO (REV)' : 'THUẬN (FWD)'}`;
        });

        // 3D Canvas render
        render3d(d.axes || []);
    }

    // DROPDOWN MENU UTILITIES
    function toggleJointMenu(e, axis) {
        e.stopPropagation();
        const menu = document.getElementById(`menu-dropdown-${axis}`);
        const isShown = menu.classList.contains('show');
        closeAllMenus();
        if (!isShown) menu.classList.add('show');
    }

    function closeAllMenus() {
        document.querySelectorAll('.dropdown-menu').forEach(m => m.classList.remove('show'));
    }

    document.addEventListener('click', closeAllMenus);

    // 3D KINEMATICS WIREFRAME ENGINE
    let cvs, ctx;
    let rotX = 20, rotY = 40;
    let dragging = false, lx = 0, ly = 0;

    function init3D() {
        cvs = document.getElementById('robot3d-canvas');
        if (!cvs) return;
        ctx = cvs.getContext('2d');
        cvs.width = cvs.parentElement.clientWidth;
        cvs.height = cvs.parentElement.clientHeight;

        cvs.onmousedown = (e) => { dragging = true; lx = e.clientX; ly = e.clientY; };
        window.onmouseup = () => dragging = false;
        window.onmousemove = (e) => {
            if (!dragging) return;
            rotY += (e.clientX - lx) * 0.6;
            rotX += (e.clientY - ly) * 0.6;
            lx = e.clientX; ly = e.clientY;
            if (latestTelemetry) render3d(latestTelemetry.axes || []);
        };
    }

    function projectPoint(x, y, z, cx, cy, sc) {
        const radY = (rotY * Math.PI) / 180;
        const radX = (rotX * Math.PI) / 180;

        const x1 = x * Math.cos(radY) - y * Math.sin(radY);
        const y1 = x * Math.sin(radY) + y * Math.cos(radY);
        const z1 = z;

        const y2 = y1 * Math.cos(radX) - z1 * Math.sin(radX);
        const z2 = y1 * Math.sin(radX) + z1 * Math.cos(radX);

        return { px: cx + x1 * sc, py: cy - z2 * sc };
    }

    function render3d(axes) {
        if (!ctx || !cvs) return;
        const w = cvs.width, h = cvs.height;
        ctx.clearRect(0, 0, w, h);

        const cx = w / 2, cy = h / 2 + 50, scale = 0.55;

        // Ground grid
        ctx.strokeStyle = '#1e293b';
        ctx.lineWidth = 1;
        for (let g = -160; g <= 160; g += 40) {
            const p1 = projectPoint(-160, g, 0, cx, cy, scale);
            const p2 = projectPoint(160, g, 0, cx, cy, scale);
            ctx.beginPath(); ctx.moveTo(p1.px, p1.py); ctx.lineTo(p2.px, p2.py); ctx.stroke();

            const p3 = projectPoint(g, -160, 0, cx, cy, scale);
            const p4 = projectPoint(g, 160, 0, cx, cy, scale);
            ctx.beginPath(); ctx.moveTo(p3.px, p3.py); ctx.lineTo(p4.px, p4.py); ctx.stroke();
        }

        // Kinematics Link positions
        const rad = Math.PI / 180;
        const t1 = (axes[0] ? axes[0].currentAngle : 0) * rad;
        const t2 = (axes[1] ? axes[1].currentAngle : 0) * rad;
        const t3 = (axes[2] ? axes[2].currentAngle : 0) * rad;
        const t4 = (axes[3] ? axes[3].currentAngle : 0) * rad;
        const t5 = (axes[4] ? axes[4].currentAngle : 0) * rad;

        const d1 = 139, a2 = 138, a3 = 88, d4 = 126;

        const p0 = { x: 0, y: 0, z: 0 };
        const p1 = { x: 0, y: 0, z: d1 };

        const r2 = a2 * Math.sin(t2);
        const z2 = d1 + a2 * Math.cos(t2);
        const p2 = { x: r2 * Math.cos(t1), y: r2 * Math.sin(t1), z: z2 };

        const r3 = r2 + a3 * Math.sin(t2 + t3);
        const z3 = z2 + a3 * Math.cos(t2 + t3);
        const p3 = { x: r3 * Math.cos(t1), y: r3 * Math.sin(t1), z: z3 };

        const r4 = r3 + d4 * Math.sin(t2 + t3 + t5);
        const z4 = z3 + d4 * Math.cos(t2 + t3 + t5);
        const p4 = { x: r4 * Math.cos(t1), y: r4 * Math.sin(t1), z: z4 };

        const pts = [p0, p1, p2, p3, p4].map(p => projectPoint(p.x, p.y, p.z, cx, cy, scale));

        // Links
        ctx.strokeStyle = '#38bdf8';
        ctx.lineWidth = 4;
        ctx.beginPath();
        ctx.moveTo(pts[0].px, pts[0].py);
        for (let i = 1; i < pts.length; i++) ctx.lineTo(pts[i].px, pts[i].py);
        ctx.stroke();

        // Joint spheres
        pts.forEach((pt, i) => {
            ctx.fillStyle = (i === pts.length - 1) ? '#f43f5e' : '#10b981';
            ctx.beginPath();
            ctx.arc(pt.px, pt.py, (i === pts.length - 1) ? 6 : 5, 0, Math.PI * 2);
            ctx.fill();
        });
    }

    // KEYBOARD SHORTCUTS
    window.addEventListener('keydown', (e) => {
        if (e.target.tagName === 'INPUT') return;
        if (e.key === ' ' || e.key === 'Escape') {
            e.preventDefault();
            sendEmergencyStop();
        }
    });

    // POLLING LOOP WITH ACTIVE USER BACKOFF (Zero socket congestion, 2.5Hz smooth refresh)
    async function pollingLoop() {
        if (Date.now() - lastActionTimestamp > 300) {
            await fetchTelemetry();
        }
        const delay = (Date.now() - lastActionTimestamp < 300) ? 500 : 400;
        setTimeout(pollingLoop, delay);
    }

    window.onload = () => {
        initJointStrip();
        init3D();
        fetchTelemetry();
        pollingLoop();
        logConsole("Hệ thống điều khiển 6-Axis Robot Studio đã khởi động thành công.");
    };
</script>

</body>
</html>
)rawliteral";

#endif // WEB_UI_H
