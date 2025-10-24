# hwmond
A minimalistic /sys/class/**hwmon** managing **daemon**, useful for fan speed controling; along with a minimalistic **hwmonctl**.

## usage
sudo hwctl list
sudo hwctl list-rules

sudo hwctl add-fixed /sys/class/hwmon/hwmon2/pwm1 150
sudo hwctl add-curve /sys/class/hwmon/hwmon2/pwm1 loud /sys/class/hwmon/hwmon1/temp1_input
*add- commands automatically signal the daemon, so changes apply immediately.*

sudo hwctl reload

## requirements
**systemd**, some hwmon interfaces probably...?

## installation
### compile
gcc -O2 -Wall -Wextra -o hwctl hwctl.c
gcc -O2 -Wall -Wextra -o hwmond hwmond.c -lm


### copy daemon, perms, state
sudo cp hwmond /usr/local/sbin/
sudo chmod 755 /usr/local/sbin/hwmond

sudo mkdir -p /var/lib/hwmond
sudo touch /var/lib/hwmond/state.txt
sudo chmod 644 /var/lib/hwmond/state.txt

### copy service
sudo cp hwmond.service /etc/systemd/system/hwmond.service

### config
sudo mkdir -p /etc
sudo nano /etc/hwmond.conf
*(add some initial rules or leave empty)*

### enable daemon
sudo systemctl daemon-reload
sudo systemctl enable hwmond
sudo systemctl start hwmond
sudo systemctl status hwmond

### use
now just use hwctl as shown above, optionally you may add it to PATH.

## note
Originally made for my dell latitude 5400. Compiles and works, for me! (I use arch btw.)
