# MQTT example application for 96Boards Nitrogen

[[_TOC_]]

## Overview

Application acting as MQTT publisher or subscriber (depending on the
configuration).
It's indented to be used primarily on 96Boards Nitrogen using IPv6
over BLE (6LoWPAN).

### Requirements
- Supported boards:
  - 96Boards Nitrogen

## Building and flashing
### Customization
See `config.h`, `prj.conf` and `boards/96b_nitrogen.conf` files fore some
constants and options you might want to customize, like IP addresses, client's ID, etc.

#### Minimal required customization
User needs to set at least two IPv6 addresses - our own IP and broker's IP
in the configuration.  
It can be done by modifying `boards/96b_nitrogen.conf` file or by
passing them to `west build` command as shown below.  

* Modifying `boards/96b_nitrogen.conf` example
```
CONFIG_NET_CONFIG_MY_IPV6_ADDR="fe80::d9e7:2fff:fe33:959d"
CONFIG_NET_CONFIG_PEER_IPV6_ADDR="fe80::ccc0:79ff:feb3:e40c"

```

* Passing values in command line example
```
west build (...) -DCONFIG_NET_CONFIG_MY_IPV6_ADDR="fe80::d9e7:2fff:fe33:959d" \
			   -DCONFIG_NET_CONFIG_PEER_IPV6_ADDR="fe80::ccc0:79ff:feb3:e40c"
```

#### App mode customization

In order to change app's behavior from `publisher` to `subscriber` set following
option anywhere in the configuration files (project or board):
```
CONFIG_MQTT_SUBSCRIBER=y
```
or in the `west build` command line:
```
west build (...) -DCONFIG_MQTT_SUBSCRIBER=y
```

**Note:** by default the application serves as a `publisher`.

### Building and flashing with Zephyr SDK
Provided Zephyr SDK is installed and source on your computer, just do:

```
$ west build -p auto -b 96b_nitrogen /path/to/this/app -DCONFIG_XYZ  && west flash
```
from `/path/to/zephyrproject/zephyr/` directory.

**Note:** currently, the earliest code base eligible to run the outbound TCP connection
          from Zephyr without timeout is v2.6.0-rc1. Please update your sources
	  if needed.

## Running

### Configuring and running Linux broker and publisher

Use Linux machine as a broker and client for testing. The machine running Linux
should be equipped in bluetooth interface as well as MQTT broker/client (for
instance Mosquitto).   
The example below was tested on 96Boards Avenger running OpenHarmony.  

1. Install `mosquitto` and `mosquitto-clients`  
If using yocto, please add following line to your `conf/local.conf` file
```
IMAGE_INSTALL_append = " mosquitto mosquitto-clients"
```
`mosquitto-clients` will install `mosquitto_pub` and `mosquitto_sub` apps.

2. Configure mosquitto broker service   
Modify following configuration file in your system:
```
/etc/mosquitto/mosquitto.conf
```
by adding
```
socket_domain ipv6
```

That will force listener to only use IPv6.  
Remember to restart the machine or service after updating the configuration
file.
```
# systemctl restart mosquitto.service
```

3. Load `6lowpan` kernel module   

```
# modprobe bluetooth_6lowpan
```

**Note:** `6lowpan` module has a bug that prevents it from creating a successful
          TCP connection. Make sure that this problem is fixed by applying
	  [this](https://git.ostc-eu.org/OSTC/OHOS/meta-av96/-/raw/develop/meta-av96-core/recipes-kernel/linux-stm32mp/files/0005-Bluetooth-6lowpan-fix-direct-peer-to-peer-connection.patch)
	  patch.

4. Enable `6lowpan` (should be enabled by default)
```
# echo 1 > /sys/kernel/debug/bluetooth/6lowpan_enable
```

5. Initialize LoWPAN connection on Linux

Shortly after board running Zephyr boots, one should be able to see following
on the Zephyr's console:

```
Booting Zephyr OS build zephyr-v2.5.0-3762-gf89771dd9ccf  ***
[00:00:00.269,317] <inf> bt_hci_core: HW Platform: Nordic Semiconductor (0x0002)
[00:00:00.269,348] <inf> bt_hci_core: HW Variant: nRF52x (0x0002)
[00:00:00.269,348] <inf> bt_hci_core: Firmware: Standard Bluetooth controller (0x00) Version 2.6 Build 0
[00:00:00.270,019] <inf> bt_hci_core: Identity: D9:E7:2F:33:95:9D (random)
[00:00:00.270,019] <inf> bt_hci_core: HCI: version 5.2 (0x0b) revision 0x0000, manufacturer 0x05f1
[00:00:00.270,019] <inf> bt_hci_core: LMP: version 5.2 (0x0b) subver 0xffff
[00:00:00.270,477] <inf> net_config: Initializing network
[00:00:00.270,629] <inf> net_config: Waiting interface 1 (0x200011b0) to be up...
```

at that point, type following in the Linux terminal:
```
# echo "connect <Nitrogen's MAC address> 2" > /sys/kernel/debug/bluetooth/6lowpan_control
```

If LoWPAN connection is up, `bt0` networking interface should appear.

6. Verbose output from `mosquitto`  
Normally, `mosquitto` runs as a daemon in background. To see verbose output from
the service perform following:  

Disable service first
```
# systemctl stop mosquitto.service
```

Run it manually with verbose output  
```
# mosquitto -c /etc/mosquitto/mosquitto.conf -v
```

7. Publishing something on a **topic**  

In this case we don't want our screen to be flooded by published messages and pings so we keep mosquitto service running silently as is.   
Instead we will use shell terminal to publish message to a broker which is also assumed to run locally on our Linux machine.  
```
# -t specifies topic on which we publish
# -m specifies message to be sent in payload
# -d is a debugging option
# Application publishes to localhost by default
mosquitto_pub -t "sensors" -m "test message" -d
```

### Flashing and running Zephyr application on the board
1. Flash the board as described above.
2. Connect to serial port of the boards. Look for the line showing
    the advertised name and MAC address
```
[00:00:00.270,019] <inf> bt_hci_core: Identity: D9:E7:2F:33:95:9D (random)
```
3. Wait until you see

```
[00:00:00.270,477] <inf> net_config: Initializing network
[00:00:00.270,629] <inf> net_config: Waiting interface 1 (0x200011b0) to be up...
```
4. Establish LoWPAN connection from the Linux machine
```
# echo "connect D9:E7:2F:33:95:9D 2" > /sys/kernel/debug/bluetooth/6lowpan_control
```

The interface in the application should turn `up` and it will connect to MQTT
broker.
```
[00:00:05.540,588] <inf> net_config: Interface 1 (0x200011b0) coming up
[00:00:05.640,197] <inf> net_config: IPv6 address: fe80::d9e7:2fff:fe33:959d
[00:00:05.640,228] <inf> net_mqtt_publisher_sample: attempting to connect:
[00:00:07.394,958] <inf> net_mqtt_publisher_sample: MQTT client connected!
[00:00:07.394,989] <inf> net_mqtt_publisher_sample: try_to_connect: 0 <OK>
[00:00:07.784,973] <inf> net_mqtt_publisher_sample: SUBACK packet id: 1
[00:00:12.597,595] <inf> net_mqtt_publisher_sample: mqtt_ping: 0 <OK>
```
