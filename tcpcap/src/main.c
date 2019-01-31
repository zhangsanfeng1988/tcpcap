#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pcap.h>
#include <pthread.h>
#include <errno.h>
#include <syslog.h>
#include <stdarg.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <dirent.h>

#include "debug.h"
#include "header.h"
#include "analysis.h"
#include "swap.h"


void packet_handler(u_char *user, const struct pcap_pkthdr *pkt_header, const u_char *pkt_data)
{
	pcap_dump(user, pkt_header, pkt_data);
	//printf("Jacked a packet with length of [%d]\n", pkt_header->len);
}



void packet_capture(void)
{
    pcap_t *handle;
	pcap_dumper_t* out_pcap;
	char *dev;

    char errbuf[PCAP_ERRBUF_SIZE];
	char debug_file[1024];

    bpf_u_int32 mask;
    bpf_u_int32 net;
	struct bpf_program filter;
    //char filter_app[] = "ip and tcp and not port 22";
	char filter_app[] = "ip and tcp";

	dev = pcap_lookupdev(errbuf);
    pcap_lookupnet(dev, &net, &mask, errbuf);

    handle = pcap_open_live(dev, BUFSIZ, 1, 0, errbuf);

    pcap_compile(handle, &filter, filter_app, 0, net);
    pcap_setfilter(handle, &filter);

	time_t current_time = time(NULL);
	sprintf(debug_file,"./%ld.pcap",current_time);
    out_pcap  = pcap_dump_open(handle,debug_file);

    pcap_loop(handle,30,packet_handler,(u_char *)out_pcap);

    pcap_dump_flush(out_pcap);

    pcap_close(handle);
    pcap_dump_close(out_pcap);
}

void *capture_loop(void)
{
	while(1) {
		packet_capture();
		printf("hello\n");
	}
	return NULL;
}

void pcap_file_compile(char *file_name)
{
    int res;
	long length;
    FILE *fp;
	sniff_file_header pfh;
	sniff_header ph;
	u_char buff[1024*10];

    fp = fopen(file_name, "r");
    if(fp == NULL) {
		printf("fail to open! \n");
		goto err;
	}

	fseek(fp, 0, SEEK_END);
    length = ftell(fp);
	if (length < 24){
		printf("file is too small! \n");
		goto err;
	}
	//printf("The pcap file length:%ld\n",length);
    fseek(fp, 0, SEEK_SET);

	res = fread(&pfh, sizeof(sniff_file_header),1, fp);
	if (res <= 0) {
		printf("can't read sniff_file_header struct !\n");
		goto err;
	}

	while(1) {
		res = fread(&ph, sizeof(sniff_header), 1, fp);
		if (res <= 0) {
			printf("can't read sniff_header struct !\n");
			goto err;
		}

		//printf("pcap_pkthdr:%ld\n",sizeof(sniff_header));
		//printf("capture_len:%u\n",ph.capture_len);
		//printf("len:%u\n",ph.len);

		memset(buff,'\0',sizeof(buff));
		res = fread(buff, ph.len, 1, fp);
		if (res <= 0) {
			printf("can't read sniff !\n");
			goto err;
		}
		protocol_analysis(buff,ph.len);
    }

err:
	fclose(fp);
    return;
}

char *find_compile_file(char *FilePath)
{
	DIR *dir = NULL;
	struct dirent *entry;
	char *ptr;
	static char farth_file[1204];
	int num;
	
	dir = opendir(FilePath);
	if (dir == NULL) {
		printf("opendir failed!");
		return NULL;
	}

	num = 0;
	memset(farth_file,0xff,1024);
	for(;;) {
		entry = readdir(dir);
		if (entry == NULL){
			break;
		}
		ptr = strstr(entry->d_name,".pcap");
		if (ptr) {
			if (strcmp(farth_file,entry->d_name)>0){
				strcpy(farth_file,entry->d_name);
			}
			num++;
			//printf("%s\n",entry->d_name);
		}
	}
	closedir(dir);

	if (num >= 2) {
		return farth_file;
	}

	return NULL;
}

void *compile_loop(void)
{
	char *file;
	char cmd[1024];
	while(1) {
		file = find_compile_file("./");
		if (file != NULL) {
			pcap_file_compile(file);
			printf("                       %s",file);
			sprintf(cmd,"rm -rf %s",file);
			system(cmd);
		}
		sleep(1);
	}
	return NULL;
}

int main()
{
	int result;
    pthread_t tid,sid;

	
    result = pthread_create(&tid, NULL, (void *)capture_loop, NULL);
    if (result != 0) {
        debug(LOG_ERR, "FATAL: Failed to create a new thread (wdctl) - exiting");
    }

    result = pthread_create(&sid, NULL, (void *)compile_loop, NULL);
    if (result != 0) {
        debug(LOG_ERR, "FATAL: Failed to compile the capture file");
    }

	pthread_join(sid, NULL);
    pthread_join(tid, NULL);

	return 1;
}