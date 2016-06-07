#!/usr/bin/python

import sys

###############################################
# CLASS DECLARATIONS
###############################################

class SB:
    def __init__(self, magic_number, inode_count, block_count, block_size, fragment_size, blocks_per_group, inodes_per_group, fragments_per_group, first_data_block):
                 self.magic_number = magic_number
                 self.inode_count = inode_count
                 self.block_count = block_count
                 self.block_size = block_size
                 self.fragment_size = fragment_size
                 self.blocks_per_group = blocks_per_group
                 self.inodes_per_group = inodes_per_group
                 self.fragments_per_group = fragments_per_group
                 self.first_data_block = first_data_block
    def __str__(self):
        return "%u %u %u %u %u %u %u %u %u" % (self.magic_number, self.inode_count, self.block_count, self.block_size, self.fragment_size, self.blocks_per_group, self.inodes_per_group, self.fragments_per_group, self.first_data_block)

class GD:
    def __init__(self, contained_block_count, free_block_count, free_inode_count, directory_count, inode_bitmap, block_bitmap, inode_table):
                 self.contained_block_count = contained_block_count
                 self.free_block_count = free_block_count
                 self.free_inode_count = free_inode_count
                 self.directory_count = directory_count
                 self.inode_bitmap = inode_bitmap
                 self.block_bitmap = block_bitmap
                 self.inode_table = inode_table

    def __str__(self):
        return "%u %u %u %u %u %u %u" % (self.contained_block_count, self.free_block_count, self.free_inode_count, self.directory_count, self.inode_bitmap, self.block_bitmap, self.inode_table);

    
class BM:
    def __init__(self, block_number, block_inode_number):
                 self.block_number = block_number
                 self.block_inode_number = block_inode_number
         
    def __str__(self):
        return "%u %u" % (self.block_number, self.block_inode_number);

class IN:
    def __init__(self, inode_number, file_type, mode, owner, group, link_count, creation_time, modification_time, access_time, file_size, num_blocks, block_pointers):
                 self.inode_number = inode_number
                 self.file_type = file_type
                 self.mode = mode
                 self.owner = owner
                 self.group = group
                 self.link_count = link_count
                 self.creation_time = creation_time
                 self.modification_time = modification_time
                 self.access_time = access_time
                 self.file_size = file_size
                 self.num_blocks = num_blocks
                 self.block_pointers = block_pointers
    def __str__(self):
        inode = "%u,%s,%o,%u,%u,%u,%x,%x,%x,%u,%u," % (self.inode_number, self.file_type, self.mode, self.owner, self.group, self.link_count, self.creation_time, self.modification_time, self.access_time, self.file_size, self.num_blocks)
        for block_pointer in self.block_pointers:
            inode = inode + hex(block_pointer).replace("0x", "") + ','
        inode = inode[:-1]
        return inode
    
class DIR:
    def __init__(self, parent_inode_number, entry_number, entry_length, name_length, file_inode_number, name):
                 self.parent_inode_number = parent_inode_number
                 self.entry_number = entry_number
                 self.entry_length = entry_length
                 self.name_length = name_length
                 self.file_inode_number = file_inode_number
                 self.name = name
           
    def __str__(self):
        return "%u,%u,%u,%u,%u,\"%s\"" % (self.parent_inode_number, self.entry_number, self.entry_length, self.name_length, self.file_inode_number, self.name)

class IB:
    def __init__(self, block_number, entry_number, block_pointer):
                 self.block_number = block_number
                 self.entry_number = entry_number
                 self.block_pointer = block_pointer
         
    def __str__(self):
        return "%x,%u,%x" % (self.block_number, self.entry_number, self.block_pointer);

###############################################
# END CLASS DECLARATIONS
###############################################

def main():
    sb = readSuperblock()
#    print sb
    gds = readGroupDescriptor()
#    for gd_elem in gds:
#        print gd_elem
    bms = readBitmap()
#    for bm_elem in bms:
#        print bm_elem
    inodes = readInodes()
#    for inode in inodes:
#        print inode
    directories = readDirectory()
