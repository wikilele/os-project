

main: main.o pcb.o tcb.o msg.o ssi.o handlers.o p2test.o 
	arm-none-eabi-gcc --specs=nosys.specs -T    /usr/include/uarm/ldscripts/elf32ltsarm.h.uarmcore.x -o main \
	 /usr/include/uarm/crtso.o /usr/include/uarm/libuarm.o pcb.o tcb.o msg.o ssi.o handlers.o p2test.o  main.o

pcb.o: pcb.c const.h listx.h mikabooq.h
	arm-none-eabi-gcc -mcpu=arm7tdmi  -I. -c -o pcb.o pcb.c

tcb.o: tcb.c const.h listx.h mikabooq.h
	arm-none-eabi-gcc -mcpu=arm7tdmi  -I. -c -o tcb.o tcb.c

msg.o: msg.c const.h listx.h mikabooq.h
	arm-none-eabi-gcc -mcpu=arm7tdmi  -I. -c -o msg.o msg.c 

ssi.o: utility.h
	arm-none-eabi-gcc -mcpu=arm7tdmi  -I. -c -o ssi.o ssi.c 	

handlers.o: utility.h
	arm-none-eabi-gcc -mcpu=arm7tdmi  -I. -c -o handlers.o handlers.c 

p2test.o: p2test.c uARMconst.h uARMtypes.h libuarm.h nucleus.h
	arm-none-eabi-gcc -mcpu=arm7tdmi  -I. -c -o p2test.o p2test.c 

main.o: utility.h
	arm-none-eabi-gcc -mcpu=arm7tdmi  -I. -c -o main.o main.c 



clean:
	rm main.o msg.o tcb.o pcb.o p2test.o handlers.o ssi.o main
