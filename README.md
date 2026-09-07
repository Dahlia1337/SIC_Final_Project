# Mô-đun Edge AI: Phân loại Tiện nghi Nhiệt (TinyML Thermal Comfort)

Mô-đun AI của dự án thực hiện suy luận cục bộ (on-device inference) trên vi điều khiển **ESP32-S3**, phân loại trạng thái vi khí hậu thành 4 cấp độ tiện nghi nhiệt để tự động điều phối tốc độ quạt (PWM) và chỉ thị màu LED NeoPixel.

---

## 1. Dữ liệu & Đặc trưng đầu vào (Inputs)

Mô hình tiếp nhận vector đầu vào 4 chiều ($1 \times 4$) kiểu `float32`:
* $T$ (`temperature_c`): Nhiệt độ môi trường (°C) từ cảm biến DHT22.
* $H$ (`humidity_pct`): Độ ẩm tương đối (%) từ cảm biến DHT22.
* $\Delta T$ (`delta_temp`): Biến thiên nhiệt độ giữa 2 chu kỳ đo liên tiếp.
* $\Delta H$ (`delta_humi`): Biến thiên độ ẩm giữa 2 chu kỳ đo liên tiếp.

> **Chuẩn hóa dữ liệu:** Layer `Normalization` (Z-score) được nhúng trực tiếp vào đồ thị mô hình TFLite, giúp vi điều khiển đưa dữ liệu thô vào tensor mà không cần tính toán tiền xử lý thủ công bằng code C++.

---

## 2. Nhãn phân loại & Ánh xạ điều khiển (Outputs)

Mô hình đưa ra phân phối xác suất qua hàm kích hoạt **Softmax** cho 4 phân lớp độc lập:

| Lớp (Class) | Nhãn (`comfort_label`) | Trạng thái môi trường | Tốc độ quạt (PWM) | Chỉ thị LED NeoPixel |
| :---: | :--- | :--- | :---: | :---: |
| **0** | `COLD` | Lạnh ($T < 22^\circ\text{C}$) | $0\%$ (Tắt quạt) | Xanh dương (`Blue`) |
| **1** | `COMFORT` | Dễ chịu ($22^\circ\text{C} \le T < 28^\circ\text{C}$) | $35\%$ (Quạt êm) | Xanh lá (`Green`) |
| **2** | `WARM_HUMID` | Nóng ẩm / Hầm bí ($H \ge 75\%$, $T \ge 26.5^\circ\text{C}$) | $70\%$ (Thông gió) | Vàng / Cam (`Orange`) |
| **3** | `HOT` | Nóng gắt ($T \ge 32^\circ\text{C}$ hoặc $HI \ge 33^\circ\text{C}$) | $100\%$ (Tối đa) | Đỏ rực (`Red`) |

---

## 3. Kiến trúc mạng nơ-ron (MLP)

* **Input Layer:** `(None, 4)`
* **Pre-processing Layer:** `tf.keras.layers.Normalization(axis=-1)`
* **Dense Layer 1:** 32 neurons, kích hoạt `ReLU`
* **Dense Layer 2:** 16 neurons, kích hoạt `ReLU`
* **Output Layer:** 4 neurons, kích hoạt `Softmax`
* **Loss Function:** `sparse_categorical_crossentropy`
* **Optimizer:** Adam ($\text{lr} = 0.003$)

Mô hình được chuyển đổi sang định dạng TensorFlow Lite (`.tflite`) và xuất thành mảng byte C trong `include/dht_anomaly_model.h` để nhúng vào bộ nhớ Flash của chip.

---

## 4. Quy trình vận hành trên ESP32-S3 (`tinyml.cpp`)

1. **Khởi tạo (`setupTinyML`):** Cấp phát vùng nhớ tĩnh `tensor_arena` ($10\,\text{KB}$), khởi tạo `MicroInterpreter` và trích xuất con trỏ input/output.
2. **Lấy mẫu & Tính đạo hàm:** Mỗi chu kỳ $2\,\text{giây}$, đọc $T, H$ từ biến toàn cục và tính toán $\Delta T, \Delta H$.
3. **Suy luận (`Invoke`):** Gọi `interpreter->Invoke()` để tính toán.
4. **Hậu xử lý:** 
   * Tìm chỉ số lớp có xác suất cao nhất (`argmax`) gán vào `comfort_class`.
   * Gọi `rgb_display_comfort()` để đổi màu LED theo trạng thái vi khí hậu.
   * Ở chế độ `AUTO`, `update_auto_fan_logic()` tự động gán tốc độ PWM tương ứng cho quạt.

---

## 5. Hướng dẫn cập nhật & Huấn luyện lại

Khi cần tinh chỉnh ngưỡng hoặc tập dữ liệu:
1. Chạy file huấn luyện trên máy tính:
   ```bash
   python train_comfort.py