#    for dir in directories:
#        print dir
    indirect_blocks = readIndirect()
#    for indir in indirect_blocks:
#        print indir

    checkUnallocatedBlocks(sb, gds, bms, inodes, indirect_blocks)
    checkDuplicateBlocks(inodes, indirect_blocks)
    checkUnallocatedInode(directories, inodes)
    checkMissingInodes(sb, gds, inodes, bms)
    findIncorrectLinkCount(inodes, directories)
    findIncorrectDirectoryEntries(inodes, directories)
    checkInvalidBlockPointers(sb, inodes, indirect_blocks)

###############################################
# READ IN FILES
###############################################

def readSuperblock():
    # open and read file
    try:
        with open('super.csv') as file:
            for line in file:
                # convert string into array delimited by ','
                sb_arr = line.split(',')
                # deal with special case of hex
                magic_num = int(sb_arr[0], 16)
                sb_arr.remove(sb_arr[0])
                # for rest of non-hex values convert string to int
                sb_arr = [ int(x) for x in sb_arr ]
                sb = SB(magic_num, sb_arr[0], sb_arr[1], sb_arr[2], sb_arr[3], sb_arr[4], sb_arr[5], sb_arr[6], sb_arr[7])
        return sb
    except IOError as exc:
        print("Cannot open super.csv")
        sys.exit(exc.errno)

def readGroupDescriptor():
    gd_all = []
    try:
        with open('group.csv') as file:
            for line in file:
                group_desc = line.split(',')
                # deal with hex cases
                inode_bitmap = int(group_desc[4], 16)
                block_bitmap = int(group_desc[5], 16)
                inode_table = int(group_desc[6], 16)
                # end hex case
                group_desc = [ int(x) for x in group_desc ]
                gd = GD(group_desc[0], group_desc[1], group_desc[2], group_desc[3], inode_bitmap, block_bitmap, inode_table)
                gd_all.append(gd)
        return gd_all
    except IOError as exc:
        print("Cannot open group.csv")
        sys.exit(exc.errno)


def readBitmap():
    bm_all = []
    try:
        with open('bitmap.csv') as file:
            for line in file:
                bm_arr = line.split(',')
                block_num = int(bm_arr[0], 16)
                block_inode_num = int(bm_arr[1])
                bm = BM(block_num, block_inode_num)
                bm_all.append(bm)
        return bm_all
    except IOError as exc:
        print("Cannot open bitmap.csv")
        sys.exit(exc.errno)


def readInodes():
    inodes_all = []
    try:
        with open('inode.csv') as file:
            for line in file:
                inode_arr = line.split(',')
                inode_num = int(inode_arr[0])
                file_type = inode_arr[1]
                mode = int(inode_arr[2], 8)
                owner = int(inode_arr[3])
                group = int(inode_arr[4])
                link_count = int(inode_arr[5])
                c_time = int(inode_arr[6], 16)
                m_time = int(inode_arr[7], 16)
                a_time = int(inode_arr[8], 16)
                file_size = int(inode_arr[9])
                num_blocks = int(inode_arr[10])
                block_pointer = []
                for block_pointer_index in range(11, 11+15):
                    block_pointer.append(int(inode_arr[block_pointer_index], 16))
                inode = IN(inode_num, file_type, mode, owner, group, link_count, c_time, m_time, a_time, file_size, num_blocks, block_pointer)
                inodes_all.append(inode)
        return inodes_all
    except IOError as exc:
        print("Cannot open inode.csv")
        sys.exit(exc.errno)



def readDirectory():
    dir_all = []
    try:
        with open('directory.csv') as file:
            item = ""
            inName = False
            dir_arr = []
        
            while True:            
                char = file.read(1)
                if not char:
                    return dir_all
            
                if char == ',' and inName is False:
                    dir_arr.append(item)
                    item = ""
                elif char == '"':
                    inName = not inName
                elif char == '\n' and inName is False:
                    name = item
                    item = ""
                    dir_arr = [ int(x) for x in dir_arr ]
                    dir = DIR(dir_arr[0], dir_arr[1], dir_arr[2], dir_arr[3], dir_arr[4], name)
                    dir_all.append(dir)
                    dir_arr = []
                else:
                    item = item + char    

    except IOError as exc:
        print("Cannot open directory.csv")
        sys.exit(exc.errno)

