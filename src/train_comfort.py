import os
import sys
import numpy as np
import pandas as pd
import tensorflow as tf
from sklearn.utils.class_weight import compute_class_weight

if sys.stdout.encoding != 'utf-8':
    try:
        sys.stdout.reconfigure(encoding='utf-8')
    except Exception:
        pass

# ==========================================
# 1. ĐỌC DỮ LIỆU TỪ ASHRAE DATABASE II
# ==========================================
csv_path = "ashrae_db2.01.csv"
if not os.path.exists(csv_path):
    csv_path = os.path.join("src", "ashrae_db2.01.csv")

if not os.path.exists(csv_path):
    raise FileNotFoundError(f"Không tìm thấy file {csv_path}")

print(f"--> [1/6] Đang đọc dữ liệu từ {csv_path}...")
df_raw = pd.read_csv(csv_path, low_memory=False)

# Lấy các cột cốt lõi
cols = ['Air temperature (C)', 'Relative humidity (%)']
df = df_raw[cols].dropna().copy()

# Lọc các giá trị vật lý hợp lệ
df = df[(df['Air temperature (C)'] >= 10.0) & (df['Air temperature (C)'] <= 45.0)]
df = df[(df['Relative humidity (%)'] >= 15.0) & (df['Relative humidity (%)'] <= 100.0)]

# ==========================================
# 2. TÍNH TOÁN CHỈ SỐ NHIỆT & GÁN NHÃN THEO README
# ==========================================
print("--> [2/6] Tính toán Heat Index và phân loại 4 lớp tiện nghi nhiệt...")

def compute_heat_index(t, h):
    tf_val = t * 1.8 + 32.0
    if tf_val >= 80.0:
        hi_f = (-42.379 + 2.04901523 * tf_val + 10.14333127 * h
                - 0.22475541 * tf_val * h - 0.00683783 * (tf_val ** 2)
                - 0.05481717 * (h ** 2) + 0.00122874 * (tf_val ** 2) * h
                + 0.00085282 * tf_val * (h ** 2) - 0.00000199 * (tf_val ** 2) * (h ** 2))
        return (hi_f - 32.0) / 1.8
    return t

def map_comfort_class(t, h):
    hi_c = compute_heat_index(t, h)
    
    # Class 0: COLD (T < 22°C)
    if t < 22.0:
        return 0
    # Class 3: HOT (T >= 32°C hoặc HI >= 33°C)
    elif t >= 32.0 or hi_c >= 33.0:
        return 3
    # Class 2: WARM_HUMID (H >= 75% & T >= 26.5°C, hoặc 27.5°C <= T < 32°C, hoặc HI >= 28.5°C)
    elif (h >= 75.0 and t >= 26.5) or (t >= 27.5 and t < 32.0) or (hi_c >= 28.5):
        return 2
    # Class 1: COMFORT (22°C <= T < 27.5°C)
    else:
        return 1

df['comfort_class'] = [map_comfort_class(t, h) for t, h in zip(df['Air temperature (C)'], df['Relative humidity (%)'])]
label_map = {0: 'COLD', 1: 'COMFORT', 2: 'WARM_HUMID', 3: 'HOT'}
df['comfort_label'] = df['comfort_class'].map(label_map)

df.rename(columns={
    'Air temperature (C)': 'temperature_c',
    'Relative humidity (%)': 'humidity_pct'
}, inplace=True)

# Mô phỏng vi sai thời gian thực giữa 2 chu kỳ đo liên tiếp (2 giây) trên ESP32
np.random.seed(42)
df['delta_temp'] = np.random.normal(0, 0.15, size=len(df)).clip(-1.5, 1.5)
df['delta_humi'] = np.random.normal(0, 0.4, size=len(df)).clip(-5.0, 5.0)

# Cân bằng dữ liệu (Balanced Sampling: 5.000 mẫu cho mỗi class -> 20.000 mẫu tổng cộng)
dfs = []
for c in range(4):
    sub = df[df['comfort_class'] == c]
    dfs.append(sub.sample(n=min(5000, len(sub)), random_state=42, replace=(len(sub) < 5000)))
df_clean = pd.concat(dfs).sample(frac=1.0, random_state=42).reset_index(drop=True)

# ==========================================
# 3. CHUẨN HÓA DỮ LIỆU & XUẤT CSV
# ==========================================
print("--> [3/6] Chuẩn hóa đặc trưng & xuất 2 file CSV...")
features = ['temperature_c', 'humidity_pct', 'delta_temp', 'delta_humi']

means = df_clean[features].mean()
stds = df_clean[features].std()

df_normalized = df_clean.copy()
for col in features:
    df_normalized[f'{col}_norm'] = (df_clean[col] - means[col]) / stds[col]

