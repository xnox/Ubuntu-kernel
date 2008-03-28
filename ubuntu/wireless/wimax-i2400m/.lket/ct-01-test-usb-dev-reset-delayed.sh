if grep -q 'void usb_dev_reset_delayed' $KSRC/include/linux/usb.h
then
    echo "include/config.h:#undef NEED_USB_DEV_RESET_DELAYED"
else
    echo "include/config.h:#define NEED_USB_DEV_RESET_DELAYED"
fi
