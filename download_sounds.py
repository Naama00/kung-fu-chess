import os
import wave
import struct
import math

# נתיב היעד ליצירת קבצי השמע
SOUNDS_DIR = os.path.join("assets", "sounds")

# ==============================================================================
# הגדרת סגנון הליכה - שנו את המשתנה הזה כדי לבחור את הסאונד שאתם הכי אוהבים!
# האפשרויות הזמינות: "felt" (מומלץ) , "whoosh" (רחף מודרני) , "steps" (צעדי עץ)
# ==============================================================================
WALK_STYLE = "felt" 


def create_wave_file(filename, samples, sample_rate=44100):
    filepath = os.path.join(SOUNDS_DIR, filename)
    if not os.path.exists(SOUNDS_DIR):
        os.makedirs(SOUNDS_DIR)
        
    print(f"Generating and writing {filename} (Style: {WALK_STYLE if filename == 'walk.wav' else 'default'})...")
    with wave.open(filepath, 'w') as wav_file:
        wav_file.setparams((1, 2, sample_rate, len(samples), "NONE", "not compressed"))
        packed_data = b""
        for s in samples:
            val = int(max(-32767, min(32767, s * 32767)))
            packed_data += struct.pack('<h', val)
        wav_file.writeframes(packed_data)
    print(f"Successfully saved to: {filepath}")

# --- סגנון 1: החלקת לבד ועץ אורגנית ורכה ---
def generate_felt_slide(sample_rate=44100):
    duration = 0.6  # לופ של 600ms
    num_samples = int(sample_rate * duration)
    samples = []
    
    seed = 98765
    prev_noise = 0.0
    lp_noise = 0.0
    
    for i in range(num_samples):
        t = i / sample_rate
        
        # יצירת רעש לבן (White Noise)
        seed = (1103515245 * seed + 12345) & 0xffffffff
        noise = ((seed % 2000) / 1000.0) - 1.0
        
        # 1. פילטר High-pass פשוט (מחסיר דגימות עוקבות) כדי להעלים רעשים נמוכים מעיקים
        hp_noise = noise - prev_noise
        prev_noise = noise
        
        # 2. פילטר Low-pass פשוט (ממוצע נע מחליק) כדי לרכך את התדרים הגבוהים שלא יישמעו כמו סטטיק
        lp_noise = lp_noise * 0.88 + hp_noise * 0.12
        
        # 3. תהודה עמומה וחלשה מאוד המדמה את חלל לוח העץ (130Hz)
        board_resonance = math.sin(2 * math.pi * 130 * t) * 0.02
        
        # אפנון מקצבי עדין (מדמה תנועת יד שאינה חדגונית לחלוטין)
        rhythm = 0.75 + 0.25 * math.sin(2 * math.pi * 5 * t)
        
        # מעטפת Fade-In/Out של 60ms כדי למנוע קליקים בממשק הלופ
        fade_len = int(sample_rate * 0.06)
        fade = 1.0
        if i < fade_len:
            fade = i / fade_len
        elif i > num_samples - fade_len:
            fade = (num_samples - i) / fade_len
            
        sample = (lp_noise * 0.22 + board_resonance) * rhythm * fade * 0.35
        samples.append(sample)
        
    return samples

# --- סגנון 2: רחף מודרני / רוח עדינה ---
def generate_whoosh_slide(sample_rate=44100):
    duration = 0.5  # לופ של 500ms
    num_samples = int(sample_rate * duration)
    samples = []
    
    seed = 54321
    lp_noise = 0.0
    
    for i in range(num_samples):
        t = i / sample_rate
        
        seed = (1103515245 * seed + 12345) & 0xffffffff
        noise = ((seed % 2000) / 1000.0) - 1.0
        
        # פילטר Low-pass עמוק לקבלת סאונד רך של רוח גולשת
        lp_noise = lp_noise * 0.96 + noise * 0.04
        
        # תדר סינוס עדין שנע במעגל של עליות וירידות תדר (Sweep)
        freq = 200 + 40 * math.sin(2 * math.pi * 4 * t)
        sine_wave = math.sin(2 * math.pi * freq * t) * 0.05
        
        fade_len = int(sample_rate * 0.05)
        fade = 1.0
        if i < fade_len:
            fade = i / fade_len
        elif i > num_samples - fade_len:
            fade = (num_samples - i) / fade_len
            
        sample = (lp_noise * 0.5 + sine_wave) * fade * 0.45
        samples.append(sample)
        
    return samples

