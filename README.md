![](pic.jpg?raw=true) 
Simple project for using DMA on Raspberry Pi for displaying digital clocks with seven-segment displays. In my case, board also has shift register for segments, so I was need to write 8 bit in serial mode then light up one digit etc. That is require a lot of CPU cycles and very precise time mapping (for uniform display illumination) in case of using simple GPIO. DMA allows to solve this task almost without using CPU.  

Based on RPIO - https://pythonhosted.org/RPIO/  

License - GNU Lesser General Public License v3
