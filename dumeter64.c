#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ncurses.h>

#define MAX_INTERFACE_NAME (32)
#define MAX_INTERFACES (32)

typedef struct {
	long long int byte_in;
	long long int byte_out;
	long long int prev_byte_in;
	long long int prev_byte_out;
	long long int delta_byte_in;
	long long int delta_byte_out;
	char interface_name[MAX_INTERFACE_NAME];
} interface_info_s;

static interface_info_s interfaces_infos[MAX_INTERFACES];

static interface_info_s *find_interface(const char *if_name){
	int i = MAX_INTERFACES;
	interface_info_s *a_renvoyer = interfaces_infos;
	while(i--){
		if(a_renvoyer->interface_name[0]){
			if(0 == strcmp(if_name, a_renvoyer->interface_name)){
				return(a_renvoyer); // Slot with the same interface name
			}
		}else{
			return(a_renvoyer); // First empty slot
		}
		a_renvoyer++;
	}
	return(NULL);
}

static int update_interface_info(const char *if_name, const char *if_rx_bytes, const char *if_tx_bytes, int first_time){
	interface_info_s *interface = find_interface(if_name);
	if(interface){
		strcpy(interface->interface_name, if_name);
		interface->byte_in = strtoll(if_rx_bytes, NULL, 10);
		interface->byte_out = strtoll(if_tx_bytes, NULL, 10);
		if(first_time){
			interface->prev_byte_in = interface->byte_in;
			interface->prev_byte_out = interface->byte_out;
			interface->delta_byte_in = 0;
			interface->delta_byte_out = 0;
		}else{
			interface->delta_byte_in = interface->byte_in - interface->prev_byte_in;
			interface->delta_byte_out = interface->byte_out - interface->prev_byte_out;
			interface->prev_byte_in = interface->byte_in;
			interface->prev_byte_out = interface->byte_out;
		}
	}
	return(0);
}


int main(int argc, const char *argv[], const char *env[]){
	int refresh_timeout = 1000;
	if(argc > 1){
		if(1 != sscanf(argv[1], "%d", &refresh_timeout)){
			fprintf(stderr, "Invalid timeout specification [%s]" "\n", argv[1]);
			fprintf(stderr, "Usage %s [timeout millisecond]" "\n", argv[0]);
			exit(1);
		}else{
			fprintf(stderr, "timeout = %d" "\n", refresh_timeout);
		}
	}
	memset(interfaces_infos, 0, sizeof(interfaces_infos));
	initscr();			/* Start curses mode 		  */
	raw(); /* line buffering disabled */
	noecho();
	timeout(refresh_timeout);
	refresh();			/* Print it on to the real screen */
	int first_time = 1;
	for(;;){
		clear();
		FILE *net_dev = fopen("/proc/net/dev", "r");
		if(net_dev){
			char ligne[256];
			while(fgets(ligne, sizeof(ligne), net_dev)){
				char *colon = strchr(ligne, ':');
				if(colon){
					*colon = '\0';
					const char *if_name = ligne;
					char *reste_ligne = colon + 1;
					char *saveptr;
					char *token = strtok_r(reste_ligne, " ", &saveptr);
					int index = 0;
					const char *byte_in = "0";
					const char *byte_out = "0";
					while(token){
						if(0 == index){
							byte_in = token;
						}
						if(8 == index){
							byte_out = token;
						}
						token = strtok_r(NULL, " ", &saveptr);
						index++;
					}
					update_interface_info(if_name, byte_in, byte_out, first_time);
				}
			}
			fclose(net_dev);
			interface_info_s *interface = interfaces_infos;
			printw("ifname |         Rx Bytes       |         Rx Rate        |        Tx Bytes        |      Tx Rate           |" "\n");
			while(interface->interface_name[0]){
				char formatted_rx[24];
				char formatted_tx[24];
				char formatted_delta_rx[24];
				char formatted_delta_tx[24];


				void format_number(char *string, int formatted_length, char separator){
					// transform "18446744073709551615" into "18E446P744T073G709M551K615" if('\0' == separator)
					// transform "18446744073709551615" into "18.446.744.073.709.551.615" else
					// transform "44073709551615" into "        44T073G709M551K615" if('\0' == separator)
					// transform "44073709551615" into "        44.073.709.551.615" else if('.' == separator)
					int len = strlen(string);
					string[formatted_length] = '\0';
					char separators[8];
					if('\0' == separator){
						strcpy(separators, "KMGTPE");
					}else{
						memset(separators, separator, 6);
					}
					const char *s = separators;
					char *dst = string + formatted_length;
					char *src = string + len;
					int separator_insert = 0;
					while(len--){
						*(--dst) = *(--src);
						separator_insert++;
						if(3 == separator_insert){
							separator_insert = 0;
							*(--dst) = *s++;
						}
					}
					if(0 == separator_insert){
						dst++;
					}
					while(dst > string){
						*(--dst) = ' ';
					}
				}

				snprintf(formatted_rx, sizeof(formatted_rx), "%lld", interface->byte_in);
				snprintf(formatted_tx, sizeof(formatted_tx), "%lld", interface->byte_out);
				snprintf(formatted_delta_rx, sizeof(formatted_delta_rx), "%lld", (1000 * interface->delta_byte_in)/refresh_timeout);
				snprintf(formatted_delta_tx, sizeof(formatted_delta_tx), "%lld", (1000 * interface->delta_byte_out)/refresh_timeout);
				format_number(formatted_rx, sizeof(formatted_rx) - 1, '.');
				format_number(formatted_tx, sizeof(formatted_tx) - 1, '.');
				format_number(formatted_delta_rx, sizeof(formatted_delta_rx) - 1, '.');
				format_number(formatted_delta_tx, sizeof(formatted_delta_tx) - 1, '.');
				char fifth = interface->interface_name[6];
				interface->interface_name[6] = '\0'; // truncate to 6 digits
				printw("%s |%s |%s |%s |%s |" "\n", interface->interface_name, formatted_rx, formatted_delta_rx, formatted_tx, formatted_delta_tx);
				interface->interface_name[6] = fifth;
				interface++;
			}
		}else{
			break;
		}
		refresh();
		int car = getch();
		if(car > 0){
			if(('Q' == car) || ('q' == car)){
				break;
			}
		}
		first_time = 0;
	}
	endwin();			/* End curses mode		  */
	return(0);
}

