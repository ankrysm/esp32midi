# ESP32-MIDI (work in progress - stay tuned )


It is a replacement of an old door bell system based on a Z80 with 0 KB RAM (only with registers content) and 2 K EPROM created inthe 80's
It colud play 3 trakcs simultaneously.

But after 35 years it doesn't work anymore. So I will replace it with an ESP32 System and a nanosynth module based on a SAM2695

It is a copy of the "Simple HTTP File Server Example" and other examples from esp-idf repository

It will contain
* A web service to manage the MIDI-Files and show other informations
* an NTP-Client to get into real time
* parts to decode and play MIDI-Files

## Building

I use a Mac but it will also work on linux.

### Preparation

Read the instructions on `https://docs.espressif.com/projects/esp-idf/en/latest/get-started/macos-setup.html`
 
You need python and Xcode commandline tools `xcode-select --install`

Follow the instructions and get the xtensa-esp32 toolchain.

To mimimize the system configuration I use the shell script `mk`

To flash the ESP32 install the usb-serial driver as needed.


### Build

build and flash with `./mk flash`. To get the serial output you can do a `./mk monitor` or bot: `./mk flash monitor`
 
Sometimes a `make clean` helps.

Sometimes especially at the first build after make clean make failes. Call `make` again


### Hints

* Configure the project using `make menuconfig` and goto `Example Configuration` ->
    1. WIFI SSID: WIFI network to which your PC is also connected to.
    2. WIFI Password: WIFI password


### Note

Browsers often send large header fields when an HTML form is submit. Therefore, for the purpose of this example, `HTTPD_MAX_REQ_HDR_LEN` has been increased to 1024 in `sdkconfig.defaults`. User can adjust this value as per their requirement, keeping in mind the memory constraint of the hardware in use.

## MIDI-Files

Something about MIDI-Files:
`http://www.larsrichter-online.de/lmids/midformat.php`
`http://www.music.mcgill.ca/~ich/classes/mumt306/StandardMIDIfileformat.html`
`https://www.csie.ntu.edu.tw/~r92092/ref/midi/`
`http://somascape.org/midi/tech/mfile.html`

## Hardware 

see esp32-hardware.pdf


