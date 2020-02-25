
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/msg.h>

//#include <sys/io.h>             // ioperm
#include <unistd.h>             // ioperm
#include <termio.h>

// Serial inteface headers
#include <sys/select.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "serialport.h"
#include "m2mdbg.h"


int OpenSerial(char *dev_name, int baudrate, int flowctrl)
{
  int h;

	h = open(dev_name, O_RDWR | O_NOCTTY/* | O_NONBLOCK */);       // open device
	if (h == -1) {
		DEBUG(MSG_ERROR, "Couldn't open serial port!\n");
		return -1;
	}

	if (isatty(h) != 1) {
		DEBUG(MSG_ERROR, "Invalid terminal device\n");
		close(h);
		return -1;
	}

	stty_raw(h, baudrate, flowctrl);		// configure the device 

  DEBUG(MSG_HIGH, "Success to open %s\r\n", dev_name);

  return h;
}

void CloseSerial(int h)
{
  if (h > 0)
  {
    close(h);
    DEBUG(MSG_HIGH, "COM Port Closed!\r\n");
  }
}

int WriteSerial(int h, BYTE *szString, int nLen)
{
	int size = 0;
	int maxfd = 0;
	int s = 0;
	fd_set fdset;

	size = nLen;

	FD_ZERO(&fdset);
	FD_SET(h, &fdset);

	maxfd = h + 1;
	select(maxfd, 0, &fdset, 0, NULL);    // block until the data arrives

	if (FD_ISSET(h, &fdset) == FALSE)    // data ready?
	{
		DEBUG(MSG_ERROR, "Not Ready (W)\n");
		return 0;						  //writen continues trying
	}

	s = write(h, szString, size);            // dump the data to the device
	fsync(h); 							  // sync the state of the file

  return s;
}



int ReadSerial(int h, char *data, int size_data) {
  return read(h, data, size_data);
}

static speed_t getBaudrate(int baudrate)
{
	switch(baudrate) {
	case 0: return B0;
	case 50: return B50;
	case 75: return B75;
	case 110: return B110;
	case 134: return B134;
	case 150: return B150;
	case 200: return B200;
	case 300: return B300;
	case 600: return B600;
	case 1200: return B1200;
	case 1800: return B1800;
	case 2400: return B2400;
	case 4800: return B4800;
	case 9600: return B9600;
	case 19200: return B19200;
	case 38400: return B38400;
	case 57600: return B57600;
	case 115200: return B115200;
	case 230400: return B230400;
	case 460800: return B460800;
	case 500000: return B500000;
	case 576000: return B576000;
	case 921600: return B921600;
	case 1000000: return B1000000;
	case 1152000: return B1152000;
	case 1500000: return B1500000;
	case 2000000: return B2000000;
	case 2500000: return B2500000;
	case 3000000: return B3000000;
	case 3500000: return B3500000;
	case 4000000: return B4000000;
	default: return B115200;
	}
}


void rts_on(int h) {
	int j = 0;
	/* Get command register status bits */
	if (ioctl(h, TIOCMGET, &j) < 0) {
		DEBUG(MSG_ERROR, "Error getting hardware status bits in serial.c:rts_on()\n");
		return;
	}

	j = j | (TIOCM_RTS);

	/* Send back to port with ioctl */
	ioctl(h, TIOCMSET, &j);
}

void rts_off(int h) {
	int j;
	/* Get command register status bits */
	if (ioctl(h, TIOCMGET, &j) < 0) {
		DEBUG(MSG_ERROR, "Error getting hardware status bits in serial.c:rts_off()\n");
	}

	j = j & (~TIOCM_RTS);

	/* Send back to port with ioctl */
	ioctl(h, TIOCMSET, &j);
}

int cts_check(int h) {
	DEBUG(MSG_HIGH, "check CTS\n");

	int cts;
	// Get command register status bits
	if (ioctl(h, TIOCMGET, &cts) < 0) {
		DEBUG(MSG_ERROR, "Error getting hardware status bits in dts_modem:cts_check()\n");
		return -1 ;
	}

	cts = cts & TIOCM_CTS;

	if (cts > 0) {
		return 1 ;
	}

	return 0 ;
}

//#define CBAUD	0010017
//#define CBAUDEX 0010000

/*
 * serial port configuration
 */
int stty_raw(int h, int baudrate, int flowctrl) {
	struct termios tty_cfg;
	speed_t speed = B115200;

	if (tcgetattr(h, &tty_cfg) < 0) {
	  perror("tcgetattr ");
		exit(-1);
	}

	speed = getBaudrate(baudrate);
	
	tty_cfg.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
	tty_cfg.c_oflag &= ~OPOST;
	tty_cfg.c_lflag &= ~(ICANON | ECHO | ECHONL | ISIG | IEXTEN);
	tty_cfg.c_cflag &= ~(CSIZE | PARENB | (CBAUD|CBAUDEX));
	tty_cfg.c_cflag |= (CS8 | speed);
	if (flowctrl)  tty_cfg.c_cflag |= CRTSCTS;
  else           tty_cfg.c_cflag &= ~CRTSCTS;
	tty_cfg.c_cc[VMIN] = 1;
	tty_cfg.c_cc[VTIME] = 0;

	if (cfsetospeed(&tty_cfg, speed) < 0) {
		perror("cfsetospeed ");
		exit(-1);
	}

	if (tcsetattr(h, TCSAFLUSH, &tty_cfg) < 0) {
		perror("tcsetattr ");
		exit(-1);
	}

	return 0;
}

