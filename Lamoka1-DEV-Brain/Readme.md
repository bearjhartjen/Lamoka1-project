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
  For more information on PIE, go
  <a href="../SOFTWARE/README.md">here</a>.
</p>

<p align="center">
  <a href="https://htmlpreview.github.io/?https://raw.githubusercontent.com/bearjhartjen/Lamoka1-project/Lamoka1/Lamoka1-DEV-Brain/Kicad%20files/BOM/Lamoka1-DEV-Brain.html">
    <strong>View Interactive BOM</strong>
  </a>
</p>


---

> [!CAUTION]
> This device is purely for testing. Many decisions were made due to cost and time constraints. The component layout does not follow Raspberry Pi’s recommended layout guidance. Utilize these files at your own discretion.


 Flamingo and Cactus have paid PCBway to manufacture and assemble the DEV Brain. It has arrived and works much better than expected. It does, however, make some high-frequency audible noise when plugged into low-quality power sources, so it is quite sensitive to low-quality sources. 

The DEV-Brain brings the Lamoka1’s MCU, lights, and simplicity into a 70x30mm laminated sandwich (PCB).

We are using the DEV-Brain to get an idea of how things work on the control side. We can use this model board to develop code for the lights and button. The DEV-Brain carries ten RGB LEDs (D1-D10) and one button. The main board's interface uses five LEDs (D1-D5) and one button, so the LED count is a per-board difference to keep in mind when porting firmware.

The DEV-Brain is a control-only board: it carries no power stage and is unaffected
by the Lamoka1/2's Rev C revision (the 21.75 V boost bus and the 120.0 V outlet
target on the main board). PIE V1.8 stays the DEV-Brain head; the main board
runs PIE V1.9.


This board was run through Tomachie to get the design checked; the link is [here](https://tomachie.com/r/a1f3f256-a8ca-43de-aa92-e70ef981c3c2/Lamoka1-DEV-board_report.html#design-summary).


## Why did we develop the Lamoka1-DEV-Brain?

We need an easy platform to better understand how and where to work on the Lamoka1 control. Without a doubt, the control and logic side of the Lamoka1 is extremely important, as it allows us to expose our creativity and community for free, and to just truly have fun. The DEV-Brain also just progresses the project quite a bit and gets us moving; we don't know what to do on designing the inverter and charger sections without getting control down.
 
## How does one reproduce the Lamoka1-DEV-Brain?

 We have published the board to PCBway's share projects platform [here](https://www.pcbway.com/project/shareproject/Lamoka1_DEV_Brain_49e4fd0a.html). There, you will be able to order the Lamoka1-DEV-Brain exactly as we did. 

## Project files

- **KiCad source** — the full KiCad 10 project lives in the `Kicad files` folder. Open [Lamoka1-DEV-board.kicad_sch](<Kicad files/Lamoka1-DEV-board.kicad_sch>) for the schematic, [Lamoka1-DEV-board.kicad_pcb](<Kicad files/Lamoka1-DEV-board.kicad_pcb>) for the board layout, and [Lamoka1-DEV-board.kicad_pro](<Kicad files/Lamoka1-DEV-board.kicad_pro>) for the project settings. Symbols and footprints are embedded in the `.kicad_sch` and `.kicad_pcb` files, so the schematic and layout open without extra libraries. 3D models are NOT included: the layout only stores file-path references (`${KIPRJMOD}/components/3dmodels/*.step`), and that folder is excluded from the repo by `.gitignore`. Expect missing 3D bodies in the 3D viewer until you supply your own `.step`/`.wrl` files.
- **Fab files** — everything needed to have the board manufactured yourself is in the `Lamoka1-DEV-Brain fab files` folder: the [bill of materials](<Lamoka1-DEV-Brain fab files/Lamoka1-DEV-board BOM.csv>), the [pick and place file](<Lamoka1-DEV-Brain fab files/Lamoka1-DEV-board Pick and Place.csv>), the [Gerber and drill package](<Lamoka1-DEV-Brain fab files/Lamoka1-DEV-board>), and the [PCBway assembly file](<Lamoka1-DEV-Brain fab files/PCBway files/T-1N4W1065266A-1units-Lamoka1-DEV-board+BOM(2026-07-15).xls>) we sent when we ordered our own boards.
- **3D model** — [Lamoka1-DEV-Brain.step](../HARDWARE/Mechanical/Lamoka1-DEV-Brain.step) is the board exported from the layout (outline, holes, and component bodies) for enclosure and fit checks.

<p align="center">
  <img src="../Images/lamoka1-dev-brain-render-iso.png" alt="Lamoka1-DEV-Brain isometric render" width="420">
  <img src="../Images/lamoka1-dev-brain-render-top.png" alt="Lamoka1-DEV-Brain top view" width="420">
  <br><em>3D renders of the laid-out board — exported with <code>kicad-cli pcb render</code>.</em>
</p>

## Tests

We were able to run the Lamoka1-DEV-Brain for 72 hours running the idle blue dot bouncing animation seen in [PIE v1.8](<../SOFTWARE/Portal Interface Engine/V1.8.ino>). The environment was an average of 70 degrees Fahrenheit, and an average humidity of 70 percent. We stopped the test early (there were no signs of issues) because we needed to use the Lamoka1-DEV-Brain to test the HTML file creation concept for getting diagnostic data on the Lamoka1. 