def readIndirect():
    indir_all = []
    try:
        with open('indirect.csv') as file:
            for line in file:
                indir_arr = line.split(',')
                block_num = int(indir_arr[0], 16)
                entry_num = int(indir_arr[1])
                block_pointer = int(indir_arr[2], 16)
                indir = IB(block_num, entry_num, block_pointer)
                indir_all.append(indir)
        return indir_all
    except IOError as exc:
        print("Cannot open indirect.csv")
        sys.exit(exc.errno)


###############################################
# END READ IN FILES
###############################################

###############################################
# COMPARISONS
###############################################

def checkUnallocatedBlocks(super_block, group_desc, bitmap, inodes, indirect):
    # get bitmap for block bitmap only (ignore inode bitmaps)
    # block_bitmap refers to the bitmap entries for free blocks only
    block_bitmap_blocks = []
    for gd in group_desc:
        block_bitmap_blocks.append(gd.block_bitmap)
    block_bitmap = [elem for elem in bitmap if elem.block_number in block_bitmap_blocks]
    
    # check each inode's block pointers for block in free block bitmap
    # for each inode
    #    if the inode has a block pointer to a block that is found in the free block list
    #    then output
    for inode in inodes:
        # direct block pointers
        checkUnallocatedDirectBlocks(inode.block_pointers[0:11], block_bitmap, True, inode.inode_number)
        
        # for only indirect blocks, find all blocks it points to
        # cross reference with bitmap with those direct block ptrs inside indirect block
        # to find invalid blocks
        single_indirect_block_ptr = inode.block_pointers[12]
        if single_indirect_block_ptr != 0:
            # get all direct blocks for this indirect block and compare to bitmap
            direct_blocks = [elem.block_pointer for elem in indirect if single_indirect_block_ptr == elem.block_number]
            checkUnallocatedDirectBlocks(direct_blocks, block_bitmap, False, inode.inode_number)

        double_indirect_block_ptr = inode.block_pointers[13]
        if double_indirect_block_ptr != 0:
            single_ptr_blocks = [elem.block_pointer for elem in indirect if double_indirect_block_ptr == elem.block_number]
            direct_blocks = []
            for elem in indirect:
                if elem.block_number in single_ptr_blocks:
                    direct_blocks.append(elem.block_pointer)
            checkUnallocatedDirectBlocks(direct_blocks, block_bitmap, False, inode.inode_number)

        triple_indirect_block_ptr = inode.block_pointers[14]
        if triple_indirect_block_ptr != 0:
            double_ptr_blocks = [elem.block_pointer for elem in indirect if triple_indirect_block_ptr == elem.block_number]
            single_ptr_blocks = []
            for elem in indirect:
                if elem.block_number in double_ptr_blocks:
                    single_ptr_blocks.append(elem.block_pointer)
            direct_blocks = []
            for elem in indirect:
                if elem.block_number in single_ptr_blocks:
                    direct_blocks.append(elem.block_pointer)
            checkUnallocatedDirectBlocks(direct_blocks, block_bitmap, False, inode.inode_number)
            
        
            
def checkUnallocatedDirectBlocks(direct_blocks, block_bitmap, direct, inode_number):
    check = open('lab3b_check.txt', 'a+')
    block_index = 0
    for block in direct_blocks:
#        if direct == True:
        str = "UNALLOCATED BLOCK < %u > REFERENCED BY INODE < %d >" % (block, inode_number)
#        else:
#            str = "UNALLOCATED BLOCK < %u > REFERENCED BY INODE < %d >" % (block.block_pointer, inode_number)
            
        for bitmap_entry in block_bitmap:
            if block == bitmap_entry.block_inode_number:
                if direct == True:
                    str = str + " ENTRY < %d >" % (block_index)
                else:
                    str = str + " INDIRECT BLOCK < %d  > ENTRY < %d >" % (block.block_number, block.entry_number)
                    
        if "ENTRY" in str or "INDIRECT" in str:
            check.write(str + "\n")
        block_index = block_index + 1

