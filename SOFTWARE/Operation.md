When the Lamoka1/2 is connected to USB-C 5v, it will display a similar yet improved interface as PIE v1.8. As soon as the units MUX shows a voltage on the input net that is above 11v yet below 14v, the device will comence a boot up sequence. The units fan is only connected to the buck 5v rail. The boot sequence will go up stream to verify everything is working correctly. 

 ## First phase 

 The MUX will be detecting the source rail, if this rail is at an appropriate voltage, (Which it will be in order to even start the sequence) stage one will be validated as functioning. 

 ## Second phase

 Next, the MCU must send a output to start spinning the fan at 25%, after one second, the MCU will read the tach sensors output to verify that the fan is spinning and functioning properly at 25% PWM. The MCU will next test 50% for 1.5 second, then 100% for 3/4 second. When it is confirmed that the fan is completely functional along these three speeds (Normal operation will have fan speed proportional to highest temperture recorded, this is just a bootup test) the second phase will be confirmed functioning. 

 ## Third phase

 Up next, the MUX will detect the output of the 11-14v to 17v boost converter, if this rail is within 3/4 of a volt from 17v this stage will be verified as correctly operating. 

 ## Forth phase

 Things will get a bit tricky now, we are monitoring alternating current waveforms after this point. 