# Xuất ra thư mục src nếu chạy từ root
out_dir = "src" if os.path.isdir("src") else "."
df_clean[['temperature_c', 'humidity_pct', 'delta_temp', 'delta_humi', 'comfort_class', 'comfort_label']].to_csv(
    os.path.join(out_dir, "ashrae_cleaned_comfort.csv"), index=False, encoding='utf-8'
)
norm_cols = ['temperature_c_norm', 'humidity_pct_norm', 'delta_temp_norm', 'delta_humi_norm', 'comfort_class', 'comfort_label']
df_normalized[norm_cols].to_csv(
    os.path.join(out_dir, "ashrae_normalized_comfort.csv"), index=False, encoding='utf-8'
)

print(f"    - Đã lưu {os.path.join(out_dir, 'ashrae_cleaned_comfort.csv')}")
print(f"    - Đã lưu {os.path.join(out_dir, 'ashrae_normalized_comfort.csv')}")
print("Phân bố số lượng các lớp đã cân bằng:")
for c in range(4):
    print(f"   Class {c} ({label_map[c]}): {(df_clean['comfort_class'] == c).sum()} mẫu")

# ==========================================
# 4. HUẤN LUYỆN MẠNG NƠ-RON TINYML
# ==========================================
print("\n--> [4/6] Bắt đầu huấn luyện mô hình Keras...")
X = df_clean[features].values.astype(np.float32)
y = df_clean['comfort_class'].values.astype(np.int32)

# Nhúng layer Normalization trực tiếp vào Model
norm_layer = tf.keras.layers.Normalization(axis=-1)
norm_layer.adapt(X)

model = tf.keras.Sequential([
    tf.keras.layers.Input(shape=(4,)),
    norm_layer,
    tf.keras.layers.Dense(32, activation='relu'),
    tf.keras.layers.Dense(16, activation='relu'),
    tf.keras.layers.Dense(4, activation='softmax')
])

model.compile(optimizer=tf.keras.optimizers.Adam(learning_rate=0.003),
              loss='sparse_categorical_crossentropy',
              metrics=['accuracy'])

class_weights = compute_class_weight('balanced', classes=np.unique(y), y=y)
weight_dict = dict(enumerate(class_weights))

model.fit(X, y, epochs=25, batch_size=64, validation_split=0.15, class_weight=weight_dict, verbose=1)

# ==========================================
# 5. ĐÁNH GIÁ MÔ HÌNH VỚI CÁC TRƯỜNG HỢP KIỂM THỬ
# ==========================================
print("\n--> [5/6] Kiểm thử nhanh các ngưỡng điều kiện thực tế...")
test_cases = [
    (19.0, 50.0, "Class 0 - COLD (Tắt quạt)"),
    (24.0, 50.0, "Class 1 - COMFORT (Quạt 35%)"),
    (27.0, 80.0, "Class 2 - WARM_HUMID (Nóng ẩm/Hầm bí, Quạt 70%)"),
    (28.0, 55.0, "Class 2 - WARM (Ấm, Quạt 70%)"),
    (29.7, 93.6, "Class 3 - HOT (Rất oi bức, Quạt 100%)"),
    (33.0, 65.0, "Class 3 - HOT (Nóng gắt, Quạt 100%)")
]
for t, h, desc in test_cases:
    inp = np.array([[t, h, 0.0, 0.0]], dtype=np.float32)
    probs = model.predict(inp, verbose=0)[0]
    best_c = np.argmax(probs)
    print(f"   T={t:4.1f}°C, H={h:4.1f}% -> Dự đoán: Class {best_c} ({label_map[best_c]:10s}) | Max Prob: {probs[best_c]:.2f} | Mục tiêu: {desc}")

# ==========================================
# 6. CHUYỂN ĐỔI SANG TFLITE & XUẤT FILE HEADER C++
# ==========================================
print("\n--> [6/6] Đang chuyển đổi sang TFLite và xuất file header C++...")
converter = tf.lite.TFLiteConverter.from_keras_model(model)
tflite_model = converter.convert()

header_content = (
    "#ifndef DHT_ANOMALY_MODEL_H\n"
    "#define DHT_ANOMALY_MODEL_H\n\n"
    "alignas(16) const unsigned char dht_anomaly_model_tflite[] = {\n  "
)
bytes_str = ", ".join(f"0x{b:02x}" for b in tflite_model)
lines = [bytes_str[i:i+72] for i in range(0, len(bytes_str), 72)]
header_content += "\n  ".join(lines)
header_content += (
    f"\n}};\n\n"
    f"const unsigned int dht_anomaly_model_tflite_len = {len(tflite_model)};\n\n"
    f"#endif // DHT_ANOMALY_MODEL_H\n"
)

# Ghi vào cả src và include để đảm bảo PlatformIO luôn nhận đúng file mới nhất
destinations = [
    os.path.join(out_dir, "dht_anomaly_model.h"),
    os.path.join("include", "dht_anomaly_model.h") if os.path.isdir("include") else None
]

for dest in destinations:
    if dest:
        with open(dest, "w", encoding="utf-8") as f:
            f.write(header_content)
        print(f"--> Đã tạo file header thành công: {dest} (Kích thước: {len(tflite_model)} bytes)")

print("\n--> Hoàn tất toàn bộ quy trình huấn luyện & triển khai!")