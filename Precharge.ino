// DEBUG - Uncomment to enable
// #define DEBUG

// Pin Assignments
const int ir_plus                = 3;  // IR+ relay
const int ir_minus               = 5;  // IR- relay
const int precharge              = 7;  // Precharge relay
const int precharge_complete_out = 10; // AUX 1 - Precharge completion signal to RTD
const int rtd_button_in          = 9;  // RTD enable button
const int shutdown_in            = 11; // Shutdown signal
const int complete_led           = 4;  // Precharge complete green LED
const int fault_led              = 6;  // Fault red LED
const int tractive_system_in     = A0; // Tractive system voltage in
const int battery_in             = A1; // Battery voltage in

// Constants
const float PRECHARGE_COMPLETE_PERCENT = 0.91; // Voltage ratio threshold for completion
const float MIN_BATTERY_VOLTAGE = 2.0; // Minimum battery voltage in case of read errors
const int PRECHARGE_TIMEOUT_MS = 6000; // Time to wait before precharge timeout (6 seconds)

// State
enum States { IDLE, CHARGING_WAIT, COMPLETE };
States state = IDLE;
int last_button_state = LOW; // For edge detection of button press
unsigned long precharge_start_time = 0; // The time (ms) the precharge process started at

// Debug
#ifdef DEBUG
unsigned long last_debug_print = 0;// Limits the debug print rate
const int DEBUG_PRINT_DELAY = 100; // Time to wait before printing another debug statement
#endif


void setup() {
#ifdef DEBUG
  // Initialize serial for debugging
  Serial.begin(9600);
#endif

  // Configure pins
  pinMode(shutdown_in,            INPUT);
  pinMode(rtd_button_in,          INPUT);
  pinMode(ir_plus,                OUTPUT);
  pinMode(ir_minus,               OUTPUT);
  pinMode(precharge,              OUTPUT);
  pinMode(complete_led,           OUTPUT);
  pinMode(fault_led,              OUTPUT);
  pinMode(precharge_complete_out, OUTPUT);

  // Initialize pin state
  digitalWrite(ir_plus, LOW); // Open relay
  digitalWrite(ir_minus, LOW); // Open relay
  digitalWrite(precharge, LOW); // Open relay
  digitalWrite(fault_led, HIGH); // Precharge incomplete, red light
  digitalWrite(complete_led, LOW); // Precharge incomplete, no green light
  digitalWrite(precharge_complete_out, LOW); // Precharge incomplete, no signal
}



void loop() {
  // Button rising edge detection (prevent repeated trigger)
  int current_button_state = digitalRead(rtd_button_in);
  bool is_pressed = (current_button_state == HIGH) && (last_button_state == LOW);
  last_button_state = current_button_state;
  
  // Shutdown check
  if (digitalRead(shutdown_in) == LOW) {
#ifdef DEBUG
    if (millis() - last_debug_print >= DEBUG_PRINT_DELAY) {
      Serial.println("Shutdown off!");
      last_debug_print = millis();
    }
#endif
    state = IDLE;
  }

  // Status LEDs
  digitalWrite(fault_led,    (state == COMPLETE) ? LOW : HIGH);
  digitalWrite(complete_led, (state == COMPLETE) ? HIGH : LOW);

  // Send precharge state to RTD
  digitalWrite(precharge_complete_out, (state == COMPLETE) ? HIGH : LOW);

  switch (state) {
    // Idle - wait for button press
    case IDLE:
      digitalWrite(ir_plus,   LOW);
      digitalWrite(ir_minus,  LOW);
      digitalWrite(precharge, LOW);

      // If button is pressed, start charging process
      if (is_pressed && digitalRead(shutdown_in) == HIGH) {
#ifdef DEBUG
        Serial.println("Button pressed, waiting for precharge to complete...");
#endif
        precharge_start_time = millis();
        state = CHARGING_WAIT;
      }
      break;
    

    // Charging wait - close IR- and Precharge relays until sufficient charge
    case CHARGING_WAIT:
      digitalWrite(ir_plus,   LOW);
      digitalWrite(ir_minus,  HIGH); // Closed
      digitalWrite(precharge, HIGH); // Closed

      // Timeout check
      if (millis() - precharge_start_time >= PRECHARGE_TIMEOUT_MS) {
#ifdef DEBUG
        Serial.println("ERROR - Precharge timeout!");
#endif
        state = IDLE;
        break;
      }
      
      // Read voltages
      float tractive_voltage = analogRead(tractive_system_in) * (5.0 / 1023.0);
      float battery_voltage = analogRead(battery_in) * (5.0 / 1023.0);
      if (battery_voltage <= MIN_BATTERY_VOLTAGE) {
#ifdef DEBUG
        Serial.println("ERROR - Battery voltage too low!");
#endif
        state = IDLE;
        break;
      }
      float charge_ratio = tractive_voltage / battery_voltage;
#ifdef DEBUG
      if (millis() - last_debug_print >= DEBUG_PRINT_DELAY) {
        printVoltages(tractive_voltage, battery_voltage, charge_ratio);
        last_debug_print = millis();
      }
#endif

      // If charge is sufficient, signal completion
      if (charge_ratio >= PRECHARGE_COMPLETE_PERCENT) {
#ifdef DEBUG
        Serial.println("Precharge complete!");
#endif
        state = COMPLETE;
      }
      break;
    

    // Complete - close IR+ relay and open Precharge relay
    case COMPLETE:
      digitalWrite(ir_minus,  HIGH); // Closed
      digitalWrite(ir_plus,   HIGH); // Closed
      digitalWrite(precharge, LOW);
      break;

    // Broken state - return to idle
    default:
#ifdef DEBUG
      Serial.println("ERROR - Invalid state!");
#endif
      state = IDLE;
      break;
  }
}



// Formats and sends the voltages over serial for debugging.
// Example: XX.xx% Charged    Tractive: Y.yy V    Battery: Z.zz V
#ifdef DEBUG
void printVoltages(float tractive, float battery, float ratio) {
  Serial.print(ratio * 100, 2);
  Serial.print("%% Charged    Tractive: ");
  Serial.print(tractive, 2);
  Serial.print("V    Battery: ");
  Serial.print(battery, 2);
  Serial.println("V");
}
#endif
