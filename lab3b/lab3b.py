#!/usr/local/cs/bin/python3

# NAME: Alex Chen
# EMAIL: achen2289@gmail.com
# ID: 005299047

import sys

class FileSystemConsistency:

    def __init__(self, csv_file_name):
        """
        Read in csv input of file system summary, run analysis of blocks, inodes, directories
        """
        try: # Read in csv file
            with open(csv_file_name, "r") as fp:
                self.csv_file = fp.read().splitlines()
        except Exception as e:
            sys.stderr.write("ERROR: Invalid argument")
            sys.exit(1)
        
        self.inconsistency = False # set to True if there are inconsistencies
        self.csv_dict = FileSystemConsistency.convert_csv_to_dict(self.csv_file)

        self.all_free_blocks = [int(line[1]) for line in self.csv_dict["BFREE"]]
        self.all_free_blocks = set(self.all_free_blocks)
        self.all_free_inodes = [int(line[1]) for line in self.csv_dict["IFREE"]]
        self.all_free_inodes = set(self.all_free_inodes)
        self.first_inode = int(self.csv_dict["SUPERBLOCK"][0][7])
        self.last_inode = int(self.csv_dict["GROUP"][0][3])

        self.read_blocks()
        self.read_inodes()
        self.read_directories()

        if self.inconsistency:
            sys.exit(2)
    
    @staticmethod
    def convert_csv_to_dict(csv_file):
        """
        Convert csv input to dictionary for efficient parsing
        """
        # Holds all corresponding lines as a list of lines
        csv_dict = {"SUPERBLOCK": [],
                    "GROUP": [],
                    "BFREE": [],
                    "IFREE": [],
                    "INODE": [],
                    "DIRENT": [],
                    "INDIRECT": []}

        for line in csv_file:
            line = line.split(",")
            csv_dict[line[0]].append(line)
        
        return csv_dict

    def read_blocks(self):
        """
        Analyze blocks of all inodes
        Check for invalid, reserved, unreferenced, allocated, and duplicate blocks
        """
        first_inode_block = int(self.csv_dict["GROUP"][0][8]) # first used inode block
        inodes_per_group = int(self.csv_dict["GROUP"][0][3]) # number of inodes
        inode_size = int(self.csv_dict["SUPERBLOCK"][0][4])
        block_size = int(self.csv_dict["SUPERBLOCK"][0][3])

        # Range of valid data blocks
        first_data_block = int(first_inode_block + inodes_per_group * inode_size / block_size)
        last_data_block = int(self.csv_dict["GROUP"][0][2])

        referenced_data_blocks = [0] * last_data_block # data blocks that are referenced
        duplicate_data_blocks = [[] for _ in range(last_data_block)] # stores block locations in case of duplicates

        # Iterate through each inode, and the blocks within
        for inode in self.csv_dict["INODE"]:
            inode_number = int(inode[1])
            for idx in range(12, 27): # iterate through each type of block
                if idx == 24:
                    block_type = "INDIRECT BLOCK"
                elif idx == 25:
                    block_type = "DOUBLE INDIRECT BLOCK"
                elif idx == 26:
                    block_type = "TRIPLE INDIRECT BLOCK"
                else:
                    block_type = "BLOCK"

                # Determine logical offset values of each type of block
                if idx == 25:
                    offset = 12 + 256
                elif idx == 26:
                    offset = 12 + 256 + 256 * 256
                else:
                    offset = idx - 12
                
                block_number = int(inode[idx])
                if block_number < 0 or block_number > last_data_block:
                    print (f"INVALID {block_type} {block_number} IN INODE {inode_number} AT OFFSET {offset}")
                    self.inconsistency = True
                elif block_number < first_data_block and block_number > 0:
                    print (f"RESERVED {block_type} {block_number} IN INODE {inode_number} AT OFFSET {offset}")
                    self.inconsistency = True
                else:
                    if block_number in self.all_free_blocks:
                        print (f"ALLOCATED {block_type} {block_number} ON FREELIST")
                        self.inconsistency = True
                    duplicate_data_blocks[block_number].append(f"DUPLICATE {block_type} {block_number} IN INODE {inode_number} AT OFFSET {offset}")
                    referenced_data_blocks[block_number] += 1
        
        for indirect_ref in self.csv_dict["INDIRECT"]:
            referenced_data_blocks[int(indirect_ref[5])] += 1

        for block_number in range(first_data_block, last_data_block):
            if referenced_data_blocks[block_number] == 0 and block_number not in self.all_free_blocks:
                print (f"UNREFERENCED BLOCK {block_number}")
                self.inconsistency = True
            
            if len(duplicate_data_blocks[block_number]) > 1:
                for duplicate in duplicate_data_blocks[block_number]:
                    print (duplicate)
                self.inconsistency = True
        
    def read_inodes(self):
        """
        Analyze inodes and check if allocated inodes are present on freelist,
        or if unallocated inodes are not present on freelist
        """
        allocated_inodes = set()
        for inode in self.csv_dict["INODE"]:
            inode_number = int(inode[1])
            if inode_number in self.all_free_inodes:
                print (f"ALLOCATED INODE {inode[1]} ON FREELIST")
                self.inconsistency = True
            allocated_inodes.add(int(inode[1]))
        
        for inode_number in range(self.first_inode, self.last_inode):
            if inode_number not in allocated_inodes and inode_number not in self.all_free_inodes:
                print (f"UNALLOCATED INODE {inode_number} NOT ON FREELIST")
                self.inconsistency = True
    
    def read_directories(self):
        """
        Analyze directories to check that all inode entries are valid and 
        that the link count for each inode is correct
        """
        actual_link_counts = {inode_number: 0 for inode_number in range(self.last_inode)}
        inode_parents = [0] * (self.last_inode)

        allocated_inodes = set()
        for inode in self.csv_dict["INODE"]:
            allocated_inodes.add(int(inode[1]))
        
        for dir_entry in self.csv_dict["DIRENT"]:
            dir_inode = int(dir_entry[1])
            entry_inode = int(dir_entry[3])
            directory_name = dir_entry[6]
            if entry_inode < 1 or entry_inode > self.last_inode:
                print (f"DIRECTORY INODE {dir_inode} NAME {directory_name} INVALID INODE {entry_inode}")
                self.inconsistency = True
            elif entry_inode not in allocated_inodes:
                print (f"DIRECTORY INODE {dir_inode} NAME {directory_name} UNALLOCATED INODE {entry_inode}")
                self.inconsistency = True
            else:
                actual_link_counts[entry_inode] += 1

                if directory_name == "'.'" and dir_inode != entry_inode:
                    print (f"DIRECTORY INODE {dir_inode} NAME {directory_name} LINK TO INODE {entry_inode} SHOULD BE {dir_inode}")
                    self.inconsistency = True
                if directory_name == "'..'" and dir_inode == 2 and dir_inode != entry_inode:
                    print (f"DIRECTORY INODE {dir_inode} NAME {directory_name} LINK TO INODE {entry_inode} SHOULD BE {dir_inode}")
                    self.inconsistency = True
                
                if directory_name != "'.'" and directory_name != "'..'":
                    inode_parents[entry_inode] = dir_inode
        
        for inode in self.csv_dict["INODE"]:
            inode_number = int(inode[1])
            reported_link_count = int(inode[6])
            actual_link_count = actual_link_counts[inode_number]
            if reported_link_count != actual_link_count:
                print (f"INODE {inode_number} HAS {actual_link_count} LINKS BUT LINKCOUNT IS {reported_link_count}")
                self.inconsistency = True
        
        for dir_entry in self.csv_dict["DIRENT"]:
            dir_inode = int(dir_entry[1])
            entry_inode = int(dir_entry[3])
            directory_name = dir_entry[6]
            if dir_inode != 2 and directory_name == "'..'" and int(inode_parents[dir_inode]) != entry_inode:
                print (f"DIRECTORY INODE {dir_inode} NAME {directory_name} LINK TO INODE {entry_inode} SHOULD BE {dir_inode}")
                self.inconsistency = True

def main():
    if len(sys.argv) != 2:
        print ("ERROR: Invalid arguments")
        sys.exit(1)
    
    FileSystemConsistency(str(sys.argv[1]))
    sys.exit(0)
    
if __name__ == "__main__":
    main()