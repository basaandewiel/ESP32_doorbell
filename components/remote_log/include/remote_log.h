//#pragma once

#define LOG_FORMAT_BUF_LEN 2048

int remote_log_init();
int init_udp_client(); //baswi added logging via UDP; this does NOT block execution, when log 
// is not opgevangen (UDP does not send ack)
// while logging via telnet DOES block execution

int remote_log_free();
int udp_client_free(); //baswi added; 