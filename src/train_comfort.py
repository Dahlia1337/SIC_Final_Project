import numpy as np
import pandas as pd
import tensorflow as tf
from sklearn.utils.class_weight import compute_class_weight
# ==========================================
# 1. ĐỌC DỮ LIỆU TỪ ASHRAE DATABASE II
# ==========================================
print("--> [1/6] Đang đọc dữ liệu từ ashrae_db2.01.csv...")
df_raw = pd.read_csv("ashrae_db2.01.csv", low_memory=False)

# Lọc 3 cột cốt lõi và bỏ dữ liệu rỗng (NaN)
cols = ['Air temperature (C)', 'Relative humidity (%)', 'Thermal sensation']
df = df_raw[cols].dropna().copy()

# Lọc các giá trị vật lý ngoại lai
df = df[(df['Air temperature (C)'] >= 10.0) & (df['Air temperature (C)'] <= 45.0)]
df = df[(df['Relative humidity (%)'] >= 10.0) & (df['Relative humidity (%)'] <= 100.0)]

# ==========================================
# 2. TÍNH TOÁN ĐẶC TRƯNG ĐỘNG HỌC & GÁN NHÃN
# ==========================================
print("--> [2/6] Tính toán vi sai biến thiên và ánh xạ 4 Class...")
df['delta_temp'] = df['Air temperature (C)'].diff().fillna(0).clip(-1.5, 1.5)
df['delta_humi'] = df['Relative humidity (%)'].diff().fillna(0).clip(-5.0, 5.0)

def map_to_fan_class(s):
    if s <= -1.0:
        return 0  # COLD
    elif s <= 0.5:
        return 1  # COMFORT
    elif s <= 1.5:
        return 2  # WARM_HUMID
    else:
        return 3  # HOT

df['comfort_class'] = df['Thermal sensation'].apply(map_to_fan_class)
label_map = {0: 'COLD', 1: 'COMFORT', 2: 'WARM_HUMID', 3: 'HOT'}
df['comfort_label'] = df['comfort_class'].map(label_map)

df.rename(columns={
    'Air temperature (C)': 'temperature_c',
    'Relative humidity (%)': 'humidity_pct'
}, inplace=True)

# Lấy mẫu ngẫu nhiên (20.000 dòng để huấn luyện TinyML tối ưu)
df_clean = df.sample(n=min(20000, len(df)), random_state=42).copy()

# ==========================================
# 3. CHUẨN HÓA DỮ LIỆU (Z-SCORE NORMALIZATION)
# ==========================================
print("--> [3/6] Thực hiện chuẩn hóa đặc trưng (Mean = 0, Std = 1)...")
features = ['temperature_c', 'humidity_pct', 'delta_temp', 'delta_humi']

means = df_clean[features].mean()
stds = df_clean[features].std()

df_normalized = df_clean.copy()
for col in features:
    df_normalized[f'{col}_norm'] = (df_clean[col] - means[col]) / stds[col]

# ==========================================
# 4. XUẤT RA 2 FILE CSV
# ==========================================
# File 1: Dữ liệu gốc đã làm sạch
df_clean[['temperature_c', 'humidity_pct', 'delta_temp', 'delta_humi', 'comfort_class', 'comfort_label']].to_csv(
    "ashrae_cleaned_comfort.csv", index=False, encoding='utf-8'
)

# File 2: Dữ liệu đã chuẩn hóa hoàn chỉnh
norm_cols = ['temperature_c_norm', 'humidity_pct_norm', 'delta_temp_norm', 'delta_humi_norm', 'comfort_class', 'comfort_label']
df_normalized[norm_cols].to_csv(
    "ashrae_normalized_comfort.csv", index=False, encoding='utf-8'
)

print("--> [4/6] Đã xuất 2 file CSV:")
print("    - ashrae_cleaned_comfort.csv (Dữ liệu gốc)")
print("    - ashrae_normalized_comfort.csv (Dữ liệu đã chuẩn hóa)")
print("\nThông số chuẩn hóa:")
for col in features:
    print(f"   {col:15s}: Mean = {means[col]:.4f}, Std = {stds[col]:.4f}")

# ==========================================
# 5. HUẤN LUYỆN MẠNG NƠ-RON TINYML
# ==========================================
print("\n--> [5/6] Bắt đầu huấn luyện mô hình Keras...")
X = df_clean[features].values.astype(np.float32)
y = df_clean['comfort_class'].values.astype(np.int32)

# Nhúng layer Normalization trực tiếp vào Model để ESP32 không cần tính toán thủ công
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
model.fit(X, y, epochs=40, batch_size=64, validation_split=0.2, verbose=1)

# ==========================================
# 6. CHUYỂN ĐỔI SANG TFLITE & XUẤT HEADER C++
# ==========================================
print("\n--> [6/6] Đang chuyển đổi sang TFLite và ghi file header C++...")
converter = tf.lite.TFLiteConverter.from_keras_model(model)
tflite_model = converter.convert()

with open("dht_anomaly_model.h", "w", encoding="utf-8") as f:
    f.write("#ifndef DHT_ANOMALY_MODEL_H\n#define DHT_ANOMALY_MODEL_H\n\n")
    f.write("alignas(16) const unsigned char dht_anomaly_model_tflite[] = {\n  ")
    for i, byte in enumerate(tflite_model):
        f.write(f"0x{byte:02x}, ")
        if (i + 1) % 12 == 0:
            f.write("\n  ")
    f.write("\n};\n\n")
    f.write(f"const unsigned int dht_anomaly_model_tflite_len = {len(tflite_model)};\n\n")
    f.write("#endif // DHT_ANOMALY_MODEL_H\n")

print("--> Hoàn tất! File dht_anomaly_model.h đã được tạo thành công.")