# hwmond
A minimalistic /sys/class/**hwmon** manager **daemon**, useful for fan speed controling; along with a minimalistic **hwmonctl**. 

Supports setting fixed values, curves & triggers.\
Supports 5 built-in curves:
> silent, loud, aggressive, log, step



## usage
>sudo hwctl list

shows active rules.

>sudo hwctl list-rules

shows the config file.

>sudo hwctl add-fixed /sys/class/hwmon/hwmon2/pwm1 150

tells the daemon to update *hwmon2/pwm1* to 150.

>sudo hwctl add-curve /sys/class/hwmon/hwmon2/pwm1 loud /sys/class/hwmon/hwmon1/temp1_input

tells the daemon to manage *hwmon2/pwm1* using the curve loud, with data from *hwmon1/temp1_input*.


>sudo hwctl add-trigger /sys/class/hwmon/hwmon1/temp1_input > 75 "hwctl apply /sys/class/hwmon/hwmon2/pwm1 255"

max out fans on *hwmon2/pwm1*, when *hwmon1/temp1_input* is larger than 75 degrees.

>sudo hwctl reload

*commands automatically signal the daemon, so changes apply immediately.*

## requirements
**systemd**, some hwmon interfaces probably...?

## installation
### compile
>sudo chmod +x build.sh

>./build.sh


### copy daemon, perms, state
>sudo cp hwmond /usr/local/sbin/

>sudo chmod 755 /usr/local/sbin/hwmond

>sudo mkdir -p /var/lib/hwmond

>sudo touch /var/lib/hwmond/state.txt

>sudo chmod 644 /var/lib/hwmond/state.txt

### copy service
>sudo cp hwmond.service /etc/systemd/system/hwmond.service

### config
>sudo mkdir -p /etc

>sudo nano /etc/hwmond.conf

*(add some initial rules or leave empty)*

### enable daemon
>sudo systemctl daemon-reload

>sudo systemctl enable hwmond

>sudo systemctl start hwmond

>sudo systemctl status hwmond

### use
now just use hwctl as shown above, optionally you may add it to PATH.

## note
Originally made for my dell latitude 5400. Compiles and works, for me! (I use arch btw.)
