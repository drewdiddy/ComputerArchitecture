#include "tips.h"
#include <math.h>

/* The following two functions are defined in util.c */

/* finds the highest 1 bit, and returns its position, else 0xFFFFFFFF */
unsigned int uint_log2(word w); 

/* return random int from 0..x-1 */
int randomint( int x );

/*
  This function allows the lfu information to be displayed

    assoc_index - the cache unit that contains the block to be modified
    block_index - the index of the block to be modified

  returns a string representation of the lfu information
 */
char* lfu_to_string(int assoc_index, int block_index)
{
  /* Buffer to print lfu information -- increase size as needed. */
	static char buffer[9];
	sprintf(buffer, "%u", cache[assoc_index].block[block_index].accessCount);

	return buffer;
}

/*
  This function allows the lru information to be displayed

    assoc_index - the cache unit that contains the block to be modified
    block_index - the index of the block to be modified

  returns a string representation of the lru information
 */
char* lru_to_string(int assoc_index, int block_index)
{
  /* Buffer to print lru information -- increase size as needed. */
	static char buffer[9];
	sprintf(buffer, "%u", cache[assoc_index].block[block_index].lru.value);

	return buffer;
}

/*
  This function initializes the lfu information

    assoc_index - the cache unit that contains the block to be modified
    block_number - the index of the block to be modified

*/
void init_lfu(int assoc_index, int block_index)
{
	cache[assoc_index].block[block_index].accessCount = 0;
}

/*
  This function initializes the lru information

    assoc_index - the cache unit that contains the block to be modified
    block_number - the index of the block to be modified

*/
void init_lru(int assoc_index, int block_index)
{
	cache[assoc_index].block[block_index].lru.value = 0;
}

