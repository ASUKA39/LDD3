cmd_/home/asuka/ldd/ch2/Module.symvers := sed 's/\.ko$$/\.o/' /home/asuka/ldd/ch2/modules.order | scripts/mod/modpost -m -a  -o /home/asuka/ldd/ch2/Module.symvers -e -i Module.symvers   -T -
