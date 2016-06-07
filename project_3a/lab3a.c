#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

unsigned int super_block_size, super_num_inodes, super_num_blocks, super_inodes_per_group, super_blocks_per_group;
int fd;
int BLOCK_OFFSET(int block) { return  block * super_block_size; }
unsigned int readAtOffset(int blockNum, int offset, int size);
void parseBlocks();
int iterateOverDirectoryBlockGivenBlockNum(unsigned int block_num, unsigned int super_num_inodes, unsigned int super_block_size, unsigned int inode_num, FILE *file);
int iterateOverBitmap(int inode, unsigned int block_num, unsigned int gd_size, int gd, unsigned int num_per_group, FILE *file);
void handleIndirectPointer(unsigned int block_num, FILE *file);

int main(int argc, char **argv) {
  // Check args
  if (argc != 2) {
    fprintf(stderr, "Invalid argument count.\n");
    exit(1);
  }
  if ((fd = open(argv[1], O_RDONLY)) < 0) {
    perror(argv[1]);
    exit(1);  
  }

  parseBlocks();
}

void parseBlocks() {
  // SUPER BLOCK
  //  printf("SUPERBLOCK\n");

  // Block size
  unsigned int super_block_size_shift = readAtOffset(-1, 24, 4);
  super_block_size = 1024 << super_block_size_shift;

  // Magic number
  char super_magic_num_c[2];
  int i;
  for (i = 0; i < 2; i++) {
    pread(fd, &super_magic_num_c[i], 1, BLOCK_OFFSET(1) + 56 + i);
  }
  unsigned int super_magic_num = *(int *)super_magic_num_c & 0xFFFF;
  
  // Number of inodes
  super_num_inodes = readAtOffset(1, 0, 4);
  //  printf("num inodes: %u\n", super_num_inodes); 

  // Number of blocks
  super_num_blocks = readAtOffset(1, 4, 4);
  //  printf("num blocks: %u\n", super_num_blocks); 

  // Fragment size
  unsigned int super_fragment_size_shift = readAtOffset(1, 28, 4);
  unsigned int super_fragment_size = 1024 << super_fragment_size_shift;
  //  printf("fragment size: %u\n", super_fragment_size); 

  // Blocks per group
  super_blocks_per_group = readAtOffset(1, 32, 4);
  //  printf("blocks per group: %u\n", super_blocks_per_group); 

  // Inodes per group
  super_inodes_per_group = readAtOffset(1, 40, 4);
  //  printf("inodes per group: %u\n", super_inodes_per_group); 

  // Fragments per group
  unsigned int super_fragments_per_group = readAtOffset(1, 36, 4);
  //  printf("fragments per group: %u\n", super_fragments_per_group); 

  // First data block
  int first_data_block;
  if (super_block_size >= 1000)
    first_data_block = 1;
  else
    first_data_block = 0;
  //  printf("first data block: %d\n", first_data_block);

  // first non reserved inode
  unsigned int first_inode = readAtOffset(1, 84, 4);
  //  printf("first unreserved inode: %u\n", first_inode);
  
  // Make super.csv file
  FILE *superfd = fopen("super.csv", "w");
  fprintf(superfd, "%x,%u,%u,%u,%u,%u,%u,%u,%u\n", super_magic_num, super_num_inodes, super_num_blocks, super_block_size, super_fragment_size, super_blocks_per_group, super_inodes_per_group, super_fragments_per_group, first_data_block);
  fclose(superfd);
  //  printf("END SUPERBLOCK\n\n");

  //  printf("GROUP DESCRIPTOR\n");
  int num_gd = 1 + (super_num_blocks - 1) / super_blocks_per_group;
  int gd_size = 32;

  // Make group/csv
  FILE *groupfd = fopen("group.csv", "w");

  for (i = 0; i < num_gd; i++) {
    unsigned int gd_num_contained_blocks = super_blocks_per_group;
    if (i == num_gd - 1)
      gd_num_contained_blocks = super_num_blocks % super_blocks_per_group;
    unsigned int gd_num_free_blocks = readAtOffset(2, 12 + gd_size * i, 1);
    gd_num_free_blocks &= 0xFFFF;
    //    printf("num free blocks: %u\n", gd_num_free_blocks);
    unsigned int gd_num_free_inodes = readAtOffset(2, 14 + gd_size * i, 1);
    gd_num_free_inodes &= 0xFFFF;
    //    printf("num free inodes: %u\n", gd_num_free_inodes);
    unsigned int gd_num_dir = readAtOffset(2, 16 + gd_size * i, 1);
    gd_num_dir &= 0xFFFF;
    //    printf("num dir: %u\n", gd_num_dir);
    unsigned int gd_inode_bm_block = readAtOffset(2, 4 + gd_size * i, 4);
    //    printf("free inode bitmap block: 0x%02x\n", gd_inode_bm_block);
    unsigned int gd_block_bm_block = readAtOffset(2, gd_size * i, 4);
    //    printf("free block bitmap block: 0x%02x\n", gd_block_bm_block);
    unsigned int gd_inode_table_start = readAtOffset(2, 8 + gd_size * i, 4);
    //    printf("inode table start block: 0x%02x\n\n", gd_inode_table_start);
    fprintf(groupfd, "%u,%u,%u,%u,%x,%x,%x\n", gd_num_contained_blocks, gd_num_free_blocks, gd_num_free_inodes, gd_num_dir, gd_inode_bm_block, gd_block_bm_block, gd_inode_table_start);
  }
  fclose(groupfd);
  //  printf("END GROUP DESCRIPTOR\n\n");
  
  //  printf("BITMAP\n");
  FILE *bitmapfd = fopen("bitmap.csv", "w");
  int gd;
  for (gd = 0; gd < num_gd; gd++) {
    int byte_index;
    // Block bitmap
    unsigned int bm_block_num_block = readAtOffset(2, gd_size * gd, 4);
    iterateOverBitmap(-1, bm_block_num_block, gd_size, gd, super_blocks_per_group, bitmapfd);

    // Inode bitmap
    unsigned int bm_block_num_inode = readAtOffset(2, 4 + gd_size * gd, 4);
    iterateOverBitmap(-1, bm_block_num_inode, gd_size, gd, super_inodes_per_group, bitmapfd);
  }
  fclose(bitmapfd);
  //  printf("END BITMAP\n\n");
  
  //  printf("INODE\n");
  FILE *inodefd = fopen("inode.csv", "w");
  unsigned int inode_size = readAtOffset(1, 88, 1) & 0xFFFF;
  int inode_num;
  
  for (inode_num = 1; inode_num <= super_num_inodes; inode_num++) {
    int block_group = (inode_num - 1) / super_inodes_per_group;
    unsigned int inode_table_start = readAtOffset(2, 8 + gd_size * block_group, 4);
    unsigned int gd_inode_bm_block = 0;
    // Check if valid inode
    for (gd = 0; gd < num_gd; gd++) {
      unsigned int gd_inode_table_start = readAtOffset(2, 8 + gd_size * gd, 4);
      if (gd_inode_table_start == inode_table_start) {
	 gd_inode_bm_block = readAtOffset(2, 4 + gd_size * gd, 4);
	 break;
      }
    }
    unsigned int bm_inode = readAtOffset(2, 4 + gd_size * gd, 4);
    int allocated = iterateOverBitmap(inode_num, gd_inode_bm_block, gd_size, gd, super_inodes_per_group, NULL);
    if (!allocated)
      continue;

    unsigned int offset = inode_size * ((inode_num - 1) - super_inodes_per_group * block_group);
    unsigned int inode_data = readAtOffset(inode_table_start, offset, 1);
    unsigned int inode_type = inode_data & 0xF000;

    char inode_type_c;
    switch(inode_type) {
    case 0x4000:
      inode_type_c = 'd';
      break;
    case 0x8000:
      inode_type_c = 'f';
      break;
    case 0xA000:
      inode_type_c = 's';
      break;
    default:
      inode_type_c = '?';
      break;
    }

    unsigned int inode_mode = inode_data & 0xFFFF;
    unsigned int inode_owner = readAtOffset(inode_table_start, 2 + offset, 1);
    inode_owner &= 0xFFFF;
    unsigned int inode_group = readAtOffset(inode_table_start, 24 + offset, 1);
    inode_group &= 0xFFFF;
    unsigned int inode_link_num = readAtOffset(inode_table_start, 26 + offset, 1);
    inode_link_num &= 0xFFFF;
    unsigned int inode_creation_time = readAtOffset(inode_table_start, 12 + offset, 4);
    unsigned int inode_modification_time = readAtOffset(inode_table_start, 16 + offset, 4);
    unsigned int inode_access_time = readAtOffset(inode_table_start, 8 + offset, 4);
    unsigned int inode_file_size = readAtOffset(inode_table_start, 4 + offset, 4);
    unsigned int inode_block_count = readAtOffset(inode_table_start, 28 + offset, 4) / 2;
    
    fprintf(inodefd, "%d,%c,%o,%u,%u,%u,%x,%x,%x,%u,%u,", inode_num, inode_type_c, inode_mode, inode_owner, inode_group, inode_link_num, inode_creation_time, inode_modification_time, inode_access_time, inode_file_size, inode_block_count);

    int num_direct_block;
    for (num_direct_block = 0; num_direct_block <= 11; num_direct_block++) {
      int direct_block_num = readAtOffset(inode_table_start, 40 + 4 * num_direct_block + offset, 4);
      fprintf(inodefd, "%x,", direct_block_num);
    }

    unsigned int single_indirect_block_num = readAtOffset(inode_table_start, 88 + offset, 4);
    //    printf("single indirect %02x\n", single_indirect_block_num);
    fprintf(inodefd, "%x,", single_indirect_block_num);

    unsigned int double_indirect_block_num = readAtOffset(inode_table_start, 92 + offset, 4);
    //    printf("double indirect %02x\n", double_indirect_block_num);
    fprintf(inodefd, "%x,", double_indirect_block_num);

    unsigned int triple_indirect_block_num = readAtOffset(inode_table_start, 96 + offset, 4);
    //    printf("triple indirect %02x\n", triple_indirect_block_num);
    fprintf(inodefd, "%x\n", triple_indirect_block_num);
  }
  fclose(inodefd);
  //  printf("END INODES\n");
  
  //  printf("DIRECTORIES\n");

  FILE *directoryfd = fopen("directory.csv", "w");
  for (inode_num = 1; inode_num <= super_num_inodes; inode_num++) {
    int block_group = (inode_num - 1) / super_inodes_per_group;
    unsigned int inode_table_start_block = readAtOffset(2, 8 + gd_size * block_group, 4);
    unsigned int offset = inode_size * ((inode_num - 1) - super_inodes_per_group * block_group);
    unsigned int inode_data = readAtOffset(inode_table_start_block, offset, 1);
    unsigned int inode_type = inode_data & 0xF000;
    
    if (inode_type == 0x4000) {
      // for each direct pointer block of the directory
      int num_direct_p_block;
      int dir_entry_index = 0;
      int num_entries = 0;
      for (num_direct_p_block = 0; num_direct_p_block <= 11; num_direct_p_block++) {
     	int direct_p_block_num = readAtOffset(inode_table_start_block, 40 + 4 * num_direct_p_block + offset, 4);
	// traversing linked list of directory entries)
	if (direct_p_block_num == 0)
	  break;
	int entry_base = 0;
	unsigned int entry_length;
	do {
	  // check if directory is valid/allocated
	  entry_length = readAtOffset(direct_p_block_num, entry_base + 4, 1);
	  entry_length &= 0xFFFF;
	  // stop looking through directory block if no more entries
	  if (entry_length <= 0)
	    break;
	  unsigned int parent_inode_num = readAtOffset(direct_p_block_num, entry_base, 4);
	  unsigned int entry_name_length = readAtOffset(direct_p_block_num, entry_base + 6, 1);
	  entry_name_length &= 0xFF;

	  // get file name
	  int char_index;
	  char name[entry_name_length + 1];
	  pread(fd, &name, entry_name_length, BLOCK_OFFSET(direct_p_block_num) + entry_base + 8);
	  name[entry_name_length] = 0;

	  entry_base += entry_length;
	  if (entry_base % 4 != 0) {
	    entry_base += (4 - entry_base % 4);
	  }
	  if (parent_inode_num == 0 || parent_inode_num > super_num_inodes || strlen(name) == 0) {
	    num_entries++;
	    continue;
	  }

	  fprintf(directoryfd, "%u,%u,%u,%u,%u,\"%s\"\n", inode_num, num_entries, entry_length, entry_name_length, parent_inode_num, name);
	  
	  num_entries++;
	} while (entry_base < 1024);
      } // end direct blocks
    }     
  }
  fclose(directoryfd);
  //  printf("END DIRECTORIES\n");
  
  //  printf("INDIRECT BLOCK ENTRIES\n");

  FILE *indirectfd = fopen("indirect.csv", "w");

  for (inode_num = 1; inode_num <= super_num_inodes; inode_num++) {
    int block_group = (inode_num - 1) / super_inodes_per_group;
    unsigned int inode_table_start_block = readAtOffset(2, 8 + gd_size * block_group, 4);
    unsigned int offset = inode_size * ((inode_num - 1) - super_inodes_per_group * block_group);

    int count = 0;
    unsigned int single_indirect_p_block_num = readAtOffset(inode_table_start_block, 88 + offset, 4);

    if (single_indirect_p_block_num != 0) {
      int entry_num_single = 0;
      int num_single_indirect_p_block;
      for (num_single_indirect_p_block = 0; num_single_indirect_p_block < super_block_size / 4; num_single_indirect_p_block++) {
	unsigned int indirect_block_num = readAtOffset(single_indirect_p_block_num, num_single_indirect_p_block * 4 , 4);
	if (indirect_block_num == 0) {
	  entry_num_single++;
	  continue;
	}
	fprintf(indirectfd, "%x,%d,%x\n", single_indirect_p_block_num, entry_num_single, indirect_block_num);
	entry_num_single++;
      }
     
      single_indirect_p_block_num++;
      
    }

    unsigned int double_indirect_p_block_num = readAtOffset(inode_table_start_block, 92 + offset, 4);
    handleIndirectPointer(double_indirect_p_block_num, indirectfd);
    
    // Doubly indirect pointer block
    if (double_indirect_p_block_num != 0) {
      int num_doubly_indirect_p_block;
      for (num_doubly_indirect_p_block = 0; num_doubly_indirect_p_block < super_block_size / 4; num_doubly_indirect_p_block++) {
	unsigned int indirect_block_num = readAtOffset(double_indirect_p_block_num, num_doubly_indirect_p_block * 4 , 4);
	handleIndirectPointer(indirect_block_num, indirectfd);
      }
    } // end doubly indirect pointer block
    
    unsigned int triple_indirect_p_block_num = readAtOffset(inode_table_start_block, 96 + offset, 4);
    handleIndirectPointer(triple_indirect_p_block_num, indirectfd);

    if (triple_indirect_p_block_num != 0) {
      int num_triple_indirect_p_block;
      for (num_triple_indirect_p_block = 0; num_triple_indirect_p_block < super_block_size / 4; num_triple_indirect_p_block++) {
	unsigned int indirect_block_num = readAtOffset(double_indirect_p_block_num, num_triple_indirect_p_block * 4 , 4);
	if (indirect_block_num == 0)
	  continue;
	int num_indirect;
	for (num_indirect = 0; num_indirect < super_block_size / 4; num_indirect++) {
	  unsigned int block_num = readAtOffset(indirect_block_num, num_indirect * 4 , 4);
	  handleIndirectPointer(block_num, indirectfd);
	}
      }
    }
  } // end for each inode for indirect pointers
    
  fclose(indirectfd);
  //  printf("END INDIRECT BLOCK ENTRIES\n");
  
}