/*
  This is the primary function you are filling out,
  You are free to add helper functions if you need them

  @param addr 32-bit byte address
  @param data a pointer to a SINGLE word (32-bits of data)
  @param we   if we == READ, then data used to return
              information back to CPU

              if we == WRITE, then data used to
              update Cache/DRAM
*/
void accessMemory(address addr, word* data, WriteEnable we)
{
  /* Declare variables here */

  /* handle the case of no cache at all - leave this in */
	if(assoc == 0) {
		accessDRAM(addr, (byte*)data, WORD_SIZE, we);
		return;
	}
  /*
  You need to read/write between memory (via the accessDRAM() function) and
  the cache (via the cache[] global structure defined in tips.h)

  Remember to read tips.h for all the global variables that tell you the
  cache parameters

  The same code should handle random, LFU, and LRU policies. Test the policy
  variable (see tips.h) to decide which policy to execute. The LRU policy
  should be written such that no two blocks (when their valid bit is VALID)
  will ever be a candidate for replacement. In the case of a tie in the
  least number of accesses for LFU, you use the LRU information to determine
  which block to replace.

  Your cache should be able to support write-through mode (any writes to
  the cache get immediately copied to main memory also) and write-back mode
  (and writes to the cache only gets copied to main memory when the block
  is kicked out of the cache.

  Also, cache should do allocate-on-write. This means, a write operation
  will bring in an entire block if the block is not already in the cache.

  To properly work with the GUI, the code needs to tell the GUI code
  when to redraw and when to flash things. Descriptions of the animation
  functions can be found in tips.h
  */

  /* Start adding code here */

	unsigned int indexBitNum, offsetBitNum, tagBitNum, index, offset, tag;
	int hit = 0; //false
	int assocNum = 0, Val = 0;
	TransferUnit wordSize;

	offsetBitNum = uint_log2(block_size);
	indexBitNum = uint_log2(set_count);
	tagBitNum = 32 - offsetBitNum - indexBitNum;

	wordSize = uint_log2(block_size);
  	//printf("%d\n", wordSize);

  	//printf("index bits: %u,\toffset bits: %u,\ttag bits: %u\n", indexBitNum, offsetBitNum, tagBitNum);
  	//printf("address: %u\n", addr);

	//to find the values of offset, index, and tag I used this source:
	//https://cboard.cprogramming.com/c-programming/156061-cache-simulator-bit-shifting.html
	offset = addr & ((1 << offsetBitNum) - 1);
	//printf("offset: %u\n", offset);
	index = (addr >> offsetBitNum) & ((1 << indexBitNum) - 1);
	//printf("index: %u\n", offset);
	tag = (addr >> (offsetBitNum + indexBitNum)) & ((1 << tagBitNum) - 1);
	//printf("tag: %u\n", tag);

	//start with write
	if(we == WRITE){
		//cycle through our associativity
		for(int i = 0; i < assoc; i++){
			//if we got a hit (tags match and the block is valid)
			if(tag == cache[index].block[i].tag && cache[index].block[i].valid == VALID){
				hit = 1;	//true
				//source: https://www.tutorialspoint.com/c_standard_library/c_function_memcpy.htm
				//memcpy(dest, src, #ofBits)
				memcpy(cache[index].block[i].data+offset, data, 4);	//set up for write to cache
				cache[index].block[i].dirty = DIRTY;		//dirty bit = 1
				cache[index].block[i].accessCount++;		//increment the access count
				break; 										//got a hit no need to iterate more
			}
		}

		//if we missed
		if(hit == 0){
			//cover our replacement policies (random/LRU)
			if(policy == RANDOM)
				assocNum = randomint(assoc);				//chooses random associativity
			else if(policy == LRU){
				//cycle through associativity
				for(int i = 0; i < assoc; i++){
					/*if(cache[index].block[i].valid == INVALID){
						assocNum = i;
					}
					*/
					if(Val <= cache[index].block[i].accessCount){
						assocNum = i;
						Val = cache[index].block[i].accessCount;
					}
				}
				cache[index].block[assocNum].lru.value++;
			}
			if(cache[index].block[assocNum].dirty == DIRTY){
				cache[index].block[assocNum].dirty = VIRGIN;
				accessDRAM(addr, cache[index].block[assocNum].data, wordSize, WRITE);
			}
			
			cache[index].block[assocNum].tag = tag;			
			cache[index].block[assocNum].valid = VALID;		//we've written to cache, make it valid
			cache[index].block[assocNum].accessCount = 0;	//reset our access count
		}
	}
	//We are READING now
	else{
		for(int i = 0; i < assoc; i++){
			//if we got a hit (tags match and the block is valid)
			if(tag == cache[index].block[i].tag && cache[index].block[i].valid == VALID){
				//assocNum = i;
				hit = 1;
				//memcpy(dest, src, #ofBits)
				memcpy(data, cache[index].block[i].data+offset, 4);		//read
				cache[index].block[i].accessCount++;
				highlight_offset(index, assocNum, offset, HIT);			//this will highlight the offset in green
				break;
			}
		}
		
		//if we missed
		if(hit == 0){
			//cover our replacement policies (random/LRU)
			if(policy == RANDOM){
				assocNum = randomint(assoc);
			}
			else if(policy == LRU){
				for(int i = 0; i < assoc; i++){
					/*if(cache[index].block[i].valid == INVALID){
						assocNum = i;
					}
					*/
					if(Val <= cache[index].block[i].accessCount){
						assocNum = i;
						Val = cache[index].block[i].accessCount;
					}
				}
			}

			cache[index].block[assocNum].tag = tag;
			cache[index].block[assocNum].valid = VALID;
			cache[index].block[assocNum].accessCount = 0;

			accessDRAM(addr, cache[index].block[assocNum].data, wordSize, READ);
			highlight_offset(index, assocNum, offset, MISS);			//this will highlight the offset in red
		}

	}



  /* This call to accessDRAM occurs when you modify any of the
     cache parameters. It is provided as a stop gap solution.
     At some point, ONCE YOU HAVE MORE OF YOUR CACHELOGIC IN PLACE,
     THIS LINE SHOULD BE REMOVED.
  */
//accessDRAM(addr, (byte*)data, WORD_SIZE, we);
}