def getAllInodesUsingBlock(inode_number, block_pointers, indirect, block_inode_map):
    block_index = 0
    for block in block_pointers:
        # ignore invalid blocks
        if block == 0:
            continue
        
        if block not in block_inode_map:
            # { inode_number : entry_number }
            block_inode_map[block] = [{ inode_number : block_index }]
        else:
            block_inode_map[block].append({ inode_number : block_index })

        block_index = block_index + 1


def checkDuplicateBlocks(inodes, indirect):
    check = open("lab3b_check.txt", "a+")

    block_inode_map = {}
    
    for inode in inodes:
        getAllInodesUsingBlock(inode.inode_number, inode.block_pointers[0:11], indirect, block_inode_map)

        single_indirect_block_ptr = inode.block_pointers[12]
        if single_indirect_block_ptr != 0:
            # get all direct blocks for this indirect block and compare to bitmap
            direct_blocks = [elem.block_pointer for elem in indirect if single_indirect_block_ptr == elem.block_number]
            getAllInodesUsingBlock(inode.inode_number, direct_blocks, indirect, block_inode_map)

        double_indirect_block_ptr = inode.block_pointers[13]
        if double_indirect_block_ptr != 0:
            single_indirect_blocks = [elem.block_pointer for elem in indirect if double_indirect_block_ptr == elem.block_number]
            direct_blocks = []
            for elem in indirect:
                if elem.block_number in single_indirect_blocks:
                    direct_blocks.append(elem.block_pointer)
            getAllInodesUsingBlock(inode.inode_number, direct_blocks, indirect, block_inode_map)

        triple_indirect_block_ptr = inode.block_pointers[14]
        if triple_indirect_block_ptr != 0:
            double_ptr_blocks = [elem.block_pointer for elem in indirect if triple_indirect_block_ptr == elem.block_number]
            single_ptr_blocks = []
            for elem in indirect:
                if elem.block_number in double_ptr_blocks:
                    single_ptr_blocks.append(elem.block_pointer)
            direct_blocks = []
            for elem in indirect:
                if elem.block_number in single_ptr_blocks:
                    direct_blocks.append(elem.block_pointer)
            getAllInodesUsingBlock(inode.inode_number, direct_blocks, indirect, block_inode_map)


    # print out final list of duplicates
    for key in block_inode_map:
        if len(block_inode_map[key]) > 1:
            str = "MULTIPLY REFERENCED BLOCK < %d > BY" % (key)
            for inode in block_inode_map[key]:
                str = str + " INODE < %d > ENTRY < %d >" % (inode.items()[0][0], inode.items()[0][1])
            str = str + "\n"
            check.write(str)


def checkUnallocatedInode(directories, inodes):
    check = open("lab3b_check.txt", "a+")
    unallocated_inodes = []
    for dir_entry in directories:
        inode_of_dir = dir_entry.file_inode_number
        allocated = False
        for inode in inodes:
            if inode_of_dir == inode.inode_number:
                allocated = True
                break
        if allocated is False:
            unallocated_inodes.append([inode_of_dir, dir_entry.parent_inode_number, dir_entry.entry_number])

    unallocated_inodes.sort(key=lambda x: x[0])

    for elem in unallocated_inodes:
        check.write("UNALLOCATED INODE < %d > REFERENCED BY DIRECTORY < %d > ENTRY < %d >\n" % (elem[0], elem[1], elem[2]))

