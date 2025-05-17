/*
 * SPDX-FileCopyrightText: 2024 M5Stack Technology CO LTD (Modified by User)
 *
 * SPDX-License-Identifier: MIT
 */
#include "M5CoreS3.h"         // ★★★ M5CoreS3.h を使用 ★★★
#include "esp_camera.h"       // ★★★ ESP-IDFカメラヘッダーを使用 ★★★
#include <M5ModuleLLM.h>      // M5ModuleLLMライブラリ
#include <ArduinoJson.h>      // ArduinoJsonライブラリ

M5ModuleLLM module_llm;
String yolo_work_id;

const int YOLO_DISPLAY_Y_OFFSET = -40;

// (splitLines 関数は変更なし)
std::vector<String> splitLines(const String& s) {
    std::vector<String> lines;
    int start = 0;
    int next = 0;
    while ((next = s.indexOf('\n', start)) != -1) {
        lines.push_back(s.substring(start, next));
        start = next + 1;
    }
    lines.push_back(s.substring(start));
    return lines;
}

void setup() {
    auto cfg = M5.config(); // M5.config() はそのまま使えると仮定
    CoreS3.begin(cfg);      // ★★★ CoreS3.begin() を使用 ★★★

    CoreS3.Display.setTextSize(2); // ★★★ CoreS3.Display を使用 ★★★
    Serial.begin(115200);

    Serial.println("M5Stack CoreS3 Camera YOLO with M5ModuleLLM Test");
    CoreS3.Display.println("CoreS3 Cam YOLO Test");
    CoreS3.Display.setTextColor(TFT_WHITE, CoreS3.Display.getBaseColor());

    // カメラ初期化
    Serial.println("Initializing Camera...");
    if (!CoreS3.Camera.begin()) { // ★★★ CoreS3.Camera.begin() を使用 ★★★
        Serial.println("Camera Init Fail");
        CoreS3.Display.fillScreen(TFT_RED); CoreS3.Display.setCursor(10,10);
        CoreS3.Display.println("Camera Init FAIL");
        while (1) delay(1000);
    }
    Serial.println("Camera Init Success");
    CoreS3.Display.println("Camera OK!");

    // カメラセンサー設定 (例: フレームサイズをQVGAに設定)
    // sensor_t *s = CoreS3.Camera.sensor; // sensor構造体へのポインタを取得
    // if (s != nullptr) {
    //     s->set_framesize(s, FRAMESIZE_QVGA); // 320x240
    //     s->set_quality(s, 12); // JPEG品質 (0-63, 値が小さいほど高品質)
    //     Serial.println("Camera framesize set to QVGA, quality set to 12.");
    // } else {
    //     Serial.println("Failed to get camera sensor instance.");
    // }
    // CoreS3.Camera.sensor は直接 sensor_t* を返すかもしれないので、上記は冗長な場合あり
    if (CoreS3.Camera.sensor != nullptr) {
         CoreS3.Camera.sensor->set_framesize(CoreS3.Camera.sensor, FRAMESIZE_QVGA);
         // CoreS3.Camera.sensor->set_quality(CoreS3.Camera.sensor, 20); // JPEG品質 (0-63,低いほど高画質)
         Serial.println("Camera framesize set to QVGA.");
    }


    int rxd = 18;
    int txd = 17;
    Serial.printf("Using UART Pins -> RXD: %d, TXD: %d\n", rxd, txd);
    // CoreS3.Display.printf("UART: %d/%d\n", rxd, txd); // Displayはすでに使っているので省略可

    Serial2.begin(115200, SERIAL_8N1, rxd, txd);
    Serial.println("Serial2.begin() OK");

    module_llm.begin(&Serial2);
    Serial.println("module_llm.begin() OK");

    Serial.println("Checking ModuleLLM connection...");
    // CoreS3.Display.print("Connecting LLM...");
    uint8_t retries = 0;
    while (!module_llm.checkConnection()) {
        Serial.print(".");
        delay(500);
        retries++;
        if (retries > 10) {
            Serial.println("\nModuleLLM Connection Failed!");
            CoreS3.Display.fillScreen(TFT_RED); CoreS3.Display.setCursor(10,10); CoreS3.Display.print("LLM Connect FAIL");
            while (1) delay(1000);
        }
    }
    Serial.println("\nModuleLLM Connected!"); // CoreS3.Display.println("LLM OK");
    delay(500);

    Serial.println("Resetting ModuleLLM...");
    module_llm.sys.reset();
    Serial.println("ModuleLLM Reset attempt done.");
    delay(500);

    Serial.println("Setting up YOLO on M5ModuleLLM...");
    // CoreS3.Display.print("Setup YOLO...");
    m5_module_llm::ApiYoloSetupConfig_t yolo_config = {};
    // yolo_config.model_name = "yolovX.bin"; // ★★★ モデル名が必要なら指定 ★★★
    yolo_work_id = module_llm.yolo.setup(yolo_config, "yolo_cores3_cam");
    Serial.printf("YOLO setup attempt. Work ID: %s\n", yolo_work_id.c_str());

    if (yolo_work_id.isEmpty() || yolo_work_id.indexOf("error") != -1) {
        Serial.printf("YOLO setup FAILED! Work ID: %s\n", yolo_work_id.c_str());
        CoreS3.Display.fillScreen(TFT_RED); CoreS3.Display.setCursor(10,10); CoreS3.Display.print("YOLO Setup FAIL");
        while (1) delay(1000);
    }
    Serial.println("YOLO setup on M5ModuleLLM Successful!"); // CoreS3.Display.println("YOLO OK");

    CoreS3.Display.fillScreen(CoreS3.Display.getBaseColor());
    Serial.println("Setup complete. Starting detection loop...");
}