# --- סגנון 3: צעדי עץ מהירים ומקצביים ---
def generate_steps_slide(sample_rate=44100):
    duration = 0.6  # לופ של 600ms
    num_samples = int(sample_rate * duration)
    samples = [0.0] * num_samples
    
    # 4 צעדים/הקשות מהירות במרווחי זמן קבועים בתוך הלופ
    step_times = [0.0, 0.15, 0.30, 0.45]
    
    for step_t in step_times:
        start_sample = int(step_t * sample_rate)
        for i in range(num_samples - start_sample):
            t = i / sample_rate
            envelope = math.exp(-25 * t)  # דעיכת הקשה מהירה
            if envelope < 0.001:
                break
                
            angle = 2 * math.pi * 220 * t
            val = math.sin(angle) * envelope * 0.15
            samples[start_sample + i] += val
            
    # החלשה קלה בקצוות הלופ
    fade_len = int(sample_rate * 0.03)
    for i in range(num_samples):
        if i < fade_len:
            samples[i] *= (i / fade_len)
        elif i > num_samples - fade_len:
            samples[i] *= ((num_samples - i) / fade_len)
            
    return samples

# --- מחוללי שאר צלילי המשחק (ללא שינוי) ---
def generate_move_sound(sample_rate=44100):
    duration = 0.08
    num_samples = int(sample_rate * duration)
    samples = []
    for i in range(num_samples):
        t = i / sample_rate
        envelope = math.exp(-15 * t)
        freq = 350 - (250 * (t / duration))
        angle = 2 * math.pi * freq * t
        samples.append(math.sin(angle) * envelope)
    return samples

def generate_capture_sound(sample_rate=44100):
    duration = 0.3
    num_samples = int(sample_rate * duration)
    samples = [0.0] * num_samples
    offsets = [0.0, 0.03, 0.06, 0.09]
    frequencies = [400, 320, 240, 160]
    seed = 12345
    for idx, offset in enumerate(offsets):
        start_sample = int(offset * sample_rate)
        freq_start = frequencies[idx]
        for i in range(num_samples - start_sample):
            t = i / sample_rate
            envelope = math.exp(-35 * t)
            if envelope < 0.001:
                break
            freq = freq_start - (80 * (t / 0.1))
            angle = 2 * math.pi * freq * t
            tone_val = math.sin(angle)
            seed = (1103515245 * seed + 12345) & 0xffffffff
            noise_val = ((seed % 2000) / 1000.0) - 1.0
            sample_val = (tone_val * 0.4 + noise_val * 0.6) * envelope * 0.7
            samples[start_sample + i] += sample_val
    max_val = max(abs(s) for s in samples) if samples else 0
    if max_val > 1.0:
        samples = [s / max_val for s in samples]
    return samples

def generate_game_over_sound(sample_rate=44100):
    samples = []
    dur1 = 0.15
    num_s1 = int(sample_rate * dur1)
    for i in range(num_s1):
        t = i / sample_rate
        envelope = math.exp(-4 * t)
        angle = 2 * math.pi * 300 * t
        samples.append(math.sin(angle) * envelope * 0.7)
    dur2 = 0.15
    num_s2 = int(sample_rate * dur2)
    for i in range(num_s2):
        t = i / sample_rate
        envelope = math.exp(-4 * t)
        angle = 2 * math.pi * 240 * t
        samples.append(math.sin(angle) * envelope * 0.7)
    dur3 = 0.4
    num_s3 = int(sample_rate * dur3)
    for i in range(num_s3):
        t = i / sample_rate
        envelope = math.exp(-2.5 * t)
        angle = 2 * math.pi * 180 * t
        samples.append(math.sin(angle) * envelope * 0.9)
    return samples

if __name__ == "__main__":
    print("--- Starting Chess Sounds Generator (Multi-Style Walk Update) ---")
    
    create_wave_file("move.wav", generate_move_sound())
    create_wave_file("capture.wav", generate_capture_sound())
    create_wave_file("game_over.wav", generate_game_over_sound())
    
    # הפעלת מחולל סגנון ההליכה שנבחר
    if WALK_STYLE == "felt":
        create_wave_file("walk.wav", generate_felt_slide())
    elif WALK_STYLE == "whoosh":
        create_wave_file("walk.wav", generate_whoosh_slide())
    elif WALK_STYLE == "steps":
        create_wave_file("walk.wav", generate_steps_slide())
    else:
        print(f"Error: Unknown style '{WALK_STYLE}'. Defaulting to 'felt'.")
        create_wave_file("walk.wav", generate_felt_slide())
        
    print("\n--- All sounds synthesized successfully! ---")