/*
Main program for the virtual memory project.
Make all of your modifications to this file.
You may add or rearrange any code or data as you need.
The header files page_table.h and disk.h explain
how to use the page table and disk interfaces.
*/

#include "page_table.h"
#include "disk.h"
#include "program.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

struct disk *disk;
const char *replace;
int frame_table[100];

void page_fault_handler( struct page_table *pt, int page )
{
	int block = disk_nblocks(disk);
	int frame = 0;
	int bits = 0;
	int i;
	for(i = 0;i < block;i++){
		if(!(frame_table[i])){
			frame = i;
			break;
		}
	}
	if(!(frame)){
		if(!strcmp(replace, "rand")){
			frame = rand() % block;
		} else if(!strcmp(replace, "fifo")) {
			
		} else if(!strcmp(replace, "custom")) {
	
		} else {
			printf("unknown algorithm: %s\n", replace);
			exit(1);
		}
	}
	page_table_get_entry(pt, page, &frame, &bits);
	if(bits&PROT_READ && !(bits&PROT_WRITE)){
		page_table_set_entry(pt, page, frame, PROT_READ|PROT_WRITE);
	} else if (bits&PROT_WRITE) {
		int npage = rand() % page_table_get_npages(pt);
		char * mem = page_table_get_physmem(pt);
		disk_write(disk, npage, &mem[3*block]);
		disk_read(disk, page, &mem[3*block]);
		page_table_set_entry(pt, page, frame, PROT_READ);
		page_table_set_entry(pt, npage, frame, 0);
		frame_table[frame] = 0;
	} else {
		page_table_set_entry(pt, page, frame, PROT_READ);
		char * mem = page_table_get_physmem(pt);
		disk_read(disk, page, &mem[3*block]);
		frame_table[frame] = 1;
	}
	//printf("page fault on page #%d\n",page);
	//exit(1);
}

int main( int argc, char *argv[] )
{
	if(argc!=5) {
		printf("use: virtmem <npages> <nframes> <rand|fifo|lru|custom> <sort|scan|focus>\n");
		return 1;
	}

	int npages = atoi(argv[1]);
	int nframes = atoi(argv[2]);
	replace = argv[3];
	const char *program = argv[4];

	disk = disk_open("myvirtualdisk",npages);
	if(!disk) {
		fprintf(stderr,"couldn't create virtual disk: %s\n",strerror(errno));
		return 1;
	}

	struct page_table *pt = page_table_create( npages, nframes, page_fault_handler );
	if(!pt) {
		fprintf(stderr,"couldn't create page table: %s\n",strerror(errno));
		return 1;
	}

	char *virtmem = page_table_get_virtmem(pt);

	char *physmem = page_table_get_physmem(pt);

	if(!strcmp(program,"sort")) {
		sort_program(virtmem,npages*PAGE_SIZE);

	} else if(!strcmp(program,"scan")) {
		scan_program(virtmem,npages*PAGE_SIZE);

	} else if(!strcmp(program,"focus")) {
		focus_program(virtmem,npages*PAGE_SIZE);

	} else {
		fprintf(stderr,"unknown program: %s\n",argv[3]);
		return 1;
	}

	page_table_delete(pt);
	disk_close(disk);

	return 0;
}
