When the Lamoka1/2 is connected to USB-C 5v, it will display a similar yet improved version of the PIE v1.8 interface. As soon as the unit's MUX shows a voltage on the input net that is above 11v yet below 14v, the device will commence a boot-up sequence. The unit's fan is only connected to the buck 5v rail. The boot sequence will go upstream to verify everything is working correctly. 

 ## First phase 

 The MUX will be detecting the source rail. If this rail is at an appropriate voltage (which it will be in order to even start the sequence), stage one will be validated as functioning. 

 ## Second phase

 Next, the MCU must send an output to start spinning the fan at 25%. After one second, the MCU will read the tach sensor's output to verify that the fan is spinning and functioning properly at 25% PWM. The MCU will next test 50% for 1.5 seconds, then 100% for 3/4 of a second. When it is confirmed that the fan is completely functional at these three speeds (normal operation will have fan speed proportional to the highest temperature recorded; this is just a boot-up test), the second phase will be confirmed functioning. 

 ## Third phase

 Up next, the MUX will detect the output of the 11-14v to 17v boost converter. If this rail is within 3/4 of a volt of 17v, this stage will be verified as correctly operating. 

 ## Fourth phase

 Things will get a bit tricky now; we are monitoring alternating current waveforms after this point. 