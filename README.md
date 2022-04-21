# Esp32_http_server

(See the README.md file in the upper level 'examples' directory for more information about examples.)

### Configure the project

Open the project configuration menu (`idf.py menuconfig`). 

In the `Example Configuration` menu:

* Set the Wi-Fi configuration.
    * Set `WiFi SSID`.
    * Set `WiFi Password`.

Optional: If you need, change the other options according to your requirements.

### Build and Flash

Build the project and flash it to the board, then run the monitor tool to view the serial output:

Run `idf.py -p PORT flash monitor` to build, flash and monitor the project.

(To exit the serial monitor, type ``Ctrl-]``.)

```
# To test the server
Get ip address assigned to esp as url for server
Use any http cient like Postman, Rest client for testing get and post resquest
