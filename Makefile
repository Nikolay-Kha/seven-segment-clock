CC=arm-linux-gnueabi-gcc
CFLAGS=-O2 -c -Wall -static
LDFLAGS= -O2 -static
SOURCES=main.c pwm.c c_gpio.c
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=frontpanel
IP=192.168.0.212

all: $(SOURCES) $(EXECUTABLE)

clean:
	@rm -f *.o
	
$(EXECUTABLE): $(OBJECTS) 
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

.cpp.o:
	$(CC) $(CFLAGS) $< -o $@

upload: all
	@sshpass -p 'openelec' ssh root@${IP} killall -9 frontpanel | true
	@sshpass -p 'openelec' scp $(EXECUTABLE) root@${IP}:~

deploy: upload
	@sshpass -p 'openelec' ssh -n -f root@${IP} "sh -c 'nohup ./frontpanel > /dev/null 2>&1 &'"
	@echo Successfully Done!