void loop() {
    // camera_fb_t は esp_camera.h で定義される
    camera_fb_t* fb = nullptr;

    uint8_t* jpg_buf = nullptr;
    size_t jpg_len = 0;

    // フレーム取得
    // fb = CoreS3.Camera.getFramebuffer(); // ドキュメントの CoreS3.Camera.get() が boolを返すなら使い方が違う
    if (CoreS3.Camera.get()) { // ★★★ サンプル通り CoreS3.Camera.get() を使用 ★★★
        fb = CoreS3.Camera.fb; // ★★★ フレームバッファは CoreS3.Camera.fb メンバー ★★★
    } else {
        Serial.println("Failed to get camera frame (CoreS3.Camera.get() failed)");
        delay(100);
        return;
    }

    if (!fb) { // fbがNULLだった場合 (get()が成功してもfbがNULLの可能性は低いが一応)
        Serial.println("Framebuffer pointer (CoreS3.Camera.fb) is NULL");
        CoreS3.Camera.free(); // get() が成功したなら free() は呼ぶべき
        delay(100);
        return;
    }

    // JPEG変換 (esp_camera.h の frame2jpg を使用)
    bool converted = frame2jpg(fb, 20, &jpg_buf, &jpg_len); // 品質20

    if (!converted || jpg_len == 0) {
        Serial.println("Failed to convert frame to JPEG or JPEG size is 0.");
        CoreS3.Camera.free(); // ★★★ サンプル通り CoreS3.Camera.free() を使用 ★★★
        if (jpg_buf) {
            free(jpg_buf); // frame2jpgがメモリを確保した場合
        }
        delay(100);
        return;
    }

    Serial.printf("JPEG image captured, size: %u bytes\n", jpg_len);

    bool initial_image_drawn = false;
    String full_result_str;

    module_llm.yolo.inferenceAndWaitResult(
        yolo_work_id,
        jpg_buf,
        jpg_len,
        [&](const String& result_chunk) {
            full_result_str += result_chunk;
            // Serial.printf("Received result chunk: %s\n", result_chunk.c_str());

            if (!initial_image_drawn && fb->buf != nullptr) {
                // CoreS3.Display を使用
                CoreS3.Display.pushImage(0, 0, fb->width, fb->height, (uint16_t*)fb->buf);
                initial_image_drawn = true;
            }

            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, full_result_str); // または result_chunk

            if (error) {
                Serial.print(F("deserializeJson() failed: "));
                Serial.println(error.f_str());
                // 1回の推論で結果が分割されて送られてくる場合、ここでreturnすると後続のチャンクを処理できない。
                // M5ModuleLLMの応答が完了した時点でパースするのが適切。
                // ここではユーザーの元コードの構成に合わせ、コールバックごとにパースを試みる。
                return;
            }

            // 応答フォーマットに合わせて修正 (ここでは1検出オブジェクトと仮定)
            if (doc.containsKey("bbox") && doc["bbox"].is<JsonArray>()) {
                JsonArray bbox = doc["bbox"].as<JsonArray>();
                if (bbox.size() == 4) {
                    int x1 = bbox[0].as<float>();
                    int y1 = bbox[1].as<float>() + YOLO_DISPLAY_Y_OFFSET;
                    int x2 = bbox[2].as<float>();
                    int y2 = bbox[3].as<float>() + YOLO_DISPLAY_Y_OFFSET;
                    String cls = doc["class"].as<String>();
                    float confidence_val = doc["confidence"].as<float>();

                    x1 = max(0, x1); y1 = max(0, y1);
                    x2 = min((int)CoreS3.Display.width() - 1, x2);
                    y2 = min((int)CoreS3.Display.height() - 1, y2);

                    if (x1 < x2 && y1 < y2) {
                        CoreS3.Display.drawRect(x1, y1, x2 - x1, y2 - y1, TFT_GREEN);
                        CoreS3.Display.setCursor(x1, max(0, y1 - 20));
                        CoreS3.Display.setTextColor(TFT_GREEN, CoreS3.Display.getBaseColor());
                        CoreS3.Display.printf("%s: %.2f", cls.c_str(), confidence_val);
                        Serial.printf("Detected: %s (%.2f) at [%d,%d,%d,%d]\n", cls.c_str(), confidence_val, x1, y1, x2, y2);
                    }
                }
            } else {
                 Serial.println("No valid bbox found in this result chunk.");
            }
            // 1回の推論に対する結果の処理が終わったら(または全チャンク受信後)、full_result_strをクリア
            // M5ModuleLLMの応答仕様による。ここでは1回のコールバックで1検出と仮定しているため、
            // コールバックの最後にクリアするのは適切でないかもしれない。
            // inferenceAndWaitResult が全結果をまとめて1つのStringでコールバックを呼ぶなら、
            // ここでクリアしてOK。
            // full_result_str = "";
        }
    );
    // 1回の推論呼び出しとコールバック処理が終わったので、文字列をクリア
    // (コールバックが複数回呼ばれる場合は、呼び出し元で完了を判断してクリアする)
    full_result_str = "";


    CoreS3.Camera.free(); // ★★★ サンプル通り CoreS3.Camera.free() を使用 ★★★
    if (jpg_buf) {
        free(jpg_buf); // frame2jpgが確保したメモリを解放
    }

    delay(100);
}
