// esp DeepSleep function
// use swq from rtc_ds1307 at 1 Hz is wake signal
// monitor swq and output signal
#define sigOut 5          //D1

void setup () {
  pinMode(sigOut, OUTPUT);
  digitalWrite(sigOut, HIGH);
  ESP.deepSleep(0);
}

void loop () {
}
