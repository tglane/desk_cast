DeskCast
========
At some point in the future this should be a linux (and maybe mac and windows) compatible CLI application to stream your current desktop to a googlecast enabled device.  
The ultimate goal is that it is even possible to use any googlecast, DIAL or airplay enabled device as a wireless secondary screen.  

Current status:
---------------
Currently the app can be used to stream an HLS encoded video file to a googlecast device. The videos .m3u8 and .ts files are currently just static files in the `test_data` directory.
The googlecast api is also capable of streaming image files or most video containers (e.g. mp4) but that is currently not implemented in the main application.

How to use:
-----------
Compile the app by simply typing `make` in the root directory of the project and make sure there is a ssl certificate file (cert.pem by default) as well as a key file (key.pem by default) in the directory.
Start the app by typing `./desk_cast`
Wait for the network scanning to finish. This will show a list of available devices on the command line. Type in the number of the device to use.
This will instruct the selected device to download the current test video from the `test_data` directory.

Development:
------------
This is developed in my spare time so new features will be added inconsistently. Feel free to contact me if you want to contribute :)

TODOs:
------
* Implement structures/classes to properly handle the CLI (seperate the user interface from the `main.cpp`)
  * Select casting mode(image / video file / livestream from desktop)
  * Select the device to cast to (from a list of all devices including googlecast, upnp and airplay if implemented)
* Correctly close connection to chromecast on error or exit.
* Sometimes the same cast device is listed multiple times. Make sure that one devices only shows up once.
* Caputre the screen (on linux via x11-grab device) with ffmpeg lib (on brach `capture`).
    * Create HLS stream from screen capture and serve this via the webserver.
    * Create a virtual display and capture this.
* Implement classes to enable casting to UPNP devices just like to googlecast devices (on branch `dlna_implementation`)
* Implement classes to enable casting to AirPlay devices just like to googlecast devices (will be on branch `airplay` in the future)
