cmd_/home/asuka/ldd/ch3/modules.order := {   echo /home/asuka/ldd/ch3/scull.ko; :; } | awk '!x[$$0]++' - > /home/asuka/ldd/ch3/modules.order
