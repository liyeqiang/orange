#!/bin/sh

    disk_dev_list=`ls /dev/sd? 2>/dev/null`
    if [ -z "$disk_dev_list" ]; then
        disk_dev_list=`ls /dev/vd? 2>/dev/null`
    fi

    for disk_dev in $disk_dev_list
    do
        space=`sudo fdisk -s $disk_dev`
        if [ $space -gt 244000000 ] && [ $space -lt 1900000000 ]; then
            disk=$disk_dev
        fi
    done

    if [ -z $disk ]; then
        echo "disk not found"
        return -1
    fi

    if [ -b ${disk}1 ]; then
        disk_dev_name=`basename ${disk}1`
        fs=`lsblk -f | grep $disk_dev_name | awk -F " " '{print $2}'`
    else
        fs="ext4"
    fi

    if [ ! -b ${disk}1 ] || [ "$fs" != "ext4" ]; then
        #sudo dd if=/dev/zero of=$disk bs=1024k count=10 >/dev/null 2>&1
        sudo fdisk $disk >/dev/null 2>&1 <<__EOF__
        d
        n
        p
        1


        w
__EOF__
        sleep 5
        if [ ! -b ${disk}1 ]; then
            return -1
        fi

        sudo mkfs.ext4 ${disk}1 >/dev/null 2>&1
    fi

    sudo mkdir -p /media/econe/SATA
    sudo mount -o discard,noquota,noacl -t ext4 ${disk}1 /media/econe/SATA >/dev/null 2>&1
    sudo chmod 777 /media/econe/SATA


