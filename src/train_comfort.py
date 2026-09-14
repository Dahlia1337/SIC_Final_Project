import os
import sys
import numpy as np
import pandas as pd
import tensorflow as tf
import matplotlib.pyplot as plt
from sklearn.model_selection import train_test_split
from sklearn.metrics import classification_report, confusion_matrix
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

# Lấy các cột cốt lõi: Nhiệt độ, Độ ẩm và Lá phiếu cảm nhận nhiệt thực tế của con người (Thermal sensation)
cols = ['Air temperature (C)', 'Relative humidity (%)', 'Thermal sensation']
df = df_raw[cols].dropna().copy()

# Lọc các giá trị vật lý hợp lệ
df = df[(df['Air temperature (C)'] >= 10.0) & (df['Air temperature (C)'] <= 45.0)]
df = df[(df['Relative humidity (%)'] >= 15.0) & (df['Relative humidity (%)'] <= 100.0)]

# ==========================================
# 2. KHẢO SÁT & TÍNH TOÁN ĐỒNG THUẬN CẢM NHẬN NHIỆT CON NGƯỜI (OCCUPANT CONSENSUS TSV)
# ==========================================
print("--> [2/6] Phân tích lá phiếu cảm nhận nhiệt thực tế (Thermal Sensation Vote - ASHRAE 55)...")

# Làm mịn nhiễu cá nhân (quần áo/thể trạng) bằng cách tính Mean TSV cho mỗi vùng vi khí hậu (0.5°C x 2% RH)
df['t_grid'] = (df['Air temperature (C)'] * 2).round() / 2
df['h_grid'] = (df['Relative humidity (%)'] / 2).round() * 2

grid_tsv = df.groupby(['t_grid', 'h_grid'])['Thermal sensation'].transform('mean')
df['consensus_tsv'] = grid_tsv

def map_tsv_to_class(tsv):
    # Class 0: COLD (TSV < -0.5: Con người cảm thấy lạnh)
    if tsv < -0.5:
        return 0
    # Class 1: COMFORT (Chuẩn ASHRAE 55: -0.5 <= TSV < +0.5 là Vùng tiện nghi trung tính Neutral Zone)
    elif tsv < 0.5:
        return 1
    # Class 2: WARM_HUMID (+0.5 <= TSV < +1.4: Hơi ấm / nóng ẩm, người bắt đầu thấy bí bức)
    elif tsv < 1.4:
        return 2
    # Class 3: HOT (TSV >= +1.4: Nóng gắt, rất oi bức)
    else:
        return 3

df['comfort_class'] = df['consensus_tsv'].apply(map_tsv_to_class)
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

# Phân chia train/val (stratified theo y) để tránh data leakage
X_train, X_val, y_train, y_val = train_test_split(
    X, y, test_size=0.15, random_state=42, stratify=y
)

# Nhúng layer Normalization trực tiếp vào Model (chỉ adapt trên X_train để tránh data leakage)
norm_layer = tf.keras.layers.Normalization(axis=-1)
norm_layer.adapt(X_train)

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

class_weights = compute_class_weight('balanced', classes=np.unique(y_train), y=y_train)
weight_dict = dict(enumerate(class_weights))

history = model.fit(
    X_train, y_train,
    epochs=25,
    batch_size=64,
    validation_data=(X_val, y_val),
    class_weight=weight_dict,
    verbose=1
)

# ==========================================
# 5a. ĐÁNH GIÁ ĐỊNH LƯỢNG MÔ HÌNH TRÊN TẬP VALIDATION
# ==========================================
print("\n--> [5a/6] Đánh giá định lượng trên tập validation (Classification Report & Confusion Matrix)...")

# Dự đoán trên tập validation
val_probs = model.predict(X_val, verbose=0)
y_val_pred = np.argmax(val_probs, axis=1)

# In và lưu Classification Report
target_names = [label_map[i] for i in range(4)]
report_str = classification_report(y_val, y_val_pred, target_names=target_names, digits=3)
print("\nBáo cáo phân loại (Classification Report):")
print(report_str)

report_path = os.path.join(out_dir, "classification_report.txt")
with open(report_path, "w", encoding="utf-8") as f:
    f.write(report_str)
print(f"    - Đã lưu báo cáo phân loại: {report_path}")

# In và vẽ Confusion Matrix
cm = confusion_matrix(y_val, y_val_pred)
print("\nMa trận nhầm lẫn (Confusion Matrix):")
print(cm)

plt.figure(figsize=(6, 5))
plt.imshow(cm, interpolation='nearest', cmap=plt.cm.Blues)
plt.title("Confusion Matrix - Comfort Classification")
plt.colorbar()
tick_marks = np.arange(len(target_names))
plt.xticks(tick_marks, target_names)
plt.yticks(tick_marks, target_names)
plt.xlabel("Dự đoán (Predicted)")
plt.ylabel("Thực tế (True)")

thresh = cm.max() / 2.0
for i in range(cm.shape[0]):
    for j in range(cm.shape[1]):
        plt.text(j, i, format(cm[i, j], 'd'),
                 horizontalalignment="center",
                 verticalalignment="center",
                 color="white" if cm[i, j] > thresh else "black")

plt.tight_layout()
cm_path = os.path.join(out_dir, "confusion_matrix.png")
plt.savefig(cm_path, dpi=150)
plt.close()
print(f"    - Đã lưu ảnh confusion matrix: {cm_path}")

# Vẽ thêm biểu đồ quá trình huấn luyện (Loss & Accuracy)
plt.figure(figsize=(10, 4))
plt.subplot(1, 2, 1)
plt.plot(history.history['loss'], label='Train Loss', color='tab:blue')
plt.plot(history.history['val_loss'], label='Val Loss', color='tab:orange')
plt.title('Loss per Epoch')
plt.xlabel('Epoch')
plt.ylabel('Loss')
plt.legend()
plt.grid(True, linestyle='--', alpha=0.6)

plt.subplot(1, 2, 2)
plt.plot(history.history['accuracy'], label='Train Acc', color='tab:blue')
plt.plot(history.history['val_accuracy'], label='Val Acc', color='tab:orange')
plt.title('Accuracy per Epoch')
plt.xlabel('Epoch')
plt.ylabel('Accuracy')
plt.legend()
plt.grid(True, linestyle='--', alpha=0.6)

plt.tight_layout()
history_plot_path = os.path.join(out_dir, "training_history.png")
plt.savefig(history_plot_path, dpi=150)
plt.close()
print(f"    - Đã lưu biểu đồ huấn luyện: {history_plot_path}")

# ==========================================
# 5b. ĐÁNH GIÁ MÔ HÌNH VỚI CÁC TRƯỜNG HỢP KIỂM THỬ
# ==========================================
print("\n--> [5b/6] Kiểm thử nhanh các ngưỡng điều kiện thực tế...")
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