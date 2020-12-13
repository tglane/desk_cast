DeskCast
========
At some point in the future this should be a linux (and maybe mac and windows) compatible CLI application to stream your current desktop to a googlecast enabled device.  
The ultimate goal is that it is even possible to use any googlecast, DIAL or airplay enabled device as a wireless secondary screen.  

Current status:
---------------
Currently the app can be used to stream an mp4 encoded video or a jpeg image to a googlecast device. 
Next step will be implementing the streaming of the current desktop using ffmpeg and a very simple webserver.

How to use:
-----------
Compile the app by simply typing `make` in the root directory of the project and make sure there is a ssl certificate file (cert.pem by default) as well as a key file (key.pem by default) in the directory.
Start the app by typing `./desk_cast`
Wait for the network scanning to finish. This will show a list of available devices on the command line. Type in the number of the device to use.
This will instruct the selected device to download the current test video from the `test_data` directory.

TODOs:
------
* Correctly close connection to chromecast on error or exit
* Sometimes the same cast device is listed multiple times. Make sure that one devices only shows up once.
* Make sure the webserver can serve HLS streams correctly.
* Caputre the screen (on linux via x11-grab device) with ffmpeg lib.
* Create HLS stream from screen capture and serve this via the webserver.
* Create a virtual display and capture this.