def checkMissingInodes(super_block, gds, inodes, bitmap):
    check = open("lab3b_check.txt", "a+")

    # get only inode bitmap entries
    inode_bitmap_blocks = []
    for gd in gds:
        inode_bitmap_blocks.append(gd.inode_bitmap)

    inode_bitmap = [elem for elem in bitmap if elem.block_number in inode_bitmap_blocks]
    
    # get all free inodes based on inode.csv content
    possible_inodes = list(range(11, super_block.inode_count))

    used_inodes = []

    free_inodes = []

    for inode in inodes[10:]:
        if inode.link_count == 0:
            free_inodes.append(inode.inode_number)
            possible_inodes.remove(inode.inode_number)
        else:
            used_inodes.append(inode.inode_number)

    for inode in possible_inodes:
        if inode not in used_inodes:
            free_inodes.append(inode)

    free_inodes.sort()
    
    for inode_num in free_inodes:
        if freeInodeInBitmap(inode_bitmap, inode_num):
            continue
        # find which list the missing inode should belong to
        block_group = (inode_num - 1) / super_block.inodes_per_group
        list_num = gds[block_group].inode_bitmap
            
        #if not any(bm_entry.block_inode_number == inode_num for bm_entry in inode_bitmap):
        check.write("MISSING INODE < %d > SHOULD BE IN FREE LIST < %d >\n" % (inode_num, list_num))

def freeInodeInBitmap(inode_bitmap, inode_num):
    for bm_entry in inode_bitmap:
        # stop looking if found free inode in free list
        if bm_entry.block_inode_number == inode_num:
            return True
    return False

def findIncorrectLinkCount(inodes, directories):
    check = open("lab3b_check.txt", "a+")

    # iterate over directories and keep dictionary of inodes
    # and how many directories point to that inode
    dir_pointed = {}
    for dir_entry in directories:
        if dir_entry.file_inode_number not in dir_pointed:
            dir_pointed[dir_entry.file_inode_number] = 1
        else:
            dir_pointed[dir_entry.file_inode_number] = dir_pointed[dir_entry.file_inode_number] + 1

    # iterate over inodes and compare link count w/ above dictionary
    for inode in inodes:
        if inode.inode_number in dir_pointed and inode.link_count != dir_pointed[inode.inode_number]:
            check.write("LINKCOUNT < %d > IS < %d > SHOULD BE < %d >\n" % (inode.inode_number, inode.link_count, dir_pointed[inode.inode_number]))

def findIncorrectDirectoryEntries(inodes, directories):
    check = open("lab3b_check.txt", "a+")

    # make dictionary for correct parent tracking if file is directory and name not . or ..
    #    { file_inode_num : parent_inode: <parent_inode_num> }
    #
    # for only . and .. entries in directory.csv, x
    #    check . entries that parent and file inode match (.)
    #    get file inode number of x (..)
    #    for each directory in that inode thats not . and ..
    #       check if the file inode number is equal to x's parent inode
    #    if reached here, then theres an issue with this entry x
    parent_mapping = {}
    dir_inode_num = []
    for inode in inodes:
        if inode.file_type == 'd':
            dir_inode_num.append(inode.inode_number)

    # make dictionary showing the correct parent inode for a directory
    for dir_entry in directories:
        if dir_entry.file_inode_number in dir_inode_num:
            if dir_entry.name != '.' and dir_entry.name != '..':
                parent_mapping[dir_entry.file_inode_number] = dir_entry.parent_inode_number
    # handle root directory entry whose parent inode is itself
    for dir_num in dir_inode_num:
        if dir_num not in parent_mapping:
            parent_mapping[dir_num] = dir_num

    # find the . and .. entries whose parent linking doesn't match the expected in parent_mapping
    nav_dirs = [dir_entry for dir_entry in directories if dir_entry.name == '.' or dir_entry.name == '..']
    for dir_entry in  nav_dirs:
        if dir_entry.name == '.':
            if dir_entry.parent_inode_number != dir_entry.file_inode_number:
                check.write("INCORRECT ENTRY IN < %d > NAME < . > LINK TO < %d > SHOULD BE < %d >\n" % (dir_entry.parent_inode_number, dir_entry.file_inode_number, dir_entry.parent_inode_number))
        else:
            if dir_entry.file_inode_number != parent_mapping[dir_entry.parent_inode_number]:
                check.write("INCORRECT ENTRY IN < %d > NAME < .. > LINK TO < %d > SHOULD BE < %d >\n" % (dir_entry.parent_inode_number, dir_entry.file_inode_number, parent_mapping[dir_entry.parent_inode_number]))
            
