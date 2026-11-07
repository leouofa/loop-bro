#!/usr/bin/env python3
"""
build_amxd.py - Generates LoopBroControl.amxd Max for Live device
"""

import os
import json
import struct
import shutil

def generate_amxd():
    # Define Max Patcher JSON
    patcher = {
        "patcher": {
            "fileversion": 1,
            "appversion": {
                "major": 8,
                "minor": 6,
                "revision": 0,
                "architecture": "x64",
                "modernui": 1
            },
            "classnamespace": "box",
            "rect": [80.0, 80.0, 950.0, 720.0],
            "openrect": [0.0, 0.0, 765.0, 165.0],
            "bglocked": 0,
            "openinpresentation": 1,
            "default_fontsize": 11.0,
            "default_fontface": 0,
            "default_fontname": "Ableton Sans Medium Regular",
            "gridonopen": 1,
            "gridsize": [8.0, 8.0],
            "gridsnaponopen": 1,
            "objectsnaponopen": 1,
            "statusbarvisible": 2,
            "toolbarvisible": 1,
            "devicewidth": 765.0,
            "description": "Loop Bro M4L Controller & Dual Waveform Display",
            "tags": "LoopBro,Looper,Guitar,Utility",
            "boxes": [],
            "lines": []
        }
    }

    boxes = patcher["patcher"]["boxes"]
    lines = patcher["patcher"]["lines"]

    # Box helpers
    def add_box(id_name, maxclass, text="", patching_rect=[0,0,100,20], presentation=0, pres_rect=[0,0,100,20], extra=None):
        box_dict = {
            "id": id_name,
            "maxclass": maxclass,
            "numinlets": 1,
            "numoutlets": 1,
            "patching_rect": patching_rect
        }
        if text:
            box_dict["text"] = text
        if presentation:
            box_dict["presentation"] = 1
            box_dict["presentation_rect"] = pres_rect
        if extra:
            box_dict.update(extra)
        boxes.append({"box": box_dict})
        return id_name

    def add_line(src_id, src_out, dst_id, dst_in):
        lines.append({
            "patchline": {
                "source": [src_id, src_out],
                "destination": [dst_id, dst_in]
            }
        })

    # ==========================================================
    # 1. Audio IO & Oscilloscope
    # ==========================================================
    add_box("obj_plugin", "newobj", "plugin~", [30.0, 30.0, 54.0, 22.0], extra={"numoutlets": 2, "outlettype": ["signal", "signal"]})
    add_box("obj_plugout", "newobj", "plugout~", [30.0, 100.0, 54.0, 22.0], extra={"numinlets": 2})

    # Pass audio through
    add_line("obj_plugin", 0, "obj_plugout", 0)
    add_line("obj_plugin", 1, "obj_plugout", 1)

    # Live oscilloscope in presentation
    add_box("obj_scope", "live.scope~", "", [120.0, 100.0, 130.0, 60.0], presentation=1, pres_rect=[524.0, 30.0, 230.0, 120.0],
            extra={
                "numinlets": 2,
                "fgcolor": [0.04, 0.85, 1.0, 1.0],
                "bgcolor": [0.11, 0.12, 0.15, 1.0],
                "samples": 512
            })
    add_line("obj_plugin", 0, "obj_scope", 0)
    add_line("obj_plugin", 1, "obj_scope", 1)

    # ==========================================================
    # 2. OSC Core (udpreceive 9002 & udpsend 127.0.0.1 9001)
    # ==========================================================
    add_box("obj_udprcv", "newobj", "udpreceive 9002", [300.0, 30.0, 105.0, 22.0], extra={"numoutlets": 1, "outlettype": [""]})
    add_box("obj_udpsnd", "newobj", "udpsend 127.0.0.1 9001", [680.0, 480.0, 135.0, 22.0])

    # Route top-level /loopbro
    add_box("obj_route_lb", "newobj", "route /loopbro", [300.0, 70.0, 100.0, 22.0], extra={"numoutlets": 2, "outlettype": ["", ""]})
    add_line("obj_udprcv", 0, "obj_route_lb", 0)

    # Sub-route addresses
    add_box("obj_route_sub", "newobj", "route state state_name playhead meter waveform param", [300.0, 110.0, 310.0, 22.0],
            extra={"numoutlets": 7, "outlettype": ["", "", "", "", "", "", ""]})
    add_line("obj_route_lb", 0, "obj_route_sub", 0)

    # ==========================================================
    # 3. Waveform Display (buffer~ & waveform~)
    # ==========================================================
    add_box("obj_buffer", "newobj", "buffer~ loopbro_take 1000 2", [300.0, 260.0, 160.0, 22.0])
    add_box("obj_info", "newobj", "info~ loopbro_take", [470.0, 260.0, 120.0, 22.0], extra={"numoutlets": 8, "outlettype": ["float","float","float","float","float","float","float","float"]})

    add_box("obj_waveform", "waveform~", "", [300.0, 300.0, 200.0, 80.0], presentation=1, pres_rect=[254.0, 30.0, 260.0, 120.0],
            extra={
                "buffername": "loopbro_take",
                "bgcolor": [0.11, 0.12, 0.15, 1.0],
                "waveformcolor": [0.04, 0.65, 1.0, 0.85],
                "selectcolor": [0.15, 0.35, 0.6, 0.3],
                "cursorcolor": [1.0, 0.8, 0.1, 0.95],
                "setmode": 0,
                "quiet": 1
            })

    # Route waveform messages: "file" and "cleared"
    add_box("obj_route_wave", "newobj", "route file cleared", [450.0, 150.0, 115.0, 22.0], extra={"numoutlets": 3, "outlettype": ["", "", ""]})
    add_line("obj_route_sub", 4, "obj_route_wave", 0)

    # Replace buffer file
    add_box("obj_prep_read", "newobj", "prepend replace", [450.0, 190.0, 95.0, 22.0])
    add_line("obj_route_wave", 0, "obj_prep_read", 0)
    add_line("obj_prep_read", 0, "obj_buffer", 0)
    add_line("obj_prep_read", 0, "obj_info", 0) # bang info~ on reload

    # Clear buffer on cleared
    add_box("obj_clear_msg", "message", "clear", [540.0, 190.0, 50.0, 22.0])
    add_line("obj_route_wave", 1, "obj_clear_msg", 0)
    add_line("obj_clear_msg", 0, "obj_buffer", 0)

    # Playhead indicator logic:
    # route playhead outlet (idx 2): [normPos, currentSamples, totalSamples]
    # In waveform~: sending "line $1" where $1 is milliseconds draws a vertical line at $1 ms.
    # total duration (ms) comes from info~ outlet 6 (0-indexed outlet 6 = duration in ms)
    # playhead pos (ms) = normPos * durationMs
    add_box("obj_unpack_playhead", "newobj", "unpack 0. 0 0", [380.0, 150.0, 100.0, 22.0], extra={"numoutlets": 3, "outlettype": ["float","int","int"]})
    add_line("obj_route_sub", 2, "obj_unpack_playhead", 0)

    # Value storage for total duration ms (updated by info~ outlet 6)
    add_box("obj_val_dur", "newobj", "value loopbro_dur_ms", [470.0, 300.0, 130.0, 22.0])
    add_line("obj_info", 6, "obj_val_dur", 0)

    add_box("obj_calc_cursor", "newobj", "expr $f1 * $f2", [380.0, 190.0, 110.0, 22.0])
    add_line("obj_unpack_playhead", 0, "obj_calc_cursor", 0)
    add_line("obj_info", 6, "obj_calc_cursor", 1)

    add_box("obj_prep_line", "newobj", "prepend line", [380.0, 225.0, 75.0, 22.0])
    add_line("obj_calc_cursor", 0, "obj_prep_line", 0)
    add_line("obj_prep_line", 0, "obj_waveform", 0)

    # ==========================================================
    # 4. State & Labels in Presentation
    # ==========================================================
    # Title Label
    add_box("obj_lbl_title", "live.comment", "LOOP BRO CONTROL", [30.0, 300.0, 120.0, 18.0],
            presentation=1, pres_rect=[10.0, 10.0, 130.0, 18.0],
            extra={"fontname": "Ableton Sans Bold Regular", "fontsize": 11.0, "textcolor": [1.0, 1.0, 1.0, 0.95]})

    # Waveform Section Label
    add_box("obj_lbl_wave", "live.comment", "CAPTURED RIFF", [300.0, 240.0, 120.0, 18.0],
            presentation=1, pres_rect=[254.0, 10.0, 130.0, 18.0],
            extra={"fontname": "Ableton Sans Bold Regular", "fontsize": 10.0, "textcolor": [0.3, 0.7, 1.0, 0.9]})

    # Scope Section Label
    add_box("obj_lbl_scope", "live.comment", "LIVE SCOPE", [520.0, 240.0, 120.0, 18.0],
            presentation=1, pres_rect=[524.0, 10.0, 130.0, 18.0],
            extra={"fontname": "Ableton Sans Bold Regular", "fontsize": 10.0, "textcolor": [0.04, 0.85, 1.0, 0.9]})

    # State Status Badge
    add_box("obj_state_badge", "live.comment", "IDLE", [200.0, 150.0, 90.0, 18.0],
            presentation=1, pres_rect=[148.0, 10.0, 92.0, 18.0],
            extra={"fontname": "Ableton Sans Bold Regular", "fontsize": 10.0, "justification": 1, "textcolor": [1.0, 0.75, 0.1, 1.0]})

    # Format state name to badge
    add_box("obj_prep_set", "newobj", "prepend set", [200.0, 190.0, 80.0, 22.0])
    add_line("obj_route_sub", 1, "obj_prep_set", 0) # state_name outlet
    add_line("obj_prep_set", 0, "obj_state_badge", 0)

    # ==========================================================
    # 5. Buttons (ARM, PLAY, CLEAR)
    # ==========================================================
    # ARM button (live.text mode=1)
    add_box("obj_btn_arm", "live.text", "", [30.0, 340.0, 60.0, 20.0],
            presentation=1, pres_rect=[10.0, 32.0, 72.0, 22.0],
            extra={
                "text": "ARM", "texton": "DISARM", "mode": 1,
                "bgoncolor": [1.0, 0.6, 0.0, 1.0],
                "activebgoncolor": [1.0, 0.6, 0.0, 1.0]
            })
    add_box("obj_msg_arm", "newobj", "prepend /loopbro/arm", [30.0, 370.0, 130.0, 22.0])
    add_line("obj_btn_arm", 0, "obj_msg_arm", 0)
    add_line("obj_msg_arm", 0, "obj_udpsnd", 0)

    # PLAY button (live.text mode=1)
    add_box("obj_btn_play", "live.text", "", [100.0, 340.0, 60.0, 20.0],
            presentation=1, pres_rect=[86.0, 32.0, 78.0, 22.0],
            extra={
                "text": "PLAY LOOP", "texton": "STOP LOOP", "mode": 1,
                "bgoncolor": [0.2, 0.8, 0.35, 1.0],
                "activebgoncolor": [0.2, 0.8, 0.35, 1.0]
            })
    add_box("obj_msg_play", "newobj", "prepend /loopbro/play", [100.0, 370.0, 130.0, 22.0])
    add_line("obj_btn_play", 0, "obj_msg_play", 0)
    add_line("obj_msg_play", 0, "obj_udpsnd", 0)

    # CLEAR button (live.text mode=0)
    add_box("obj_btn_clear", "live.text", "", [170.0, 340.0, 60.0, 20.0],
            presentation=1, pres_rect=[168.0, 32.0, 72.0, 22.0],
            extra={
                "text": "CLEAR", "mode": 0,
                "bgcolor": [0.35, 0.16, 0.18, 1.0],
                "textcolor": [1.0, 0.45, 0.45, 1.0]
            })
    add_box("obj_msg_clear", "message", "/loopbro/clear", [170.0, 370.0, 95.0, 22.0])
    add_line("obj_btn_clear", 0, "obj_msg_clear", 0)
    add_line("obj_msg_clear", 0, "obj_udpsnd", 0)

    # State update to button visual sync:
    # state 0: idle -> arm=0, play=0
    # state 1: armed -> arm=1, play=0
    # state 2: recording -> arm=1, play=0
    # state 3: captured -> arm=0, play=0
    # state 4: playing -> arm=0, play=1
    add_box("obj_sel_state", "newobj", "select 0 1 2 3 4", [200.0, 230.0, 110.0, 22.0], extra={"numoutlets": 6, "outlettype": ["bang","bang","bang","bang","bang",""]})
    add_line("obj_route_sub", 0, "obj_sel_state", 0) # state outlet

    add_box("obj_state0", "message", "set 0", [170.0, 270.0, 40.0, 22.0])
    add_box("obj_state1", "message", "set 1", [220.0, 270.0, 40.0, 22.0])

    add_line("obj_sel_state", 0, "obj_state0", 0) # idle -> arm=0, play=0
    add_line("obj_sel_state", 1, "obj_state1", 0) # armed -> arm=1
    add_line("obj_sel_state", 2, "obj_state1", 0) # recording -> arm=1
    add_line("obj_sel_state", 3, "obj_state0", 0) # captured -> arm=0, play=0
    add_line("obj_sel_state", 4, "obj_state0", 0) # playing -> arm=0, play=1

    add_line("obj_state0", 0, "obj_btn_arm", 0)
    add_line("obj_state1", 0, "obj_btn_arm", 0)

    add_box("obj_play0", "message", "set 0", [270.0, 270.0, 40.0, 22.0])
    add_box("obj_play1", "message", "set 1", [320.0, 270.0, 40.0, 22.0])
    add_line("obj_sel_state", 0, "obj_play0", 0)
    add_line("obj_sel_state", 1, "obj_play0", 0)
    add_line("obj_sel_state", 2, "obj_play0", 0)
    add_line("obj_sel_state", 3, "obj_play0", 0)
    add_line("obj_sel_state", 4, "obj_play1", 0)
    add_line("obj_play0", 0, "obj_btn_play", 0)
    add_line("obj_play1", 0, "obj_btn_play", 0)

    # ==========================================================
    # 6. Knobs (live.dial) - Parameters
    # ==========================================================
    # Route params
    add_box("obj_route_params", "newobj", "route startThresholdDb stopThresholdDb preRollMs stopHoldMs loopGainDb inputThru",
            [620.0, 150.0, 310.0, 22.0],
            extra={"numoutlets": 7, "outlettype": ["","","","","","",""]})
    add_line("obj_route_sub", 5, "obj_route_params", 0)

    # 1. Start Threshold (-80 to 0 dB, def -30)
    add_box("obj_dial_start", "live.dial", "", [30.0, 420.0, 44.0, 48.0],
            presentation=1, pres_rect=[10.0, 62.0, 44.0, 48.0],
            extra={
                "parameter_enable": 1,
                "saved_attribute_attributes": {
                    "valueof": {
                        "parameter_shortname": "Start Thr",
                        "parameter_longname": "Start Threshold",
                        "parameter_type": 0,
                        "parameter_mmin": -80.0,
                        "parameter_mmax": 0.0,
                        "parameter_initial": -30.0,
                        "parameter_unitstyle": 4
                    }
                }
            })
    add_box("obj_msg_param_start", "newobj", "prepend /loopbro/param/startThresholdDb", [30.0, 480.0, 205.0, 22.0])
    add_line("obj_dial_start", 0, "obj_msg_param_start", 0)
    add_line("obj_msg_param_start", 0, "obj_udpsnd", 0)

    add_box("obj_set_start", "newobj", "prepend set", [620.0, 190.0, 75.0, 22.0])
    add_line("obj_route_params", 0, "obj_set_start", 0)
    add_line("obj_set_start", 0, "obj_dial_start", 0)

    # 2. Stop Threshold (-80 to 0 dB, def -48)
    add_box("obj_dial_stop", "live.dial", "", [80.0, 420.0, 44.0, 48.0],
            presentation=1, pres_rect=[57.0, 62.0, 44.0, 48.0],
            extra={
                "parameter_enable": 1,
                "saved_attribute_attributes": {
                    "valueof": {
                        "parameter_shortname": "Stop Thr",
                        "parameter_longname": "Stop Threshold",
                        "parameter_type": 0,
                        "parameter_mmin": -80.0,
                        "parameter_mmax": 0.0,
                        "parameter_initial": -48.0,
                        "parameter_unitstyle": 4
                    }
                }
            })
    add_box("obj_msg_param_stop", "newobj", "prepend /loopbro/param/stopThresholdDb", [80.0, 510.0, 205.0, 22.0])
    add_line("obj_dial_stop", 0, "obj_msg_param_stop", 0)
    add_line("obj_msg_param_stop", 0, "obj_udpsnd", 0)

    add_box("obj_set_stop", "newobj", "prepend set", [670.0, 190.0, 75.0, 22.0])
    add_line("obj_route_params", 1, "obj_set_stop", 0)
    add_line("obj_set_stop", 0, "obj_dial_stop", 0)

    # 3. Pre-Roll (0 to 2000 ms, def 50)
    add_box("obj_dial_preroll", "live.dial", "", [130.0, 420.0, 44.0, 48.0],
            presentation=1, pres_rect=[104.0, 62.0, 44.0, 48.0],
            extra={
                "parameter_enable": 1,
                "saved_attribute_attributes": {
                    "valueof": {
                        "parameter_shortname": "Pre-Roll",
                        "parameter_longname": "Pre-Roll Time",
                        "parameter_type": 0,
                        "parameter_mmin": 0.0,
                        "parameter_mmax": 2000.0,
                        "parameter_initial": 50.0,
                        "parameter_unitstyle": 2
                    }
                }
            })
    add_box("obj_msg_param_preroll", "newobj", "prepend /loopbro/param/preRollMs", [130.0, 540.0, 185.0, 22.0])
    add_line("obj_dial_preroll", 0, "obj_msg_param_preroll", 0)
    add_line("obj_msg_param_preroll", 0, "obj_udpsnd", 0)

    add_box("obj_set_preroll", "newobj", "prepend set", [720.0, 190.0, 75.0, 22.0])
    add_line("obj_route_params", 2, "obj_set_preroll", 0)
    add_line("obj_set_preroll", 0, "obj_dial_preroll", 0)

    # 4. Stop Hold (50 to 10000 ms, def 500)
    add_box("obj_dial_stophold", "live.dial", "", [180.0, 420.0, 44.0, 48.0],
            presentation=1, pres_rect=[151.0, 62.0, 44.0, 48.0],
            extra={
                "parameter_enable": 1,
                "saved_attribute_attributes": {
                    "valueof": {
                        "parameter_shortname": "Stop Hold",
                        "parameter_longname": "Stop Hold Time",
                        "parameter_type": 0,
                        "parameter_mmin": 50.0,
                        "parameter_mmax": 10000.0,
                        "parameter_initial": 500.0,
                        "parameter_unitstyle": 2
                    }
                }
            })
    add_box("obj_msg_param_stophold", "newobj", "prepend /loopbro/param/stopHoldMs", [180.0, 570.0, 190.0, 22.0])
    add_line("obj_dial_stophold", 0, "obj_msg_param_stophold", 0)
    add_line("obj_msg_param_stophold", 0, "obj_udpsnd", 0)

    add_box("obj_set_stophold", "newobj", "prepend set", [770.0, 190.0, 75.0, 22.0])
    add_line("obj_route_params", 3, "obj_set_stophold", 0)
    add_line("obj_set_stophold", 0, "obj_dial_stophold", 0)

    # 5. Loop Level (-60 to +6 dB, def 0)
    add_box("obj_dial_level", "live.dial", "", [230.0, 420.0, 44.0, 48.0],
            presentation=1, pres_rect=[198.0, 62.0, 44.0, 48.0],
            extra={
                "parameter_enable": 1,
                "saved_attribute_attributes": {
                    "valueof": {
                        "parameter_shortname": "Loop Lvl",
                        "parameter_longname": "Loop Level",
                        "parameter_type": 0,
                        "parameter_mmin": -60.0,
                        "parameter_mmax": 6.0,
                        "parameter_initial": 0.0,
                        "parameter_unitstyle": 4
                    }
                }
            })
    add_box("obj_msg_param_level", "newobj", "prepend /loopbro/param/loopGainDb", [230.0, 600.0, 190.0, 22.0])
    add_line("obj_dial_level", 0, "obj_msg_param_level", 0)
    add_line("obj_msg_param_level", 0, "obj_udpsnd", 0)

    add_box("obj_set_level", "newobj", "prepend set", [820.0, 190.0, 75.0, 22.0])
    add_line("obj_route_params", 4, "obj_set_level", 0)
    add_line("obj_set_level", 0, "obj_dial_level", 0)

    # 6. Live Thru toggle
    add_box("obj_btn_thru", "live.text", "", [30.0, 640.0, 75.0, 20.0],
            presentation=1, pres_rect=[10.0, 122.0, 80.0, 22.0],
            extra={
                "text": "THRU OFF", "texton": "THRU ON", "mode": 1,
                "bgoncolor": [0.04, 0.52, 1.0, 1.0],
                "activebgoncolor": [0.04, 0.52, 1.0, 1.0]
            })
    add_box("obj_msg_param_thru", "newobj", "prepend /loopbro/param/inputThru", [30.0, 670.0, 185.0, 22.0])
    add_line("obj_btn_thru", 0, "obj_msg_param_thru", 0)
    add_line("obj_msg_param_thru", 0, "obj_udpsnd", 0)

    add_box("obj_set_thru", "newobj", "prepend set", [870.0, 190.0, 75.0, 22.0])
    add_line("obj_route_params", 5, "obj_set_thru", 0)
    add_line("obj_set_thru", 0, "obj_btn_thru", 0)

    # Sync Button & Loadbang
    add_box("obj_btn_sync", "live.text", "", [120.0, 640.0, 60.0, 20.0],
            presentation=1, pres_rect=[96.0, 122.0, 64.0, 22.0],
            extra={"text": "SYNC", "mode": 0})
    add_box("obj_loadbang", "newobj", "loadbang", [200.0, 640.0, 60.0, 22.0])
    add_box("obj_msg_query", "message", "/loopbro/query", [120.0, 670.0, 95.0, 22.0])

    add_line("obj_btn_sync", 0, "obj_msg_query", 0)
    add_line("obj_loadbang", 0, "obj_msg_query", 0)
    add_line("obj_msg_query", 0, "obj_udpsnd", 0)

    # OSC Port Label
    add_box("obj_lbl_ports", "live.comment", "UDP: 9001/9002", [280.0, 640.0, 100.0, 18.0],
            presentation=1, pres_rect=[166.0, 124.0, 76.0, 18.0],
            extra={"fontname": "Ableton Sans Medium Regular", "fontsize": 9.0, "textcolor": [0.6, 0.65, 0.7, 0.7]})

    return patcher

