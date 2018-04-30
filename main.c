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

// struct to represent a frame: contains the page and a count to indicate when it first was placed
// in the frame for the FIFO algorithm
typedef struct {
	int page;
	int recency;
} Frame;

struct disk *disk;
const char *replace;
Frame* frame_table;
int nframes;
int page_faults;
int disk_readN;
int disk_writeN;
int last_replaced;

void page_fault_handler( struct page_table *pt, int page )
{
	page_faults++;
	int block = disk_nblocks(disk);
	int frame = -1;
	int bits = 0;

	int i;
	for(i = 0;i < nframes;i++){
		// first check for an unused frame or if the current page is already in a fram
		if(frame_table[i].recency == 0 || frame_table[i].page == page){
			frame = i;
			break;
		}else{
			// increment the tracker on when a page was placed in a frame
			frame_table[i].recency++;
		}
	}
	if(frame == -1){
		// case if all frames are full
		if(!strcmp(replace, "rand")){
			frame = rand() % nframes;
		} else if(!strcmp(replace, "fifo")) {
			// find oldest frame in use and choose to evict this one
			int i, high = frame_table[0].recency, high_frame = 0; 
			for (i = 1; i < nframes; i++) {
				if (frame_table[i].recency > high) {
					high = frame_table[i].recency;
					high_frame = i;
				}
			}
			frame = high_frame;
		} else if(!strcmp(replace, "custom")) {
			// prioritizes replacing pages with only an R flag over those with the RW flags
			int i, temp_bits, temp;
			for (i=0; i<nframes; i++) {
			 	// find first page with only R flag that's not the last one replaced
			 	page_table_get_entry(pt, frame_table[i].page, &temp, &temp_bits);
				if (bits&PROT_READ && !(bits&PROT_WRITE) && frame_table[i].page != page) {
					frame = i;
					break;
				} 
			}
 			// in case all frames have the RW flag, then just make sure the last one replaced is not picked
			if (frame == -1) {
				if (0 != last_replaced) {
					frame = 0;
				} else {
					frame = 1;
				}
			}
			last_replaced = frame;
		} else {
			printf("unknown algorithm: %s\n", replace);
			exit(1);
		}
	}
	// get the entry for the page in the frame
	int newframe;
	page_table_get_entry(pt, frame_table[frame].page, &newframe, &bits);
	
	if(bits&PROT_READ && !(bits&PROT_WRITE)){
		// update page in a frame if it already exists inside
		page_table_set_entry(pt, page, frame, PROT_READ|PROT_WRITE);
		if(!(strcmp(replace, "custom"))){
			frame_table[newframe].recency = 1;
		}
	} else if (bits&PROT_WRITE) {
		// evict page
		int npage = rand() % page_table_get_npages(pt);
		char * mem = page_table_get_physmem(pt);
		disk_write(disk, npage, &mem[3*block]);
		disk_read(disk, page, &mem[3*block]);
		page_table_set_entry(pt, page, frame, PROT_READ);
		page_table_set_entry(pt, npage, frame, 0);
		frame_table[frame].recency = 1;
		frame_table[frame].page = page;
		disk_readN++;
		disk_writeN++;
	} else {
		// place new page inside the frame table
		page_table_set_entry(pt, page, frame, PROT_READ);
		char * mem = page_table_get_physmem(pt);
		disk_read(disk, page, &mem[3*block]);
		frame_table[frame].recency = 1;
		frame_table[frame].page = page;
		disk_readN++;
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
	nframes = atoi(argv[2]);
	replace = argv[3];
	const char *program = argv[4];

	// open disk and check for errors
	disk = disk_open("myvirtualdisk",npages);
	if(!disk) {
		fprintf(stderr,"couldn't create virtual disk: %s\n",strerror(errno));
		return 1;
	}

	// create page table and check for errors
	struct page_table *pt = page_table_create( npages, nframes, page_fault_handler );
	if(!pt) {
		fprintf(stderr,"couldn't create page table: %s\n",strerror(errno));
		return 1;
	}

	frame_table = (Frame*) calloc(nframes, sizeof(Frame));
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
	printf("%d %d %d\n", page_faults, disk_readN, disk_writeN);
	page_table_delete(pt);
	free(frame_table);
	disk_close(disk);

	return 0;
}
