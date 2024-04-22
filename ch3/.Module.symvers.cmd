cmd_/home/asuka/ldd/ch3/Module.symvers := sed 's/\.ko$$/\.o/' /home/asuka/ldd/ch3/modules.order | scripts/mod/modpost -m -a  -o /home/asuka/ldd/ch3/Module.symvers -e -i Module.symvers   -T -
