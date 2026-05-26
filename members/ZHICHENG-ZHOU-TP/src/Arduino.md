# Arduino Code (Timer Fan)

```cpp
// タイマーで自動停止する扇風機制御

// ピン定義
const byte PIN_START_BUTTON = 2;
const byte PIN_STOP_BUTTON  = 3;
const byte PIN_LED_STATUS   = 6;
const byte PIN_MOTOR_CTRL   = 9;

// 状態定義
const byte STATE_IDLE       = 0;  // 待機中
const byte STATE_RUNNING    = 1;  // 動作中
const byte STATE_FINISHED   = 2;  // 停止完了
const byte STATE_ERROR      = 3;  // 異常停止

// 時間定数
const unsigned long DEBOUNCE_DELAY_MS     = 50;
const unsigned long TIMER_TICK_MS         = 1000;
const unsigned long FINISH_BLINK_INTERVAL = 500;
const unsigned long ERROR_BLINK_INTERVAL  = 200;
const unsigned long LONG_PRESS_MS         = 2000;

// 妥当範囲（異常値判定）
const unsigned int MAX_REMAINING_SECONDS = 1800; // 30分

// グローバル変数
byte currentState = STATE_IDLE;

unsigned long lastTickMillis       = 0;
unsigned long lastBlinkMillis      = 0;
unsigned long lastButtonMillis     = 0;
unsigned long stopPressStartMillis = 0;
unsigned long lastErrorBlinkMillis = 0;

byte selectedMinutes       = 10;
unsigned int remainingSeconds = 0;
bool startButtonEvent      = false;
bool stopButtonEvent       = false;
bool ledState              = false;
bool stopLongPressHandled  = false;

// ボタン前回状態（INPUT_PULLUPなので未押下=HIGH, 押下=LOW）
byte prevStartRaw = HIGH;
byte prevStopRaw  = HIGH;

// 待機中の「時間設定」と「開始」を両立するための補助
bool startArmed = false;
unsigned long lastStartEventMillis = 0;

void setup() {
	// 入出力ピン初期化
	pinMode(PIN_START_BUTTON, INPUT_PULLUP);
	pinMode(PIN_STOP_BUTTON, INPUT_PULLUP);
	pinMode(PIN_LED_STATUS, OUTPUT);
	pinMode(PIN_MOTOR_CTRL, OUTPUT);

	// 安全のため起動時は停止状態にする
	digitalWrite(PIN_MOTOR_CTRL, LOW);
	digitalWrite(PIN_LED_STATUS, LOW);

	currentState = STATE_IDLE;
	selectedMinutes = 10;
	remainingSeconds = 0;

	unsigned long now = millis();
	// millis()基準の初期時刻を保持
	lastTickMillis = now;
	lastBlinkMillis = now;
	lastButtonMillis = now;
	lastErrorBlinkMillis = now;

	Serial.begin(9600);
	Serial.println("[BOOT] Timer fan controller started.");
	Serial.println("[INFO] Idle: 1x START=change(10/20/30), 2x START within 1s=run");

	updateOutputs(currentState);
}

void loop() {
	unsigned long now = millis();

	// 毎ループ: 入力更新→長押し判定→停止最優先処理
	readButtons(now);
	factoryResetByLongPress(now);
	handleStopReset(now); // 停止要求を最優先

	if (currentState == STATE_IDLE) {
		// 1回押しで設定変更、1秒以内の2回目で運転開始
		if (startButtonEvent) {
			if (startArmed && (now - lastStartEventMillis <= 1000)) {
				// 開始時に設定分を秒へ変換
				remainingSeconds = (unsigned int)selectedMinutes * 60;
				currentState = STATE_RUNNING;
				lastTickMillis = now;
				startArmed = false;

				Serial.print("[STATE] RUNNING start, remainingSeconds=");
				Serial.println(remainingSeconds);

				updateOutputs(currentState);
			} else {
				// 1回目押下は設定時間の切替として扱う
				handleTimeSetting();
				startArmed = true;
				lastStartEventMillis = now;
			}
			startButtonEvent = false;
		}

		// 2回押し待ち時間を超えたら待機解除
		if (startArmed && (now - lastStartEventMillis > 1000)) {
			startArmed = false;
		}
	}

	else if (currentState == STATE_RUNNING) {
		// 動作中は1秒周期で残り時間を減算
		runTimerControl(now);

		if (remainingSeconds == 0) {
			// 残り0で自動停止し、完了表示へ
			currentState = STATE_FINISHED;
			lastBlinkMillis = now;
			ledState = false;

			Serial.println("[STATE] FINISHED (auto stop)");

			updateOutputs(currentState);
		}
	}

	else if (currentState == STATE_FINISHED) {
		// 停止完了中はLEDを点滅
		blinkOnFinish(now);

		if (startButtonEvent) {
			// 開始ボタンで完了表示を解除して待機へ
			startButtonEvent = false;
			currentState = STATE_IDLE;
			remainingSeconds = 0;
			startArmed = false;
			Serial.println("[STATE] IDLE (ack finish)");
			updateOutputs(currentState);
		}
	}

	else if (currentState == STATE_ERROR) {
		// 異常時はモーター停止を維持
		digitalWrite(PIN_MOTOR_CTRL, LOW);
		if (now - lastErrorBlinkMillis >= ERROR_BLINK_INTERVAL) {
			// 異常通知の高速点滅
			lastErrorBlinkMillis = now;
			ledState = !ledState;
			digitalWrite(PIN_LED_STATUS, ledState ? HIGH : LOW);
		}

		if (stopButtonEvent) {
			// 停止操作でエラー復帰
			stopButtonEvent = false;
			currentState = STATE_IDLE;
			remainingSeconds = 0;
			Serial.println("[STATE] IDLE (recover from error)");
			updateOutputs(currentState);
		}
	}
}

void readButtons(unsigned long now) {
	// イベントは毎ループでクリアし、立下り時のみtrueにする
	startButtonEvent = false;
	stopButtonEvent = false;

	byte startRaw = digitalRead(PIN_START_BUTTON);
	byte stopRaw  = digitalRead(PIN_STOP_BUTTON);

	// 同時押しは安全側として停止優先
	if (startRaw == LOW && stopRaw == LOW) {
		if (now - lastButtonMillis >= DEBOUNCE_DELAY_MS) {
			stopButtonEvent = true;
			lastButtonMillis = now;
			prevStartRaw = startRaw;
			prevStopRaw = stopRaw;
		}
		return;
	}

	bool changed = (startRaw != prevStartRaw) || (stopRaw != prevStopRaw);
	if (!changed) {
		// 変化なしなら何もしない
		return;
	}

	if (now - lastButtonMillis < DEBOUNCE_DELAY_MS) {
		// デバウンス時間内の変化は無視
		return;
	}

	// HIGH→LOW(立下り)を押下イベントとして扱う
	if (prevStartRaw == HIGH && startRaw == LOW) {
		startButtonEvent = true;
	}
	if (prevStopRaw == HIGH && stopRaw == LOW) {
		stopButtonEvent = true;
	}

	prevStartRaw = startRaw;
	prevStopRaw = stopRaw;
	lastButtonMillis = now;
}

void handleTimeSetting() {
	// 待機中の開始ボタンイベントで設定時間を循環更新
	if (currentState != STATE_IDLE || !startButtonEvent) {
		return;
	}

	if (selectedMinutes == 10) {
		selectedMinutes = 20;
	} else if (selectedMinutes == 20) {
		selectedMinutes = 30;
	} else {
		selectedMinutes = 10;
	}

	Serial.print("[SET] selectedMinutes=");
	Serial.println(selectedMinutes);
}

void runTimerControl(unsigned long now) {
	// 異常値は安全側でエラー遷移
	if (remainingSeconds > MAX_REMAINING_SECONDS) {
		currentState = STATE_ERROR;
		digitalWrite(PIN_MOTOR_CTRL, LOW);
		Serial.println("[ERROR] remainingSeconds out of range");
		return;
	}

	if (now - lastTickMillis >= TIMER_TICK_MS) {
		lastTickMillis = now;
		// 1秒ごとに1ずつ減算
		if (remainingSeconds > 0) {
			remainingSeconds--;
			Serial.print("[TIMER] remainingSeconds=");
			Serial.println(remainingSeconds);
		}
	}
}

void handleStopReset(unsigned long now) {
	// 停止イベントが無ければ何もしない
	if (!stopButtonEvent) {
		return;
	}

	(void)now;
	// 状態に関係なく即時停止して待機へ戻す
	digitalWrite(PIN_MOTOR_CTRL, LOW);
	currentState = STATE_IDLE;
	remainingSeconds = 0;
	ledState = false;
	startArmed = false;
	digitalWrite(PIN_LED_STATUS, LOW);

	Serial.println("[STATE] IDLE (manual stop/reset)");

	stopButtonEvent = false;
}

void updateOutputs(byte state) {
	// 状態に応じたモーター/LED出力
	if (state == STATE_IDLE) {
		digitalWrite(PIN_MOTOR_CTRL, LOW);
		ledState = false;
		digitalWrite(PIN_LED_STATUS, LOW);
		return;
	}

	if (state == STATE_RUNNING) {
		digitalWrite(PIN_MOTOR_CTRL, HIGH);
		ledState = true;
		digitalWrite(PIN_LED_STATUS, HIGH);
		return;
	}

	if (state == STATE_FINISHED) {
		digitalWrite(PIN_MOTOR_CTRL, LOW);
		return;
	}

	// 定義外状態は安全停止＋高速点滅へ
	digitalWrite(PIN_MOTOR_CTRL, LOW);
	currentState = STATE_ERROR;
}

void blinkOnFinish(unsigned long now) {
	// 停止完了中の500ms点滅
	if (now - lastBlinkMillis >= FINISH_BLINK_INTERVAL) {
		lastBlinkMillis = now;
		ledState = !ledState;
		digitalWrite(PIN_LED_STATUS, ledState ? HIGH : LOW);
	}
}

void factoryResetByLongPress(unsigned long now) {
	// 停止ボタン長押しで設定値を初期化
	byte stopRaw = digitalRead(PIN_STOP_BUTTON);

	if (stopRaw == LOW) {
		// 押下開始時刻を記録
		if (stopPressStartMillis == 0) {
			stopPressStartMillis = now;
		}

		if (!stopLongPressHandled && (now - stopPressStartMillis >= LONG_PRESS_MS)) {
			// 連続実行防止フラグで1回だけ実行
			selectedMinutes = 10;
			stopLongPressHandled = true;

			// 初期化通知としてLEDを2回点滅
			for (int i = 0; i < 2; i++) {
				digitalWrite(PIN_LED_STATUS, HIGH);
				delay(120);
				digitalWrite(PIN_LED_STATUS, LOW);
				delay(120);
			}

			Serial.println("[FACTORY RESET] selectedMinutes=10");
			updateOutputs(currentState);
		}
	} else {
		// ボタン解放で次回長押し判定を再許可
		stopPressStartMillis = 0;
		stopLongPressHandled = false;
	}
}
```

