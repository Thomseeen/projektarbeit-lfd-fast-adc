# How to get the PAHO.MQTT library for C++ in MS Visual Studio

1. Install CMake https://cmake.org/
    * 64bit version
    * Add to PATH!
2. Build paho.mqtt.c
    1. Clone/Download https://github.com/eclipse/paho.mqtt.c
    2. Open MSBuild Command Prompt
    3. Navigate to paho.mqtt.c sources (`<paho.mqtt.c-sources>`)
    4. Build paho.mqtt.c easily from the MSBuild Command Prompt
        ```
        > cmake -Bbuild -H. -DCMAKE_INSTALL_PREFIX=<paho-c-install-path>
        > cmake --build build/ --target install
        ```
    5. Clone/Download https://github.com/eclipse/paho.mqtt.cpp
    6. Open CMake-GUI
    7. Select paho.mqtt.cpp sources (`<paho.mqtt.cpp-sources>`) as source code
    8. Select some `<paho-cpp-install-path>`
    9. Hit Configure
    10. Check only `PAHO_BUILD_STATIC` (no tests, no ssl)!
    11. Set `PAHO_MQTT_C_INCLUDE_DIRS` to `<paho-c-install-path>\include`
    12. Set `PAHO_MQTT_C_LIBRARIES` to `<paho-c-install-path>\lib`
    13. Hit Generate and open the created VS-Project
    14. In VS set build to Release x64 and hit build on the whole solution
3. Static C++ library should now be at `<paho-cpp-install-path>\src\Release\paho-mqttpp3-static.lib`
4. Set the project to Release x64
5. In project simple-msvc-client open the *Properties* and add in *VC++ Directories* at *Include Directories* `<paho.mqtt.cpp-sources>\src` and `<paho.mqtt.c-sources>\src`
6. In the *Properties* also add at *Inputs* in *Linker* at *Additional Dependencies* all three `*.libs`.
    * `<paho-c-install-path>\lib\paho-mqtt3a.lib`
    * `<paho-c-install-path>\lib\paho-mqtt3c.lib`
    * `<paho-cpp-install-path>\src\Release\paho-mqttpp3-static.lib`
7. The executable also needs access to the DLLs from the C-Library at runtime so copy both into the project.
    * `<paho-c-install-path>\bin\paho-mqtt3a.dll`
    * `<paho-c-install-path>\bin\paho-mqtt3a.dll`