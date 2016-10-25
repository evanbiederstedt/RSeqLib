//Including Fermi into the R wrapper but, must know what C++ code is used to get it going.

//#include "SeqLib/FermiAssembler.h" // a directive that loads other files (FermiAssembler.h) into this file.

//#include "SeqLib/FermiAssembler.h"
//using namespace SeqLib;

#include <stdio.h>

namespace SeqLib {
  /** Sequence assembly using FermiKit from Heng Li 
   */
  class FermiAssembler {

  public:

    /** Create an empty FermiAssembler with default parameters */
    FermiAssembler();

    /** Destroy by clearing all stored reads from memory */
    ~FermiAssembler();

    /** Provide a set of reads to be assembled
     * @param brv Reads with or without quality scores
     * @note This will copy the reads and quality scores
     * into this object. Deallocation is automatic with object
     * destruction, or with ClearReads. 
     */
    void AddReads(const BamRecordVector& brv); //the parameter being passed is not a copy - this function can manipulate the value of it.


    /** Clear all of the sequences and deallocate memory.
     * This is not required, as it will be done on object destruction
     */
    //void ClearReads();

    /** Clear all of the contigs and deallocate memory. 
     * This is not required, asit will be done on object destruction.
     */
    //void ClearContigs();

    /** Perform Bloom filter error correction of the reads
     * in place. */
    //void CorrectReads();

    /**Perform Bloom filter error correction of the reads
     * in place. Also remove unique reads. */
    //void CorrectAndFilterReads();

    /** Return the sequences in this object, which may have 
     * been error-corrected
     */
  };
}

int main()
{
  printf("Hello World\n");
}

