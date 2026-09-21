# 相容性與驗證

更新日期：2026-09-21。**v0.8 FAP 建置**與主機端模型測試於當日通過；實機觀察來自 **2026-09-20** 的先前建置，尚未以此次 v0.8 產物重新驗證硬體。使用與編譯方式見 [README](../README.md)。

## 已測環境

| 項目 | 測試環境與結果 |
| --- | --- |
| App 建置 | 2026-09-21，v0.8、官方 firmware SDK API **88.2**；FAP 建置、SDKCHK 與 APPCHK 通過 |
| 裝置韌體 | 2026-09-20 的先前建置：Flipper Zero，Unleashed **unlshd-093e／API 88.9**；App 安裝及啟動通過 |
| 主機接收 | 2026-09-20 的先前建置：macOS CoreMIDI；成功列舉 **Flipper RF MIDI** 並收到 Channel 1 CC |

這些結果僅涵蓋上述組合，未驗證其他韌體版本、Windows 或 Linux 的 USB MIDI 接收。編譯用 SDK 與裝置韌體的 API 必須相容；載入時若出現 API 不相容，請使用對應 SDK 重新編譯。

## Repository 內可重現的模型測試

在 repository 根目錄執行：

```sh
./rf_midi/tests/run_model_tests.sh
```

測試使用 C11、嚴格編譯警告、AddressSanitizer 與 UndefinedBehaviorSanitizer，涵蓋：

- 全部 **3,184 種設定／2,733,464 個頻點**：首尾頻率、頻率排序、軌內嚴格遞增，以及所有取樣點均落在硬體支援的接收視窗。
- 軌數與點數上下限、非法參數、64-bit 頻率計算。
- 非 1 kHz 的 tick 換算、tick 回繞、掃描時間估計與計時樣本更新。
- 停用軌的 CC 清零遮罩，以及 RSSI 映射、截斷與 NaN 輸入。

成功時會輸出：

```text
PASS model: 3184 configurations, 2733464 frequency samples, timing/limits/masks/RSSI
```

此測試只執行純 C 模型，不會連接 Flipper，也不涵蓋實際 USB、按鍵、螢幕或 RF。

## 過往實機觀察

在預設 **10 軌 × 50 點**下，25 秒穩定測試段接收到 **119 筆 CC20–29**，每個啟用 CC 都持續出現；同一 CC 兩次接收的平均間隔約 **2,104 ms**。

時間來自主機 MIDI callback 的 monotonic clock，不是裝置畫面的 Last 值，也不是 RF 校準結果。這是特定環境的一次觀察，不能視為固定更新率或量測準確度保證。CC30–35 當時只觀察到啟動清零，尚未確認啟用 16 軌後的持續輸出。

過往另做過 128×64 離線畫面檢查，以及模擬 HAL 的操作流程測試。相關輔助程式未隨本 repository 提供；這些歷史結果無法由上面的模型測試指令重現，也不作為硬體驗收依據。

## 尚待實機確認

- **本次 v0.8 產物**的安裝、啟動、USB MIDI 列舉及主機接收。
- **16 軌 × 200 點**的持續 CC 輸出、按鍵操作反應，以及畫面 Est／Last。
- 暫停／恢復、Back 正常退出後的 USB 還原，以及再次開啟 App。
- USB suspend、bus reset、拔插後的恢復與清零行為。
- 已知 RF 訊號對 RSSI 的反應、量測準確度，以及暫停／退出後的實際 RF 狀態。

App 執行時使用 USB MIDI 介面，不提供 CDC／RPC，因此上述按鍵與畫面項目需在裝置上操作確認。USB 斷線時無法保證主機收到退出前的清零訊息；主機軟體需自行處理斷線後保留的控制值。