def save_amxd(patcher, output_path):
    json_str = json.dumps(patcher, indent=4)
    json_bytes = json_str.encode('utf-8')

    # AMXD standard header (32 bytes)
    header = b'ampf\x04\x00\x00\x00aaaameta\x04\x00\x00\x00\x00\x00\x00\x00ptch' + struct.pack('<I', len(json_bytes))
    full_data = header + json_bytes

    os.makedirs(os.path.dirname(os.path.abspath(output_path)), exist_ok=True)
    with open(output_path, 'wb') as f:
        f.write(full_data)
    print(f"Successfully generated AMXD device: {output_path} ({len(full_data)} bytes)")

if __name__ == "__main__":
    repo_amxd = "/Users/leonid/src/loop-bro/m4l/LoopBroControl.amxd"
    ableton_user_lib_dir = "/Users/leonid/Music/Ableton/User Library/Presets/Audio Effects/Max Audio Effect"
    ableton_amxd = os.path.join(ableton_user_lib_dir, "LoopBroControl.amxd")

    p = generate_amxd()
    save_amxd(p, repo_amxd)

    if os.path.isdir(ableton_user_lib_dir):
        shutil.copy2(repo_amxd, ableton_amxd)
        print(f"Installed to Ableton User Library: {ableton_amxd}")
    else:
        print(f"Ableton User Library directory not found: {ableton_user_lib_dir}")
