// DEBUG - Uncomment to enable
// #define DEBUG

// Pin Assignments
#define ir_plus                (3)  // IR+ relay
#define ir_minus               (5)  // IR- relay
#define precharge              (7)  // Precharge relay
#define precharge_complete_in  (10) // AUX 1 - Precharge completion signal from CAN shield
#define rtd_button_in          (9)  // RTD enable button
#define shutdown_in            (11) // Shutdown signal
#define complete_led           (4)  // Precharge complete green LED
#define fault_led              (6)  // Fault red LED

// Constants
#define PRECHARGE_TIMEOUT_MS (8000) // Time to wait before precharge timeout
#define SHUTDOWN_LOSS_MS     (100)  // Time to consider a loss of shutdown (ignored if too short)

// State
enum States { IDLE, CHARGING_WAIT, COMPLETE };
States state = IDLE;
int last_button_state = LOW; // For edge detection of button press
unsigned long precharge_start_time = 0; // The time (ms) the precharge process started at
unsigned long last_high_shutdown_time = 0; // The time (ms) that shutdown was last HIGH

// Debug
#ifdef DEBUG
unsigned long last_debug_print = 0;// Limits the debug print rate
#define DEBUG_PRINT_DELAY (100) // Time to wait before printing another debug statement
#endif


void setup() {
#ifdef DEBUG
  // Initialize serial for debugging
  Serial.begin(9600);
#endif

  // Configure pins
  pinMode(shutdown_in,            INPUT);
  pinMode(rtd_button_in,          INPUT);
  pinMode(precharge_complete_in,  INPUT);
  pinMode(ir_plus,                OUTPUT);
  pinMode(ir_minus,               OUTPUT);
  pinMode(precharge,              OUTPUT);
  pinMode(complete_led,           OUTPUT);
  pinMode(fault_led,              OUTPUT);

  // Initialize pin state
  digitalWrite(ir_plus, LOW); // Open relay
  digitalWrite(ir_minus, LOW); // Open relay
  digitalWrite(precharge, LOW); // Open relay
  digitalWrite(fault_led, HIGH); // Precharge incomplete, red light
  digitalWrite(complete_led, LOW); // Precharge incomplete, no green light
}



void loop() {
  // Button rising edge detection (prevent repeated trigger)
  int current_button_state = digitalRead(rtd_button_in);
  bool is_pressed = (current_button_state == HIGH) && (last_button_state == LOW);
  last_button_state = current_button_state;
  
  // Shutdown check
  int shutdown_status = digitalRead(shutdown_in);

  if (shutdown_status == LOW && millis() - last_high_shutdown_time >= SHUTDOWN_LOSS_MS) {
#ifdef DEBUG
    if (millis() - last_debug_print >= DEBUG_PRINT_DELAY) {
      Serial.println("Shutdown off!");
      last_debug_print = millis();
    }
#endif
    state = IDLE;
  }
  
  if (shutdown_status == HIGH) {
    last_high_shutdown_time = millis();
  }

  // Status LEDs
  digitalWrite(fault_led,    (state == COMPLETE) ? LOW : HIGH);
  digitalWrite(complete_led, (state == COMPLETE) ? HIGH : LOW);

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

      // If charge is sufficient, signal completion
      if (digitalRead(precharge_complete_in) == HIGH) {
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