def checkInvalidBlockPointers(super_block, inodes, indirect):
    check = open("lab3b_check.txt", "a+")

    for inode in inodes[10:]:
        str = ""
        # check if 0 block in middle of allocated blocks
        for i in range(11):
            if inode.block_pointers[i] == 0 and inode.block_pointers[i+1] != 0:
                check.write("INVALID BLOCK < %d > IN INODE < %d > ENTRY < %d >\n" % (inode.block_pointers[i], inode.inode_number, i))
                
        # direct blocks
        block_index = 0
        for block_ptr in inode.block_pointers[0:11]:
            if block_ptr >= super_block.block_count:
                check.write("INVALID BLOCK < %d > IN INODE < %d > ENTRY < %d >\n" % (block_ptr, inode.inode_number, block_index))
            block_index = block_index + 1

        
        for i in range(12,14):
            if inode.block_pointers[i] == 0 and inode.block_pointers[i+1] != 0:
                check.write("INVALID BLOCK < %d > IN INODE < %d > ENTRY < %d >\n" % (inode.block_pointers[i], inode.inode_number, i))

        # single indirect block
        single_indirect_block_ptr = inode.block_pointers[12]
        #if single_indirect_block_ptr == 0:
        #    print "INVALID BLOCK < %d > IN INODE < %d > ENTRY < %d >" % (block_ptr, inode.inode_number, 12)
        if single_indirect_block_ptr >= super_block.block_count:
            for entry_num in range(super_block.block_size / 4 - 1):
                check.write("INVALID BLOCK < %d > IN INODE < %d > INDIRECT BLOCK < %d > ENTRY < %d >\n" % (block_ptr, inode.inode_number, 12, entry_num))
        else:
            direct_blocks = [elem.block_pointer for elem in indirect if single_indirect_block_ptr == elem.block_number]
            direct_block_count = 0
            for block in direct_blocks:
                if block >= super_block.block_count:
                    check.write("INVALID BLOCK < %d > IN INODE < %d > ENTRY < %d >\n" % (block_ptr, inode.inode_number, direct_block_count))
                direct_block_count = direct_block_count + 1

        # double indirect block pointer        
        double_indirect_block_ptr = inode.block_pointers[13]
        if double_indirect_block_ptr != 0:
            single_indirect_blocks = [elem.block_pointer for elem in indirect if double_indirect_block_ptr == elem.block_number]
            direct_blocks = []
            for elem in indirect:
                if elem.block_number in single_indirect_blocks:
                    direct_blocks.append(elem.block_pointer)
            direct_block_count = 0
            for block in direct_blocks:
                if block >= super_block.block_count:
                    check.write("INVALID BLOCK < %d > IN INODE < %d > ENTRY < %d >\n" % (block, inode.inode_number, direct_block_count))
                if direct_block_count == super_block.block_count / 4 - 1:
                    direct_block_count = 0
                else:
                    direct_block_count = direct_block_count + 1

        # double indirect block pointer
        triple_indirect_block_ptr = inode.block_pointers[14]
        if triple_indirect_block_ptr != 0:
            double_ptr_blocks = [elem.block_pointer for elem in indirect if triple_indirect_block_ptr == elem.block_number]
            single_ptr_blocks = []
            for elem in indirect:
                if elem.block_number in double_ptr_blocks:
                    single_ptr_blocks.append(elem.block_pointer)
            direct_blocks = []
            for elem in indirect:
                if elem.block_number in single_ptr_blocks:
                    direct_blocks.append(elem.block_pointer)
            direct_block_count = 0
            for block in direct_blocks:
                if block >= super_block.block_count:
                    check.write("INVALID BLOCK < %d > IN INODE < %d > ENTRY < %d >\n" % (block, inode.inode_number, direct_block_count))
                if direct_block_count == super_block.block_count / 4 - 1:
                    direct_block_count = 0
                else:
                    direct_block_count = direct_block_count + 1


                                                                                                    
if __name__ == '__main__':
    main()