unsigned int readAtOffset(int blockNum, int offset, int size) {
  char read_c[size + 1];
  int i;
  int block = blockNum;
  // if first time getting block size
  if (blockNum == -1)
    block = 1;
  for (i = 0; i <= size; i++) {
    pread(fd, &read_c[i], 1, BLOCK_OFFSET(block) + offset + i);
  }
  unsigned int num = *(int *)read_c;
  return num;
}

int iterateOverDirectoryBlockGivenBlockNum(unsigned int block_num, unsigned int super_num_inodes, unsigned int super_block_size, unsigned int inode_num, FILE *file) {
  // traversing linked list of directory entries)
  int num_entries = 0;
  int entry_base = 0;
  if (block_num == 0)
    return num_entries;

  unsigned int entry_length;
  do {
    // check if directory is valid/allocated
    entry_length = readAtOffset(block_num, entry_base + 4, 1);
    entry_length &= 0xFFFF;
    // stop looking through directory block if no more entries
    if (entry_length <= 0)
      return num_entries;
    unsigned int parent_inode_num = readAtOffset(block_num, entry_base, 4);
    unsigned int entry_name_length = readAtOffset(block_num, entry_base + 6, 1);
    entry_name_length &= 0xFF;

    // get file name
    int char_index;
    char name[entry_name_length + 1];
    pread(fd, &name, entry_name_length, BLOCK_OFFSET(block_num) + entry_base + 8);
    name[entry_name_length] = 0;

    entry_base += entry_length;
    if (entry_base % 4 != 0) {
      entry_base += (4 - entry_base % 4);
    }
    if (parent_inode_num == 0 || parent_inode_num > super_num_inodes || strlen(name) == 0) {
      continue;
    } 
    if (file != NULL)
      fprintf(file, "%u,%u,%u,%u,%u,\"%s\"\n", inode_num, num_entries, entry_length, entry_name_length, parent_inode_num, name);
	  
    num_entries++;
  } while (entry_base < 1024);

  return num_entries;
}

