<p align="center">
  <img
    src="https://raw.githubusercontent.com/bearjhartjen/Lamoka1-project/Lamoka1/Images/Lamoka1-DEV-Brain-image.webp"
    alt="Lamoka1 DEV Brain"
    width="700"
  >
</p>

<p align="center">
  <strong>Lamoka1-DEV-Brain</strong>
</p>

<p align="center">
  This board can now be ordered
  <a href="https://www.pcbway.com/project/shareproject/Lamoka1_DEV_Brain_49e4fd0a.html">here</a>.
</p>

<p align="center">
  PIE versions v1.8 and below will function on this board.
  For more information on PIE, go <a href="SOFTWARE/README.md">here</a>.
</p>

<p align="center">
  <a href="https://htmlpreview.github.io/?https://raw.githubusercontent.com/bearjhartjen/Lamoka1-project/Lamoka1/Lamoka1-DEV-Brain/Kicad%20files/BOM/Lamoka1-DEV-Brain.html">
    <strong>View Interactive BOM</strong>
  </a>
</p>

---

> [!CAUTION]
> This device is purely for testing. Many decisions were made due to cost and time constraints. The component layout does not follow Raspberry Pi’s recommended layout guidance. Utilize these files at your own discretion.


 Flamingo and Cactus has paid PCBway to manufacture and assemble the DEV Brain, It has arrived, and works much better than expected. It does however make some high frequency audible noise when plugged in to low quality power sources so it is quite sensitive to low quality sources. 

The DEV-Brain brings the Lamoka1’s MCU, lights, and simplicity into a 70x30mm laminated sandwich (PCB).

We are using the DEV-Brain to get an idea of how things work on the control side. We can use this model board to develop code for the lights and button. We think that the DEV-Brain will be very similar to the Lamoka1, with only ten RGB LEDs and one button.


This board was ran through tomachie to get the design checked, [here](https://tomachie.com/r/a1f3f256-a8ca-43de-aa92-e70ef981c3c2/Lamoka1-DEV-board_report.html#design-summary) is the link.


## Why did we develop the Lamoka1-DEV-Brain?

We need a easy platform to better understand how and where to work on the Lamoka1 control. Without a doubt, The control and logic side of the Lamoka1 is extremely important as it allows us to expose our creativity and community for free, to just truly have fun. The DEV-Brain also just progresses the project quite a bit and gets us moving, we dont know what to do on designing the inverter and charger sections without getting control down.
 
## How does one reproduce the Lamoka1-DEV-Brain?

 We have published the board to PCBway's share projects platform [here](https://www.pcbway.com/project/shareproject/Lamoka1_DEV_Brain_49e4fd0a.html). There you will be able to order the Lamoka1-DEV-Brain exactly as we did. 

## Tests

We were able to run the Lamoka1-DEV-Brain for 72 hours running the idle blue dot bouncing animation seen in PIE v1.8. The enviroment was an average of 70 degrees farinheight, and an average humidity of 70 percent. We stopped the test early (there were no signs of issues) because we needed to use the Lamoka1-DEV-Brain to test html file creation concept for getting diagnostic data on the Lamoka1. 


