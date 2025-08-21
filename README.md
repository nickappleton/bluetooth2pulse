# bluetooth2pulse

A project for making an old rotary phone able to appear as a bluetooth hands-free device.

THIS IS A WORK IN PROGRESS.

## Circuit Description

### BOM

- 1x. Adafruid QT Py RP2040
- 1x. Silvertel Ag1171 SLIC module

### 2.5v Buffer




### Resonant filter


TODO: update component values to the better set below:

c1 = 1.0e-08
c2 = 3.3e-08
rx = 1.8e+03
r4 = 2.7e+05
r3 = 1.8e+03
r1 = 330k


```
                             C2 68n
                               | |
                           |---| |----------------|
                           |   | |      R4 150k   |
                           |         |--\/\/\/\---|
           R1 180k         |         |            |
PWM1  -----\/\/\/\----|    | C1 10n  |  |\        |
           R2 180k    |    |   | |   |  |  \      |
PWM2  -----\/\/\/\----+----+---| |---+--| -  \    |
           R3 1.5k    |        | |      |      ---+-- TONE
+2.5v -----\/\/\/\----|              |--| +  /
                           R5 1.5k   |  |  /
                   +2.5v --\/\/\/\---|  |/
```

The filter is designed to remove the harmonics of square waves with fundamental frequencies close to 400 Hz.

If $R_x$ is the parallel impedance of $R_1$, $R_2$ and $R_3$, then the centre frequency of the resonator is:

$$ R_x = \frac{R_1 R_2 R_3}{R_1 R_2 + R_2 R_3 + R_3 R_1} \approx 1.47 $$

$$ w_0 = \frac{1}{\sqrt{C_2 C_1 R_4 R_x}} \approx 2582.5 $$






## Software Build Procedure

You should be able to make this work on any system - but I am using OS X.

### OS X instructions

1. Download and install ARM GCC from ARM https://developer.arm.com/Tools%20and%20Software/GNU%20Toolchain
2. Download and install CMake https://cmake.org
3. Clone the Pico SDK with submodules:

```sh
git clone git@github.com:raspberrypi/pico-sdk.git --recurse-submodules
```

4. Clone the Pico examples somewhere. We use the WS2812 support included here.
```sh
git clone git@github.com:raspberrypi/pico-examples.git
```

5. Generate makefiles in some empty folder not in the source tree:

```sh
PICO_SDK_PATH=(path where Pico SDK was cloned) PICO_EXAMPLES_PATH=(path where Pico Examples was cloned) cmake (path to where this example repository was cloned) -DPICO_BOARD=adafruit_qtpy_rp2040 -DPICO_TOOLCHAIN_PATH=/Applications/ARM
```

`/Applications/ARM` is probably where the ARM tools were installed to. If not, change the path.

6. Build the example.

```sh
make -j
```

7. Plug in the QT Py while holding the "boot" button. Then copy the generated `blink.uf2` file to the QT Py.