// If inode parameter is -1 then we aren't looking for any specific inode, so just output file
int iterateOverBitmap(int inode, unsigned int block_num, unsigned int gd_size, int gd, unsigned int num_per_group, FILE *file) {
  // Inode bitmap
  // Loop over each byte in block bitmap and look at each byte's bits for
  // free/allocated blocks
  int byte_index;
  for (byte_index = 0; byte_index < super_block_size; byte_index++) {
    unsigned int bitmap = readAtOffset(block_num, byte_index, 1);
    bitmap &= 0xFF;
      
    int bit_index = 0;
    while (bit_index < 8) {
      int index =  1 + bit_index + 8 * byte_index + gd * num_per_group;
      if (inode == index && (bitmap & 1)) {
	return 1; // aka allocated
      }

      if (!(bitmap & 1)) {
	if (inode == -1)
	  fprintf(file, "%x,%u\n", block_num, index);
      }
      bit_index++;
      bitmap = bitmap >> 1;
    }
  }
  return 0;
  // end inodes bitmap
}

void handleIndirectPointer(unsigned int block_num, FILE *file) {
  if (block_num != 0) {
    int entry_num_single = 0;
    int num_single_indirect_p_block;
    for (num_single_indirect_p_block = 0; num_single_indirect_p_block < super_block_size / 4; num_single_indirect_p_block++) {
      unsigned int d_block_num = readAtOffset(block_num, num_single_indirect_p_block * 4 , 4);
      if (d_block_num == 0)
	continue;
      fprintf(file, "%x,%d,%x\n", block_num, entry_num_single, d_block_num);
      entry_num_single++;
    } 
  }
}
