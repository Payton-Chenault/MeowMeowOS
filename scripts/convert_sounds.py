#!/usr/bin/env python3
"""
tools/convert_sounds.py
Scans the scripts/sound_assets/ directory for audio files (.wav, .ogg),
formats/converts them into standard 16-bit 44.1kHz Stereo PCM WAV files,
and flattens them into a staging directory for the OS installer to dynamically organize.
"""

import os
import sys
import wave
import shutil
import subprocess

# Standardized Logging Implementation matching the C kernel structure
def log_trace(msg):   print(f"[TRACE] SOUND_CONVERTER: {msg}")
def log_debug(msg):   print(f"[DEBUG] SOUND_CONVERTER: {msg}")
def log_info(msg):    print(f"[INFO]  SOUND_CONVERTER: {msg}")
def log_warning(msg): print(f"[WARN]  SOUND_CONVERTER: {msg}")
def log_error(msg):   print(f"[ERROR] SOUND_CONVERTER: {msg}")

def process_audio_file(src_path, dst_path, is_ogg=False):
    log_info(f"Staging audio asset {src_path} -> {dst_path}")
    os.makedirs(os.path.dirname(dst_path), exist_ok=True)
    
    if is_ogg:
        log_debug(f"Detected .ogg file, invoking ffmpeg for conversion to 16-bit 44.1kHz Stereo PCM WAV")
        try:
            # Force overwrite (-y), convert to 44100Hz (-ar), 2 channels (-ac), 16-bit PCM (-c:a pcm_s16le)
            result = subprocess.run([
                'ffmpeg', '-y', '-i', src_path, 
                '-ar', '44100', '-ac', '2', '-c:a', 'pcm_s16le', dst_path
            ], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            
            if result.returncode != 0:
                log_error(f"ffmpeg conversion failed for {src_path}: {result.stderr.decode()}")
            else:
                log_trace(f"Successfully converted {src_path} to WAV using ffmpeg")
        except FileNotFoundError:
            log_error("ffmpeg is not installed or not in PATH. Required for .ogg conversion.")
        return

    try:
        with wave.open(src_path, 'rb') as w_in:
            n_channels = w_in.getnchannels()
            sampwidth = w_in.getsampwidth()
            framerate = w_in.getframerate()
            n_frames = w_in.getnframes()
            audio_frames = w_in.readframes(n_frames)

            if n_channels == 2 and sampwidth == 2 and framerate == 44100:
                log_trace(f"File {src_path} is already compliant, copying directly.")
                shutil.copyfile(src_path, dst_path)
                return

            log_debug(f"Formatting .wav file {src_path} to 16-bit 44.1kHz Stereo PCM")
            if n_channels == 1 and sampwidth == 2:
                stereo_frames = bytearray()
                for i in range(0, len(audio_frames), 2):
                    sample = audio_frames[i:i+2]
                    stereo_frames.extend(sample)
                    stereo_frames.extend(sample)
                audio_frames = bytes(stereo_frames)
                n_channels = 2

            with wave.open(dst_path, 'wb') as w_out:
                w_out.setnchannels(n_channels)
                w_out.setsampwidth(sampwidth)
                w_out.setframerate(framerate)
                w_out.writeframes(audio_frames)
            log_trace(f"Successfully formatted {src_path}")

    except Exception as e:
        log_warning(f"Fallback direct copy for {src_path}: {e}")
        shutil.copyfile(src_path, dst_path)

def main():
    src_dir = sys.argv[1] if len(sys.argv) > 1 else "scripts/sound_assets"
    out_dir = sys.argv[2] if len(sys.argv) > 2 else "build/staged_sounds"

    if not os.path.exists(src_dir):
        log_warning(f"Source sound assets directory '{src_dir}' not found.")
        return 0

    os.makedirs(out_dir, exist_ok=True)

    for root, _, files in os.walk(src_dir):
        for f in files:
            is_ogg = f.lower().endswith(".ogg")
            is_wav = f.lower().endswith(".wav")
            
            if not (is_wav or is_ogg):
                continue

            file_path = os.path.join(root, f)
            # Strip the 4-character extension (.wav or .ogg)
            base_name = f[:-4]

            # We save it as a flat .wav file in the staging directory.
            # install.elf will parse the name and categorize it on the OS side.
            dst_subpath = os.path.join(out_dir, f"{base_name}.wav")

            process_audio_file(file_path, dst_subpath, is_ogg=is_ogg)

    log_info("Sound assets processed and staged dynamically successfully.")
    return 0

if __name__ == "__main__":
    sys.exit(main())