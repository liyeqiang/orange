#!/bin/bash

sudo cp cb-hddledd-nv1 /usr/bin/
sudo chmod +x /usr/bin/cb-hddledd-nv1
sudo cp hddled.sh /etc/init.d/
sudo chmod +x /etc/init.d/hddled.sh
cd /etc/rc5.d
sudo ln -vsf /etc/init.d/hddled.sh S01hddled


cd -
sudo cb-heart /usr/bin/
sudo chmod +x /usr/bin/cb-heart
sudo cp heartbeat.sh /etc/init.d/
sudo chmod +x /etc/init.d/heartbeat.sh
cd /etc/rc5.d
sudo ln -vsf /etc/init.d/heartbeat.sh S01heartbeat


