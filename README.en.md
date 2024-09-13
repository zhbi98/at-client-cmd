# AT Command

[![License](https://img.shields.io/badge/license-Apache%202-green.svg)](https://gitee.com/moluo-tech/AT-Command/blob/master/LICENSE)

## Brief

AT command(V2) is an interactive component for managing AT command communication. It is suitable for Modem, WIFI module, Bluetooth and other scenarios that use AT command or ASCII command line communication. It covers most forms of AT communication, such as parameter setting, query, binary data sending, etc. It also supports interactive management of custom commands. Since each command request is asynchronous, it is also supported for non-operating system environments. Compared to the V1 version, the new version has a lot of optimization in command reception matching, URC variable length data capture, and memory security, allowing it to handle more complex product applications.

## Feature
- All command requests are asynchronous and can be run without an operating system.
- Supports single-line commands, batch commands, variable-parameter commands, and user-defined AT commands.
- Supports command response timeout, error retransmission, and priority management.
- Support variable length URC(unsolicited active primary) message capture.
- Supports communication management of multiple AT devices.
- Supports memory usage monitoring and limiting.
- Supports life cycle management of command requests, and monitors command execution status in real time.

## System Requirements

In order for AT commands to communicate properly, the target system must meet the following requirements：
- Dynamic memory support.
- RAM resource: At least 1KB (depending on the Settings of the receive buffer and URC buffer). It is recommended for systems that can allocate more than 3KB of memory.
- Compiler: The system uses some C99 features (flexible arrays, inline), so the compiler needs to enable C99 support. For IAR, GCC they are turned on by default, while Keil MDK requires manual addition of compilation options (--c99 --gnu).

## The new version differs from the V1 version

The V1 version is divided into two modules, "at" module is only suitable for operating in the OS environment, while "at_chat" module is suitable for operating in the environment without an operating system. It adopts the way of pre-allocated memory to manage AT requests, and does not require dynamic memory support, which also limits its application scope. It also runs on the OS, but the support is not perfect. V2 version mainly optimizes the "at_chat" module as a whole, supports URC function, and also strengthens the support for OS environment. Since it uses dynamic memory to manage AT command requests, it has higher requirements on RAM resources, but it is more convenient to use.

### How to choose
If the platform RAM resources used (such as 8-bit microcontrollers) are limited and only used for simple AT communication, the V1 version is appropriate, while the V2 version is recommended if the RAM resources are sufficient.

## Getting Started

The following is a brief introduction of how to use, 4 steps to complete:

### 1. Define the adapter and set the driver interface and buffer

```c
/**
 * @brief AT adapter
 */
static const at_adapter_t at_adapter = {
    .lock          = at_mutex_lock，           //Multi-task lock (NULL for non-OS)
    .unlock        = at_mutex_unlock，         //Multi-task unlock (NULL for non-OS)
    .write         = at_device_write，         //Data write interface (non-blocking)
    .read          = at_device_read，          //Data read interface (non-blocking)
    .debug         = at_debug，                //Debug print interface (NULL if not needed)
    .recv_bufsize  = 256                       //Receive buffer size (as required)
};
```

### 2. Use the AT adapter to create an AT communication object
```c
    at_obj_t *at_obj;
    //....
    at_obj = at_obj_create(&at_adapter);
    if (at_obj == NULL) {
        printf("at object create failed\r\n");
    }  
    //...
```

### 3. Add a scheduled polling task
```c
/**
 * @brief Polling handler
 */
void at_device_process(void)
{
    static unsigned int timer;
    //To speed up the AT command processing response, you are advised to poll the AT command once within 5ms.
    if (get_tick() - timer > 5) {
        timer = get_tick();
        at_obj_process(&at_obj);
    }    
}

```
### 4. Send the AT command

After completing the above steps, you can run the AT command to request. The following uses querying the signal quality of the MODEM as an example to demonstrate how to send the AT command and parse the response content.

**The command format is as follows:**

```shell

=>  AT+CSQ
<=  +CSQ: <rssi>，<ber>
<=  OK

```

**Code:**

```C

/**
 * @brief  Command response handler
 */
static void csq_respose_callback(at_response_t *r)   
{
    int rssi， ber;
    //+CSQ: <rssi>，<ber>
    if (r->code == AT_RESP_OK) {
        //After the command response is successful, extract rssi and ber
        if (sscanf(r->prefix， "+CSQ:%d，%d"， &rssi， &ber) == 2) {
            printf("rssi:%d， ber:%d\r\n"， rssi， ber);
        }
    } else {
        printf("'CSQ' command response failed!\r\n");
    }
}  
/**
 * @brief  Send a read CSQ value request
 */
static void read_csq(void)
{
    at_send_singlline(at_obj， csq_respose_callback， 1000， 0， "AT+CSQ"); 
}

```
**Here is a rendering of it running on the M169 WIFI (example:`at_chat/samples/none_os`)**
![wifi](images/../docs/images/wifi.png)

## More cases
For more application examples, check out the directory 'at_chat/samples', which provides examples of several typical platforms.

Take Linux as an example. You can run the AT communication emulator by running the following command:

```shell
    cd ./at_chat/samples/linux
    make clean & make
    ./output/demo
```

If you are using vscode, go directly to the samples/linux directory and press F5 to start running.

If the program runs properly, you will see the following information printed on the terminal:

```c
*******************************************************
This is an asynchronous AT command framework.
Author:roger.luo, Mail:morro_luo@163.com
*******************************************************

Please input test item:
        1:Display memory information.
        2:Testing singlline command.
        3:Testing multiline commands.
        4:Testing variable parameter command.
        5:Testing response timeout retry.
        6:Testing response error retry.
        7:Testing command abort.
        8:Testing specific response prefix.
        9:Testing custom command.
        10:Testing buffer send.
        11:Testing 'at_do_work' 1.
        12:Testing read binary data via 'at work'.
        13:Testing capture unsolicited binary data.
        14:Testing at context interface.
*******************************************************
<=
+POWER:1
 ...
Device power on event detected!
```

Following the command line prompts, enter the serial number and press enter to verify the corresponding use case.


**For more detailed instructions, please refer to:**

- [Introduction](http://moluo-tech.gitee.io/at-command/#/README.md)
- [Quick Start Guide](http://moluo-tech.gitee.io/at-command/#/quickStart.m)
- [Advanced course](http://moluo-tech.gitee.io/at-command/#/Expert.md)
- [Platform porting](http://moluo-tech.gitee.io/at-command/#/Porting.md)
