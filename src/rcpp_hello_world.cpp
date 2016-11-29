
#include <Rcpp.h>
#include "SeqLib/FastqReader.h"
using namespace Rcpp;
using namespace SeqLib;

// [[Rcpp::export]]
List rcpp_hello_world() {

    CharacterVector x = CharacterVector::create( "foo", "bar" )  ;
    NumericVector y   = NumericVector::create( 0.0, 1.0 ) ;
    List z            = List::create( x, y ) ;

    return z ;
}

// ################## S
// ##################  E
// ##################   Q
// ##################    L
// ##################     I
// ##################      B


// [[Rcpp::export]]
bool fastqReader_Open(const std::string& f) {
  // initialize object of type FastqReader;
  SeqLib::FastqReader instance; 
  
  // check to see if file is available;
  bool available = instance.Open(f);

  return(available);
  //bool available FastqReader::Open(f);
}

bool fastqReader_GetNextSequence(struct UnalignedSequence f){
  return false; 
}


// ################## F
// ##################  E
// ##################   R
// ##################    M
// ##################     I