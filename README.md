# Operating System Project

This is my OS project. It is a microkernel for the arm architecture. This project  was a part of the Operating System exam at UniBo (2016/2017).

# How to run it

You should download the arm emulator at:  
https://github.com/mellotanica/uARM

Follow carefully the instruction to INSTALL it.  
As the README says you will need some packages to run the emulator:

- qmake (qt5-qmake)
- make
- qt >= v5.3.0 (including declarative)
- gcc (g++)
- libelf1
- libelf-dev
- libboost-dev

Then you'll need  to run  
 ```sudo apt-get install gcc-arm-none-eabi```  
so that you can compile the kernel

Finally just run ```make``` on the main directory and load the **main** executable in the uARM emulator (open the configuration panel and add it as **core file**  )