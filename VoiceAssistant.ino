/*
 * SPDX-FileCopyrightText: 2024 M5Stack Technology CO LTD (Modified by User)
 *
 * SPDX-License-Identifier: MIT
 */
#include <Arduino.h>
#include <M5Unified.h>
#include <M5ModuleLLM.h> // M5ModuleLLMライブラリ

M5ModuleLLM module_llm;
// M5ModuleLLM_VoiceAssistant クラスがライブラリに存在するか確認が必要です。
// もし別のヘッダーファイルが必要な場合は、それもインクルードしてください。
// 例: #include <M5ModuleLLM_VoiceAssistant.h> (仮のヘッダー名)
M5ModuleLLM_VoiceAssistant voice_assistant(&module_llm);

/* On ASR (Automatic Speech Recognition) data callback */
void on_asr_data_input(String data, bool isFinish, int index) {
    // 画面のクリアは状況に応じて検討 (連続表示で見にくくなる場合)
    // if (index == 0 && !isFinish) M5.Display.clear(); // 認識開始時にクリアする例

    M5.Display.setTextColor(TFT_GREEN, M5.Display.getBaseColor());
    // M5.Display.setFont(&fonts::efontCN_12); // 中国語表示サポート
    M5.Display.printf("ASR: %s", data.c_str()); // ">>" を "ASR: " に変更して識別しやすく
    Serial.printf("ASR Data: %s (isFinish: %d, index: %d)\n", data.c_str(), isFinish, index);

    /* If ASR data is finish */
    if (isFinish) {
        M5.Display.print(" [Done]\n"); // 完了を示す
        M5.Display.setTextColor(TFT_YELLOW, M5.Display.getBaseColor()); // LLM応答待ちの色
        M5.Display.print("LLM Processing...\n");
        Serial.println("ASR Finished. Waiting for LLM response...");
    } else {
        M5.Display.print("..."); // 途中であることを示す
    }
}

/* On LLM (Large Language Model) data callback */
void on_llm_data_input(String data, bool isFinish, int index) {
    M5.Display.setTextColor(TFT_CYAN, M5.Display.getBaseColor()); // LLMの応答はシアンで表示
    M5.Display.print(data);
    Serial.printf("LLM Data: %s (isFinish: %d, index: %d)\n", data.c_str(), isFinish, index);

    /* If LLM data is finish */
    if (isFinish) {
        M5.Display.print("\n\n"); // 応答完了後に改行
        M5.Display.setTextColor(TFT_WHITE, M5.Display.getBaseColor()); // 通常の色に戻す
        M5.Display.println("Ready for next command.");
        Serial.println("LLM Response Finished.");
    }
}

void setup() {
    M5.begin();
    M5.Display.setTextSize(2);
    M5.Display.setTextScroll(true);
    Serial.begin(115200); // PCとのデバッグ出力用

    M5.Display.println("Voice Assistant Test Started");
    Serial.println("Voice Assistant Test Started");

    /* Init module serial port */
    // 以下のピン設定は、前回のテストで成功したものを採用します。
    // M5Stack CoreS3で内蔵UART Groveポート (G18/G17) に接続している場合の例
    int rxd = 18;
    int txd = 17;
    // int rxd = M5.getPin(m5::pin_name_t::port_c_rxd); // こちらはコメントアウト
    // int txd = M5.getPin(m5::pin_name_t::port_c_txd); // こちらはコメントアウト

    M5.Display.printf("Using UART Pins -> RXD: %d, TXD: %d\n", rxd, txd);
    Serial.printf("Using UART Pins -> RXD: %d, TXD: %d\n", rxd, txd);

    Serial2.begin(115200, SERIAL_8N1, rxd, txd);
    M5.Display.println("Serial2.begin() OK");
    Serial.println("Serial2.begin() OK");

    /* Init module */
    module_llm.begin(&Serial2);
    M5.Display.println("module_llm.begin() OK");
    Serial.println("module_llm.begin() OK");

    /* Make sure module is connected */
    M5.Display.println("Checking ModuleLLM connection...");
    Serial.println("Checking ModuleLLM connection...");
    uint8_t retries = 0;
    while (!module_llm.checkConnection()) {
        M5.Display.print(".");
        Serial.print(".");
        delay(500);
        retries++;
        if (retries > 10) { // 約5秒試行
            M5.Display.println("\nModuleLLM Connection Failed!");
            Serial.println("\nModuleLLM Connection Failed!");
            while (1) delay(1000); // 接続失敗時はここで停止
        }
    }
    M5.Display.println("\nModuleLLM Connected!");
    Serial.println("\nModuleLLM Connected!");
    delay(500); // 少し待つ

    /* Begin voice assistant preset */
    M5.Display.println("Beginning Voice Assistant setup...");
    Serial.println("Beginning Voice Assistant setup...");
    // デフォルトの英語設定で試す
    // ウェイクワードは "HELLO" (大文字である必要があるか確認)
    // M5ModuleLLMのドキュメントで、ウェイクワードの形式や利用可能な言語を確認してください。
    int ret = voice_assistant.begin("HELLO"); // ウェイクワード。大文字小文字を区別する可能性あり。
    // int ret = voice_assistant.begin("你好你好", "", "zh_CN"); // 中国語の場合の例

    if (ret == MODULE_LLM_OK) { // MODULE_LLM_OK の定義を確認 (通常は0か正の値)
        M5.Display.println("Voice Assistant begin SUCCEEDED!");
        Serial.println("Voice Assistant begin SUCCEEDED!");
    } else {
        M5.Display.setTextColor(TFT_RED, M5.Display.getBaseColor());
        M5.Display.printf("Voice Assistant begin FAILED! Code: %d\n", ret);
        Serial.printf("Voice Assistant begin FAILED! Code: %d\n", ret);
        M5.Display.println("Check module firmware, SD card (if needed for VA), and wake word.");
        Serial.println("Check module firmware, SD card (if needed for VA), and wake word.");
        while (1) {
            delay(1000);
        }
    }

    /* Register on ASR data callback function */
    M5.Display.println("Registering ASR callback...");
    Serial.println("Registering ASR callback...");
    voice_assistant.onAsrDataInput(on_asr_data_input);
    M5.Display.println("ASR callback registered.");
    Serial.println("ASR callback registered.");

    /* Register on LLM data callback function */
    M5.Display.println("Registering LLM callback...");
    Serial.println("Registering LLM callback...");
    voice_assistant.onLlmDataInput(on_llm_data_input);
    M5.Display.println("LLM callback registered.");
    Serial.println("LLM callback registered.");

    M5.Display.setTextColor(TFT_WHITE, M5.Display.getBaseColor());
    M5.Display.println("\n>> Voice assistant ready <<");
    M5.Display.println("Say 'HELLO' to activate."); // ユーザーへの指示
    Serial.println(">> Voice assistant ready. Say 'HELLO' to activate. <<");
}

void loop() {
    /* Keep voice assistant preset update */
    // voice_assistant.update() が M5.update() のようにメインループで
    // 定期的に呼ばれる必要がある関数か確認。
    // イベントドリブンでコールバックが呼ばれるだけなら、
    // loop内で明示的に何かをする必要がない場合もあります。
    // M5.update(); // M5Unifiedのイベント処理等も忘れずに (もし必要なら)
    voice_assistant.update();
    delay(10); // CPUを占有しすぎないように短いdelayを入れる (必要に応じて調整)
}
