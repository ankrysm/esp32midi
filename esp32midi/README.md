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

Read the instructions at `https://github.com/espressif/esp-idf/blob/master/docs/en/get-started/index.rst`

* Install python - with brew (homebrew):
    * brew install python
    * in case of a link error:
        * `sudo mkdir /usr/local/Frameworks`
        * `sudo chown $(whoami):admin /usr/local/Frameworks`
        * `brew link python`
    * Install pip `sudo easy_install pip`

* Install Xcode
    * Install commandline tools `xcode-select --install`

* Toolchain (`https://docs.espressif.com/projects/esp-idf/en/stable/get-started/macos-setup.html`)
* Download the Macos toolchain: `wget https://dl.espressif.com/dl/xtensa-esp32-elf-osx-1.22.0-80-g6c4433a-5.2.0.tar.gz`
* extract it and add to PATH:  `export PATH=$PATH:<extract path>/xtensa-esp32-elf/bin`

* checkout then esp-idf `https://github.com/espressif/esp-idf.git`
* set the environment `IDF_PATH=<where you checked out then esp-idf>/esp/esp-idf`
* add `<where you checked out then esp-idf>/esp/xtensa-esp32-elf/bin` to PATH-Variable `export PATH=$PATH:<..>`

* `python3 -m pip install --user -r $IDF_PATH/requirements.txt`

### Build

call `make` sometimes a `make clean` helps.

Sometimes especially at the first build after make clean make failes. Call `make` again

## MIDI-Files

Something about MIDI-Files:
`http://www.larsrichter-online.de/lmids/midformat.php`
`http://www.music.mcgill.ca/~ich/classes/mumt306/StandardMIDIfileformat.html`
`https://www.csie.ntu.edu.tw/~r92092/ref/midi/`
`http://somascape.org/midi/tech/mfile.html`

## Hints from the HTTP-Fileserver example

(See the README.md file in the upper level 'examples' directory for more information about examples.)

HTTP file server example demonstrates file serving with both upload and download capability, using the `esp_http_server` component of ESP-IDF. The following URIs are provided by the server:

| URI                  | Method  | Description                                                                               |
|----------------------|---------|-------------------------------------------------------------------------------------------|
|`index.html`          | GET     | Redirects to `/`                                                                          |
|`favicon.ico`         | GET     | Browsers use this path to retrieve page icon which is embedded in flash                   |
|`/`                   | GET     | Responds with webpage displaying list of files on SPIFFS and form for uploading new files |
|`/<file path>`        | GET     | For downloading files stored on SPIFFS                                                    |
|`/upload/<file path>` | POST    | For uploading files on to SPIFFS. Files are sent as body of HTTP post requests            |
|`/delete/<file path>` | POST    | Command for deleting a file from SPIFFS                                                   |

File server implementation can be found under `main/file_server.c` which uses SPIFFS for file storage. `main/upload_script.html` has some HTML, JavaScript and Ajax content used for file uploading, which is embedded in the flash image and used as it is when generating the home page of the file server.

### Note

`/index.html` and `/favicon.ico` can be overridden by uploading files with same pathname to SPIFFS.

### Usage

* Configure the project using `make menuconfig` and goto `Example Configuration` ->
    1. WIFI SSID: WIFI network to which your PC is also connected to.
    2. WIFI Password: WIFI password

* In order to test the file server demo :
    1. compile and burn the firmware `make flash`
    2. run `make monitor` and note down the IP assigned to your ESP module. The default port is 80
    3. test the example interactively on a web browser (assuming IP is 192.168.43.130):
        1. open path `http://192.168.43.130/` or `http://192.168.43.130/index.html` to see an HTML web page with list of files on the server (initially empty)
        2. use the file upload form on the webpage to select and upload a file to the server
        3. click a file link to download / open the file on browser (if supported)
        4. click the delete link visible next to each file entry to delete them
    4. test the example using curl (assuming IP is 192.168.43.130):
        1. `myfile.html` is uploaded to `/path/on/device/myfile_copy.html` using `curl -X POST --data-binary @myfile.html 192.168.43.130:80/upload/path/on/device/myfile_copy.html`
        2. download the uploaded copy back : `curl 192.168.43.130:80/path/on/device/myfile_copy.html > myfile_copy.html`
        3. compare the copy with the original using `cmp myfile.html myfile_copy.html`

### Note

Browsers often send large header fields when an HTML form is submit. Therefore, for the purpose of this example, `HTTPD_MAX_REQ_HDR_LEN` has been increased to 1024 in `sdkconfig.defaults`. User can adjust this value as per their requirement, keeping in mind the memory constraint of the hardware in use.
