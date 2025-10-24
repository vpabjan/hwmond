#!/bin/bash
echo "compiling..."
sudo chmod +x build.sh
./build.sh

echo "copy daemon to sbin..."
sudo cp hwmond /usr/local/sbin/

echo "give daemon perms..."
sudo chmod 755 /usr/local/sbin/hwmond

echo "prepare /var..."
sudo mkdir -p /var/lib/hwmond
sudo touch /var/lib/hwmond/state.txt
sudo chmod 644 /var/lib/hwmond/state.txt

echo "copy service..."
sudo cp hwmond.service /etc/systemd/system/hwmond.service

echo "create hwmond.conf..."
sudo mkdir -p /etc
sudo touch /etc/hwmond.conf

echo "reload and enable daemon..."
sudo systemctl daemon-reload
sudo systemctl enable hwmond
sudo systemctl start hwmond
sudo systemctl status hwmond
