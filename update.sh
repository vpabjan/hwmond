#!/bin/bash

echo "compiling..."
sudo chmod +x build.sh
./build.sh

echo "disable daemon..."
sudo systemctl stop hwmond

echo "copying daemon..."
sudo cp hwmond /usr/local/sbin/
sudo chmod 755 /usr/local/sbin/hwmond
#sudo cp hwmond.service /etc/systemd/system/hwmond.service

echo "enable daemon..."
sudo systemctl start hwmond

echo "done."
