cmd_/home/asuka/ldd/ch2/modules.order := {   echo /home/asuka/ldd/ch2/hello.ko; :; } | awk '!x[$$0]++' - > /home/asuka/ldd/ch2/modules.order
